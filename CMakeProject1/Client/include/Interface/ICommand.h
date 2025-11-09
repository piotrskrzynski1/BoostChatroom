// ICommand.h
#pragma once

#include <string>
#include <Server/ClientServerManager.h>

/**
 * @brief Abstract base class for all client commands.
 */
class ICommand {
public:
    virtual ~ICommand() = default;

    /**
     * @brief Executes the command.
     * @param mng The ClientServerManager to operate on.
     * @param args The arguments for the command (the string after the command name).
     */
    virtual void execute(ClientServerManager& mng, const std::string& args) = 0;
};