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

    std::vector<char> serialize() const override;
    void deserialize(const std::vector<char>& data) override;
    std::string to_string() const override;
    void save_file() const;
};
