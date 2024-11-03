#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalBufferAllocator.h"

#include "GameProfile/GameProfile.h"

class MetalMemoryManager
{
public:
    MetalMemoryManager(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer}, m_bufferAllocator(metalRenderer, m_mtlr->GetOptimalBufferStorageMode()), m_framePersistentBufferAllocator(metalRenderer, MTL::ResourceStorageModePrivate), m_tempBufferAllocator(metalRenderer) {}
    ~MetalMemoryManager();

    MetalDefaultBufferAllocator& GetBufferAllocator()
    {
        return m_bufferAllocator;
    }

    MetalDefaultBufferAllocator& GetFramePersistentBufferAllocator()
    {
        return m_framePersistentBufferAllocator;
    }

    MetalTemporaryBufferAllocator& GetTemporaryBufferAllocator()
    {
        return m_tempBufferAllocator;
    }

    MTL::Buffer* GetBufferCache()
    {
        return m_bufferCache;
    }

    void* GetTextureUploadBuffer(size_t size);

    // Buffer cache
    void InitBufferCache(size_t size);
    void UploadToBufferCache(const void* data, size_t offset, size_t size);
    void CopyBufferCache(size_t srcOffset, size_t dstOffset, size_t size);

    // Getters
    bool UseHostMemoryForCache() const
    {
        return (m_bufferCacheType == BufferCacheType::Host);
    }

    MPTR GetImportedMemBaseAddress() const
    {
        return m_importedMemBaseAddress;
    }

    size_t GetHostAllocationSize() const
    {
        return m_hostAllocationSize;
    }

private:
    class MetalRenderer* m_mtlr;

    std::vector<uint8> m_textureUploadBuffer;

    MetalDefaultBufferAllocator m_bufferAllocator;
    MetalDefaultBufferAllocator m_framePersistentBufferAllocator;
    MetalTemporaryBufferAllocator m_tempBufferAllocator;

    MTL::Buffer* m_bufferCache = nullptr;
    BufferCacheType m_bufferCacheType;
    MPTR m_importedMemBaseAddress;
    size_t m_hostAllocationSize = 0;
};
