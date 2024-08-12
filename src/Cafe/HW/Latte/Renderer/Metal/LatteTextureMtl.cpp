#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"
#include "HW/Latte/Renderer/Metal/MetalCommon.h"
#include "Metal/MTLRenderPass.hpp"

LatteTextureMtl::LatteTextureMtl(class MetalRenderer* mtlRenderer, Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels, uint32 swizzle,
	Latte::E_HWTILEMODE tileMode, bool isDepth)
	: LatteTexture(dim, physAddress, physMipAddress, format, width, height, depth, pitch, mipLevels, swizzle, tileMode, isDepth), m_mtlr(mtlRenderer), m_format(format), m_isDepth(isDepth)
{
    MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
    desc->setStorageMode(MTL::StorageModeShared); // TODO: use private?

	sint32 effectiveBaseWidth = width;
	sint32 effectiveBaseHeight = height;
	sint32 effectiveBaseDepth = depth;
	if (overwriteInfo.hasResolutionOverwrite)
	{
		effectiveBaseWidth = overwriteInfo.width;
		effectiveBaseHeight = overwriteInfo.height;
		effectiveBaseDepth = overwriteInfo.depth;
	}
	effectiveBaseDepth = std::max(1, effectiveBaseDepth);

	desc->setWidth(effectiveBaseWidth);
	desc->setHeight(effectiveBaseHeight);
	desc->setMipmapLevelCount(mipLevels);

	MTL::TextureType textureType;
	switch (dim)
    {
    case Latte::E_DIM::DIM_1D:
        textureType = MTL::TextureType1D;
        break;
    case Latte::E_DIM::DIM_2D:
    case Latte::E_DIM::DIM_2D_MSAA:
        textureType = MTL::TextureType2D;
        break;
    case Latte::E_DIM::DIM_2D_ARRAY:
        textureType = MTL::TextureType2DArray;
        break;
    case Latte::E_DIM::DIM_3D:
        textureType = MTL::TextureType3D;
        break;
    case Latte::E_DIM::DIM_CUBEMAP:
        if (effectiveBaseDepth % 6 != 0)
            debug_printf("cubemaps must have an array length multiple of 6, length: %u\n", effectiveBaseDepth);

        if (effectiveBaseDepth <= 6)
            textureType = MTL::TextureTypeCube;
        else
            textureType = MTL::TextureTypeCubeArray;
        break;
    default:
        cemu_assert_unimplemented();
        textureType = MTL::TextureType2D;
        break;
    }
    desc->setTextureType(textureType);

	if (textureType == MTL::TextureType3D)
	{
		desc->setDepth(effectiveBaseDepth);
	}
	else if (textureType == MTL::TextureTypeCube)
	{
	    // Do notjing
	}
	else if (textureType == MTL::TextureTypeCubeArray)
	{
		desc->setArrayLength(effectiveBaseDepth / 6);
	}
	else
	{
		desc->setArrayLength(effectiveBaseDepth);
	}

	auto formatInfo = GetMtlPixelFormatInfo(format, isDepth);
	desc->setPixelFormat(formatInfo.pixelFormat);

	// HACK: even though the textures are never written to from a shader, we still need to use `ShaderWrite` usage to prevent pink lines over the screen
	MTL::TextureUsage usage = MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite;
	// TODO: add more conditions
	if (!Latte::IsCompressedFormat(format))
	{
		usage |= MTL::TextureUsageRenderTarget;
	}
	desc->setUsage(usage);

	m_texture = mtlRenderer->GetDevice()->newTexture(desc);

	desc->release();
}

LatteTextureMtl::~LatteTextureMtl()
{
	m_texture->release();
}

void LatteTextureMtl::Flush(uint32 firstMip, uint32 firstSlice)
{
    for (uint32 mip = firstMip; mip < mipLevels; mip++)
    {
        // TODO: depth?
        for (uint32 slice = firstSlice; slice < depth; slice++)
        {
            FlushPart(mip, slice);
        }
    }
}

void LatteTextureMtl::FlushRegion(uint32 firstMip, uint32 mipCount, uint32 firstSlice, uint32 sliceCount)
{
    for (uint32 mip = firstMip; mip < (firstMip + mipCount); mip++)
    {
        for (uint32 slice = firstSlice; slice < (firstSlice + sliceCount); slice++)
        {
            FlushPart(mip, slice);
        }
    }
}

bool LatteTextureMtl::FlushRegionAtRenderPassBegin(uint32 mip, uint32 slice, MTL::RenderPassDescriptor* renderPassDescriptor, uint32 colorAttachmentIndex, bool hasStencil)
{
    auto it = m_clearInfos.find(PackUint32x2(mip, slice));
    if (it == m_clearInfos.end())
    {
        MTL::RenderPassAttachmentDescriptor* attachment;
        if (m_isDepth)
        {
            attachment = renderPassDescriptor->depthAttachment();
            if (hasStencil)
            {
                auto stencilAttachment = renderPassDescriptor->stencilAttachment();
                stencilAttachment->setTexture(m_texture);
                stencilAttachment->setLoadAction(MTL::LoadActionLoad);
                stencilAttachment->setStoreAction(MTL::StoreActionStore);
                stencilAttachment->setLevel(mip);
                stencilAttachment->setSlice(slice);
            }
        }
        else
        {
            attachment = renderPassDescriptor->colorAttachments()->object(colorAttachmentIndex);
        }

        attachment->setTexture(m_texture);
        attachment->setLoadAction(MTL::LoadActionLoad);
        attachment->setStoreAction(MTL::StoreActionStore);
        attachment->setLevel(mip);
        attachment->setSlice(slice);

        return false;
    }

    SetupAttachment(it->second, renderPassDescriptor, colorAttachmentIndex, mip, slice);

    m_clearInfos.erase(it);

    return true;
}

LatteTextureView* LatteTextureMtl::CreateView(Latte::E_DIM dim, Latte::E_GX2SURFFMT format, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount)
{
	cemu_assert_debug(mipCount > 0);
	cemu_assert_debug(sliceCount > 0);
	cemu_assert_debug((firstMip + mipCount) <= this->mipLevels);
	cemu_assert_debug((firstSlice + sliceCount) <= this->depth);

	return new LatteTextureViewMtl(m_mtlr, this, dim, format, firstMip, mipCount, firstSlice, sliceCount);
}

void LatteTextureMtl::AllocateOnHost()
{
	cemuLog_log(LogType::MetalLogging, "not implemented");
}

void LatteTextureMtl::SetupAttachment(const MetalTextureClearInfo& clearInfo, MTL::RenderPassDescriptor* renderPassDescriptor, uint32 colorAttachmentIndex, uint32 mip, uint32 slice)
{
    MTL::RenderPassAttachmentDescriptor* attachment;
    if (m_isDepth)
    {
        if (clearInfo.depthStencil.clearDepth)
        {
            auto depthAttachment = renderPassDescriptor->depthAttachment();
            depthAttachment->setClearDepth(clearInfo.depthStencil.depthValue);
            attachment = depthAttachment;
        }
        if (clearInfo.depthStencil.clearStencil)
        {
            auto stencilAttachment = renderPassDescriptor->stencilAttachment();
            stencilAttachment->setClearStencil(clearInfo.depthStencil.stencilValue);
            attachment = stencilAttachment;
        }
    }
    else
    {
        auto colorAttachment = renderPassDescriptor->colorAttachments()->object(colorAttachmentIndex);
        colorAttachment->setClearColor(MTL::ClearColor{clearInfo.color.r, clearInfo.color.g, clearInfo.color.b, clearInfo.color.a});
        attachment = colorAttachment;
    }

    attachment->setTexture(m_texture);
    attachment->setLoadAction(MTL::LoadActionClear);
    attachment->setStoreAction(MTL::StoreActionStore);
    attachment->setLevel(mip);
    attachment->setSlice(slice);
}

void LatteTextureMtl::FlushPart(uint32 mip, uint32 slice)
{
    auto it = m_clearInfos.find(PackUint32x2(mip, slice));
    if (it == m_clearInfos.end())
        return;

    auto renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
    SetupAttachment(it->second, renderPassDescriptor, 0, mip, slice);

    MTL::Texture* colorRenderTargets[8] = {nullptr};
    MTL::Texture* depthStencilRenderTarget = nullptr;
    if (m_isDepth)
        depthStencilRenderTarget = m_texture;
    else
        colorRenderTargets[0] = m_texture;
    m_mtlr->GetRenderCommandEncoder(renderPassDescriptor, colorRenderTargets, depthStencilRenderTarget, true);
    renderPassDescriptor->release();

    m_clearInfos.erase(it);
}
