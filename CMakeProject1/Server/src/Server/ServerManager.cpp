#include <Server/ServerManager.h>
#include <iostream>
#include <thread>
#include <Server/MessageSender.h>
#include <algorithm>
#include <boost/asio.hpp>
#include <MessageTypes/Text/TextMessage.h>
#include <MessageTypes/File/FileMessage.h>

#include "MessageTypes/SendHistory/SendHistoryMessage.h"

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
    // text message callback
    messageReciever_.register_handler(TextTypes::Text,
                                      [this](const std::shared_ptr<tcp::socket>& sender, std::shared_ptr<IMessage> msg)
                                      {
                                          // Cast to specific type
                                          auto textMsg = std::dynamic_pointer_cast<TextMessage>(msg);
                                          if (textMsg)
                                          {
#ifdef _DEBUG
                                              std::cout << textMsg->to_string() << std::endl;
#endif
                                              this->Broadcast(sender, textMsg->to_string());
                                          }
                                      });

    // filemessages callback
    fileReciever.register_handler(TextTypes::File,
                                  [this](const std::shared_ptr<tcp::socket>& sender, std::shared_ptr<IMessage> msg)
                                  {
                                      auto fileMsg = std::dynamic_pointer_cast<FileMessage>(msg);
                                      if (fileMsg)
                                      {
                                          auto rawData = std::make_shared<std::vector<char>>(msg->serialize());
                                          this->Broadcast(sender, rawData);
                                      }
                                  });
    //sendhistory callback
    // Handler for SendHistory: when a client sends this to the text socket,
    // server sends the stored history only to that client.
    // SIMPLIFIED: SendHistory handler - send files only to requesting client's IP
// In StartServer(), update the SendHistory handler:
messageReciever_.register_handler(TextTypes::SendHistory,
[this](const std::shared_ptr<tcp::socket>& sender, std::shared_ptr<IMessage> msg)
{
    if (!sender || !sender->is_open())
        return;

    // Cast to get the file port
    auto histMsg = std::dynamic_pointer_cast<SendHistoryMessage>(msg);
    if (!histMsg)
    {
        std::cerr << "SendHistory: failed to cast message\n";
        return;
    }

    unsigned short client_file_port = histMsg->get_file_port();

    // Get sender's IP
    std::string sender_ip = GetSocketIP(sender);
    if (sender_ip.empty())
    {
        std::cerr << "SendHistory: couldn't get sender IP\n";
        return;
    }

    std::cout << "Client requested history from " << sender_ip
              << " with file port " << client_file_port << std::endl;

    // Begin sending history
    boost::system::error_code ec;
    SendMessage(sender, std::make_shared<TextMessage>("--- Begin Message History ---"), ec);

    // Find the EXACT file socket matching IP AND port
    std::shared_ptr<tcp::socket> matching_file_socket;
    {
        std::scoped_lock lk(file_port_clients_mutex_);
        for (const auto& file_sock : file_port_clients_)
        {
            if (!file_sock || !file_sock->is_open()) continue;

            boost::system::error_code ep_ec;
            auto ep = file_sock->remote_endpoint(ep_ec);
            if (ep_ec) continue;

            std::string file_sock_ip = ep.address().to_string();
            unsigned short file_sock_port = ep.port();

            // Match by BOTH IP and port
            if (file_sock_ip == sender_ip && file_sock_port == client_file_port)
            {
                matching_file_socket = file_sock;
                std::cout << "Found matching file socket: " << file_sock_ip
                         << ":" << file_sock_port << std::endl;
                break;
            }
        }
    }

    // Get the queue for this specific file socket
    std::shared_ptr<FileTransferQueue> file_q = nullptr;
    if (matching_file_socket && matching_file_socket->is_open())
    {
        file_q = GetOrCreateFileQueueForSocket(matching_file_socket);
    }
    else
    {
        std::cerr << "SendHistory: no file socket found for " << sender_ip
                 << ":" << client_file_port << "\n";
    }

    // Send history
    {
        std::scoped_lock lock(history_mutex_);

        for (const auto& msg_ptr : message_history_)
        {
            if (!msg_ptr)
                continue;

            if (auto text_msg = std::dynamic_pointer_cast<TextMessage>(msg_ptr))
            {
                boost::system::error_code sendErr;
                SendMessage(sender, text_msg, sendErr);
                if (sendErr)
                    std::cerr << "SendHistory: error sending text: " << sendErr.message() << "\n";
            }
            else if (auto file_msg = std::dynamic_pointer_cast<FileMessage>(msg_ptr))
            {
                if (file_q)
                {
                    file_q->enqueue(file_msg);
                }
                else
                {
                    std::cerr << "SendHistory: skipping file (no queue)\n";
                }
            }
        }
    }

    SendMessage(sender, std::make_shared<TextMessage>("--- End Message History ---"), ec);
    std::cout << "History sent to " << sender_ip << ":" << client_file_port << std::endl;
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
    bool sendGreeting)
{
    auto socket = std::make_shared<tcp::socket>(io_context);

    acceptor->async_accept(*socket,
        [this, acceptor, socket, &client_list, &client_list_mutex, &receiver, sendGreeting]
        (const boost::system::error_code& error)
        {
            const bool is_shutdown_error = (error == boost::asio::error::operation_aborted ||
                error == boost::asio::error::bad_descriptor);

            if (error)
            {
                if (!is_shutdown_error)
                {
                    std::cerr << "Accept error: " << error.message() << std::endl;
                }
            }

            if (!error && socket && socket->is_open())
            {
                // Add socket to client list
                {
                    std::scoped_lock lock(client_list_mutex);
                    bool exists = std::any_of(client_list.begin(), client_list.end(),
                        [&](const auto& s)
                        {
                            return s && s->native_handle() == socket->native_handle();
                        });
                    if (!exists) client_list.push_back(socket);
                }

                // Get client IP for logging
                boost::system::error_code ec;
                auto ep = socket->remote_endpoint(ec);
                std::string client_ip = "unknown";
                if (!ec)
                {
                    client_ip = ep.address().to_string() + ":" + std::to_string(ep.port());
                }

                // If this is a file socket, create its queue
                if (&client_list == &file_port_clients_)
                {
                    GetOrCreateFileQueueForSocket(socket);
                    std::cout << "File client connected from " << client_ip << std::endl;
                }
                else
                {
                    std::cout << "Text client connected from " << client_ip << std::endl;
                }

                // GREETING
                if (sendGreeting)
                {
                    auto helloMessage = std::make_shared<TextMessage>("Hello client");
                    boost::system::error_code sendErr;
                    SendMessage(socket, helloMessage, sendErr);
                    if (sendErr && sendErr != boost::asio::error::operation_aborted)
                        std::cerr << "ERROR sending hello: " << sendErr.message() << std::endl;
                }

                // Start receiving from this socket
                receiver.start_read_header(socket);
            }

            // Re-arm accept
            if (!io_context.stopped() && !is_shutdown_error)
            {
                AcceptConnection(acceptor, client_list, client_list_mutex, receiver, sendGreeting);
            }
        });
}

void ServerManager::AcceptTextConnection(const std::shared_ptr<tcp::acceptor>& acceptor)
{
    AcceptConnection(acceptor, text_port_clients_, text_port_clients_mutex_, messageReciever_, false);
}

void ServerManager::AcceptFileConnection(const std::shared_ptr<tcp::acceptor>& acceptor)
{
    AcceptConnection(acceptor, file_port_clients_, file_port_clients_mutex_, fileReciever, false);
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

        // send to everyone except sender
        if (sender && clientSock->native_handle() == sender->native_handle()) continue;
        boost::system::error_code sendErr;
        SendMessage(clientSock, text_log, sendErr);
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
        SendMessage(clientSock, msg, sendErr);
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
