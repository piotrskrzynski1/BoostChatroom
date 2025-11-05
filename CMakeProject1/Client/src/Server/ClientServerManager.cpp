#include <Server/ClientServerManager.h>
#include <MessageTypes/TextMessage.h>
#include <Server/ServerMessageSender.h>
using boost::asio::ip::tcp;

void ClientServerManager::handle_connect(const boost::system::error_code& error)
{
    if (error) std::cerr << "Connection failed: " << error.message() << std::endl;

    std::cout << "Connected to server!\n";
    std::cout << "Server ip: " << client_socket->remote_endpoint().address()
        << " Server port: " << client_socket->remote_endpoint().port() << std::endl;

    messageReceiver_.start_read_header(client_socket);
}

ClientServerManager::ClientServerManager(boost::asio::io_context& io)
{
    endpoint = tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 5555);
    client_socket = std::make_shared<tcp::socket>(io);
    client_socket->async_connect(endpoint,
                                 [this](const boost::system::error_code& ec)
                                 {
                                     this->handle_connect(ec);
                                 });
}


void ClientServerManager::Disconnect() const
{
    if (!client_socket && !client_socket->is_open()) std::cout << "Not connected � no need to disconnect.\n";

    boost::system::error_code ec;

    client_socket->cancel(ec);
    if (ec)
        std::cerr << "Cancel error: " << ec.message() << std::endl;

    client_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    if (ec)
        std::cerr << "Shutdown error: " << ec.message() << std::endl;

    client_socket->close(ec);
    if (ec)
        std::cerr << "Close error: " << ec.message() << std::endl;
    else
        std::cout << "Disconnected from server.\n";
}

void ClientServerManager::Message(const std::shared_ptr<IMessage>& message) const
{
    boost::system::error_code err;
    Utils::SendMessage(client_socket, message, err);
}
