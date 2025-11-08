#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

// Forward declare FileMessage
class FileMessage;

class FileTransferQueue
{
public:
    /**
     * @brief Defines the state of a transfer item in the queue.
     */
    enum class State {
        Queued,     // Waiting to be sent
        Sending,    // In-flight
        Done,       // Sent successfully
        Failed,     // Failed to send, can be retried
        Canceled    // Canceled by user, will not be retried
    };

    /**
     * @brief Represents a single file transfer item in the queue.
     */
    struct Item {
        uint64_t id;
        std::filesystem::path path;
        State state;
        uint32_t retries;
        std::string last_error;
        std::shared_ptr<FileMessage> message; // Pre-built message
    };

    /**
     * @brief Type alias for the socket getter function.
     * The queue calls this when it needs the socket to send a file.
     */
    using SocketGetter = std::function<std::shared_ptr<boost::asio::ip::tcp::socket>()>;

    /**
     * @brief Constructs the queue and starts its worker thread.
     * @param socket_getter A function that returns the socket to use for sending.
     */
    FileTransferQueue(SocketGetter socket_getter);

    /**
     * @brief Stops the worker thread and destroys the queue.
     */
    ~FileTransferQueue();

    // --- Public API ---

    /**
     * @brief Adds a file path to the queue for sending.
     * @param path The full path to the file.
     * @return A unique ID for this transfer item.
     */
    uint64_t enqueue(const std::filesystem::path& path);

    /**
     * @brief Removes an item from the queue entirely.
     * @param id The ID returned by enqueue().
     * @return true if the item was found and removed, false otherwise.
     */
    bool remove(uint64_t id);

    /**
     * @brief Marks a 'Failed' item to be retried.
     * @param id The ID of the item to retry.
     * @return true if the item was found and marked for retry, false otherwise.
     */
    bool retry(uint64_t id);

    /**
     * @brief Pauses the worker thread. No new files will be sent.
     */
    void pause();

    /**
     * @brief Resumes the worker thread.
     */
    void resume();

    /**
     * @brief Cancels a specific item. If it's sending, attempts to close the socket.
     * @param id The ID of the item to cancel.
     * @return true if the item was found, false otherwise.
     */
    bool cancel(uint64_t id);

    /**
     * @brief Marks all Queued, Failed, or Sending items as Canceled.
     * Attempts to close the socket to interrupt any in-flight transfer.
     */
    void cancel_all();

    /**
     * @brief Gets a thread-safe copy of the current queue state.
     * @return A vector containing all current items.
     */
    std::vector<Item> list_snapshot();

    /**
     * @brief Stops the worker thread.
     */
    void stop();


private:
    void worker_loop();
    std::shared_ptr<FileMessage> make_file_message(const std::filesystem::path& p);

    std::vector<Item> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;

    std::atomic<bool> running_{true};
    std::atomic<bool> paused_{false};
    std::atomic<uint64_t> next_id_{1};

    SocketGetter socket_getter_;
};