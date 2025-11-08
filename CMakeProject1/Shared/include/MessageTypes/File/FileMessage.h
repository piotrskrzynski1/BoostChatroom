#pragma once
#include <MessageTypes/Interface/IMessage.hpp>
#include <filesystem>
#include <vector>
#include <string>

class FileMessage : public IMessage
{
private:
    std::vector<char> bytes_;
    std::string filename_;

public:
    FileMessage() = default;
    explicit FileMessage(const std::filesystem::path& path);

    // Convert message to bytes to send over socket
    std::vector<char> serialize() const override;
    // Load message from bytes received
    void deserialize(const std::vector<char>& data) override;
    // Optional: get a human-readable representation
    std::string to_string() const override;
    // Optional: save the file upon recieving
    void save_file() const override;
};
