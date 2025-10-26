#include <ServerManager/ServerManager.h>
#include <iostream>
#include <thread>
ServerManager::ServerManager(int port, std::string&& ipAddress) {
	this->port = port;
	this->address = std::move(ipAddress);
	std::cout << "Server configured at address: " << this->address << " Port: " << this->port << std::endl;
}

void ServerManager::StartServer() {
	try
	{
		tcp::acceptor acceptor(io_context, tcp::endpoint(boost::asio::ip::make_address_v4(this->address), this->port));
		auto socket = std::make_shared<tcp::socket>(io_context);
		acceptor.async_accept(*socket, [this, socket, &acceptor](const boost::system::error_code& error) {
            auto hellomessage = std::make_shared<TextMessage>("Hello client");
			SendMessage(socket, hellomessage, error);

			StartServer();
		});
		io_context.run();
	}
	catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}
}
void ServerManager::SendMessage(std::shared_ptr<tcp::socket> socket,
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
int ServerManager::GetPort() {
	return this->port;
}
std::string ServerManager::GetIpAddress() {
	return this->address;
}