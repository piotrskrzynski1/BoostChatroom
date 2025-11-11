#include <fstream>
#include <iostream>
#include <MessageTypes/File/FileMessage.h>
#include <MessageTypes/Utilities/HeaderHelper.hpp>
// from uint8_t bytes


FileMessage::FileMessage(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("File does not exist: " + path.string());
    }
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error("Path is not a regular file: " + path.string());
    }

    const auto file_size = std::filesystem::file_size(path);
    bytes_.resize(static_cast<size_t>(file_size));

    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open file: " + path.string());

    file.read(reinterpret_cast<char*>(bytes_.data()), file_size);
    if (!file) throw std::runtime_error("Failed to read full file: " + path.string());

    filename_ = path.filename().string();
}

std::vector<char> FileMessage::serialize() const
{
    constexpr auto id = static_cast<uint32_t>(TextTypes::File); // host order
    const uint64_t name_length = filename_.size();
    const uint64_t file_length = bytes_.size();

    const uint64_t payload_size = sizeof(name_length) + sizeof(file_length) + name_length + file_length;
    const uint64_t total_size = sizeof(id) + sizeof(payload_size) + payload_size;

    std::vector<char> buffer;
    buffer.reserve(static_cast<size_t>(total_size));

    // HeaderHelper takes care of converting to network byte order
    Utils::HeaderHelper::append_u32(buffer, id);
    Utils::HeaderHelper::append_u64(buffer, payload_size);
    Utils::HeaderHelper::append_u64(buffer, name_length);
    Utils::HeaderHelper::append_u64(buffer, file_length);

    buffer.insert(buffer.end(), filename_.begin(), filename_.end());
    buffer.insert(buffer.end(), bytes_.begin(), bytes_.end());

    return buffer;
}


void FileMessage::deserialize(const std::vector<char>& data)
{
// TODO potential spam and memory overflow (we dont limit message size) (TODO also in MessageReciever.cpp)
    if (data.size() < sizeof(uint32_t) + sizeof(uint64_t))
    {
        throw std::runtime_error("Message is too short in deserialzation");
    }

    // Basic structural check
    if (data.size() < sizeof(uint32_t) + 3 * sizeof(uint64_t))
        throw std::runtime_error("Invalid FileMessage data (too short)");

    size_t offset = 0;

    uint32_t id = 0;
    Utils::HeaderHelper::read_u32(data, offset, id);
    offset += sizeof(uint32_t);

    uint64_t payload_size = 0;
    Utils::HeaderHelper::read_u64(data, offset, payload_size);
    offset += sizeof(uint64_t);

    uint64_t name_length = 0;
    Utils::HeaderHelper::read_u64(data, offset, name_length);
    offset += sizeof(uint64_t);

    uint64_t file_length = 0;
    Utils::HeaderHelper::read_u64(data, offset, file_length);
    offset += sizeof(uint64_t);

    size_t expected_total = sizeof(uint32_t) + sizeof(uint64_t) + static_cast<size_t>(payload_size);
    if (data.size() < expected_total)
        throw std::runtime_error("Incomplete FileMessage buffer");

    if (data.size() < offset + name_length + file_length)
        throw std::runtime_error("Corrupted FileMessage lengths");

    filename_.assign(data.begin() + offset, data.begin() + offset + name_length);
    offset += static_cast<size_t>(name_length);

    bytes_.assign(data.begin() + offset, data.begin() + offset + file_length);
}

std::string FileMessage::to_string() const
{
    return "FileMessage: " + filename_ + " (" + std::to_string(bytes_.size()) + " bytes)";
}

std::vector<char> FileMessage::to_data_send() const
{
    return bytes_;
}
void FileMessage::save_file() const
{
    // std::cout << "[DEBUG] bytes_ size = " << bytes_.size() << "\n";  // REMOVED

    namespace fs = std::filesystem;
    try {
        if (bytes_.empty()) {
            std::cerr << "No data to write!\n";
            return;
        }
        fs::path output_dir = get_desktop_path();
        if (!fs::exists(output_dir)) fs::create_directories(output_dir);
        fs::path safe_filename = fs::path(filename_).filename();

        fs::path output_path = output_dir / safe_filename;


        std::ofstream out_file(output_path, std::ios::binary);
        if (!out_file)
            throw std::runtime_error("Cannot write file: " + output_path.string());

        out_file.write(bytes_.data(), bytes_.size());
        out_file.close();

        // Optional: Keep this for actual errors/important info
        // std::cout << "File saved: " << output_path << " (" << bytes_.size() << " bytes)\n";
    }
    catch (const std::exception& e) {
        std::cerr << "FileMessage::save_file error: " << e.what() << std::endl;
    }
}

