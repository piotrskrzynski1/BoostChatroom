#include <Server/ClientServerConnectionManager.h>
#include <MessageTypes/Text/TextMessage.h>
#include <MessageTypes/File/FileMessage.h>
#include <Server/MessageSender.h>
#include <iostream>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

void ClientServerConnectionManager::handle_connect(
    const boost::system::error_code& error,
    const std::string& socket_name,
    const std::shared_ptr<tcp::socket>& socket)
{
    if (error)
    {
        std::cerr << "Connection error (" << socket_name << "): " << error.message() << std::endl;
        return;
    }

    std::cout << "Connected to server (" << socket_name << ")\n";
    std::cout << "Server IP: " << socket->remote_endpoint().address()
              << " | Port: " << socket->remote_endpoint().port() << std::endl;

    // Start the correct receiver for the correct socket
    if (socket_name == "TextSocket")
    {
        textMessageReceiver_.start_read_header(socket);
    }
    else if (socket_name == "FileSocket")
    {
        fileMessageReceiver_.start_read_header(socket);
    }
}

ClientServerConnectionManager::ClientServerConnectionManager(
    boost::asio::io_context& io, std::string ip,
    unsigned short int textport,
    unsigned short int fileport) : io_context_(io)
{
    try
    {
        endpoint = tcp::endpoint(boost::asio::ip::make_address(ip), textport);
        endpoint_file = tcp::endpoint(boost::asio::ip::make_address(ip), fileport);

        client_socket = std::make_shared<tcp::socket>(io_context_);
        client_file_socket = std::make_shared<tcp::socket>(io_context_);

        // Create file queue
        file_queue_ = std::make_shared<FileTransferQueue>([this]() -> std::shared_ptr<boost::asio::ip::tcp::socket>
        {
            return this->client_file_socket;
        });

        // Configure the callbacks for both receiver instances

        // 1. Configure the TEXT receiver
        // Now we just need to set the callback that TextMessage.handle() will invoke
        textMessageReceiver_.register_handler(TextTypes::Text,
        [this](const std::shared_ptr<tcp::socket>& sender, std::shared_ptr<IMessage> msg)
            {
                // Display the message
                const auto textMsg = std::dynamic_pointer_cast<TextMessage>(msg);
                std::cout << textMsg->to_string() << std::endl;

            });

        // 2. Configure the FILE receiver
        // Now we just need to set the callback that FileMessage.handle() will invoke
        fileMessageReceiver_.register_handler(TextTypes::File,
        [this](const std::shared_ptr<tcp::socket>& sender, std::shared_ptr<IMessage> msg)
            {
                if (msg)
                {
                    try
                    {
                        // Deserialize the raw data into a FileMessage
                        auto fm = std::dynamic_pointer_cast<FileMessage>(msg);
                        auto rawData = std::make_shared<std::vector<char>>(msg->serialize());
                        fm->deserialize(*rawData);

                        // Display info about the file
                        std::string msg_str = fm->to_string();
                        std::cout << msg_str << std::endl;

                        // Save the file to disk
                        fm->save_file();
                    }
                    catch (const std::exception& e)
                    {
                        std::cerr << "Client file deserialize error: " << e.what() << "\n";
                    }
                }
            });

        // Connect for text messages
        client_socket->async_connect(endpoint,
            [this](const boost::system::error_code& ec)
            {
                this->handle_connect(ec, "TextSocket", client_socket);
            });

        // Connect for file transfers
        client_file_socket->async_connect(endpoint_file,
            [this](const boost::system::error_code& ec)
            {
                this->handle_connect(ec, "FileSocket", client_file_socket);
            });
    }
    catch (const std::exception& e)
    {
        std::cerr << "ClientServerManager init error: " << e.what() << std::endl;
    }
}


void ClientServerConnectionManager::Disconnect() const
{
    auto disconnect_socket = [](const std::shared_ptr<tcp::socket>& sock, const std::string& name)
    {
        if (!sock || !sock->is_open())
        {
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

void ClientServerConnectionManager::Message(const TextTypes type, const std::shared_ptr<IMessage>& message) const
{
    boost::system::error_code err;

    switch (type)
    {
    case TextTypes::Text:
        if (client_socket && client_socket->is_open())
        {
            SendMessage(client_socket, message, err);
        }
        else
        {
            std::cerr << "TextSocket is not connected.\n";
        }
        break;

    case TextTypes::File:
        if (client_file_socket && client_file_socket->is_open())
        {
            SendMessage(client_file_socket, message, err);
        }
        else
        {
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

uint64_t ClientServerConnectionManager::EnqueueFile(const std::filesystem::path& path) const
{
    if (!file_queue_)
    {
        std::cerr << "File queue not initialized\n";
        return 0;
    }

    if (!std::filesystem::exists(path))
    {
        std::cerr << "File does not exist: " << path << std::endl;
        return 0;
    }
    return file_queue_->enqueue(path);
}

void ClientServerConnectionManager::CancelAndReconnectFileSocket()
{
    // 1) pause queue so worker won't immediately start new items
    if (file_queue_) file_queue_->pause();

    // 2) mark all queued items as canceled (keeps history)
    if (file_queue_) file_queue_->cancel_all();

    // 3) attempt to cancel/close existing socket so in-flight send is interrupted
    if (client_file_socket && client_file_socket->is_open())
    {
        boost::system::error_code ec;
        client_file_socket->cancel(ec);
        if (ec) std::cerr << "Cancel socket error: " << ec.message() << std::endl;

        client_file_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec && ec != boost::system::errc::not_connected)
            std::cerr << "Shutdown socket error: " << ec.message() << std::endl;

        client_file_socket->close(ec);
        if (ec)
            std::cerr << "Close socket error: " << ec.message() << std::endl;
        else
            std::cout << "File socket closed to abort transfers.\n";
    }

    // 4) create new socket and async_connect it.
    client_file_socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
    client_file_socket->async_connect(endpoint_file,
        [this](const boost::system::error_code& ec)
        {
            this->handle_connect(ec, "FileSocket", this->client_file_socket);

            if (!ec && this->file_queue_)
            {
                // Only resume queue after successful connection
                this->file_queue_->resume();
            }
        });

    std::cout << "File transfer queue cancelled and file socket reconnecting...\n";
}

std::vector<FileTransferQueue::Item> ClientServerConnectionManager::FileQueueSnapshot() const
{
    if (file_queue_) return file_queue_->list_snapshot();
    return {};
}

void ClientServerConnectionManager::PauseQueue() const
{
    if (file_queue_) file_queue_->pause();
}

void ClientServerConnectionManager::ResumeQueue() const
{
    if (file_queue_) file_queue_->resume();
}

void ClientServerConnectionManager::CancelFile(uint64_t id) const
{
    if (file_queue_) file_queue_->cancel(id);
}

void ClientServerConnectionManager::RetryFile(uint64_t id) const
{
    if (file_queue_) file_queue_->retry(id);
}