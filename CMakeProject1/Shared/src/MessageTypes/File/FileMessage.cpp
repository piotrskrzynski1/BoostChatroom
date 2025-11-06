#include <fstream>
#include <iostream>
#include <MessageTypes/File/FileMessage.h>
#include <MessageTypes/Utilities/HeaderHelper.h>

FileMessage::FileMessage(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("File does not exist: " + path.string());
    }
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error("Path is not a regular file: " + path.string());
    }

    // Read file bytes
    const auto file_size = std::filesystem::file_size(path);
    bytes_.resize(static_cast<size_t>(file_size));

    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open file: " + path.string());

    file.read(reinterpret_cast<char*>(bytes_.data()), file_size);
    if (!file) throw std::runtime_error("Failed to read full file: " + path.string());

    // Store the filename (with extension)
    filename_ = path.filename().string();
}

std::vector<char> FileMessage::serialize() const
{
    const uint32_t id = static_cast<uint32_t>(TextTypes::File);
    const uint64_t name_length = filename_.size();
    const uint64_t file_length = bytes_.size();

    size_t total_size = sizeof(id) + sizeof(name_length) + sizeof(file_length)
                        + name_length + file_length;

    std::vector<char> buffer;
    buffer.reserve(total_size);

    Utils::HeaderHelper::append_u32(buffer, id);
    Utils::HeaderHelper::append_u64(buffer, name_length);
    Utils::HeaderHelper::append_u64(buffer, file_length);

    buffer.insert(buffer.end(), filename_.begin(), filename_.end());
    buffer.insert(buffer.end(), bytes_.begin(), bytes_.end());

    return buffer;
}

void FileMessage::deserialize(const std::vector<char>& data)
{
    if (data.size() < sizeof(uint32_t) + 2 * sizeof(uint64_t))
        throw std::runtime_error("Invalid FileMessage data");

    uint32_t id;
    Utils::HeaderHelper::read_u32(data, 0, id);

    uint64_t name_length = 0;
    uint64_t file_length = 0;
    Utils::HeaderHelper::read_u64(data, sizeof(uint32_t), name_length);
    Utils::HeaderHelper::read_u64(data, sizeof(uint32_t) + sizeof(uint64_t), file_length);

    size_t expected_total = sizeof(uint32_t) + 2 * sizeof(uint64_t) + name_length + file_length;
    if (data.size() < expected_total)
        throw std::runtime_error("Incomplete FileMessage buffer");

    // Extract filename and bytes
    size_t name_start = sizeof(uint32_t) + 2 * sizeof(uint64_t);
    filename_ = std::string(data.begin() + name_start, data.begin() + name_start + name_length);

    size_t file_start = name_start + name_length;
    bytes_ = std::vector<char>(data.begin() + file_start, data.begin() + file_start + file_length);
}

std::string FileMessage::to_string() const
{
    return "FileMessage: " + filename_ + " (" + std::to_string(bytes_.size()) + " bytes)";
}

void FileMessage::save_file() const
{
    namespace fs = std::filesystem;

    try {
        fs::path images_dir = fs::current_path() / "images";
        if (!fs::exists(images_dir))
            fs::create_directories(images_dir);

        std::string filename = filename_.empty() ? "received_file.bin" : filename_;
        fs::path output_path = images_dir / filename;

        std::ofstream out_file(output_path, std::ios::binary);
        if (!out_file) throw std::runtime_error("Cannot write file: " + output_path.string());

        out_file.write(reinterpret_cast<const char*>(bytes_.data()), bytes_.size());
        out_file.close();

        std::cout << "✅ File saved: " << output_path << " (" << bytes_.size() << " bytes)\n";
    }
    catch (const std::exception& e) {
        std::cerr << "❌ FileMessage::save_file error: " << e.what() << std::endl;
    }
}
