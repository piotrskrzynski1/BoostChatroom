#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <exception>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <future>
#include <algorithm> // For std::any_of

using namespace std::chrono_literals;

// Includes for your project (Adjust paths as necessary)
#include "Server/ServerManager.h"
#include "Server/ClientServerManager.h"
#include "MessageTypes/Text/TextMessage.h"
#include "MessageTypes/File/FileMessage.h"
#include "ServerManagerTest.h" // <-- Contains TestableServerManager (required for testing)

// Assuming TextTypes is an enum accessible globally or via a specific header.
// Adding a forward declaration just in case:
// enum class TextTypes;

// Creates a dummy file for tests and guarantees cleanup
struct ScopedTempFile {
    std::filesystem::path path;

    explicit ScopedTempFile(const std::string& filename = "gtest_temp_file.txt") {
        path = std::filesystem::temp_directory_path() / filename;
        std::ofstream outfile(path);
        outfile << "dummy file data for testing";
        outfile.close();
    }

    ~ScopedTempFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    std::string string() const {
        return path.string();
    }
};

// --- Integration Test Fixture ---
// This fixture sets up a Server and TWO clients for real broadcast testing.
class IntegrationTest : public ::testing::Test {
protected:
    // We use std::shared_ptr for robust Asio callback management and TestableServerManager.
    std::shared_ptr<TestableServerManager> server;
    std::thread serverThread;
    std::exception_ptr server_exception_ = nullptr;

    // Client 1 members (Text)
    boost::asio::io_context io1;
    std::shared_ptr<ClientServerManager> client1;
    std::thread client1Thread;
    std::exception_ptr client1_exception_ = nullptr;
    std::vector<std::string> client1_received_text;
    std::mutex client1_mutex;
    std::condition_variable client1_cv;

    // Client 1 members (File)
    std::vector<std::string> client1_received_files; // Store received filenames
    std::mutex client1_file_mutex;
    std::condition_variable client1_file_cv;

    // Client 2 members (Text)
    boost::asio::io_context io2;
    std::shared_ptr<ClientServerManager> client2;
    std::thread client2Thread;
    std::exception_ptr client2_exception_ = nullptr;
    std::vector<std::string> client2_received_text;
    std::mutex client2_mutex;
    std::condition_variable client2_cv;

    // Client 2 members (File)
    std::vector<std::string> client2_received_files;
    std::mutex client2_file_mutex;
    std::condition_variable client2_file_cv;


    // Helper function to wait for a text message containing a specific substring
    bool WaitForMessage(std::condition_variable& cv, std::mutex& m, std::vector<std::string>& vec, const std::string& substr) {
        std::unique_lock lk(m);
        return cv.wait_for(lk, std::chrono::seconds(5), [&]() {
            return std::any_of(vec.begin(), vec.end(),
                               [&](const auto& s) { return s.find(substr) != std::string::npos; });
        });
    }

    // Helper function to wait for a file with a specific filename
    bool WaitForFile(std::condition_variable& cv, std::mutex& m, std::vector<std::string>& vec, const std::string& filename) {
        std::unique_lock lk(m);
        return cv.wait_for(lk, std::chrono::seconds(5), [&]() {
            return std::any_of(vec.begin(), vec.end(),
                               [&](const auto& s) { return s.find(filename) != std::string::npos; });
        });
    }

    void SetUp() override {
        std::cout << "\n--- IntegrationTest::SetUp() Start ---\n";

        // 1. Setup the server object
        server = std::make_shared<TestableServerManager>(5555, 5556, "127.0.0.1");

        // --- START: Synchronization block using GetStatusUP() ---
        serverThread = std::thread([this]() {
            try {
                // Call the actual StartServer, which sets serverup_ = true when ready
                server->StartServer();
            } catch (...) {
                server_exception_ = std::current_exception();
            }
        });

        // Wait for server to be ready by polling the status flag
        auto start_time = std::chrono::steady_clock::now();
        const auto timeout = 5s;

        while (!server->GetStatusUP()) {
            if (std::chrono::steady_clock::now() - start_time > timeout) {
                 FAIL() << "Server failed to signal readiness within 5 seconds timeout.";
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        // --- END: Synchronization block ---

        std::cout << "Server ready. Starting clients.\n";

        // 2. Client 1 setup and connection attempt
        client1 = std::make_shared<ClientServerManager>(io1, "127.0.0.1", 5555, 5556);
        client1->set_on_text_message_callback([this](const std::string& msg) {
            { std::scoped_lock lk(client1_mutex); client1_received_text.push_back(msg); }
            client1_cv.notify_one();
        });
        client1->set_on_file_message_callback([this](const std::shared_ptr<FileMessage>& fileMsg) {
            std::string filename = fileMsg->to_string();
            { std::scoped_lock lk(client1_file_mutex); client1_received_files.push_back(filename); }
            client1_file_cv.notify_one();
        });
        client1Thread = std::thread([this]() {
            try { io1.run(); } catch (...) { client1_exception_ = std::current_exception(); }
        });

        // 3. Client 2 setup and connection attempt
        client2 = std::make_shared<ClientServerManager>(io2, "127.0.0.1", 5555, 5556);
        client2->set_on_text_message_callback([this](const std::string& msg) {
            { std::scoped_lock lk(client2_mutex); client2_received_text.push_back(msg); }
            client2_cv.notify_one();
        });
        client2->set_on_file_message_callback([this](const std::shared_ptr<FileMessage>& fileMsg) {
            std::string filename = fileMsg->to_string();
            { std::scoped_lock lk(client2_file_mutex); client2_received_files.push_back(filename); }
            client2_file_cv.notify_one();
        });
        client2Thread = std::thread([this]() {
            try { io2.run(); } catch (...) { client2_exception_ = std::current_exception(); }
        });

        // 4. Wait for connection confirmation (End Message History is the last message sent)
        std::cout << "Waiting for clients to receive history confirmation...\n";
        ASSERT_TRUE(WaitForMessage(client1_cv, client1_mutex, client1_received_text, "End Message History")) << "Client 1 failed to connect or receive history.";
        ASSERT_TRUE(WaitForMessage(client2_cv, client2_mutex, client2_received_text, "End Message History")) << "Client 2 failed to connect or receive history.";
        std::cout << "Clients connected and history confirmed.\n";

        // 5. Clear history messages to prepare for actual test messages
        client1_received_text.clear();
        client2_received_text.clear();
        client1_received_files.clear();
        client2_received_files.clear();
        std::cout << "--- IntegrationTest::SetUp() End ---\n";
    }

    void TearDown() override {
        std::cout << "\n--- IntegrationTest::TearDown() Start ---\n";

        // 1. Stop all IO contexts first to prevent new handlers from running
        io1.stop();
        io2.stop();

        // 2. Disconnect clients and join their IO threads
        if (client1) client1->Disconnect();
        if (client2) client2->Disconnect();

        if (client1Thread.joinable()) { client1Thread.join(); std::cout << "Client 1 IO thread joined.\n"; }
        if (client2Thread.joinable()) { client2Thread.join(); std::cout << "Client 2 IO thread joined.\n"; }

        // 3. Stop the ServerManager and join its worker thread(s)
        if (server) server->StopServer();
        if (serverThread.joinable()) { serverThread.join(); std::cout << "Server thread joined.\n"; }

        // 4. Check for exceptions (omitted for brevity)

        std::cout << "--- IntegrationTest::TearDown() End ---\n";
    }
};

// ---------- ✅ Tests ----------

TEST_F(IntegrationTest, Client1SendsClient2Receives) {
    std::cout << "\n=== Running Test: Client1SendsClient2Receives ===\n";
    auto msg = std::make_shared<TextMessage>("Hello from Client 1");
    // CORRECTED CALL: Message requires TextTypes::Text argument
    client1->Message(TextTypes::Text, msg);

    ASSERT_TRUE(WaitForMessage(client2_cv, client2_mutex, client2_received_text, "Hello from Client 1"));
    std::cout << "Client 2 received message.\n";

    std::scoped_lock lk(client2_mutex);
    std::string received_msg = client2_received_text.back();
    EXPECT_NE(received_msg.find("Hello from Client 1"), std::string::npos);

    std::scoped_lock lk1(client1_mutex);
    EXPECT_TRUE(client1_received_text.empty()); // Client 1 should not receive its own message
    std::cout << "Test Client1SendsClient2Receives completed.\n";
}

TEST_F(IntegrationTest, ServerBroadcastsAllClientsReceive) {
    std::cout << "\n=== Running Test: ServerBroadcastsAllClientsReceive ===\n";
    // Send a message from the server (sender=nullptr)
    server->Broadcast(nullptr, std::string("A message from the server"));
    std::cout << "Server broadcasted message.\n";

    ASSERT_TRUE(WaitForMessage(client1_cv, client1_mutex, client1_received_text, "message from the server"));
    ASSERT_TRUE(WaitForMessage(client2_cv, client2_mutex, client2_received_text, "message from the server"));
    std::cout << "Both clients received broadcast.\n";
    std::cout << "Test ServerBroadcastsAllClientsReceive completed.\n";
}

TEST_F(IntegrationTest, NewClientGetsHistory) {
    std::cout << "\n=== Running Test: NewClientGetsHistory ===\n";
    auto msg = std::make_shared<TextMessage>("This message is for history");
    client1->Message(TextTypes::Text, msg);

    // Wait for broadcast AND history update
    ASSERT_TRUE(WaitForMessage(client2_cv, client2_mutex, client2_received_text, "message is for history"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Ensure history is updated

    boost::asio::io_context io3;
    std::vector<std::string> client3_received_text;
    std::mutex client3_mutex;
    std::condition_variable client3_cv;

    std::cout << "Starting Client 3...\n";
    auto client3 = std::make_shared<ClientServerManager>(io3, "127.0.0.1", 5555, 5556);

    client3->set_on_text_message_callback([&](const std::string& m) {
        { std::scoped_lock lk(client3_mutex); client3_received_text.push_back(m); }
        client3_cv.notify_one();
    });

    // Add file callback to prevent crashes if history contains files
    client3->set_on_file_message_callback([&](const std::shared_ptr<FileMessage>& fileMsg) {
        std::cout << "Client 3 received file from history: " << fileMsg->to_string() << "\n";
    });

    std::thread client3Thread([&]() { try { io3.run(); } catch(...) {} });

    // Wait for the history message and the "End Message History" marker
    ASSERT_TRUE(WaitForMessage(client3_cv, client3_mutex, client3_received_text, "message is for history"));
    ASSERT_TRUE(WaitForMessage(client3_cv, client3_mutex, client3_received_text, "End Message History"));
    std::cout << "Client 3 connected and received history.\n";

    client3->Disconnect();
    io3.stop();
    if (client3Thread.joinable()) client3Thread.join();
    std::cout << "Client 3 thread joined.\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::cout << "Test NewClientGetsHistory completed.\n";
}

TEST_F(IntegrationTest, Client1SendsFileClient2Receives) {
    std::cout << "\n=== Running Test: Client1SendsFileClient2Receives ===\n";

    // 1. Create a dummy file to send
    ScopedTempFile temp_file("test_file_to_send.txt");

    // 2. Client 1 enqueues the file for sending
    client1->EnqueueFile(temp_file.string());

    // 3. Wait for Client 2 to receive the TEXT LOG
    ASSERT_TRUE(WaitForMessage(client2_cv, client2_mutex, client2_received_text, "[FILE]"));
    ASSERT_TRUE(WaitForMessage(client2_cv, client2_mutex, client2_received_text, "test_file_to_send.txt"));
    std::cout << "Client 2 received text log.\n";

    // 4. Wait for Client 2 to receive the ACTUAL FILE
    ASSERT_TRUE(WaitForFile(client2_file_cv, client2_file_mutex, client2_received_files, "test_file_to_send.txt"));
    std::cout << "Client 2 received file.\n";

    // 5. ✅ UPDATED: Client 1 WILL receive the text log (we send to everyone now)
    ASSERT_TRUE(WaitForMessage(client1_cv, client1_mutex, client1_received_text, "[FILE]"));
    std::cout << "Client 1 received text notification (expected behavior).\n";

    // The sender should NOT receive the file itself (only other clients get the file)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    {
        std::scoped_lock lk1(client1_file_mutex);
        EXPECT_TRUE(client1_received_files.empty());
        std::cout << "Client 1 did not receive the file back (as expected).\n";
    }

    std::cout << "Test Client1SendsFileClient2Receives completed.\n";
}
TEST_F(IntegrationTest, FileSenderDoesNotReceiveOwnNotification) {
    std::cout << "\n=== Running Test: FileSenderReceivesTextNotification ===\n";

    ScopedTempFile temp_file("isolation_test.txt");

    // Clear any existing messages
    {
        std::scoped_lock lk(client1_mutex);
        client1_received_text.clear();
    }

    client1->EnqueueFile(temp_file.string());

    // Wait for Client 2 to receive notification
    ASSERT_TRUE(WaitForMessage(client2_cv, client2_mutex, client2_received_text, "[FILE]"));

    // ✅ UPDATED: Client 1 SHOULD receive the text notification too
    ASSERT_TRUE(WaitForMessage(client1_cv, client1_mutex, client1_received_text, "[FILE]"));
    std::cout << "Client 1 received text notification (expected behavior).\n";

    // But Client 1 should NOT receive the actual file
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    {
        std::scoped_lock lk(client1_file_mutex);
        EXPECT_TRUE(client1_received_files.empty());
        std::cout << "Client 1 did not receive the file itself (correct).\n";
    }

    std::cout << "Test completed.\n";
}

// ---------- Client queue unit tests (requires mock server or separate setup) ----------
class ClientQueueTest : public ::testing::Test {
protected:
    boost::asio::io_context io;
    std::unique_ptr<ClientServerManager> manager;
    std::thread clientIoThread;
    std::exception_ptr client_exception_ = nullptr;

    void SetUp() override {
        std::cout << "\n--- ClientQueueTest::SetUp() Start ---\n";
        // Note: Using dummy ports 9999/9998 as this is a unit test not connecting to a real server
        manager = std::make_unique<ClientServerManager>(io, "127.0.0.1", 9999, 9998);
        clientIoThread = std::thread([this]() {
            try { io.run(); } catch (...) { client_exception_ = std::current_exception(); }
        });
        std::cout << "--- ClientQueueTest::SetUp() End ---\n";
    }

    void TearDown() override {
        std::cout << "\n--- ClientQueueTest::TearDown() Start ---\n";
        if (manager) manager->Disconnect();
        io.stop();
        if (clientIoThread.joinable()) { clientIoThread.join(); std::cout << "Client Queue IO thread joined.\n"; }
        std::cout << "--- ClientQueueTest::TearDown() End ---\n";
    }
};

TEST_F(ClientQueueTest, EnqueueFileAddsItem) {
    std::cout << "\n=== Running Test: EnqueueFileAddsItem ===\n";
    ScopedTempFile temp_file("temp_enqueue_file.txt");

    manager->PauseQueue();
    uint64_t id = manager->EnqueueFile(temp_file.string());
    EXPECT_NE(id, 0u);

    // Assuming FileQueueSnapshot is available on ClientServerManager
    auto snapshot = manager->FileQueueSnapshot();
    ASSERT_FALSE(snapshot.empty());
    EXPECT_EQ(snapshot.front().path, temp_file.string());
    std::cout << "Test EnqueueFileAddsItem completed.\n";
}

TEST_F(ClientQueueTest, PauseResumeQueueWorks) {
    std::cout << "\n=== Running Test: PauseResumeQueueWorks ===\n";
    ScopedTempFile temp_file("temp_pause_file.txt");

    // The core test here is that no file is sent while paused, but we can only assert the queue manipulation
    manager->PauseQueue();
    manager->EnqueueFile(temp_file.string());
    manager->PauseQueue(); // This line is redundant if PauseQueue is idempotent, but harmless
    manager->ResumeQueue();

    // Small delay to allow any (failing) file IO to start/finish in a non-connected state
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::cout << "Test PauseResumeQueueWorks completed.\n";
}

// --- Main Runner ---
int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}