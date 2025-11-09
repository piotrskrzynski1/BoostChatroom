// CommandProcessor.h
#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include "Interface/ICommand.h"

class CommandProcessor {
public:
    /**
     * @brief Constructor that registers all available commands.
     */
    CommandProcessor();

    /**
     * @brief Tries to find and execute a command from a full input line.
     * @param mng The ClientServerManager to pass to the command.
     * @param line The raw line of input from the user.
     * @return true if a command was found and executed, false otherwise.
     */
    bool process(ClientServerManager& mng, const std::string& line);

private:
    std::unordered_map<std::string, std::unique_ptr<ICommand>> commands_;
};