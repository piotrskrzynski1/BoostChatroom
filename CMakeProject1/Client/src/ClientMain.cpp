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
#include <arpa/inet.h> // for htonl, ntohl, etc.
#endif

bool match_file_command(const std::string& input, std::string& path_out)
{
    // Regex: starts with "/file " followed by any path
    static const std::regex pattern{R"(^/file\s+(.+)$)"};

    std::smatch match;
    if (std::regex_match(input, match, pattern)) {
        path_out = match[1].str();
        return true;
    }
    return false;
}

void print_help()
{
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

int main()
{
    try
    {
        boost::asio::io_context io_context;
        ClientServerManager mng(io_context);  // not const, since we’ll call control functions

        std::thread t([&io_context]()
        {
            try {
                io_context.run();
            }
            catch (std::exception& e) {
                std::cerr << "Asio thread exception: " << e.what() << std::endl;
            }
        });

        std::cout << "Enter messages (type 'quit' to exit). Type /help for commands." << std::endl;

        std::string line;
        while (std::getline(std::cin, line))
        {
            if (line == "quit") break;
            if (line.empty()) continue;

            std::string path;

            // --- Help ---
            if (line == "/help") {
                print_help();
                continue;
            }

            // --- Queue management commands ---
            if (line == "/queue") {
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
                continue;
            }

            // /history shows items with state == Done
            if (line == "/history") {
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
                continue;
            }

            if (line == "/pause") { mng.PauseQueue(); std::cout << "Queue paused.\n"; continue; }
            if (line == "/resume") { mng.ResumeQueue(); std::cout << "Queue resumed.\n"; continue; }

            if (line.rfind("/cancel ", 0) == 0) {
                try {
                    uint64_t id = std::stoull(line.substr(8));
                    mng.CancelFile(id);
                    std::cout << "Requested cancel for id " << id << "\n";
                } catch (...) {
                    std::cerr << "Invalid id for /cancel\n";
                }
                continue;
            }

            if (line == "/cancelall") {
                mng.CancelAndReconnectFileSocket();
                continue;
            }

            if (line.rfind("/retry ", 0) == 0) {
                try {
                    uint64_t id = std::stoull(line.substr(7));
                    mng.RetryFile(id);
                    std::cout << "Requested retry for id " << id << "\n";
                } catch (...) {
                    std::cerr << "Invalid id for /retry\n";
                }
                continue;
            }

            // --- File sending ---
            if (match_file_command(line, path))
            {
                try {
                    uint64_t id = mng.EnqueueFile(path);
                    if (id == 0)
                        std::cerr << "Failed to enqueue file\n";
                    else
                        std::cout << "Enqueued file id=" << id << " path=" << path << "\n";
                }
                catch (const std::exception& e) {
                    std::cerr << "Enqueue failed: " << e.what() << std::endl;
                }
                continue;
            }

            // --- Regular text message ---
            try {
                TextMessage text(line);
                std::shared_ptr<IMessage> pointer = std::make_shared<TextMessage>(text);
                mng.Message(TextTypes::Text, pointer);
            }
            catch (const std::exception& e) {
                std::cerr << "TextMessage send failed: " << e.what() << std::endl;
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
