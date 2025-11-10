#pragma once
#include <iostream>
#include <memory>
#include <boost/asio.hpp>

class MessageReciever
{
private:
    std::function<void(std::shared_ptr<boost::asio::ip::tcp::socket>, const std::string&)> on_message_text;
    std::function<void(std::shared_ptr<boost::asio::ip::tcp::socket>, const std::shared_ptr<std::vector<char>>&)>
    on_message_file;


    void start_read_body(const std::shared_ptr<std::vector<char>>& header_buffer,
                         uint64_t body_length,
                         const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
                         const std::shared_ptr<std::string>& napis);
    void handle_read_message(const std::shared_ptr<std::vector<char>>& buffer,
                             const boost::system::error_code& error,
                             std::size_t bytes_transferred,
                             const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
                             const std::shared_ptr<std::string>& napis);

public:
    void start_read_header(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                           const std::shared_ptr<std::string>& napis = nullptr);
    void set_on_message_callback(
        std::function<void(std::shared_ptr<boost::asio::ip::tcp::socket>, const std::string&)> callback);
    void set_on_file_callback(
        std::function<void(std::shared_ptr<boost::asio::ip::tcp::socket>, const std::shared_ptr<std::vector<char>>&)>
        callback);
    void set_on_message_callback(
        std::function<void(std::shared_ptr<boost::asio::ip::tcp::socket>, const std::shared_ptr<std::vector<char>>&)>
        callback);
};
