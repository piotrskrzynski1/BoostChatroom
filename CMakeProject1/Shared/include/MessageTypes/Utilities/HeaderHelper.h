#pragma once
#include <cstdint>
#include <vector>
#include <cstring>
#include <arpa/inet.h>

namespace Utils {

inline uint64_t htonll(uint64_t val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    return (((uint64_t)htonl(val & 0xFFFFFFFFULL)) << 32) | htonl(val >> 32);
#else
    return val;
#endif
}

inline uint64_t ntohll(uint64_t val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    return (((uint64_t)ntohl(val & 0xFFFFFFFFULL)) << 32) | ntohl(val >> 32);
#else
    return val;
#endif
}

struct HeaderHelper {
    static void append_u32(std::vector<char>& buffer, uint32_t value)
    {
        uint32_t net = htonl(value);
        const char* ptr = reinterpret_cast<const char*>(&net);
        buffer.insert(buffer.end(), ptr, ptr + sizeof(net));
    }

    static void append_u64(std::vector<char>& buffer, uint64_t value)
    {
        uint64_t net = htonll(value);
        const char* ptr = reinterpret_cast<const char*>(&net);
        buffer.insert(buffer.end(), ptr, ptr + sizeof(net));
    }

    static bool read_u32(const std::vector<char>& buffer, size_t offset, uint32_t& value)
    {
        if (offset + sizeof(uint32_t) > buffer.size()) return false;
        uint32_t net;
        std::memcpy(&net, buffer.data() + offset, sizeof(uint32_t));
        value = ntohl(net);
        return true;
    }

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
