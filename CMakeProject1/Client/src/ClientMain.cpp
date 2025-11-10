#include <iostream>
#include <string>
#include <memory>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <MessageTypes/Text/TextMessage.h>
#include <Server/ClientServerConnectionManager.h>
#include <Server/CommandProcessor.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

void send_text_message(const ClientServerConnectionManager& mng, const std::string& line)
{
    try
    {
        TextMessage text(line);
        const std::shared_ptr<IMessage> pointer = std::make_shared<TextMessage>(text);
        mng.Message(TextTypes::Text, pointer);
    }
    catch (const std::exception& e)
    {
        std::cerr << "TextMessage send failed: " << e.what() << std::endl;
    }
}

// Helper function to get valid port input
int get_port_input(const std::string& prompt, const int default_port)
{
    std::cout << prompt << " [" << default_port << "]: ";
    std::string input;
    std::getline(std::cin, input);

    if (input.empty()) return default_port;

    try {
        int port = std::stoi(input);
        if (port < 1 || port > 65535) {
            std::cerr << "Invalid port. Using default: " << default_port << std::endl;
            return default_port;
        }
        return port;
    }
    catch (...) {
        std::cerr << "Invalid input. Using default: " << default_port << std::endl;
        return default_port;
    }
}

// Helper function to get IP input
std::string get_ip_input(const std::string& prompt, const std::string& default_ip)
{
    std::cout << prompt << " [" << default_ip << "]: ";
    std::string input;
    std::getline(std::cin, input);

    if (input.empty()) return default_ip;

    // Basic validation
    if (input == "localhost" || input == "127.0.0.1" ||
        input.find('.') != std::string::npos) {
        return input;
    }

    std::cerr << "Invalid IP format. Using default: " << default_ip << std::endl;
    return default_ip;
}

int main()
{
    try
    {
        std::cout << "=== Client Configuration ===" << std::endl;
        std::cout << "Enter server connection details" << std::endl;
        std::cout << "(Press Enter to use defaults shown in brackets)" << std::endl;
        std::cout << std::endl;

        // Get configuration from user
        std::string server_ip = get_ip_input("Enter server IP address", "0.0.0.0");
        int text_port = get_port_input("Enter text message port", 5555);
        int file_port = get_port_input("Enter file transfer port", 5556);

        std::cout << "\n=== Connecting to Server ===" << std::endl;
        std::cout << "Server IP: " << server_ip << std::endl;
        std::cout << "Text Port: " << text_port << std::endl;
        std::cout << "File Port: " << file_port << std::endl;
        std::cout << "===========================\n" << std::endl;

        boost::asio::io_context io_context;

        // Create client with user-provided configuration
        ClientServerConnectionManager mng(io_context, server_ip,
                                         static_cast<unsigned short>(text_port),
                                         static_cast<unsigned short>(file_port));

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

        // Give connection time to establish
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::cout << "Connected! Enter messages (type 'quit' to exit, '/help' for commands)" << std::endl;

        // --- Main Application Loop ---
        std::string line;

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

        std::cout << "Disconnecting..." << std::endl;
        mng.Disconnect();

        t.join();
    }
    catch (std::exception& e)
    {
        std::cerr << "Main exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}