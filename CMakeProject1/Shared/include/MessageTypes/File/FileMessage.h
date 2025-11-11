#pragma once
#include <MessageTypes/Interface/IMessage.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <cstdint>

#include <filesystem>
#include <cstdlib>   // getenv
#include <string>

inline std::filesystem::path get_desktop_path()
{
    namespace fs = std::filesystem;

#if defined(_WIN32)
    const char* home = std::getenv("USERPROFILE");
    if (!home)
        throw std::runtime_error("USERPROFILE env variable not found");
    return fs::path(home) / "Desktop";

#elif defined(__APPLE__) || defined(__linux__)
    const char* home = std::getenv("HOME");
    if (!home)
        throw std::runtime_error("HOME env variable not found");
    return fs::path(home) / "Desktop";
#else
#error Unsupported system
#endif
}


class FileMessage : public IMessage
{
private:
    std::vector<char> bytes_;
    std::string filename_;

public:
    FileMessage() = default;
    template <class T>
    explicit FileMessage(const std::string& filename, const std::vector<T>& bytes);
    explicit FileMessage(const std::filesystem::path& path);


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
