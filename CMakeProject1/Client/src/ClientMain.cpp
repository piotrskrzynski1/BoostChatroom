#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <cstring>
#include <boost/asio.hpp>
#include <thread>
#include <MessageTypes/TextMessage.h>
#include <Server/ClientServerManager.h>
#include <Server/ServerMessageSender.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h> // Dla ntohl, htonl endiany i te sprawy
#endif

int main()
{
    try
    {
        boost::asio::io_context io_context;
        const ClientServerManager mng(io_context);

        std::thread t([&io_context]()
        {
            try
            {
                io_context.run();
            }
            catch (std::exception& e)
            {
                std::cerr << "Asio thread exception: " << e.what() << std::endl;
            }
        });

        std::cout << "Enter messages (type 'quit' to exit):" << std::endl;

        std::string line;
        while (std::getline(std::cin, line))
        {
            if (line == "quit") break;

            if (!line.empty())
            {
                TextMessage text(line);
                std::shared_ptr<IMessage> pointer = std::make_shared<TextMessage>(text);
                mng.Message(pointer);
            }
        }

        mng.Disconnect();
        t.join();
    }
    catch (std::exception& e)
    {
        std::cerr << "Main exception: " << e.what() << "\n";
    }

    return 0;
}
