#include <Server/ServerMessageSender.h>
#include <boost/asio.hpp>
#include <MessageTypes/Interface/IMessage.hpp>
#include <MessageTypes/TextMessage.h>
#include <iostream>

#ifdef _DEBUG
#define LOG(x) std::cout << "[DEBUG] " << x << std::endl
#else
#define LOG(x) ((void)0)
#endif

void Utils::SendMessage(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
                        const std::shared_ptr<IMessage>& message,
                        const boost::system::error_code& error)
{
    if (error) {std::cerr << "Accept failed: " << error.message() << "\n"; return;}
    auto data = std::make_shared<std::vector<char>>(message->serialize());
    boost::asio::async_write(*socket, boost::asio::buffer(*data),
                             [socket, data](const boost::system::error_code& ec, std::size_t /*bytes*/)
                             {
                                 if (ec) std::cerr << "Error sending: " << ec.message() << "\n";
                                 LOG("Sent message to target.\n");
                                 LOG("Target ip: " + socket->remote_endpoint().address().to_string());
                                 LOG("Target port: " + socket->remote_endpoint().port());
                             });
}
