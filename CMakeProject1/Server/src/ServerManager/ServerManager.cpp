#include <ServerManager/ServerManager.h>
#include <iostream>
ServerManager::ServerManager(int port, std::string&& ipAddress) {
	this->port = port;
	this->address = std::move(ipAddress);
	StartServer();
}

void ServerManager::StartServer() {
	try
	{
		tcp::acceptor acceptor(io_context, tcp::endpoint(boost::asio::ip::make_address_v4(this->address), this->port));
		std::cout << "Server running at address: " << this->address << " Port: " << this->port << std::endl;
		auto socket = std::make_shared<tcp::socket>(io_context);
		acceptor.async_accept(*socket, [this, socket, &acceptor](const boost::system::error_code& error) {
			SendHelloMessage(socket, error);

			StartServer();
		});
		io_context.run();
	}
	catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}
}
void ServerManager::SendHelloMessage(std::shared_ptr<tcp::socket> socket, const boost::system::error_code& error) {
	if (!error)
	{
		auto message = std::make_shared<std::string>("Successfuly connected!");
		boost::asio::async_write(*socket, boost::asio::buffer(*message),
			[socket, message](const boost::system::error_code& error, std::size_t /*bytes*/)
		{
			if (!error)
			{
				std::cout << "Sent welcome to Client.\nClient ip: " << socket->remote_endpoint().address() << " client port: " << socket->remote_endpoint().port() << std::endl;
			}
			else
			{
				std::cerr << "Error sending: " << error.message() << "\n";
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