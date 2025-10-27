#include <iostream>
#include <string>
#include <memory>
#include <boost/asio.hpp>
#include <MessageTypes/TextMessage.h>
using boost::asio::ip::tcp;

boost::asio::io_context io;
std::shared_ptr<tcp::socket> client_socket;

void start_read();

void handle_read_text(std::shared_ptr<std::vector<char>> buffer, // Zmieniono na std::vector<char>
    const boost::system::error_code& error,
    std::size_t bytes_transferred)
{
    if (!error)
    {
        // 1. Zmniejszenie bufora do faktycznie odczytanych bajtów
        buffer->resize(bytes_transferred);

        // 2. Deserializacja
        TextMessage received_msg;

        // zakladamy, że serwer wysyła tylko dane tekstowe
        try {
            received_msg.deserialize(*buffer);

        // 3. Wyświetlenie przetworzonej wiadomości
            std::cout << "Server says (deserialized): " << received_msg.to_string() << std::endl;

        }
        catch (const std::exception& e) {
            std::cerr << "Deserialization error: " << e.what() << std::endl;
        }

        start_read();
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

void start_read()
{
    auto buffertype = std::make_shared<std::vector<char>>();
    buffertype->resize(sizeof(uint32_t));

    boost::asio::async_read(*client_socket,boost::asio::buffer(*buffertype),
        [buffertype](const boost::system::error_code& err, std::size_t bytes)
        {
            auto buffersize = std::make_shared<std::vector<char>>();
            buffersize->resize(sizeof(uint32_t));
            boost::asio::async_read(*client_socket, boost::asio::buffer(*buffersize),
                [buffertype, buffersize](const boost::system::error_code& err, std::size_t bytes) {
                    uint32_t type;
                    std::memcpy(&type, buffertype->data(), buffertype->size());
                    type = ntohl(type);
                    uint32_t length;
                    std::memcpy(&length, buffersize->data(), buffersize->size());
                    length = ntohl(length);
                    auto text = std::make_shared<std::vector<char>>();
                    text->resize(length);

                    boost::asio::async_read(*client_socket, boost::asio::buffer(*text),
                        [buffertype, buffersize, text, type, length](const boost::system::error_code& err, std::size_t bytes) {
                            auto fulldata = std::make_shared<std::vector<char>>();
                            fulldata->reserve(sizeof(uint32_t) * 2 + length);

                            uint32_t net_type = htonl(type);
                            uint32_t net_length = htonl(length);

                            fulldata->insert(fulldata->end(), reinterpret_cast<char*>(&net_type),
                                reinterpret_cast<char*>(&net_type) + sizeof(net_type));
                            fulldata->insert(fulldata->end(), reinterpret_cast<char*>(&net_length),
                                reinterpret_cast<char*>(&net_length) + sizeof(net_length));
                            fulldata->insert(fulldata->end(), text->begin(), text->end());
                            handle_read_text(fulldata, err, sizeof(uint32_t) * 2 + length);
                        }
                    );

                }
            );
        });
}

void handle_connect(const boost::system::error_code& error)
{
    if (!error)
    {
        std::cout << "Connected to server!\n";
        std::cout << "Server ip: " << client_socket->remote_endpoint().address()
            << " Server port: " << client_socket->remote_endpoint().port() << std::endl;

        start_read();
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
