#pragma once

#include <Metal/Metal.hpp>

constexpr size_t INVALID_OFFSET = std::numeric_limits<size_t>::max();

inline size_t Align(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

inline std::string GetColorAttachmentTypeStr(uint32 index)
{
    return "COLOR_ATTACHMENT" + std::to_string(index) + "_TYPE";
}

inline uint64 PackUint32x2(uint32 x, uint32 y)
{
    return (static_cast<uint64>(x) << 32) | y;
}

inline void UnpackUint32x2(uint64 packed, uint32& x, uint32& y)
{
    x = static_cast<uint32>(packed >> 32);
    y = static_cast<uint32>(packed & 0xFFFF);
}
