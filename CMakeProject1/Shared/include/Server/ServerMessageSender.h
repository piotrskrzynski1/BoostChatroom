#pragma once
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <MessageTypes/Interface/IMessage.hpp>

namespace Utils
{
    //A function that sends an IMessage message through a specified socket
    void SendMessage(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
                     const std::shared_ptr<IMessage>& message,
                     const boost::system::error_code& error);
}
