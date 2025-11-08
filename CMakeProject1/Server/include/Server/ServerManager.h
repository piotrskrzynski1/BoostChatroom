#pragma once
#include <string>
#include <boost/asio.hpp>
#include <MessageTypes/Interface/IMessage.hpp>
#include <MessageTypes/Text/TextMessage.h>
#include <mutex>
#include <vector>
#include <Server/MessageReciever.h>
using boost::asio::ip::tcp;

class ServerManager
{
private:
    std::vector<std::shared_ptr<tcp::socket>> text_port_clients_;
    std::vector<std::shared_ptr<tcp::socket>> file_port_clients_;
    std::mutex text_port_clients_mutex_;
    std::mutex file_port_clients_mutex_;
    MessageReciever messageReciever_;
    MessageReciever fileReciever;
    void AcceptTextConnection(const std::shared_ptr<tcp::acceptor>& acceptor);
    void AcceptFileConnection(const std::shared_ptr<tcp::acceptor>& acceptor);
    void Broadcast(const std::shared_ptr<tcp::socket>& sender, const std::string& text);

protected:
    int port;
    int fileport;
    std::string address;
    boost::asio::io_context io_context;

public:
    std::string GetIpAddress();

    ServerManager(int port, int fileport, std::string&& ipAddress);
    void StartServer();
    int GetPort() const;
};
