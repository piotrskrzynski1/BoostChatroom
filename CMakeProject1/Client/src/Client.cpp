#include <iostream>
#include <string>
#include <memory>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

boost::asio::io_context io;

std::shared_ptr<boost::asio::ip::tcp::socket> client_socket;

void handle_read(std::shared_ptr<std::string> data,
    const boost::system::error_code& error,
    std::size_t bytes_transferred)
{
    if (!error)
    {
        std::cout << "Server says: " << data->substr(0, bytes_transferred) << std::endl;
    }
    else
    {
        std::cerr << "Read error: " << error.message() << std::endl;
    }
}

void handle_connect(const boost::system::error_code& error)
{
    if (!error)
    {
        std::cout << "Connected to server!\n";
        std::cout << "Server ip: " << client_socket->remote_endpoint().address() << " Server port: " << client_socket->remote_endpoint().port() << std::endl;

        auto data = std::make_shared<std::string>(1024, 0);

        client_socket->async_read_some(boost::asio::buffer(*data),
            [data](const boost::system::error_code& err, std::size_t bytes)
        {
            handle_read(data, err, bytes);
        });
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
        tcp::resolver resolver(io);
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
