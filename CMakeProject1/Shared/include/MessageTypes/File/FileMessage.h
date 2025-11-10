#pragma once
#include <MessageTypes/Interface/IMessage.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <cstdint>

class FileMessage : public IMessage
{
private:
    std::vector<char> bytes_;
    std::string filename_;

public:
    FileMessage() = default;
    explicit FileMessage(const std::filesystem::path& path);

    // construct directly from file data in memory
    explicit FileMessage(const std::string& filename, const std::vector<uint8_t>& bytes);
    explicit FileMessage(const std::string& filename, const std::vector<char>& bytes);

    // Convert message to bytes to send over socket
    std::vector<char> serialize() const override;
    // Load message from bytes received
    void deserialize(const std::vector<char>& data) override;
    // Optional: get a human-readable representation
    std::string to_string() const override;
    [[nodiscard]] std::vector<char> to_data_send() const override;
    // Optional: save the file upon recieving
    void save_file() const override;
};
