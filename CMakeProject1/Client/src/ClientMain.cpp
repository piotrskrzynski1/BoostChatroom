#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <cstring>
#include <regex>
#include <boost/asio.hpp>
#include <thread>
#include <../../Shared/include/MessageTypes/Text/TextMessage.h>
#include <Server/ClientServerManager.h>
#include <Server/ServerMessageSender.h>

#include "MessageTypes/File/FileMessage.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h> // Dla ntohl, htonl endiany i te sprawy
#endif

bool match_file_command(const std::string& input, std::string& path_out)
{
    // Regex explanation:
    // ^/find\\s+   → starts with "/find" followed by at least one space
    // (.+)$        → capture the rest of the line as group 1 (the path)
    static const std::regex pattern{R"(^/file\s+(.+)$)"};

    std::smatch match;
    if (std::regex_match(input, match, pattern)) {
        path_out = match[1].str(); // captured path
        return true;
    }
    return false;
}

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
                std::string path;
                if (match_file_command(line, path))
                {
                    try {
                        FileMessage file(path);
                        std::shared_ptr<IMessage> pointer = std::make_shared<FileMessage>(file);
                        mng.Message(TextTypes::File, pointer);
                    }
                    catch (const std::exception& e) {
                        std::cerr << "FileMessage creation failed: " << e.what() << std::endl;
                    }
                }else
                {
                    TextMessage text(line);
                    std::shared_ptr<IMessage> pointer = std::make_shared<TextMessage>(text);
                    mng.Message(TextTypes::Text,pointer);
                }
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
