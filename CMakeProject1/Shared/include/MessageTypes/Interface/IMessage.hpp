#pragma once
#include <cstdint>
#include <vector>
#include <string>

enum class TextTypes : uint32_t
{
    Text = 0,
    File = 1,
};

class IMessage
{
public:
    virtual ~IMessage() = default;

    /**
     * @brief Convert message to bytes to send over socket
     **/
    virtual std::vector<char> serialize() const = 0;

    /**
     * @brief Load message from bytes received
     **/
    virtual void deserialize(const std::vector<char>& data) = 0;

    /**
     * @brief Get a string value for the given message (could be anything)
     **/
    virtual std::string to_string() const = 0;

    /**
     * @brief Get a vector<char> object to send over the network.
     **/
    virtual std::vector<char> to_data_send() const = 0;
    /**
     * @brief Save file to desktop
     */
    virtual void save_file() const = 0;
};
