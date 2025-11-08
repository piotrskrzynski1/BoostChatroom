#include <Server/ClientServerManager.h>
#include <MessageTypes/Text/TextMessage.h>
#include <Server/ServerMessageSender.h>
#include <iostream>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

void ClientServerManager::handle_connect(const boost::system::error_code& error,
                                         const std::string& socket_name,
                                         const std::shared_ptr<tcp::socket>& socket)
{
    if (error) {
        std::cerr << "Connection failed (" << socket_name << "): " << error.message() << std::endl;
        return;
    }

    std::cout << "Connected to server (" << socket_name << ")\n";
    std::cout << "Server IP: " << socket->remote_endpoint().address()
              << " | Port: " << socket->remote_endpoint().port() << std::endl;

    // Start receiving messages on this socket
    messageReceiver_.start_read_header(socket);
}

ClientServerManager::ClientServerManager(boost::asio::io_context& io) : io_context_(io)
{
        try {
            endpoint = tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 5555);
            endpoint_file = tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 5556);

            client_socket = std::make_shared<tcp::socket>(io_context_);
            client_file_socket = std::make_shared<tcp::socket>(io_context_);

            // Create file queue, pass socket getter
            file_queue_ = std::make_shared<FileTransferQueue>([this]() -> std::shared_ptr<boost::asio::ip::tcp::socket> {
               return this->client_file_socket;
            });

        // Connect for text messages
        client_socket->async_connect(endpoint,
            [this](const boost::system::error_code& ec) {
                this->handle_connect(ec, "TextSocket", client_socket);
            });

        // Connect for file transfers
        client_file_socket->async_connect(endpoint_file,
            [this](const boost::system::error_code& ec) {
                this->handle_connect(ec, "FileSocket", client_file_socket);
            });

    } catch (const std::exception& e) {
        std::cerr << "ClientServerManager init error: " << e.what() << std::endl;
    }
}

void ClientServerManager::Disconnect() const
{
    auto disconnect_socket = [](const std::shared_ptr<tcp::socket>& sock, const std::string& name) {
        if (!sock || !sock->is_open()) {
            std::cout << name << " not connected — no need to disconnect.\n";
            return;
        }

        boost::system::error_code ec;
        sock->cancel(ec);
        if (ec) std::cerr << name << " cancel error: " << ec.message() << std::endl;

        sock->shutdown(tcp::socket::shutdown_both, ec);
        if (ec) std::cerr << name << " shutdown error: " << ec.message() << std::endl;

        sock->close(ec);
        if (ec)
            std::cerr << name << " close error: " << ec.message() << std::endl;
        else
            std::cout << name << " disconnected successfully.\n";
    };

    disconnect_socket(client_socket, "TextSocket");
    disconnect_socket(client_file_socket, "FileSocket");
}

void ClientServerManager::Message(TextTypes type, const std::shared_ptr<IMessage>& message) const
{
    boost::system::error_code err;

    switch (type) {
        case TextTypes::Text:
            if (client_socket && client_socket->is_open()) {
                Utils::SendMessage(client_socket, message, err);
            } else {
                std::cerr << "TextSocket is not connected.\n";
            }
            break;

        case TextTypes::File:
            if (client_file_socket && client_file_socket->is_open()) {
                Utils::SendMessage(client_file_socket, message, err);
            } else {
                std::cerr << "FileSocket is not connected.\n";
            }
            break;

        default:
            std::cerr << "Unknown message type.\n";
            break;
    }

    if (err)
        std::cerr << "SendMessage error: " << err.message() << std::endl;
}

uint64_t ClientServerManager::EnqueueFile(const std::filesystem::path& path) const
{
    if (!file_queue_) {
        std::cerr << "File queue not initialized\n";
        return 0;
    }
    return file_queue_->enqueue(path);
}

void ClientServerManager::CancelAndReconnectFileSocket()
{
    // 1) pause queue so worker won't immediately start new items
    if (file_queue_) file_queue_->pause();

    // 2) mark all queued items as canceled (keeps history)
    if (file_queue_) file_queue_->cancel_all();

    // 3) attempt to cancel/close existing socket so in-flight send is interrupted
    if (client_file_socket) {
        boost::system::error_code ec;
        // cancel pending async ops
        client_file_socket->cancel(ec);
        if (ec) std::cerr << "Cancel socket error: " << ec.message() << std::endl;

        // shutdown then close — ignore errors but log
        client_file_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec && ec != boost::system::errc::not_connected)
            std::cerr << "Shutdown socket error: " << ec.message() << std::endl;

        client_file_socket->close(ec);
        if (ec)
            std::cerr << "Close socket error: " << ec.message() << std::endl;
        else
            std::cout << "File socket closed to abort transfers.\n";
    }

    // 4) create new socket and async_connect it. When it connects messageReceiver_.start_read_header
    // will be started in handle_connect (same as initial connect)
    client_file_socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
    //DONT CALL ANYWHERE ELSE WHEN ITS RECONNECTING OK
    client_file_socket->async_connect(endpoint_file,
    [this](const boost::system::error_code& ec)
    {
        this->handle_connect(ec, "FileSocket", this->client_file_socket);

        if (!ec && this->file_queue_) {
            // Only resume queue after successful connection
            this->file_queue_->resume();
        }
    });

    std::cout << "File transfer queue cancelled and file socket reconnecting...\n";
}


std::vector<FileTransferQueue::Item> ClientServerManager::FileQueueSnapshot() const
{
    if (file_queue_) return file_queue_->list_snapshot();
    return {};
}

void ClientServerManager::PauseQueue() const
{
    if (file_queue_) file_queue_->pause();
}

void ClientServerManager::ResumeQueue() const
{
    if (file_queue_) file_queue_->resume();
}

void ClientServerManager::CancelFile(uint64_t id) const
{
    if (file_queue_) file_queue_->cancel(id);
}

void ClientServerManager::RetryFile(uint64_t id) const
{
    if (file_queue_) file_queue_->retry(id);
}


