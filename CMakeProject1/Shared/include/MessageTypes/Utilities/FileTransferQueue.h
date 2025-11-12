#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <filesystem>
#include <boost/asio.hpp>
#include "MessageTypes/File/FileMessage.h"

// Returns the socket currently associated with this queue
using SocketGetter = std::function<std::shared_ptr<boost::asio::ip::tcp::socket>()>;

class FileTransferQueue
{
public:
    enum class State { Queued, Sending, Failed, Done, Canceled };

    struct Item {
        uint64_t id = 0;
        std::filesystem::path path;                // Used if built from a local file
        std::shared_ptr<FileMessage> message;      // Used when already built (forwarded or constructed)
        State state = State::Queued;
        int retries = 0;
        std::string last_error;
    };

    explicit FileTransferQueue(SocketGetter socket_getter);
    ~FileTransferQueue();

    // === Enqueue Methods ===

    /**
     * @brief Enqueue a file from a filesystem path (loads file from disk)
     **/
    uint64_t enqueue(const std::filesystem::path& path);

    /**
     * @brief Enqueue From an already-built FileMessage (useful when forwarding)
     **/
    uint64_t enqueue(const std::shared_ptr<FileMessage>& message);

    /**
     * @brief Create a new FileMessage and enqueue
     * @param filename name for the new file
     * @param bytes byte data for the new file
     **/
    uint64_t enqueue(const std::string& filename, const std::vector<uint8_t>& bytes);

    // === Control and Management ===

    bool remove(uint64_t id);
    bool retry(uint64_t id);
    void pause();
    void resume();
    bool cancel(uint64_t id);
    void cancel_all();
    std::vector<Item> list_snapshot();
    void stop();

private:
    // === Helpers ===

    // Build a FileMessage from a file path (disk)
    static std::shared_ptr<FileMessage> make_file_message(const std::filesystem::path& p);

    // Build a FileMessage directly from raw bytes (for forwarding)
    static std::shared_ptr<FileMessage> make_file_message_from_bytes(
        const std::string& filename,
        const std::vector<uint8_t>& bytes);

    // Background worker
    void worker_loop();

private:
    SocketGetter socket_getter_;

    std::deque<Item> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;

    std::atomic<bool> running_{true};
    std::atomic<bool> paused_{false};

    uint64_t next_id_ = 1;
};
