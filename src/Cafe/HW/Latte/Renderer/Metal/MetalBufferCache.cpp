#include "Cafe/HW/Latte/Renderer/Metal/MetalBufferCache.h"

MetalBufferCache::~MetalBufferCache()
{
    if (m_bufferCache)
        m_bufferCache->release();
}

void MetalBufferCache::InitBufferCache(size_t size)
{
    cemu_assert_debug(!m_bufferCache);

    m_bufferCache = m_mtlr->GetDevice()->newBuffer(size, (m_mtlr->IsAppleGPU() ? MTL::ResourceStorageModeShared : MTL::ResourceStorageModeManaged));
#ifdef CEMU_DEBUG_ASSERT
    m_bufferCache->setLabel(GetLabel("Buffer cache", m_bufferCache));
#endif
}

void MetalBufferCache::UploadToBufferCache(const void* data, size_t offset, size_t size)
{
    cemu_assert_debug(m_bufferCache);
    cemu_assert_debug((offset + size) <= m_bufferCache->length());

    bool isFree = MarkRegionAsUsed(offset, size);
    if (isFree)
    {
        memcpy((uint8*)m_bufferCache->contents() + offset, data, size);
    }
    else
    {
        auto allocation = m_tempBufferAllocator.GetBufferAllocation(size);
        auto buffer = m_tempBufferAllocator.GetBufferOutsideOfCommandBuffer(allocation.bufferIndex);
        memcpy((uint8*)buffer->contents() + allocation.offset, data, size);

        // Lock the buffer to make sure it's not deallocated before the copy is done
        m_tempBufferAllocator.LockBuffer(allocation.bufferIndex);

        m_mtlr->CopyBufferToBuffer(buffer, allocation.offset, m_bufferCache, offset, size, ALL_MTL_RENDER_STAGES, ALL_MTL_RENDER_STAGES);

        // Make sure the buffer has the right command buffer
        m_tempBufferAllocator.GetBuffer(allocation.bufferIndex); // TODO: make a helper function for this

        // We can now safely unlock the buffer
        m_tempBufferAllocator.UnlockBuffer(allocation.bufferIndex);
    }

    // Notify vertex buffer cache about the change
    //m_vertexBufferCache.MemoryRangeChanged(offset, size);
}

void MetalBufferCache::CopyBufferCache(size_t srcOffset, size_t dstOffset, size_t size)
{
    cemu_assert_debug(m_bufferCache);

    bool isFree = MarkRegionAsUsed(srcOffset, size) && MarkRegionAsUsed(dstOffset, size);
    if (isFree)
    {
        memcpy((uint8*)m_bufferCache->contents() + dstOffset, (uint8*)m_bufferCache->contents() + srcOffset, size);
    }
    else
    {
        m_mtlr->CopyBufferToBuffer(m_bufferCache, srcOffset, m_bufferCache, dstOffset, size, ALL_MTL_RENDER_STAGES, ALL_MTL_RENDER_STAGES);
    }
}

void MetalBufferCache::CheckForFinishedCommandBuffers()
{
    if (!m_mtlr->IsAppleGPU())
        return;

    for (auto it = m_usedRegions.begin(); it != m_usedRegions.end();)
    {
        MTL::CommandBuffer* commandBuffer = it->first;
        if (CommandBufferCompleted(commandBuffer))
        {
            // Add the regions to the free list
            //for (auto& region : it->second)
            //    FreeRegion(region);

            commandBuffer->release();

            // Remove the command buffer from the used list
            it = m_usedRegions.erase(it);
        }
        else
        {
            it++;
        }
    }
}

bool MetalBufferCache::MarkRegionAsUsed(uint32 offset, uint32 size)
{
    // Non-Apple GPUs don't need to track used regions, since all copying will be done by the GPU
    if (!m_mtlr->IsAppleGPU())
        return false;

    MetalRegion region = MetalRegion::Create(offset, size);

    bool found;
    MetalRegion* foundRegion = nullptr;

    auto commandBuffer = m_mtlr->GetCurrentCommandBuffer();

    // Check if the region is already in the list of used regions
    for (auto& regions : m_usedRegions)
    {
        for (auto& usedRegion : regions.second)
        {
            if (usedRegion.CollidesWith(region))
            {
                found = true;
                if (regions.first == commandBuffer)
                    foundRegion = &usedRegion;
                break;
            }
        }
    }

    if (foundRegion)
    {
        // Grow the region
        foundRegion->m_begin = std::min(foundRegion->m_begin, region.m_begin);
        foundRegion->m_end = std::max(foundRegion->m_end, region.m_end);
    }
    else
    {
        // Add to the list of used regions
        auto& regions = m_usedRegions[commandBuffer];
        if (regions.empty())
            commandBuffer->retain();
        regions.push_back(region);
    }

    return !found;

    /*
    // Remove from free regions
    bool found = false;
    uint32 freeRegionsSize = m_freeRegions.size();
    for (uint32 i = 0; i < freeRegionsSize; i++)
    {
        auto& freeRegion = m_freeRegions[i];
        if (freeRegion == region)
        {
            m_freeRegions.erase(m_freeRegions.begin() + i);
            found = true;
            break;
        }

        if (freeRegion.CollidesInTheCenter(region))
        {
            // Split the region in 2
            MetalRegion rightRegion(region.m_end, freeRegion.m_end);
            m_freeRegions.push_back(rightRegion);

            freeRegion.m_end = region.m_begin;
            found = true;
            break;
        }

        bool collidesOnTheLeft = freeRegion.CollidesOnTheLeft(region);
        bool collidesOnTheRight = freeRegion.CollidesOnTheRight(region);

        if (collidesOnTheLeft && collidesOnTheRight)
        {
            // Erase the region
            m_freeRegions.erase(m_freeRegions.begin() + i);
            i--;
        }
        else if (collidesOnTheLeft)
        {
            // Shrink from the left
            freeRegion.m_begin = region.m_end;
        }
        else if (collidesOnTheRight)
        {
            // Shrink from the right
            freeRegion.m_end = region.m_begin;
        }
    }

    return found;
    */
}

/*
void MetalBufferCache::FreeRegion(MetalRegion region)
{
    bool left;

    MetalRegion* foundRegion = nullptr;

    // First, try to join with a region
    for (auto& freeRegion : m_freeRegions)
    {
        if (freeRegion.m_begin == region.m_end)
        {
            freeRegion.m_begin = region.m_begin;
            foundRegion = &freeRegion;
            left = true;
            break;
        }

        if (freeRegion.m_end == region.m_begin)
        {
            freeRegion.m_end = region.m_end;
            foundRegion = &freeRegion;
            left = false;
            break;
        }
    }

    if (foundRegion)
    {
        // Try to join with the next region
        for (auto it = m_freeRegions.begin(); it != m_freeRegions.end(); ++it)
        {
            auto freeRegion = *it;
            if (left && freeRegion.m_end == foundRegion->m_begin)
            {
                foundRegion->m_begin = freeRegion.m_begin;
                m_freeRegions.erase(it);
                return;
            }

            if (!left && freeRegion.m_begin == foundRegion->m_end)
            {
                foundRegion->m_end = freeRegion.m_end;
                m_freeRegions.erase(it);
                return;
            }
        }
    }
    else
    {
        // Add the region to the free list
        m_freeRegions.push_back(region);
    }
}
*/
