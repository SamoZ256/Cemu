#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalMemoryManager.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalVoidVertexPipeline.h"
#include "Common/precompiled.h"

/*
MetalVertexBufferCache::~MetalVertexBufferCache()
{
}

MetalRestridedBufferRange MetalVertexBufferCache::RestrideBufferIfNeeded(MTL::Buffer* bufferCache, uint32 bufferIndex, size_t stride, std::vector<MTL::Resource*>& barrierBuffers)
{
    auto vertexBufferRange = m_bufferRanges[bufferIndex];
    auto& restrideInfo = *vertexBufferRange.restrideInfo;

    if (stride % 4 == 0)
    {
        // No restride needed
        return {bufferCache, vertexBufferRange.offset};
    }

    MTL::Buffer* buffer;
    if (restrideInfo.memoryInvalidated || stride != restrideInfo.lastStride)
    {
        size_t newStride = Align(stride, 4);
        size_t newSize = vertexBufferRange.size / stride * newStride;
        restrideInfo.allocation = m_bufferAllocator.GetBufferAllocation(newSize);
        buffer = m_bufferAllocator.GetBuffer(restrideInfo.allocation.bufferIndex);

        //uint8* oldPtr = (uint8*)bufferCache->contents() + vertexBufferRange.offset;
        //uint8* newPtr = (uint8*)buffer->contents() + restrideInfo.allocation.offset;

        //for (size_t elem = 0; elem < vertexBufferRange.size / stride; elem++)
    	//	memcpy(newPtr + elem * newStride, oldPtr + elem * stride, stride);

        if (m_mtlr->GetEncoderType() == MetalEncoderType::Render)
        {
            auto renderCommandEncoder = static_cast<MTL::RenderCommandEncoder*>(m_mtlr->GetCommandEncoder());

            renderCommandEncoder->setRenderPipelineState(m_restrideBufferPipeline->GetRenderPipelineState());
            m_mtlr->GetEncoderState().m_renderPipelineState = m_restrideBufferPipeline->GetRenderPipelineState();

            m_mtlr->SetBuffer(renderCommandEncoder, METAL_SHADER_TYPE_VERTEX, bufferCache, vertexBufferRange.offset, GET_HELPER_BUFFER_BINDING(0));
            m_mtlr->SetBuffer(renderCommandEncoder, METAL_SHADER_TYPE_VERTEX, buffer, restrideInfo.allocation.offset, GET_HELPER_BUFFER_BINDING(1));

            struct
            {
                uint32 oldStride;
                uint32 newStride;
            } strideData = {static_cast<uint32>(stride), static_cast<uint32>(newStride)};
            renderCommandEncoder->setVertexBytes(&strideData, sizeof(strideData), GET_HELPER_BUFFER_BINDING(2));
            m_mtlr->GetEncoderState().m_buffers[METAL_SHADER_TYPE_VERTEX][GET_HELPER_BUFFER_BINDING(2)] = {nullptr};

            renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), vertexBufferRange.size / stride);

            vectorAppendUnique(barrierBuffers, static_cast<MTL::Resource*>(buffer));
        }
        else
        {
            cemu_assert_suspicious();
        }

        restrideInfo.memoryInvalidated = false;
        restrideInfo.lastStride = newStride;

        // Debug
        m_mtlr->GetPerformanceMonitor().m_vertexBufferRestrides++;
    }
    else
    {
        buffer = m_bufferAllocator.GetBuffer(restrideInfo.allocation.bufferIndex);
    }

    return {buffer, restrideInfo.allocation.offset};
}

void MetalVertexBufferCache::MemoryRangeChanged(size_t offset, size_t size)
{
    for (uint32 i = 0; i < LATTE_MAX_VERTEX_BUFFERS; i++)
    {
        auto vertexBufferRange = m_bufferRanges[i];
        if (vertexBufferRange.offset != INVALID_OFFSET)
        {
            if ((offset < vertexBufferRange.offset && (offset + size) < (vertexBufferRange.offset + vertexBufferRange.size)) ||
                (offset > vertexBufferRange.offset && (offset + size) > (vertexBufferRange.offset + vertexBufferRange.size)))
            {
                continue;
            }

            vertexBufferRange.restrideInfo->memoryInvalidated = true;
        }
    }
}
*/

void* MetalMemoryManager::GetTextureUploadBuffer(size_t size)
{
    if (m_textureUploadBuffer.size() < size)
    {
        m_textureUploadBuffer.resize(size);
    }

    return m_textureUploadBuffer.data();
}
