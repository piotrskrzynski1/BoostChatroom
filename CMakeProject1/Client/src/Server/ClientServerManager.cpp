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

ClientServerManager::ClientServerManager(boost::asio::io_context& io)
{
    try {
        // Setup endpoints for text and file communications
        endpoint = tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 5555);
        endpoint_file = tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 5556);

        // Allocate sockets
        client_socket = std::make_shared<tcp::socket>(io);
        client_file_socket = std::make_shared<tcp::socket>(io);

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
            std::cout << "🔌 " << name << " disconnected successfully.\n";
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
            std::cerr << "⚠Unknown message type.\n";
            break;
    }

    if (err)
        std::cerr << "SendMessage error: " << err.message() << std::endl;
}
