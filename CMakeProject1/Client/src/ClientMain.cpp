#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <regex>
#include <boost/asio.hpp>
#include <thread>
#include <functional>
#include <unordered_map>
#include <../../Shared/include/MessageTypes/Text/TextMessage.h>
#include <Server/ClientServerManager.h>
#include "MessageTypes/File/FileMessage.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h> // for htonl, ntohl, etc.
#endif

// --- Type alias for our command functions ---
// All command functions will take the manager and any arguments as a string
using CommandHandler = std::function<void(ClientServerManager&, const std::string&)>;

// --- Command Logic Functions (moved from main) ---

void print_help(ClientServerManager& mng, const std::string& args) {
    std::cout <<
    "Available commands:\n"
    "  /file <path>     - enqueue a file to send\n"
    "  /queue           - show queued files and their states\n"
    "  /history         - list successfully sent files (log)\n"
    "  /pause           - pause the file sending queue\n"
    "  /resume          - resume the file sending queue\n"
    "  /cancel <id>     - cancel a queued/sending file by id\n"
    "  /cancelall       - cancel ALL files currently in the queue\n"
    "  /retry <id>      - retry a failed file by id\n"
    "  /help            - show this help text\n"
    "  quit             - exit the program\n"
    "Anything else will be sent as a text message.\n";
}

void print_queue(ClientServerManager& mng, const std::string& args) {
    auto snap = mng.FileQueueSnapshot();
    if (snap.empty()) {
        std::cout << "(queue empty)\n";
    } else {
        for (auto &it : snap) {
            std::cout << "id: " << it.id
                      << " path: " << it.path
                      << " state: " << static_cast<int>(it.state)
                      << " retries: " << it.retries
                      << " err: " << it.last_error << "\n";
        }
    }
}

void print_history(ClientServerManager& mng, const std::string& args) {
    auto snap = mng.FileQueueSnapshot();
    bool found = false;
    for (auto &it : snap) {
        if (it.state == FileTransferQueue::State::Done) {
            found = true;
            std::cout << "id: " << it.id
                      << " path: " << it.path
                      << " retries: " << it.retries
                      << "\n";
        }
    }
    if (!found) std::cout << "(no history yet)\n";
}

void enqueue_file(ClientServerManager& mng, const std::string& path) {
    if (path.empty()) {
        std::cerr << "Usage: /file <path>\n";
        return;
    }
    try {
        uint64_t id = mng.EnqueueFile(path);
        if (id == 0)
            std::cerr << "Failed to enqueue file\n";
        else
            std::cout << "Enqueued file id=" << id << " path=" << path << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Enqueue failed: " << e.what() << std::endl;
    }
}

void cancel_file(ClientServerManager& mng, const std::string& args) {
    try {
        uint64_t id = std::stoull(args);
        mng.CancelFile(id);
        std::cout << "Requested cancel for id " << id << "\n";
    } catch (...) {
        std::cerr << "Invalid id for /cancel. Usage: /cancel <id>\n";
    }
}

void retry_file(ClientServerManager& mng, const std::string& args) {
    try {
        uint64_t id = std::stoull(args);
        mng.RetryFile(id);
        std::cout << "Requested retry for id " << id << "\n";
    } catch (...) {
        std::cerr << "Invalid id for /retry. Usage: /retry <id>\n";
    }
}

void send_text_message(ClientServerManager& mng, const std::string& line) {
    try {
        TextMessage text(line);
        std::shared_ptr<IMessage> pointer = std::make_shared<TextMessage>(text);
        mng.Message(TextTypes::Text, pointer);
    } catch (const std::exception& e) {
        std::cerr << "TextMessage send failed: " << e.what() << std::endl;
    }
}

// --- Main Application ---

int main()
{
    // --- Create the Command Map ---
    // This map links command strings to the functions that handle them.
    std::unordered_map<std::string, CommandHandler> command_handlers;

    // No-argument commands
    command_handlers["/help"] = print_help;
    command_handlers["/queue"] = print_queue;
    command_handlers["/history"] = print_history;
    command_handlers["/pause"] = [](auto& m, auto& a) { m.PauseQueue(); std::cout << "Queue paused.\n"; };
    command_handlers["/resume"] = [](auto& m, auto& a) { m.ResumeQueue(); std::cout << "Queue resumed.\n"; };
    command_handlers["/cancelall"] = [](auto& m, auto& a) { m.CancelAndReconnectFileSocket(); };

    // Commands with arguments
    command_handlers["/file"] = enqueue_file;
    command_handlers["/cancel"] = cancel_file;
    command_handlers["/retry"] = retry_file;


    // --- Application Setup ---
    try
    {
        boost::asio::io_context io_context;
        ClientServerManager mng(io_context,"127.0.0.1",5555,5556);

        std::thread t([&io_context]() {
            try {
                io_context.run();
            } catch (std::exception& e) {
                std::cerr << "Asio thread exception: " << e.what() << std::endl;
            }
        });

        std::cout << "Enter messages (type 'quit' to exit). Type /help for commands." << std::endl;

        // --- Main Application Loop ---
        std::string line;
        while (std::getline(std::cin, line))
        {
            if (line == "quit") break;
            if (line.empty()) continue;

            std::string command;
            std::string args;

            // Split the line into command and arguments
            size_t first_space = line.find(' ');
            if (first_space == std::string::npos) {
                // No space, command is the whole line
                command = line;
                args = "";
            } else {
                // Command is before the space, args are after
                command = line.substr(0, first_space);
                args = line.substr(first_space + 1);
            }

            // --- Find and execute the command ---
            auto it = command_handlers.find(command);
            if (it != command_handlers.end()) {
                // Found a handler, execute it
                it->second(mng, args);
            } else {
                // No command found, send as a text message
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