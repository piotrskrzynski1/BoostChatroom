#include <Server/MessageReciever.h>
#include <MessageTypes/Text/TextMessage.h>
#include <cstring>
#include <iostream>
#include <MessageTypes/Utilities/HeaderHelper.hpp>
#include <MessageTypes/Utilities/MessageFactory.h>

#ifdef _DEBUG
#define LOG(x) std::cout << "[DEBUG] " << x << std::endl
#else
#define LOG(x) ((void)0)
#endif

// --- helper for 64-bit endianness ---
static uint64_t ntohll(uint64_t value)
{
    static const int num = 1;
    if (*reinterpret_cast<const char*>(&num) == 1)
    {
        // little endian → swap 32-bit halves
        return (static_cast<uint64_t>(ntohl(static_cast<uint32_t>(value & 0xFFFFFFFFULL))) << 32) |
               ntohl(static_cast<uint32_t>(value >> 32));
    }
    return value; // already big-endian
}

// ------------------------------------------------------------------

void MessageReciever::start_read_body(std::shared_ptr<std::vector<char>> header_buffer,
                                      uint64_t body_length,
                                      std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                                      std::shared_ptr<std::string> napis)
{
    auto body_buffer = std::make_shared<std::vector<char>>(static_cast<size_t>(body_length));

    boost::asio::async_read(*socket, boost::asio::buffer(*body_buffer),
        [header_buffer, body_buffer, this, socket, napis](const boost::system::error_code& err, std::size_t bytes_transferred)
        {
            if (err)
            {
                handle_read_message(body_buffer, err, bytes_transferred, socket, napis);
                return;
            }

            auto fulldata = std::make_shared<std::vector<char>>(*header_buffer);
            fulldata->insert(fulldata->end(), body_buffer->begin(),
                             body_buffer->begin() + bytes_transferred);

            handle_read_message(fulldata, boost::system::error_code(), fulldata->size(), socket, napis);
        });
}

void MessageReciever::handle_read_message(std::shared_ptr<std::vector<char>> buffer,
                                          const boost::system::error_code& error,
                                          std::size_t bytes_transferred,
                                          std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                                          std::shared_ptr<std::string> napis)
{
    if (error) {
    if (error == boost::asio::error::eof) {
        std::cout << "Client closed the connection.\n";
    } else {
        std::cerr << "Read error: " << error.message() << std::endl;
    }

    // make a best-effort to close socket and stop reading further
    boost::system::error_code ec;
    socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    socket->close(ec);

    // If you have a callback to notify the server to remove/cleanup this socket, call it here.
    // e.g. if (on_disconnect) on_disconnect(socket);

    return;
}


    buffer->resize(bytes_transferred);

    try {
        uint32_t id;
        Utils::HeaderHelper::read_u32(*buffer, 0, id);

#ifdef _DEBUG
        std::cout << "id: " << id << std::endl;
#endif
        auto message = MessageFactory::create_from_id(id);
        message->deserialize(*buffer);

        std::string message_str = message->to_string();
        std::cout << message_str << std::endl;
#ifdef _DEBUG
        std::cout << typeid(message).name() << std::endl;
#endif
        message->save_file();

        if (on_message) on_message(socket, message_str);
        if (napis) *napis = message_str;
    }
    catch (const std::exception& e) {
        std::cerr << "Deserialization error: " << e.what() << std::endl;
    }

    this->start_read_header(socket);
}

void MessageReciever::start_read_header(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                                        std::shared_ptr<std::string> napis)
{
    const size_t header_size = sizeof(uint32_t) + sizeof(uint64_t);
    auto header_buffer = std::make_shared<std::vector<char>>(header_size);

    boost::asio::async_read(*socket, boost::asio::buffer(*header_buffer),
        [header_buffer, header_size, this, socket, napis](const boost::system::error_code& err, std::size_t bytes_transferred)
        {
            if (err) {
                handle_read_message(header_buffer, err, bytes_transferred, socket, napis);
                return;
            }

            if (bytes_transferred < header_size) {
                boost::system::error_code ec = boost::asio::error::operation_aborted;
                handle_read_message(header_buffer, ec, bytes_transferred, socket, napis);
                return;
            }

            uint32_t id;
            uint64_t body_length;
            std::memcpy(&id, header_buffer->data(), sizeof(uint32_t));
            id = ntohl(id);
            std::memcpy(&body_length, header_buffer->data() + sizeof(uint32_t), sizeof(uint64_t));
            body_length = ntohll(body_length);   // 64-bit conversion

            if (body_length != 0) {
                start_read_body(header_buffer, body_length, socket, napis);
                return;
            }

            handle_read_message(header_buffer, boost::system::error_code(),
                                header_buffer->size(), socket, napis);
        });
}

void MessageReciever::set_on_message_callback(
    std::function<void(std::shared_ptr<boost::asio::ip::tcp::socket>, const std::string&)> callback)
{
    on_message = std::move(callback);
}
