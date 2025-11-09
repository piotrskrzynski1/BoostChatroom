#include <Server/ServerManager.h>
#include <iostream>
#include <thread>
#include <Server/ServerMessageSender.h>
#include <algorithm>
#include <boost/asio.hpp>
#include <MessageTypes/Text/TextMessage.h>
#include <MessageTypes/File/FileMessage.h>

using boost::asio::ip::tcp;

#ifdef _DEBUG
#define LOG(x) std::cout << "[DEBUG] " << x << std::endl
#else
#define LOG(x) ((void)0)
#endif

ServerManager::ServerManager(int port, int fileport, std::string&& ipAddress)
{
    this->port = port;
    this->fileport = fileport;
    this->address = std::move(ipAddress);
    std::cout << "Server configured at address: " << this->address
              << "\nText Port: " << port << "\nFile Port: " << this->fileport << std::endl;
}

std::string ServerManager::GetIpAddress() { return this->address; }
int ServerManager::GetPort() const { return this->port; }

void ServerManager::StartServer()
{
    // Text messages callback
    messageReciever_.set_on_message_callback(
        [this](const std::shared_ptr<tcp::socket>& sender, const std::string& msg)
        {
            this->Broadcast(sender, msg);
        });

    // File messages callback (use vector<char> to avoid truncation)
    fileReciever.set_on_file_callback(
        [this](const std::shared_ptr<tcp::socket>& sender, const std::shared_ptr<std::vector<char>>& rawData)
        {
            this->Broadcast(sender, rawData);
        });

    try
    {
        // ✅ FIX: Reset context if it was stopped from a previous run
        if (io_context.stopped()) {
            io_context.reset();
        }

        // ✅ FIX: Use member variables for acceptors
        acceptor_ = std::make_shared<tcp::acceptor>(
            io_context,
            tcp::endpoint(boost::asio::ip::make_address_v4(this->address), this->port));

        file_acceptor_ = std::make_shared<tcp::acceptor>(
            io_context,
            tcp::endpoint(boost::asio::ip::make_address_v4(this->address), this->fileport));

        AcceptTextConnection(acceptor_);
        AcceptFileConnection(file_acceptor_);

        const unsigned int thread_count = std::max(4u, std::thread::hardware_concurrency());
        std::vector<std::thread> threads; // Keep local per your structure
        threads.reserve(thread_count);

        for (unsigned int i = 0; i < thread_count; ++i)
            threads.emplace_back([this]() { io_context.run(); });

        // This loop now correctly blocks until StopServer() is called
        for (auto& t : threads)
            t.join();

        threads.clear();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}

void ServerManager::AcceptConnection(
    const std::shared_ptr<tcp::acceptor>& acceptor,
    std::vector<std::shared_ptr<tcp::socket>>& client_list,
    std::mutex& client_list_mutex,
    MessageReciever& receiver,
    bool sendGreeting,
    const TextTypes& clientType)
{
    auto socket = std::make_shared<tcp::socket>(io_context);

   acceptor->async_accept(*socket,
    [this, acceptor, socket, &client_list, &client_list_mutex, &receiver, sendGreeting, clientType]
    (const boost::system::error_code& error)
    {
        if (!acceptor || !acceptor->is_open())
        {
            return;
        }
        if (error == boost::asio::error::operation_aborted) {
            // Expected during shutdown
            std::cout << "Acceptor shutting down." << std::endl;
            return;
        }

        if (error) {
            // Log unexpected errors; retry accepting only if io_context still running
            std::cerr << "Accept failed (" << static_cast<int>(clientType)
                      << "): " << error.message() << "\n";
            if (!io_context.stopped() && acceptor && acceptor->is_open()) {
                AcceptConnection(acceptor, client_list, client_list_mutex, receiver, sendGreeting, clientType);
            }
            return;
        }

        // Make sure socket is open before storing/starting
        if (socket && socket->is_open()) {
            {
                std::scoped_lock lock(client_list_mutex);
                bool exists = std::any_of(client_list.begin(), client_list.end(),
                                          [&](const auto& s){ return s && s->native_handle() == socket->native_handle(); });
                if (!exists) client_list.push_back(socket);
            }

            if (&client_list == &file_port_clients_) GetOrCreateFileQueueForSocket(socket);

            // Start receiving from this socket
            receiver.start_read_header(socket, nullptr);

            if (sendGreeting) {
                auto helloMessage = std::make_shared<TextMessage>("Hello client");
                boost::system::error_code sendErr;
                Utils::SendMessage(socket, helloMessage, sendErr);
                if (sendErr && sendErr != boost::asio::error::operation_aborted)
                    std::cerr << "ERROR sending hello: " << sendErr.message() << std::endl;
            }
        } else {
            // socket closed immediately — ignore
        }

        // Re-arm accept if still running
        if (!io_context.stopped()) {
            AcceptConnection(acceptor, client_list, client_list_mutex, receiver, sendGreeting, clientType);
        }
    });

}

void ServerManager::AcceptTextConnection(const std::shared_ptr<tcp::acceptor>& acceptor)
{
    AcceptConnection(acceptor, text_port_clients_, text_port_clients_mutex_, messageReciever_, true, TextTypes::Text);
}

void ServerManager::AcceptFileConnection(const std::shared_ptr<tcp::acceptor>& acceptor)
{
    AcceptConnection(acceptor, file_port_clients_, file_port_clients_mutex_, fileReciever, false, TextTypes::Text);
}

std::shared_ptr<FileTransferQueue> ServerManager::GetOrCreateFileQueueForSocket(
    const std::shared_ptr<tcp::socket>& sock)
{
    if (!sock) return nullptr;
    auto key = reinterpret_cast<std::uintptr_t>(sock.get());

    {
        std::scoped_lock lk(file_queues_mutex_);
        auto it = file_queues_.find(key);
        if (it != file_queues_.end()) return it->second;
    }

    std::weak_ptr<tcp::socket> weak_sock = sock;
    auto getter = [weak_sock]() -> std::shared_ptr<tcp::socket> { return weak_sock.lock(); };

    auto q = std::make_shared<FileTransferQueue>(std::move(getter));
    {
        std::scoped_lock lk(file_queues_mutex_);
        file_queues_.emplace(key, q);
    }
    return q;
}

void ServerManager::RemoveFileQueueForSocket(const std::shared_ptr<tcp::socket>& sock)
{
    if (!sock) return;
    auto key = reinterpret_cast<std::uintptr_t>(sock.get());
    std::shared_ptr<FileTransferQueue> q;
    {
        std::scoped_lock lk(file_queues_mutex_);
        auto it = file_queues_.find(key);
        if (it != file_queues_.end())
        {
            q = it->second;
            file_queues_.erase(it);
        }
    }
    if (q) q->stop();
}

// --- Broadcast overload for binary files ---
void ServerManager::Broadcast(const std::shared_ptr<tcp::socket>& sender,
                              const std::shared_ptr<std::vector<char>>& rawData)
{
    if (!rawData) return;

    auto fm = std::make_shared<FileMessage>();
    try { fm->deserialize(*rawData); }
    catch (const std::exception& ex)
    {
        std::cerr << "Broadcast: failed to deserialize FileMessage: " << ex.what() << "\n";
        return;
    }

    std::vector<std::shared_ptr<tcp::socket>> clientsCopy;
    {
        std::scoped_lock lk(file_port_clients_mutex_);
        file_port_clients_.erase(
            std::remove_if(file_port_clients_.begin(), file_port_clients_.end(),
                           [this](const auto& s){
                               bool dead = (!s || !s->is_open());
                               if(dead && s) RemoveFileQueueForSocket(s);
                               return dead;
                           }),
            file_port_clients_.end()
        );
        clientsCopy = file_port_clients_;
    }

    std::string sender_info = "<Server>";
    if (sender) {
        boost::system::error_code ec;
        auto ep = sender->remote_endpoint(ec);
        if (!ec) sender_info = ep.address().to_string() + ":" + std::to_string(ep.port());
    }

    for (const auto& clientSock : clientsCopy)
    {
        if (!clientSock || !clientSock->is_open()) continue;
        if (sender && clientSock->native_handle() == sender->native_handle()) continue;

        // Enqueue actual file for sending
        auto q = GetOrCreateFileQueueForSocket(clientSock);
        if (q) q->enqueue(fm);

        // Optional: send a text log to clients for display
        auto text_log = std::make_shared<TextMessage>("[FILE] From " + sender_info + ": " + fm->to_string());
        boost::system::error_code sendErr;
        Utils::SendMessage(clientSock, text_log, sendErr);
    }
}

// --- Broadcast overload for text messages ---
void ServerManager::Broadcast(const std::shared_ptr<tcp::socket>& sender, const std::string& text)
{
    std::vector<std::shared_ptr<tcp::socket>> clientsCopy;
    {
        std::scoped_lock lock(text_port_clients_mutex_);
        // Clean up disconnected clients
        text_port_clients_.erase(
            std::remove_if(text_port_clients_.begin(), text_port_clients_.end(),
                           [](const auto& s){ return !s || !s->is_open(); }),
            text_port_clients_.end()
        );
        clientsCopy = text_port_clients_;
    }

    std::string sender_info = "<Server>";
    if (sender) {
        boost::system::error_code ec;
        auto ep = sender->remote_endpoint(ec);
        if (!ec) sender_info = ep.address().to_string() + ":" + std::to_string(ep.port());
    }

    for (const auto& clientSock : clientsCopy)
    {
        if (!clientSock || !clientSock->is_open()) continue;
        if (sender && clientSock->native_handle() == sender->native_handle()) continue;

        // Send text message with proper [TEXT] prefix
        auto msg = std::make_shared<TextMessage>("[TEXT] From " + sender_info + ": " + text);

        boost::system::error_code sendErr;
        Utils::SendMessage(clientSock, msg, sendErr);
        if (sendErr) std::cerr << "ERROR sending to client: " << sendErr.message() << std::endl;
    }
}


int ServerManager::GetFilePort() const
{
    return fileport;
}
void ServerManager::StopServer()
{
    std::cout << "Server stopping signal sent...\n";
    boost::system::error_code ec;

    // Cancel & close acceptors first
    if (acceptor_) {
        acceptor_->cancel(ec);
        if (ec && ec != boost::asio::error::operation_aborted)
            std::cerr << "acceptor cancel error: " << ec.message() << "\n";
        acceptor_->close(ec);
        if (ec && ec != boost::asio::error::operation_aborted)
            std::cerr << "acceptor close error: " << ec.message() << "\n";
    }

    if (file_acceptor_) {
        file_acceptor_->cancel(ec);
        if (ec && ec != boost::asio::error::operation_aborted)
            std::cerr << "file_acceptor cancel error: " << ec.message() << "\n";
        file_acceptor_->close(ec);
        if (ec && ec != boost::asio::error::operation_aborted)
            std::cerr << "file_acceptor close error: " << ec.message() << "\n";
    }

    // Close text clients
    {
        std::scoped_lock lk(text_port_clients_mutex_);
        for (auto& s : text_port_clients_) {
            if (!s) continue;
            boost::system::error_code ec2;
            s->cancel(ec2);
            s->shutdown(tcp::socket::shutdown_both, ec2);
            s->close(ec2);
            // ignore expected errors like operation_aborted / not_connected
        }
        text_port_clients_.clear();
    }

    // Close file clients
    {
        std::scoped_lock lk(file_port_clients_mutex_);
        for (auto& s : file_port_clients_) {
            if (!s) continue;
            boost::system::error_code ec2;
            s->cancel(ec2);
            s->shutdown(tcp::socket::shutdown_both, ec2);
            s->close(ec2);
        }
        file_port_clients_.clear();
    }

    // Stop any per-socket file queues
    {
        std::scoped_lock lk(file_queues_mutex_);
        for (auto &kv : file_queues_) {
            if (kv.second) kv.second->stop();
        }
        file_queues_.clear();
    }

    // Finally stop io_context to unblock worker threads
    if (!io_context.stopped()) {
        io_context.stop();
    }
}
