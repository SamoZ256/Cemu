#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalBufferAllocator.h"

struct MetalRegion
{
    uint32 m_begin;
    uint32 m_end;

    MetalRegion(uint32 begin, uint32 end) : m_begin{begin}, m_end{end} {}

    static MetalRegion Create(uint32 offset, uint32 size)
    {
        return MetalRegion(offset, offset + size);
    }

    bool operator==(const MetalRegion& other) const
    {
        return m_begin == other.m_begin && m_end == other.m_end;
    }

    bool CollidesInTheCenter(const MetalRegion& other) const
    {
        return (other.m_begin >= m_begin && other.m_end <= m_end);
    }

    bool CollidesOnTheRight(const MetalRegion& other) const
    {
        return (m_begin >= other.m_begin && m_begin < other.m_end);
    }

    bool CollidesOnTheLeft(const MetalRegion& other) const
    {
        return (m_end > other.m_begin && m_end <= other.m_end);
    }

    bool CollidesWith(const MetalRegion& other) const
    {
        return CollidesInTheCenter(other) || CollidesOnTheRight(other) || CollidesOnTheLeft(other);
    }
};

class MetalBufferCache
{
public:
    MetalBufferCache(class MetalRenderer* metalRenderer, MetalTemporaryBufferAllocator& tempBufferAllocator) : m_mtlr{metalRenderer}, m_tempBufferAllocator{tempBufferAllocator} {}
    ~MetalBufferCache();

    void CheckForFinishedCommandBuffers();

    // Buffer cache management
    void InitBufferCache(size_t size);
    void UploadToBufferCache(const void* data, size_t offset, size_t size);
    void CopyBufferCache(size_t srcOffset, size_t dstOffset, size_t size);

    // Returns whether the region was free before
    bool MarkRegionAsUsed(uint32 offset, uint32 size);

    // Getters
    MTL::Buffer* GetBufferCache() const { return m_bufferCache; }

private:
    class MetalRenderer* m_mtlr;
    MetalTemporaryBufferAllocator& m_tempBufferAllocator;

    MTL::Buffer* m_bufferCache{nullptr};

    std::map<MTL::CommandBuffer*, std::vector<MetalRegion>> m_usedRegions;
    //std::vector<MetalRegion> m_freeRegions;

    //void FreeRegion(MetalRegion region);
};
