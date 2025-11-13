#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <random>

#include "MessageTypes/Text/TextMessage.h"
#include "MessageTypes/File/FileMessage.h"
#include "MessageTypes/Utilities/MessageFactory.h"
#include "MessageTypes/Utilities/FileTransferQueue.h"

// =====================================================================
// HELPER: Scoped temp file for testing
// =====================================================================
struct ScopedTempFile {
    std::filesystem::path path;


    explicit ScopedTempFile(const std::string& filenameprefix = "test",
                           const std::string& content = "test data",
                           const std::string& extension = ".txt"){
        // Generate a unique filename using random number generator
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dist;

        std::filesystem::path temp_dir = std::filesystem::temp_directory_path();

        // Try to create a unique file (with collision detection) (if bitcoin works then why can't this work? am i right)
        int attempts = 0;
        const int max_attempts = 100;

        do {
            uint64_t random_id = dist(gen);
            std::string new_filename = filenameprefix + "_" + std::to_string(random_id) + extension;
            path = temp_dir / new_filename;
            attempts++;

            if (attempts >= max_attempts) {
                throw std::runtime_error("Failed to generate unique temporary filename after " +
                                       std::to_string(max_attempts) + " attempts");
            }
        } while (std::filesystem::exists(path));

        // Write content to the file
        std::ofstream outfile(path);
        if (!outfile) {
            throw std::runtime_error("Failed to create temporary file: " + path.string());
        }
        outfile << content;
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

// =====================================================================
// TEST SUITE 1: Message Serialization/Deserialization Logic
// =====================================================================
class MessageSerializationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MessageSerializationTest, TextMessageSerializeDeserialize) {
    // Create original message
    std::string original_text = "Hello, World!";
    const auto msg1 = std::make_shared<TextMessage>(original_text);

    // Serialize
    const std::vector<char> serialized = msg1->serialize();
    ASSERT_FALSE(serialized.empty());

    // Deserialize into new message
    auto msg2 = std::make_shared<TextMessage>();
    ASSERT_NO_THROW(msg2->deserialize(serialized));

    // Verify content matches
    EXPECT_EQ(msg1->to_string(), msg2->to_string());
    EXPECT_NE(msg2->to_string().find(original_text), std::string::npos);
}

TEST_F(MessageSerializationTest, FileMessageSerializeDeserialize) {
    ScopedTempFile temp_file("serialize_test", "file content for testing",".txt");

    // Create from file path
    auto msg1 = std::make_shared<FileMessage>(temp_file.path);
    EXPECT_NE(msg1->to_string().find("serialize_test"), std::string::npos);
    EXPECT_NE(msg1->to_string().find("24 bytes"), std::string::npos);

    // Serialize
    std::vector<char> serialized = msg1->serialize();
    ASSERT_FALSE(serialized.empty());

    // Deserialize into new message
    auto msg2 = std::make_shared<FileMessage>();
    ASSERT_NO_THROW(msg2->deserialize(serialized));

    // Verify metadata matches
    EXPECT_NE(msg1->to_string().find("serialize_test"), std::string::npos);
    EXPECT_NE(msg1->to_string().find("24 bytes"), std::string::npos);
}

TEST_F(MessageSerializationTest, FileMessageFromBytes) {
    std::string filename = "test.bin";
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};

    // Create message from bytes
    auto msg = std::make_shared<FileMessage>(filename, data);
    EXPECT_NE(msg->to_string().find(filename), std::string::npos);
    EXPECT_NE(msg->to_string().find("5 bytes"), std::string::npos);

    // Serialize and deserialize
    auto serialized = msg->serialize();
    auto msg2 = std::make_shared<FileMessage>();
    ASSERT_NO_THROW(msg2->deserialize(serialized));

    EXPECT_EQ(msg->to_string(), msg2->to_string());
}

TEST_F(MessageSerializationTest, EmptyTextMessageHandling) {
    auto msg1 = std::make_shared<TextMessage>("");
    auto serialized = msg1->serialize();

    auto msg2 = std::make_shared<TextMessage>();
    ASSERT_NO_THROW(msg2->deserialize(serialized));
}

TEST_F(MessageSerializationTest, LargeTextMessageHandling) {
    // Create a large text message (10KB)
    std::string large_text(10240, 'A');
    auto msg1 = std::make_shared<TextMessage>(large_text);

    auto serialized = msg1->serialize();
    ASSERT_FALSE(serialized.empty());

    auto msg2 = std::make_shared<TextMessage>();
    ASSERT_NO_THROW(msg2->deserialize(serialized));

    EXPECT_NE(msg2->to_string().find(large_text.substr(0, 100)), std::string::npos);
}

// =====================================================================
// TEST SUITE 2: MessageFactory Logic
// =====================================================================
class MessageFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MessageFactoryTest, CreateTextMessage) {
    auto msg = MessageFactory::create_from_id(TextTypes::Text);

    ASSERT_NE(msg, nullptr);
    ASSERT_NE(dynamic_cast<TextMessage*>(msg.get()), nullptr);
}

TEST_F(MessageFactoryTest, CreateFileMessage) {
    auto msg = MessageFactory::create_from_id(TextTypes::File);

    ASSERT_NE(msg, nullptr);
    ASSERT_NE(dynamic_cast<FileMessage*>(msg.get()), nullptr);
}

TEST_F(MessageFactoryTest, FactoryProducesValidMessages) {
    // Text
    auto text_msg = MessageFactory::create_from_id(TextTypes::Text);
    ASSERT_NO_THROW(text_msg->serialize());

    // File
    auto file_msg = MessageFactory::create_from_id(TextTypes::File);
    ASSERT_NO_THROW(file_msg->serialize());
}

// =====================================================================
// TEST SUITE 3: FileTransferQueue Logic (WITHOUT network I/O)
// =====================================================================
class MockSocketGetter {
public:
    std::shared_ptr<boost::asio::ip::tcp::socket> operator()() const
    {
        // Return nullptr to simulate "not connected" - tests queue logic only
        return nullptr;
    }
};

class FileTransferQueueTest : public ::testing::Test {
protected:
    std::unique_ptr<FileTransferQueue> queue;

    void SetUp() override {
        MockSocketGetter getter;
        queue = std::make_unique<FileTransferQueue>(getter);
    }

    void TearDown() override {
        if (queue) queue->stop();
        queue.reset();
    }
};

TEST_F(FileTransferQueueTest, EnqueueFileAddsToQueue) {
    ScopedTempFile temp_file("queue_test");

    queue->pause(); // Prevent actual sending
    uint64_t id = queue->enqueue(temp_file.path);

    EXPECT_NE(id, 0u);

    auto snapshot = queue->list_snapshot();
    ASSERT_EQ(snapshot.size(), 1u);
    EXPECT_EQ(snapshot[0].id, id);
    EXPECT_EQ(snapshot[0].path, temp_file.path);
    EXPECT_EQ(snapshot[0].state, FileTransferQueue::State::Queued);
}

TEST_F(FileTransferQueueTest, EnqueueMultipleFiles) {
    ScopedTempFile file1("queue_test1");
    ScopedTempFile file2("queue_test2");
    ScopedTempFile file3("queue_test3");

    queue->pause();

    uint64_t id1 = queue->enqueue(file1.path);
    uint64_t id2 = queue->enqueue(file2.path);
    uint64_t id3 = queue->enqueue(file3.path);

    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);

    auto snapshot = queue->list_snapshot();
    EXPECT_EQ(snapshot.size(), 3u);
}

TEST_F(FileTransferQueueTest, RemoveFileFromQueue) {
    ScopedTempFile temp_file("remove_test");

    queue->pause();
    uint64_t id = queue->enqueue(temp_file.path);

    auto snapshot1 = queue->list_snapshot();
    EXPECT_EQ(snapshot1.size(), 1u);

    bool removed = queue->remove(id);
    EXPECT_TRUE(removed);

    auto snapshot2 = queue->list_snapshot();
    EXPECT_EQ(snapshot2.size(), 0u);
}

TEST_F(FileTransferQueueTest, RemoveNonExistentFile) {
    bool removed = queue->remove(9999);
    EXPECT_FALSE(removed);
}

TEST_F(FileTransferQueueTest, CancelFile) {
    ScopedTempFile temp_file("cancel_test");

    queue->pause();
    uint64_t id = queue->enqueue(temp_file.path);

    bool canceled = queue->cancel(id);
    EXPECT_TRUE(canceled);

    auto snapshot = queue->list_snapshot();
    ASSERT_EQ(snapshot.size(), 1u);
    EXPECT_EQ(snapshot[0].state, FileTransferQueue::State::Canceled);
    EXPECT_NE(snapshot[0].last_error.find("canceled"), std::string::npos);
}

TEST_F(FileTransferQueueTest, CancelAllFiles) {
    ScopedTempFile file1("cancel_all1");
    ScopedTempFile file2("cancel_all2");

    queue->pause();
    queue->enqueue(file1.path);
    queue->enqueue(file2.path);

    queue->cancel_all();

    auto snapshot = queue->list_snapshot();
    EXPECT_EQ(snapshot.size(), 2u);

    for (const auto& item : snapshot) {
        EXPECT_EQ(item.state, FileTransferQueue::State::Canceled);
    }
}

TEST_F(FileTransferQueueTest, PauseAndResumeQueue) {
    ScopedTempFile temp_file("pause_test");

    queue->pause();
    uint64_t id = queue->enqueue(temp_file.path);

    // While paused, item should stay Queued
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto snapshot1 = queue->list_snapshot();
    ASSERT_EQ(snapshot1.size(), 1u);
    EXPECT_EQ(snapshot1[0].state, FileTransferQueue::State::Queued);

    queue->resume();

    // After resume (and no socket), it will try to send and fail
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto snapshot2 = queue->list_snapshot();
    ASSERT_EQ(snapshot2.size(), 1u);
    // Should have attempted send and failed (no socket)
    EXPECT_TRUE(snapshot2[0].state == FileTransferQueue::State::Failed ||
                snapshot2[0].state == FileTransferQueue::State::Sending);
}

TEST_F(FileTransferQueueTest, EnqueueFileMessage) {
    auto file_msg = std::make_shared<FileMessage>("test.txt", std::vector<uint8_t>{1, 2, 3, 4, 5});

    queue->pause();
    uint64_t id = queue->enqueue(file_msg);

    EXPECT_NE(id, 0u);

    auto snapshot = queue->list_snapshot();
    ASSERT_EQ(snapshot.size(), 1u);
    EXPECT_EQ(snapshot[0].id, id);
    EXPECT_NE(snapshot[0].message, nullptr);
}

TEST_F(FileTransferQueueTest, RetryFailedFile) {
    ScopedTempFile temp_file("retry_test");

    queue->pause();
    uint64_t id = queue->enqueue(temp_file.path);

    // Simulate failure by resuming (will fail due to no socket)
    queue->resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto snapshot1 = queue->list_snapshot();
    ASSERT_EQ(snapshot1.size(), 1u);
    EXPECT_TRUE(snapshot1[0].state == FileTransferQueue::State::Failed);

    // Retry
    queue->pause(); // Pause to prevent immediate re-attempt
    bool retried = queue->retry(id);
    EXPECT_TRUE(retried);

    auto snapshot2 = queue->list_snapshot();
    ASSERT_EQ(snapshot2.size(), 1u);
    EXPECT_EQ(snapshot2[0].state, FileTransferQueue::State::Queued);
    EXPECT_GT(snapshot2[0].retries, 0u);
}

// =====================================================================
// TEST SUITE 4: File I/O Logic
// =====================================================================
class FileIOTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(FileIOTest, CreateFileMessageFromPath) {
    ScopedTempFile temp_file("io_test", "test content");

    ASSERT_NO_THROW({
        FileMessage msg(temp_file.path);
        EXPECT_NE(msg.to_string().find("io_test"), std::string::npos);
        EXPECT_NE(msg.to_string().find("12 bytes"), std::string::npos);
    });
}

TEST_F(FileIOTest, CreateFileMessageFromNonExistentFile) {
    std::filesystem::path fake_path = "/tmp/does_not_exist_12345.txt";

    EXPECT_THROW({
        FileMessage msg(fake_path);
    }, std::runtime_error);
}

TEST_F(FileIOTest, CreateFileMessageFromDirectory) {
    auto temp_dir = std::filesystem::temp_directory_path();

    EXPECT_THROW({
        FileMessage msg(temp_dir);
    }, std::runtime_error);
}

TEST_F(FileIOTest, FileMessageWithEmptyBytes) {
    std::string filename = "empty.txt";
    std::vector<uint8_t> empty_data;

    EXPECT_THROW({
        FileMessage msg(filename, empty_data);
    }, std::runtime_error);
}

TEST_F(FileIOTest, LargeFileHandling) {
    // Create a 1MB file
    auto temp_path = std::filesystem::temp_directory_path() / "large_file.bin";
    {
        std::ofstream out(temp_path, std::ios::binary);
        std::vector<char> data(1024 * 1024, 'X'); // 1MB
        out.write(data.data(), data.size());
    }

    ASSERT_NO_THROW({
        FileMessage msg(temp_path);
        EXPECT_NE(msg.to_string().find("1048576 bytes"), std::string::npos);

        auto serialized = msg.serialize();
        EXPECT_GT(serialized.size(), 1024u * 1024u);
    });

    std::error_code ec;
    std::filesystem::remove(temp_path, ec);
}

// =====================================================================
// TEST SUITE 5: Message Header/Payload Logic
// =====================================================================
class MessageProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MessageProtocolTest, TextMessageHasCorrectHeader) {
    auto msg = std::make_shared<TextMessage>("test");
    auto serialized = msg->serialize();

    // First 4 bytes should be message ID (network byte order)
    ASSERT_GE(serialized.size(), 4u);

    uint32_t id;
    std::memcpy(&id, serialized.data(), sizeof(uint32_t));
    id = ntohl(id);

    EXPECT_EQ(id, static_cast<uint32_t>(TextTypes::Text));
}

TEST_F(MessageProtocolTest, FileMessageHasCorrectHeader) {
    ScopedTempFile temp_file("protocol_test");
    FileMessage msg(temp_file.path);

    auto serialized = msg.serialize();
    ASSERT_GE(serialized.size(), 4u);

    uint32_t id;
    std::memcpy(&id, serialized.data(), sizeof(uint32_t));
    id = ntohl(id);

    EXPECT_EQ(id, static_cast<uint32_t>(TextTypes::File));
}

TEST_F(MessageProtocolTest, RoundTripPreservesData) {
    std::string original = "Round trip test message with special chars: !@#$%^&*()";
    auto msg1 = std::make_shared<TextMessage>(original);

    auto serialized = msg1->serialize();
    auto msg2 = std::make_shared<TextMessage>();
    msg2->deserialize(serialized);

    EXPECT_NE(msg2->to_string().find(original), std::string::npos);
}

TEST_F(MessageProtocolTest, CorruptedDataThrowsException) {
    std::vector<char> corrupt_data = {0x01, 0x02, 0x03}; // Too short

    auto msg = std::make_shared<TextMessage>();
    EXPECT_THROW(msg->deserialize(corrupt_data), std::runtime_error);
}

// =====================================================================
// Main Runner
// =====================================================================
int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}