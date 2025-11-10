// CommandProcessor.cpp
#include "Server/CommandProcessor.h"
#include <iostream>
#include <functional> // For std::function
#include <Server/ClientServerManager.h> // Need full def for methods

#include "Interface/ICommand.h"

// --- Anonymous Namespace to hide concrete command classes ---
namespace
{
    // Type alias from your main.cpp
    using CommandHandlerFunc = std::function<void(ClientServerManager&, const std::string&)>;

    /**
     * @brief A helper class that turns any std::function (like a lambda)
     * into an ICommand object. This is perfect for your simple commands.
     */
    class LambdaCommand : public ICommand
    {
    public:
        LambdaCommand(CommandHandlerFunc func) : func_(std::move(func))
        {
        }

        void execute(ClientServerManager& mng, const std::string& args) override
        {
            func_(mng, args);
        }

    private:
        CommandHandlerFunc func_;
    };


    // --- Concrete Command Classes ---

    class HelpCommand : public ICommand
    {
    public:
        void execute(ClientServerManager& mng, const std::string& args) override
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
    };

    class PrintQueueCommand : public ICommand
    {
    public:
        void execute(ClientServerManager& mng, const std::string& args) override
        {
            auto snap = mng.FileQueueSnapshot();
            if (snap.empty())
            {
                std::cout << "(queue empty)\n";
            }
            else
            {
                for (auto& it : snap)
                {
                    std::cout << "id: " << it.id
                        << " path: " << it.path
                        << " state: " << static_cast<int>(it.state)
                        << " retries: " << it.retries
                        << " err: " << it.last_error << "\n";
                }
            }
        }
    };

    class PrintHistoryCommand : public ICommand
    {
    public:
        void execute(ClientServerManager& mng, const std::string& args) override
        {
            auto snap = mng.FileQueueSnapshot();
            bool found = false;
            for (auto& it : snap)
            {
                if (it.state == FileTransferQueue::State::Done)
                {
                    found = true;
                    std::cout << "id: " << it.id
                        << " path: " << it.path
                        << " retries: " << it.retries
                        << "\n";
                }
            }
            if (!found) std::cout << "(no history yet)\n";
        }
    };

    class EnqueueFileCommand : public ICommand
    {
    public:
        void execute(ClientServerManager& mng, const std::string& path) override
        {
            if (path.empty())
            {
                std::cerr << "Usage: /file <path>\n";
                return;
            }
            try
            {
                uint64_t id = mng.EnqueueFile(path);
                if (id == 0)
                    std::cerr << "Failed to enqueue file\n";
                else
                    std::cout << "Enqueued file id=" << id << " path=" << path << "\n";
            }
            catch (const std::exception& e)
            {
                std::cerr << "Enqueue failed: " << e.what() << std::endl;
            }
        }
    };

    class CancelFileCommand : public ICommand
    {
    public:
        void execute(ClientServerManager& mng, const std::string& args) override
        {
            try
            {
                uint64_t id = std::stoull(args);
                mng.CancelFile(id);
                std::cout << "Requested cancel for id " << id << "\n";
            }
            catch (...)
            {
                std::cerr << "Invalid id for /cancel. Usage: /cancel <id>\n";
            }
        }
    };

    class RetryFileCommand : public ICommand
    {
    public:
        void execute(ClientServerManager& mng, const std::string& args) override
        {
            try
            {
                uint64_t id = std::stoull(args);
                mng.RetryFile(id);
                std::cout << "Requested retry for id " << id << "\n";
            }
            catch (...)
            {
                std::cerr << "Invalid id for /retry. Usage: /retry <id>\n";
            }
        }
    };
} // end anonymous namespace


// --- CommandProcessor Implementation ---

CommandProcessor::CommandProcessor()
{
    // Register all commands

    // Commands implemented as full classes
    commands_["/help"] = std::make_unique<HelpCommand>();
    commands_["/queue"] = std::make_unique<PrintQueueCommand>();
    commands_["/history"] = std::make_unique<PrintHistoryCommand>();
    commands_["/file"] = std::make_unique<EnqueueFileCommand>();
    commands_["/cancel"] = std::make_unique<CancelFileCommand>();
    commands_["/retry"] = std::make_unique<RetryFileCommand>();

    // Simple commands implemented with our LambdaCommand helper
    commands_["/pause"] = std::make_unique<LambdaCommand>(
        [](ClientServerManager& m, const std::string& a)
        {
            m.PauseQueue();
            std::cout << "Queue paused.\n";
        });

    commands_["/resume"] = std::make_unique<LambdaCommand>(
        [](ClientServerManager& m, const std::string& a)
        {
            m.ResumeQueue();
            std::cout << "Queue resumed.\n";
        });

    commands_["/cancelall"] = std::make_unique<LambdaCommand>(
        [](ClientServerManager& m, const std::string& a)
        {
            m.CancelAndReconnectFileSocket();
        });
}

bool CommandProcessor::process(ClientServerManager& mng, const std::string& line)
{
    std::string command;
    std::string args;

    // Split the line into command and arguments
    size_t first_space = line.find(' ');
    if (first_space == std::string::npos)
    {
        command = line;
        args = "";
    }
    else
    {
        command = line.substr(0, first_space);
        args = line.substr(first_space + 1);
    }

    // Find and execute the command
    auto it = commands_.find(command);
    if (it != commands_.end())
    {
        // Found a handler, execute it
        it->second->execute(mng, args);
        return true; // We handled the command
    }

    return false; // Not a command
}
