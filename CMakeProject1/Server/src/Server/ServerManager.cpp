#include <Server/ServerManager.h>
#include <iostream>
#include <thread>
#include <Server/ServerMessageSender.h>

ServerManager::ServerManager(int port, std::string&& ipAddress)
{
    this->port = port;
    this->address = std::move(ipAddress);
    std::cout << "Server configured at address: " << this->address
        << " Port: " << this->port << std::endl;
}

void ServerManager::StartServer()
{
    try
    {
        // Create acceptor on the heap so it lives for the duration of async ops
        auto acceptor = std::make_shared<tcp::acceptor>(
            io_context,
            tcp::endpoint(boost::asio::ip::make_address_v4(this->address), this->port));

        // Start accepting first connection
        DoAccept(acceptor);

        // Run io_context on multiple threads
        const unsigned int thread_count = std::max(4u, std::thread::hardware_concurrency());
        std::vector<std::thread> threads;
        threads.reserve(thread_count);

        for (unsigned int i = 0; i < thread_count; ++i)
        {
            threads.emplace_back([this]() {
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
            if (!error)
            {
                clients_.push_back(socket);
                messageReciever_.start_read_header(socket);
                auto hellomessage = std::make_shared<TextMessage>("Hello client");
                Utils::SendMessage(socket, hellomessage, error);
            }
            else
            {
                std::cerr << "Accept failed: " << error.message() << "\n";
            }

            DoAccept(acceptor);
        });
}

int ServerManager::GetPort() {
    return this->port;
}

std::string ServerManager::GetIpAddress() {
    return this->address;
}
