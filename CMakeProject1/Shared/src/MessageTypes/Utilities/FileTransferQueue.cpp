#include <MessageTypes/Utilities/FileTransferQueue.h>
#include <boost/asio.hpp>
#include <iostream>
#include <algorithm>
#include <thread>

#include "MessageTypes/File/FileMessage.h" // for constructing FileMessage directly

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
            it.message = nullptr; // rebuild message on retry
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

            // **FIX**: Check state *before* changing it
            if (it.state == State::Sending) {
                was_sending = true;
            }

            // mark canceled so worker will honor it
            it.state = State::Canceled;
            it.last_error = "canceled by user";
            break;
        }
    }

    // **FIX**: Only interrupt socket if it was actively sending
    if (was_sending) {
        try {
            auto sock = socket_getter_();
            if (sock && sock->is_open()) {
                boost::system::error_code ec;
                // shutdown then close: this will make the server's async_read fail and free it
                sock->cancel(ec); // best-effort
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
            // mark pending/failed/sending items as canceled so UI shows them
            if (it.state == State::Queued || it.state == State::Failed || it.state == State::Sending) {
                it.state = State::Canceled;
                it.last_error = "canceled by user";
            }
        }
    }

    // Attempt to interrupt any in-flight socket operations by shutting down and closing the socket
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

    // Wake worker in case it's waiting
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
        // Simpler and more direct: construct FileMessage from path (no factory indirection)
        return std::make_shared<FileMessage>(p);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create FileMessage: " << e.what() << "\n";
        return nullptr;
    }
}

void FileTransferQueue::worker_loop()
{
    while (running_.load()) {
        // wait for work
        std::unique_lock lk(mutex_);
        cv_.wait(lk, [this]() {
            return !running_.load() || (!paused_.load() && !queue_.empty());
        });

        if (!running_.load()) break;
        if (paused_.load()) continue;

        // find next queued item (skip canceled / done / sending)
        auto it = std::find_if(queue_.begin(), queue_.end(),
            [](const Item& i) { return i.state == State::Queued || i.state == State::Failed; });

        if (it == queue_.end()) {
            // nothing to do
            continue;
        }

        // prepare item for sending (copy snapshot)
        Item item = *it;
        // mark original element as Sending so UI sees it
        it->state = State::Sending;
        it->last_error.clear();
        lk.unlock();

        // ensure message is built
        if (!item.message) item.message = make_file_message(item.path);

        if (!item.message) {
            // failed to create message -> mark original element as Failed (if still present)
            std::scoped_lock lk2(mutex_);
            auto qit = std::find_if(queue_.begin(), queue_.end(), [&](const Item& x){ return x.id == item.id; });
            if (qit != queue_.end()) {

                // **FIX**: Only mark as Failed if it wasn't already Canceled
                if (qit->state != State::Canceled) {
                    qit->state = State::Failed;
                    qit->last_error = "failed to open file";
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // serialize
        auto bytes = item.message->serialize();

        // get socket
        auto sock = socket_getter_();
        if (!sock || !sock->is_open()) {
            std::scoped_lock lk2(mutex_);
            auto qit = std::find_if(queue_.begin(), queue_.end(), [&](const Item& x){ return x.id == item.id; });
            if (qit != queue_.end()) {

                // **FIX**: Only mark as Failed if it wasn't already Canceled
                if (qit->state != State::Canceled) {
                    qit->state = State::Failed;
                    qit->last_error = "socket not connected";
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        // perform synchronous write so we can determine success/failure
        boost::system::error_code ec;
        std::size_t written = 0;
        try {
            written = boost::asio::write(*sock, boost::asio::buffer(bytes), ec);
        } catch (const std::exception& ex) {
            ec = boost::asio::error::operation_aborted;
            std::cerr << "Exception during write: " << ex.what() << "\n";
        }

        // finalize: update queue state but preserve Cancelled if user canceled
        std::unique_lock lk3(mutex_);
        auto qit = std::find_if(queue_.begin(), queue_.end(), [&](const Item& i){ return i.id == item.id; });
        if (qit == queue_.end()) {
            // removed while sending
            continue;
        }

        // if user canceled while we were sending, keep it canceled
        if (qit->state == State::Canceled) {
            // keep canceled, optionally set last_error if empty
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

        // small delay to avoid tight loop
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}