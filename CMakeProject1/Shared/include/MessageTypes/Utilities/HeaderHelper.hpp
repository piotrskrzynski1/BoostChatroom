#pragma once
#include <vector>
#include <cstring>
#include <arpa/inet.h>

//HeaderHelper contains functions useful when preparing and parsing data sent over the network

namespace Utils
{
    inline uint64_t htonll(const uint64_t val)
    {
#if __BYTE_ORDER == __LITTLE_ENDIAN
        return (static_cast<uint64_t>(htonl(val & 0xFFFFFFFFULL)) << 32) | htonl(val >> 32);
#else
        return val;
#endif
    }

    inline uint64_t ntohll(uint64_t val)
    {
#if __BYTE_ORDER == __LITTLE_ENDIAN
        return (static_cast<uint64_t>(ntohl(val & 0xFFFFFFFFULL)) << 32) | ntohl(val >> 32);
#else
        return val;
#endif
    }

    struct HeaderHelper
    {
        /**
     * @brief Appends a 32-bit unsigned integer to the buffer using network byte order.
     * @param buffer Destination byte buffer to append to.
     * @param value  32-bit integer to append.
     */
        static void append_u32(std::vector<char>& buffer, uint32_t value)
        {
            uint32_t net = htonl(value);
            const char* ptr = reinterpret_cast<const char*>(&net);
            buffer.insert(buffer.end(), ptr, ptr + sizeof(net));
        }

        /**
         * @brief Appends a 64-bit unsigned integer to the buffer using network byte order.
         * @param buffer Destination byte buffer to append to.
         * @param value  64-bit integer to append.
         */
        static void append_u64(std::vector<char>& buffer, uint64_t value)
        {
            uint64_t net = htonll(value);
            const char* ptr = reinterpret_cast<const char*>(&net);
            buffer.insert(buffer.end(), ptr, ptr + sizeof(net));
        }

        /**
         * @brief Reads a 32-bit unsigned integer from the buffer starting at a given offset.
         *        Converts from network byte order to host byte order.
         *
         * @param buffer Source buffer to read from.
         * @param offset Starting index in the buffer.
         * @param value  Output: extracted 32-bit integer.
         * @return true if the read succeeded, false if the buffer was too small.
         */
        static bool read_u32(const std::vector<char>& buffer, size_t offset, uint32_t& value)
        {
            if (offset + sizeof(uint32_t) > buffer.size()) return false;
            uint32_t net;
            std::memcpy(&net, buffer.data() + offset, sizeof(uint32_t));
            value = ntohl(net);
            return true;
        }

        /**
         * @brief Reads a 64-bit unsigned integer from the buffer starting at a given offset.
         *        Converts from network byte order to host byte order.
         *
         * @param buffer Source buffer to read from.
         * @param offset Starting index in the buffer.
         * @param value  Output: extracted 64-bit integer.
         * @return true if the read succeeded, false if the buffer was too small.
         */
        static bool read_u64(const std::vector<char>& buffer, size_t offset, uint64_t& value)
        {
            if (offset + sizeof(uint64_t) > buffer.size()) return false;
            uint64_t net;
            std::memcpy(&net, buffer.data() + offset, sizeof(uint64_t));
            value = ntohll(net);
            return true;
        }
    };
} // namespace Utils
