#include <iostream>
#include <string>
#include <memory>
#include <vector>  
#include <cstring>   
#include <boost/asio.hpp>
#include <MessageTypes/TextMessage.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h> // Dla ntohl, htonl
#endif

using boost::asio::ip::tcp;

boost::asio::io_context io;
std::shared_ptr<tcp::socket> client_socket;

void start_read_header();
void start_read_body(std::shared_ptr<std::vector<char>> header_buffer, uint32_t body_length);
void handle_read_text(std::shared_ptr<std::vector<char>> buffer,
    const boost::system::error_code& error,
    std::size_t bytes_transferred);



void handle_read_text(std::shared_ptr<std::vector<char>> buffer,
    const boost::system::error_code& error,
    std::size_t bytes_transferred)
{
    if (!error)
    {
        buffer->resize(bytes_transferred);

        TextMessage received_msg;

        try {
            received_msg.deserialize(*buffer);

            std::cout << "Server says (deserialized): " << received_msg.to_string() << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Deserialization error: " << e.what() << std::endl;
        }

        start_read_header();
    }
    else if (error == boost::asio::error::eof)
    {
        std::cout << "Server closed the connection.\n";
    }
    else
    {
        std::cerr << "Read error: " << error.message() << std::endl;
    }
}


void start_read_header()
{
    //początku każdej wiadomości składa się z dwóch liczb typu uint32_t. Pierwsza liczba oznacza typ druga ilosc bajtów
    const size_t header_size = sizeof(uint32_t) * 2;
    auto header_buffer = std::make_shared<std::vector<char>>(header_size);

    boost::asio::async_read(*client_socket, boost::asio::buffer(*header_buffer),
        [header_buffer](const boost::system::error_code& err, std::size_t bytes_transferred)
        {
            if (err)
            {
                handle_read_text(header_buffer, err, bytes_transferred);
                return;
            }

            uint32_t body_length;


            std::memcpy(&body_length, header_buffer->data() + sizeof(uint32_t), sizeof(uint32_t));
            body_length = ntohl(body_length); 

            if (body_length == 0)
            {
                handle_read_text(header_buffer, boost::system::error_code(), header_buffer->size());
            }
            else
            {
                start_read_body(header_buffer, body_length);
            }
        });
}

void start_read_body(std::shared_ptr<std::vector<char>> header_buffer, uint32_t body_length)
{
    auto body_buffer = std::make_shared<std::vector<char>>(body_length);

    boost::asio::async_read(*client_socket, boost::asio::buffer(*body_buffer),
        [header_buffer, body_buffer](const boost::system::error_code& err, std::size_t bytes_transferred)
        {
            if (err)
            {
                handle_read_text(body_buffer, err, bytes_transferred);
                return;
            }

            auto fulldata = std::make_shared<std::vector<char>>(*header_buffer);
            fulldata->insert(fulldata->end(), body_buffer->begin(), body_buffer->end());

            handle_read_text(fulldata, boost::system::error_code(), fulldata->size());
        });
}


void handle_connect(const boost::system::error_code& error)
{
    if (!error)
    {
        std::cout << "Connected to server!\n";
        std::cout << "Server ip: " << client_socket->remote_endpoint().address()
            << " Server port: " << client_socket->remote_endpoint().port() << std::endl;

        start_read_header();
    }
    else
    {
        std::cerr << "Connection failed: " << error.message() << std::endl;
    }
}

int main()
{
    try
    {
        tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), 5555);
        client_socket = std::make_shared<tcp::socket>(io);

        std::cout << "Connecting to server...\n";
        client_socket->async_connect(endpoint, handle_connect);

        io.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}