#pragma once
#include <iostream>
#include <memory>
#include <boost/asio.hpp>

class MessageReciever
{
private:
    std::function<void(std::shared_ptr<boost::asio::ip::tcp::socket>, const std::string&)> on_message;

    void start_read_body(std::shared_ptr<std::vector<char>> header_buffer,
                                      uint64_t body_length,
                                      std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                                      std::shared_ptr<std::string> napis);
    void handle_read_message(std::shared_ptr<std::vector<char>> buffer,
                                              const boost::system::error_code& error,
                                              std::size_t bytes_transferred,
                                              std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                                              std::shared_ptr<std::string> napis);

public:
    void start_read_header(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                           std::shared_ptr<std::string> napis = nullptr);
    void set_on_message_callback(
        std::function<void(std::shared_ptr<boost::asio::ip::tcp::socket>, const std::string&)> callback);
};
