#include <iostream>
#include <string>
#include <memory>
#include <boost/asio.hpp>
#include <thread>
#include <MessageTypes/Text/TextMessage.h>
#include <Server/ClientServerConnectionManager.h>
#include <Server/CommandProcessor.h>


#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

void send_text_message(ClientServerConnectionManager& mng, const std::string& line)
{
    try
    {
        TextMessage text(line);
        std::shared_ptr<IMessage> pointer = std::make_shared<TextMessage>(text);
        mng.Message(TextTypes::Text, pointer);
    }
    catch (const std::exception& e)
    {
        std::cerr << "TextMessage send failed: " << e.what() << std::endl;
    }
}


int main()

{
    try

    {
        boost::asio::io_context io_context;

        //create clientservermanager
        ClientServerConnectionManager mng(io_context, "127.0.0.1", 5555, 5556);

        // Create command processor
        CommandProcessor command_processor;

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

        std::cout << "Enter messages (type 'quit' to exit). Type /help for commands." << std::endl;

        // --- Main Application Loop ---

        std::string line;

        //get line from cin
        while (std::getline(std::cin, line))
        {
            if (line == "quit") break;
            if (line.empty()) continue;

            // Ask the processor to handle the line.
            // If it returns false, it wasn't a command.
            if (!command_processor.process(mng, line))
            {
                // Not a command, send as a text message
                send_text_message(mng, line);
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
