#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <vector>
#include <filesystem> // Required for temp paths
#include <fstream>    // Required for temp file creation
#include <iostream>   // For logging in fixtures
#include <exception>  // Required for std::exception_ptr

// Assuming these are the correct paths relative to your test file
#include "ServerManagerTest.h"
#include "Server/ServerManager.h"
#include "Server/ClientServerManager.h"

// ---------- Mock message ----------
class MockMessage : public IMessage {
public:
    std::string content;
    explicit MockMessage(std::string c) : content(std::move(c)) {}
    ~MockMessage() override = default;
};

// Creates a dummy file for tests and guarantees cleanup
struct ScopedTempFile {
    std::filesystem::path path;

    ScopedTempFile(const std::string& filename = "gtest_temp_file.txt") {
        path = std::filesystem::temp_directory_path() / filename;
        std::ofstream outfile(path);
        outfile << "dummy file data for testing";
        outfile.close();
        std::cout << "DEBUG: Created temp file" << path << "exists=" << std::filesystem::exists(path) << std::endl;
    }

    ~ScopedTempFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        if (ec) {
            std::cerr << "Warning: Failed to remove temp file " << path << ": " << ec.message() << std::endl;
        }
    }

    // Helper to get path as string
    std::string string() const {
        return path.string();
    }
};


// ---------- Server fixture ----------
class TestableServerManagerTest : public ::testing::Test {
protected:
    std::unique_ptr<TestableServerManager> server;
    std::thread serverThread;

    // ✅ **FIX 1: Add exception_ptr to catch thread exceptions**
    std::exception_ptr server_exception_ = nullptr;

    void SetUp() override {
        server = std::make_unique<TestableServerManager>(5555, 5556, "127.0.0.1");

        // Run server in background thread
        serverThread = std::thread([this]() {
            try {
                server->StartServer(); // blocks internally
            } catch (...) {
                // ✅ **FIX 1: Store the exception instead of just logging**
                server_exception_ = std::current_exception();
            }
        });

        // ⚠️ NOTE: This sleep is fragile.
        // A better solution would be to make the server signal when it's ready.
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    void TearDown() override {
        if (server) server->StopServer();
        if (serverThread.joinable()) serverThread.join();

        // ✅ **FIX 1: Check for and re-throw any background exceptions**
        if (server_exception_) {
            try {
                std::rethrow_exception(server_exception_);
            } catch (const std::exception& e) {
                FAIL() << "Exception caught in server thread: " << e.what();
            } catch (...) {
                FAIL() << "Unknown exception caught in server thread.";
            }
        }
    }
};

// ---------- Client fixture ----------
class ClientServerManagerTest : public ::testing::Test {
protected:
    // Server-side components
    std::unique_ptr<TestableServerManager> server;
    std::thread serverThread;

    // Client-side components
    boost::asio::io_context io;
    std::unique_ptr<ClientServerManager> manager;
    std::thread clientIoThread;

    // Exception pointers for background threads
    std::exception_ptr server_exception_ = nullptr;
    std::exception_ptr client_exception_ = nullptr;

    void SetUp() override {
        // 1. Start the Server
        server = std::make_unique<TestableServerManager>(5555, 5556, "127.0.0.1");
        serverThread = std::thread([this]() {
            try {
                server->StartServer();
            } catch (...) {
                server_exception_ = std::current_exception();
            }
        });

        // This sleep is fragile and can cause flaky tests.
        //std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 2. Create the Client
        manager = std::make_unique<ClientServerManager>(io, "127.0.0.1", 5555, 5556);

        // 3. Run the Client's io_context in its own thread
        clientIoThread = std::thread([this]() {
            try {
                io.run();
            } catch (...) {
                client_exception_ = std::current_exception();
            }
        });

        // The test should ideally wait for the client's handle_connect
        // to set a promise/future to confirm connection.
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    void TearDown() override {
        // 1. Stop the Client (and its io_context)
        if (manager) manager->Disconnect();
        io.stop();
        if (clientIoThread.joinable()) clientIoThread.join();

        // 2. Stop the Server
        if (server) server->StopServer();
        if (serverThread.joinable()) serverThread.join();

        // 3. Check for and report any thread exceptions
        if (server_exception_) {
            try {
                std::rethrow_exception(server_exception_);
            } catch (const std::exception& e) {
                FAIL() << "Exception caught in fixture's server thread: " << e.what();
            } catch (...) {
                FAIL() << "Unknown exception caught in fixture's server thread.";
            }
        }

        if (client_exception_) {
            try {
                std::rethrow_exception(client_exception_);
            } catch (const std::exception& e) {
                FAIL() << "Exception caught in client IO thread: " << e.what();
            } catch (...) {
                FAIL() << "Unknown exception caught in client IO thread.";
            }
        }
    }
};

// ---------- Tests ----------

// --- Server-Only Tests ---

TEST_F(TestableServerManagerTest, BroadcastTextDoesNotCrash) {
    EXPECT_NO_THROW(server->Broadcast(nullptr, std::string("Hello")));
}

TEST_F(TestableServerManagerTest, BroadcastFileDoesNotCrash) {
    // Use a portable temporary file, not a hardcoded path**
    ScopedTempFile temp_file("temp_broadcast_file.txt");

    // We assume FileMessage can be constructed from a path
    // and correctly serializes.
    auto packet = FileMessage(temp_file.string());
    auto data = std::make_shared<std::vector<char>>(packet.serialize());
    EXPECT_NO_THROW(server->Broadcast(nullptr, data));
}

TEST_F(TestableServerManagerTest, GetIpAddressReturnsCorrectValue) {
    EXPECT_EQ(server->GetIpAddress(), "127.0.0.1");
}

// --- Client/Integration Tests ---

TEST_F(ClientServerManagerTest, EnqueueFileAddsItem) {
    // Use a portable temporary file**
    ScopedTempFile temp_file("temp_enqueue_file.txt");

    // 1. Pause the queue so the worker thread doesn't grab the file
    manager->PauseQueue();

    // 2. Enqueue the file
    uint64_t id;
    EXPECT_NO_THROW(id = manager->EnqueueFile(temp_file.string()));
    EXPECT_NE(id, 0u);

    // 3. Test the snapshot (this is now 100% safe)
    std::vector<FileTransferQueue::Item> snapshot;
    EXPECT_NO_THROW(snapshot = manager->FileQueueSnapshot());
    ASSERT_FALSE(snapshot.empty()); // Use ASSERT to stop if this fails
    EXPECT_EQ(snapshot.front().path, temp_file.string());
}

TEST_F(ClientServerManagerTest, PauseResumeQueueWorks) {
    ScopedTempFile temp_file("temp_pause_file.txt");

    // Pause *first* to prevent the race condition
    manager->PauseQueue();

    // Now enqueue the file. It will wait.
    manager->EnqueueFile(temp_file.string());

    // Now test the functions
    EXPECT_NO_THROW(manager->PauseQueue()); // Still paused
    EXPECT_NO_THROW(manager->ResumeQueue()); // Now it will process the file

    // Add a small sleep to give the worker time to *read* the file
    // before ScopedTempFile deletes it.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(ClientServerManagerTest, CancelAndReconnectFileSocketDoesNotCrash) {
    // This test is now meaningful, as it will cancel a real connection
    EXPECT_NO_THROW(manager->CancelAndReconnectFileSocket());
}

// ---------- Entry point ----------
int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}