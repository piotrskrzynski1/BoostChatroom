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
#ifdef _DEBUG
            std::cout << msg << std::endl;
#endif
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
        // Reset context if it was stopped from a previous run
        if (io_context.stopped())
        {
            io_context.reset();
        }

        // Use member variables for acceptors
        acceptor_ = std::make_shared<tcp::acceptor>(
            io_context,
            tcp::endpoint(boost::asio::ip::make_address_v4(this->address), this->port));

        file_acceptor_ = std::make_shared<tcp::acceptor>(
            io_context,
            tcp::endpoint(boost::asio::ip::make_address_v4(this->address), this->fileport));

        AcceptTextConnection(acceptor_);
        AcceptFileConnection(file_acceptor_);

        const unsigned int thread_count = std::max(4u, std::thread::hardware_concurrency());
        threads.reserve(thread_count);

        for (unsigned int i = 0; i < thread_count; ++i)
            threads.emplace_back([this]() { io_context.run(); });

        this->SetStatusUP(true);

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
    MessageReceiver& receiver,
    bool sendGreeting, // <-- This is the control flag
    const TextTypes& clientType)
{
    auto socket = std::make_shared<tcp::socket>(io_context);

    acceptor->async_accept(*socket,
                           [this, acceptor, socket, &client_list, &client_list_mutex, &receiver, sendGreeting,
                               clientType]
                       (const boost::system::error_code& error)
                           {
                               // Define error codes that indicate acceptor shutdown/cancellation
                               const bool is_shutdown_error = (error == boost::asio::error::operation_aborted ||
                                   error == boost::asio::error::bad_descriptor);

                               // 1. Handle error condition (Check and report UNEXPECTED errors)
                               if (error)
                               {
                                   if (!is_shutdown_error)
                                   {
                                       std::cerr << "Accept error: " << error.message() << std::endl;
                                   }
                               }

                               // 2. CRITICAL CHECK: Only proceed if the socket is valid AND open AND no major error occurred.
                               // The is_shutdown_error check is technically redundant here because socket->is_open()
                               // would likely be false, but it keeps the logic cleaner.
                               if (!error && socket && socket->is_open())
                               {
                                   // Only proceed on NO error

                                   // --- CONNECTION ESTABLISHED: ADD TO LIST AND INITIATE RECEIVING ---

                                   // Add socket to client list, checking for duplicates using native_handle()
                                   {
                                       std::scoped_lock lock(client_list_mutex);
                                       // Check if socket is already in the list.
                                       bool exists = std::any_of(client_list.begin(), client_list.end(),
                                                                 [&](const auto& s)
                                                                 {
                                                                     return s && s->native_handle() == socket->
                                                                         native_handle();
                                                                 });
                                       if (!exists) client_list.push_back(socket);
                                   }

                                   // Determine if this is a file client and get/create its queue
                                   std::shared_ptr<FileTransferQueue> file_q = nullptr;
                                   if (&client_list == &file_port_clients_)
                                   {
                                       file_q = GetOrCreateFileQueueForSocket(socket);
                                   }

                                   // --- GREETING AND HISTORY ---

                                   if (sendGreeting)
                                   {
                                       auto helloMessage = std::make_shared<TextMessage>("Hello client");
                                       boost::system::error_code sendErr;
                                       Utils::SendMessage(socket, helloMessage, sendErr);
                                       if (sendErr && sendErr != boost::asio::error::operation_aborted)
                                           std::cerr << "ERROR sending hello: " << sendErr.message() << std::endl;
                                   }

                                   // BEGIN MESSAGE HISTORY SEND
                                   std::deque<HistoryMessage> history_copy;
                                   {
                                       std::scoped_lock lock(history_mutex_);
                                       history_copy = message_history_;
                                   }

                                   if (sendGreeting)
                                   {
                                       auto history_start_msg = std::make_shared<TextMessage>(
                                           "--- Begin Message History ---");
                                       Utils::SendMessage(socket, history_start_msg, boost::system::error_code{});
                                   }

                                   for (const auto& msg_ptr : history_copy)
                                   {
                                       if (auto text_msg = std::dynamic_pointer_cast<TextMessage>(msg_ptr))
                                       {
                                           if (sendGreeting)
                                           {
                                               boost::system::error_code sendErr;
                                               Utils::SendMessage(socket, text_msg, sendErr);
                                               if (sendErr) std::cerr << "HISTORY: send text err: " << sendErr.message()
                                                   << "\n";
                                           }
                                       }
                                       else if (auto file_msg = std::dynamic_pointer_cast<FileMessage>(msg_ptr))
                                       {
                                           if (file_q)
                                           {
                                               file_q->enqueue(file_msg);
                                           }
                                       }
                                   }

                                   if (sendGreeting)
                                   {
                                       auto history_end_msg = std::make_shared<TextMessage>(
                                           "--- End Message History ---");
                                       Utils::SendMessage(socket, history_end_msg, boost::system::error_code{});
                                   }
                                   // END MESSAGE HISTORY SEND

                                   // Start receiving from this socket
                                   receiver.start_read_header(socket, nullptr);
                               }
                               else
                               {
                                   // socket closed immediately or error occurred
                               }

                               // Re-arm accept if still running AND no shutdown error occurred
                               if (!io_context.stopped() && !is_shutdown_error)
                               {
                                   AcceptConnection(acceptor, client_list, client_list_mutex, receiver, sendGreeting,
                                                    clientType);
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

    std::string sender_info = "<Server>";
    if (sender)
    {
        boost::system::error_code ec;
        auto ep = sender->remote_endpoint(ec);
        if (!ec)
        {
            sender_info = ep.address().to_string() + ":" + std::to_string(ep.port());
        }
    }

    // Create text log and add to history
    auto text_log = std::make_shared<TextMessage>("[FILE] From " + sender_info + ": " + fm->to_string());
    {
        std::scoped_lock lock(history_mutex_);
        message_history_.push_back(text_log);
        message_history_.push_back(fm);
        if (message_history_.size() > MAX_HISTORY_MESSAGES)
        {
            message_history_.pop_front();
        }
    }

    // --- 1. Enqueue file for FILE clients (except sender) ---
    std::vector<std::shared_ptr<tcp::socket>> fileClientsCopy;
    std::vector<std::shared_ptr<tcp::socket>> deadClients;
    {
        std::scoped_lock lk(file_port_clients_mutex_);
        auto it = std::remove_if(file_port_clients_.begin(), file_port_clients_.end(),
                                 [&deadClients](const auto& s)
                                 {
                                     bool dead = (!s || !s->is_open());
                                     if (dead && s) deadClients.push_back(s);
                                     return dead;
                                 });
        file_port_clients_.erase(it, file_port_clients_.end());
        fileClientsCopy = file_port_clients_;
    }

    for (const auto& s : deadClients)
    {
        RemoveFileQueueForSocket(s);
    }

    for (const auto& clientSock : fileClientsCopy)
    {
        if (!clientSock || !clientSock->is_open()) continue;
        // Skip the actual sender socket (file socket)
        if (sender && clientSock->native_handle() == sender->native_handle()) continue;

        auto q = GetOrCreateFileQueueForSocket(clientSock);
        if (q)
        {
            q->enqueue(fm);
        }
    }

    // --- 2. Send text log to ALL TEXT clients (including sender) ---
    std::vector<std::shared_ptr<tcp::socket>> textClientsCopy;
    {
        std::scoped_lock lock(text_port_clients_mutex_);
        text_port_clients_.erase(
            std::remove_if(text_port_clients_.begin(), text_port_clients_.end(),
                           [](const auto& s) { return !s || !s->is_open(); }),
            text_port_clients_.end()
        );
        textClientsCopy = text_port_clients_;
    }

    for (const auto& clientSock : textClientsCopy)
    {
        if (!clientSock || !clientSock->is_open()) continue;

        // Send to everyone
        boost::system::error_code sendErr;
        Utils::SendMessage(clientSock, text_log, sendErr);
        if (sendErr)
        {
            std::cerr << "ERROR sending file log to client: " << sendErr.message() << std::endl;
        }
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
                           [](const auto& s) { return !s || !s->is_open(); }),
            text_port_clients_.end()
        );
        clientsCopy = text_port_clients_;
    }

    std::string sender_info = "<Server>";
    if (sender)
    {
        boost::system::error_code ec;
        auto ep = sender->remote_endpoint(ec);
        if (!ec) sender_info = ep.address().to_string() + ":" + std::to_string(ep.port());
    }

    // Create message and add to history ONCE
    auto msg = std::make_shared<TextMessage>("[TEXT] From " + sender_info + ": " + text);
    {
        std::scoped_lock lock(history_mutex_);
        message_history_.push_back(msg);
        // Keep the history size limited
        if (message_history_.size() > MAX_HISTORY_MESSAGES)
        {
            message_history_.pop_front();
        }
    }

    // Now, send the pre-made message to all clients
    for (const auto& clientSock : clientsCopy)
    {
        if (!clientSock || !clientSock->is_open()) continue;
        if (sender && clientSock->native_handle() == sender->native_handle()) continue;

        boost::system::error_code sendErr;
        Utils::SendMessage(clientSock, msg, sendErr);
        if (sendErr) std::cerr << "ERROR sending to client: " << sendErr.message() << std::endl;
    }
}


int ServerManager::GetFilePort() const
{
    return fileport;
}

bool ServerManager::GetStatusUP() const
{
    return serverup_;
}

void ServerManager::SetStatusUP(bool status)
{
    serverup_ = status;
}

void ServerManager::StopServer()
{
    // Check if already stopped
    bool expected = true;
    bool status = this->GetStatusUP();
    if (!status)
    {
        std::cout << "Server already stopped\n";
        return;
    }

    std::cout << "Server stopping...\n";
    boost::system::error_code ec;

    // 1. Stop accepting new connections FIRST
    if (acceptor_ && acceptor_->is_open())
    {
        acceptor_->cancel(ec);
        if (ec && ec != boost::asio::error::operation_aborted)
        {
            std::cerr << "Text acceptor cancel error: " << ec.message() << "\n";
        }
        acceptor_->close(ec);
        if (ec)
        {
            std::cerr << "Text acceptor close error: " << ec.message() << "\n";
        }
    }

    if (file_acceptor_ && file_acceptor_->is_open())
    {
        file_acceptor_->cancel(ec);
        if (ec && ec != boost::asio::error::operation_aborted)
        {
            std::cerr << "File acceptor cancel error: " << ec.message() << "\n";
        }
        file_acceptor_->close(ec);
        if (ec)
        {
            std::cerr << "File acceptor close error: " << ec.message() << "\n";
        }
    }

    // 2. Stop and clear all file queues BEFORE closing sockets
    {
        std::scoped_lock lk(file_queues_mutex_);
        for (auto& [sock, queue] : file_queues_)
        {
            if (queue)
            {
                queue->stop(); // This must join the worker thread
            }
        }
        file_queues_.clear();
    }

    // 3. Close all client sockets
    {
        std::scoped_lock lk(text_port_clients_mutex_);
        for (const auto& s : text_port_clients_)
        {
            if (s && s->is_open())
            {
                s->cancel(ec);
                s->shutdown(tcp::socket::shutdown_both, ec);
                s->close(ec);
            }
        }
        text_port_clients_.clear();
    }

    {
        std::scoped_lock lk(file_port_clients_mutex_);
        for (const auto& s : file_port_clients_)
        {
            if (s && s->is_open())
            {
                s->cancel(ec);
                s->shutdown(tcp::socket::shutdown_both, ec);
                s->close(ec);
            }
        }
        file_port_clients_.clear();
    }

    // 4. NOW stop the io_context (after all async ops are cancelled)
    if (!io_context.stopped())
    {
        io_context.stop();
    }
    this->SetStatusUP(false);
    std::cout << "Server stopped successfully\n";
}

// Get the IP address from a socket
std::string ServerManager::GetSocketIP(const std::shared_ptr<tcp::socket>& sock)
{
    if (!sock || !sock->is_open()) return "";

    boost::system::error_code ec;
    auto ep = sock->remote_endpoint(ec);
    if (ec) return "";

    return ep.address().to_string();
}
