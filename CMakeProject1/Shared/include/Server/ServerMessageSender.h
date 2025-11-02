#pragma once
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <MessageTypes/Interface/IMessage.hpp>
namespace Utils {
    void SendMessage(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
        std::shared_ptr<IMessage> message,
        const boost::system::error_code& error);
}