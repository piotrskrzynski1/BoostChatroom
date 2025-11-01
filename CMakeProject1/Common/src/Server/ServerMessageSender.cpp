#include <Server/ServerMessageSender.h>
#include <boost/asio.hpp>
#include <MessageTypes/Interface/IMessage.hpp>
#include <MessageTypes/TextMessage.h>
#include <iostream>
    void Utils::SendMessage(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
        std::shared_ptr<IMessage> message,
        const boost::system::error_code& error)
    {
        if (!error)
        {
            auto data = std::make_shared<std::vector<char>>(message->serialize());

            boost::asio::async_write(*socket, boost::asio::buffer(*data),
                [socket, data](const boost::system::error_code& ec, std::size_t /*bytes*/)
            {
                if (!ec)
                {
                    std::cout << "Sent message to client.\n"
                        << "Client ip: " << socket->remote_endpoint().address()
                        << " client port: " << socket->remote_endpoint().port()
                        << std::endl;
                }
                else
                {
                    std::cerr << "Error sending: " << ec.message() << "\n";
                }
            });
        }
        else
        {
            std::cerr << "Accept failed: " << error.message() << "\n";
        }
    }