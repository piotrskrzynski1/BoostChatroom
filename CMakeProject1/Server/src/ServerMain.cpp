#include <Server/ServerManager.h>
#include <iostream>
#include <string>

using namespace std;

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

    // Basic validation (you could make this more robust)
    if (input == "0.0.0.0" || input == "127.0.0.1" ||
        input.find('.') != std::string::npos) {
        return input;
    }

    std::cerr << "Invalid IP format. Using default: " << default_ip << std::endl;
    return default_ip;
}

int main()
{
    std::cout << "=== Server Configuration ===" << std::endl;
    std::cout << "Note: Use 0.0.0.0 to accept connections from any interface" << std::endl;
    std::cout << "      Use 127.0.0.1 for localhost-only connections" << std::endl;
    std::cout << std::endl;

    // Get configuration from user
    std::string ip = get_ip_input("Enter server IP address", "0.0.0.0");
    int text_port = get_port_input("Enter text message port", 5555);
    int file_port = get_port_input("Enter file transfer port", 5556);

    std::cout << "\n=== Starting Server ===" << std::endl;
    std::cout << "IP: " << ip << std::endl;
    std::cout << "Text Port: " << text_port << std::endl;
    std::cout << "File Port: " << file_port << std::endl;
    std::cout << "\nPress Ctrl+C to stop the server" << std::endl;
    std::cout << "========================\n" << std::endl;

    try {
        ServerManager srvman(text_port, file_port, std::move(ip));
        srvman.StartServer();
    }
    catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}