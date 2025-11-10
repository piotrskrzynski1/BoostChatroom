#include "MessageTypes/Utilities/FileTransferQueue.h"
#include <boost/asio.hpp>
#include <iostream>
#include <algorithm>
#include <thread>

#include "MessageTypes/File/FileMessage.h" // for constructing FileMessage directly

using boost::asio::ip::tcp;

FileTransferQueue::FileTransferQueue(SocketGetter socket_getter)
    : socket_getter_(std::move(socket_getter))
{
    worker_ = std::thread([this]() { worker_loop(); });
}

FileTransferQueue::~FileTransferQueue()
{
    stop();
}

uint64_t FileTransferQueue::enqueue(const std::filesystem::path& path)
{
    std::scoped_lock lk(mutex_);
    uint64_t id = next_id_++;
    Item it;
    it.id = id;
    it.path = path;
    it.state = State::Queued;
    it.retries = 0;
    it.last_error.clear();
    it.message = nullptr;
    queue_.push_back(std::move(it));
    cv_.notify_one();
    return id;
}

uint64_t FileTransferQueue::enqueue(const std::shared_ptr<FileMessage>& message)
{
    if (!message) return 0;
    std::scoped_lock lk(mutex_);
    uint64_t id = next_id_++;
    Item it;
    it.id = id;
    it.path.clear();
    it.state = State::Queued;
    it.retries = 0;
    it.last_error.clear();
    it.message = message;
    queue_.push_back(std::move(it));
    cv_.notify_one();
    return id;
}

uint64_t FileTransferQueue::enqueue(const std::string& filename, const std::vector<uint8_t>& bytes)
{
    // Try to build a FileMessage from bytes here (if FileMessage supports it)
    auto msg = make_file_message_from_bytes(filename, bytes);
    if (msg) return enqueue(msg);

    // Fall back: cannot build message -> return 0
    std::cerr << "FileTransferQueue::enqueue(bytes): failed to build FileMessage\n";
    return 0;
}

bool FileTransferQueue::remove(uint64_t id)
{
    std::scoped_lock lk(mutex_);
    auto it = std::find_if(queue_.begin(), queue_.end(), [&](const Item& i){ return i.id == id; });
    if (it != queue_.end()) {
        queue_.erase(it);
        return true;
    }
    return false;
}

bool FileTransferQueue::retry(uint64_t id)
{
    std::scoped_lock lk(mutex_);
    for (auto &it : queue_) {
        if (it.id == id) {
            it.state = State::Queued;
            it.last_error.clear();
            it.retries++;
            it.message = nullptr; // rebuild message on retry if needed
            cv_.notify_one();
            return true;
        }
    }
    return false;
}

void FileTransferQueue::pause()
{
    paused_.store(true);
}

void FileTransferQueue::resume()
{
    paused_.store(false);
    cv_.notify_one();
}

bool FileTransferQueue::cancel(uint64_t id)
{
    std::scoped_lock lk(mutex_);
    bool found = false;
    bool was_sending = false;
    for (auto &it : queue_) {
        if (it.id == id) {
            found = true;

            if (it.state == State::Sending) {
                was_sending = true;
            }

            it.state = State::Canceled;
            it.last_error = "canceled by user";
            break;
        }
    }

    if (was_sending) {
        try {
            auto sock = socket_getter_();
            if (sock && sock->is_open()) {
                boost::system::error_code ec;
                sock->cancel(ec);
                sock->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                sock->close(ec);
                if (ec) {
                    std::cerr << "FileTransferQueue::cancel socket shutdown/close error: " << ec.message() << "\n";
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "FileTransferQueue::cancel socket_getter threw: " << e.what() << "\n";
        }
    }

    cv_.notify_one();
    return found;
}

void FileTransferQueue::cancel_all()
{
    {
        std::scoped_lock lk(mutex_);
        for (auto &it : queue_) {
            if (it.state == State::Queued || it.state == State::Failed || it.state == State::Sending) {
                it.state = State::Canceled;
                it.last_error = "canceled by user";
            }
        }
    }

    try {
        auto sock = socket_getter_();
        if (sock && sock->is_open()) {
            boost::system::error_code ec;
            sock->cancel(ec);
            sock->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            sock->close(ec);
            if (ec) {
                std::cerr << "FileTransferQueue::cancel_all socket shutdown/close error: " << ec.message() << "\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "FileTransferQueue::cancel_all: socket_getter threw: " << e.what() << "\n";
    }

    cv_.notify_one();
}

std::vector<FileTransferQueue::Item> FileTransferQueue::list_snapshot()
{
    std::scoped_lock lk(mutex_);
    std::vector<Item> out;
    out.reserve(queue_.size());
    for (auto const& it : queue_) out.push_back(it);
    return out;
}

void FileTransferQueue::stop()
{
    if (!running_.load()) return;
    running_.store(false);
    cv_.notify_one();
    if (worker_.joinable()) worker_.join();
}

std::shared_ptr<FileMessage> FileTransferQueue::make_file_message(const std::filesystem::path& p)
{
    try {
        return std::make_shared<FileMessage>(p);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create FileMessage from path: " << e.what() << "\n";
        return nullptr;
    }
}

// Attempt to build a FileMessage from bytes. If your FileMessage supports such constructor, it will succeed.
// Otherwise return nullptr.
std::shared_ptr<FileMessage> FileTransferQueue::make_file_message_from_bytes(const std::string& filename, const std::vector<uint8_t>& bytes)
{
    try {
        // Try existing constructor taking filename + bytes (if present)
        // If your FileMessage doesn't have this signature, implement it (see suggestion below).
        return std::make_shared<FileMessage>(filename, bytes);
    } catch (const std::exception& e) {
        // If that constructor does not exist, simply return nullptr
        (void)e;
        return nullptr;
    }
}

void FileTransferQueue::worker_loop()
{
    while (running_.load()) {
        std::unique_lock lk(mutex_);
        cv_.wait(lk, [this]() {
            return !running_.load() || (!paused_.load() && !queue_.empty());
        });

        if (!running_.load()) break;
        if (paused_.load()) continue;

        auto it = std::find_if(queue_.begin(), queue_.end(),
            [](const Item& i) { return i.state == State::Queued;});

        if (it == queue_.end()) {
            continue;
        }

        Item item = *it;
        it->state = State::Sending;
        it->last_error.clear();
        lk.unlock();

        // If message is missing but we have a path, try to build it.
        if (!item.message && !item.path.empty()) {
            item.message = make_file_message(item.path);
        }

        if (!item.message) {
            std::scoped_lock lk2(mutex_);
            auto qit = std::find_if(queue_.begin(), queue_.end(), [&](const Item& x){ return x.id == item.id; });
            if (qit != queue_.end()) {
                if (qit->state != State::Canceled) {
                    qit->state = State::Failed;
                    qit->last_error = "failed to build FileMessage (no path/message)";
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        auto bytes = item.message->serialize();

        auto sock = socket_getter_();
        if (!sock || !sock->is_open()) {
            std::scoped_lock lk2(mutex_);
            auto qit = std::find_if(queue_.begin(), queue_.end(), [&](const Item& x){ return x.id == item.id; });
            if (qit != queue_.end()) {
                if (qit->state != State::Canceled) {
                    qit->state = State::Failed;
                    qit->last_error = "socket not connected";
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        boost::system::error_code ec;
        try {
            std::size_t written = boost::asio::write(*sock, boost::asio::buffer(bytes), ec);
            (void)written;
        } catch (const std::exception& ex) {
            ec = boost::asio::error::operation_aborted;
            std::cerr << "Exception during write: " << ex.what() << "\n";
        }

        std::unique_lock lk3(mutex_);
        auto qit = std::find_if(queue_.begin(), queue_.end(), [&](const Item& i){ return i.id == item.id; });
        if (qit == queue_.end()) {
            continue;
        }

        if (qit->state == State::Canceled) {
            if (qit->last_error.empty()) qit->last_error = "canceled by user";
            continue;
        }

        if (ec) {
            qit->state = State::Failed;
            qit->last_error = ec.message();
            qit->retries++;
            std::cerr << "File send failed (id=" << qit->id << "): " << ec.message() << "\n";
        } else {
            qit->state = State::Done;
            qit->last_error.clear();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}
