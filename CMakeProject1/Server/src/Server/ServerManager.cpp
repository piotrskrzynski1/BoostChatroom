#include <Server/ServerManager.h>
#include <iostream>
#include <thread>
#include <Server/ServerMessageSender.h>
#include <algorithm>

#include "MessageTypes/Text/TextMessage.h"

using boost::asio::ip::tcp;

#ifdef _DEBUG
#define LOG(x) std::cout << "[DEBUG] " << x << std::endl
#else
#define LOG(x) ((void)0)
#endif

ServerManager::ServerManager(int port,int fileport, std::string&& ipAddress)
{
    this->port = port;
    this->fileport = fileport;
    this->address = std::move(ipAddress);
    std::cout << "Server configured at address: " << this->address
        << " Port: " << this->port << std::endl;

    // register callback so server broadcasts on incoming messages
    messageReciever_.set_on_message_callback(
        [this](const std::shared_ptr<boost::asio::ip::tcp::socket>& sender, const std::string& msg)
        {
            this->Broadcast(sender, msg);
        }
    );
}

void ServerManager::StartServer()
{
    messageReciever_.set_on_message_callback(
        [this](const std::shared_ptr<boost::asio::ip::tcp::socket>& sender, const std::string& msg)
        {
            this->Broadcast(sender, msg);
        });
    try
    {
        // Create acceptor on the heap so it lives for the duration of async ops
        auto acceptor = std::make_shared<tcp::acceptor>(
            io_context,
            tcp::endpoint(boost::asio::ip::make_address_v4(this->address), this->port));
        auto fileacceptor = std::make_shared<tcp::acceptor>(
            io_context,
            tcp::endpoint(boost::asio::ip::make_address_v4(this->address),this->fileport));
        // Start accepting first connection
        DoAccept(acceptor);
        DoAcceptFile(fileacceptor);

        // Run io_context on multiple threads
        const unsigned int thread_count = std::max(4u, std::thread::hardware_concurrency());
        std::vector<std::thread> threads;
        threads.reserve(thread_count);

        for (unsigned int i = 0; i < thread_count; ++i)
        {
            threads.emplace_back([this]()
            {
                io_context.run();
            });
        }

        for (auto& t : threads)
            t.join();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}

void ServerManager::DoAccept(std::shared_ptr<tcp::acceptor> acceptor)
{
    auto socket = std::make_shared<tcp::socket>(io_context);

    acceptor->async_accept(*socket, [this, acceptor, socket](const boost::system::error_code& error)
    {
        if (error) std::cerr << "Accept failed: " << error.message() << "\n";

        {
            std::scoped_lock lock(text_port_clients_mutex_);
            bool exists = false;
            for (auto& s : text_port_clients_)
            {
                if (s && s->native_handle() == socket->native_handle())
                {
                    exists = true;
                    break;
                }
            }
            if (!exists) text_port_clients_.push_back(socket);
        }

        // start async read on the new connection (do not rely on recvStr from accept)
        messageReciever_.start_read_header(socket, nullptr);

        // greet the new client
        auto helloMessage = std::make_shared<TextMessage>("Hello client");
        boost::system::error_code sendErr;
        Utils::SendMessage(socket, helloMessage, sendErr);
        if (sendErr) std::cerr << "ERROR sending hello: " << sendErr.message() << std::endl;

        // debug dump of clients
        {
            std::scoped_lock lock(text_port_clients_mutex_);
            for (const auto& s : text_port_clients_)
            {
                boost::system::error_code ec1, ec2;
                auto remote = s->remote_endpoint(ec1);
                auto local = s->local_endpoint(ec2);
                std::cout << " clientsock: "
                    << (ec1 ? std::string("<err>") : remote.address().to_string())
                    << " " << (ec1 ? 0 : remote.port())
                    << " (local " << (ec2 ? std::string("<err>") : local.address().to_string())
                    << ":" << (ec2 ? 0 : local.port()) << ")" << std::endl;
            }
        }

        DoAccept(acceptor);
    });
}

void ServerManager::DoAcceptFile(std::shared_ptr<tcp::acceptor> acceptor)
{
    auto socket = std::make_shared<tcp::socket>(io_context);

    acceptor->async_accept(*socket, [this, acceptor, socket](const boost::system::error_code& error)
    {
        if (error) std::cerr << "Accept failed: " << error.message() << "\n";

        {
            std::scoped_lock lock(file_port_clients_mutex_);
            bool exists = false;
            for (auto& s : file_port_clients_)
            {
                if (s && s->native_handle() == socket->native_handle())
                {
                    exists = true;
                    break;
                }
            }
            if (!exists) file_port_clients_.push_back(socket);
        }

        // start async read on the new connection (do not rely on recvStr from accept)
        fileReciever.start_read_header(socket, nullptr);

        // debug dump of file port clients
        {
            std::scoped_lock lock(file_port_clients_mutex_);
            for (const auto& s : file_port_clients_)
            {
                boost::system::error_code ec1, ec2;
                auto remote = s->remote_endpoint(ec1);
                auto local = s->local_endpoint(ec2);
                std::cout << " fileport clientsock: "
                    << (ec1 ? std::string("<err>") : remote.address().to_string())
                    << " " << (ec1 ? 0 : remote.port())
                    << " (local " << (ec2 ? std::string("<err>") : local.address().to_string())
                    << ":" << (ec2 ? 0 : local.port()) << ")" << std::endl;
            }
        }

        DoAcceptFile(acceptor);
    });
}

void ServerManager::Broadcast(const std::shared_ptr<tcp::socket>& sender, const std::string& text)
{
    std::vector<std::shared_ptr<tcp::socket>> clientsCopy;
    {
        std::scoped_lock lock(text_port_clients_mutex_);
        // remove closed sockets
        text_port_clients_.erase(std::remove_if(text_port_clients_.begin(), text_port_clients_.end(),
                                      [](const auto& s) { return !s || !s->is_open(); }), text_port_clients_.end());
        clientsCopy = text_port_clients_;
    }

    boost::system::error_code ec;
    auto sender_ep = sender->remote_endpoint(ec);

    for (const auto& clientSock : clientsCopy)
    {
        try
        {
            if (!clientSock || !clientSock->is_open()) continue;
            if (clientSock->native_handle() == sender->native_handle()) continue;

            std::string prefix = ec
                                     ? "<unknown>"
                                     : sender_ep.address().to_string() + ":" + std::to_string(sender_ep.port());
            auto msg = std::make_shared<TextMessage>(prefix + ": " + text);

            boost::system::error_code sendErr;
            Utils::SendMessage(clientSock, msg, sendErr);
            if (sendErr)
            {
                std::cerr << "ERROR sending to client: " << sendErr.message() << std::endl;
            }
            else
            {
                LOG("Sent message to client.");
            }
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Exception while notifying clients: " << ex.what() << std::endl;
        }
    }
}

int ServerManager::GetPort() const
{
    return this->port;
}

std::string ServerManager::GetIpAddress()
{
    return this->address;
}
