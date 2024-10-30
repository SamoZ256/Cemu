#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"
#include "Common/precompiled.h"

LatteTextureMtl::LatteTextureMtl(class MetalRenderer* metalRenderer, Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels, uint32 swizzle,
	Latte::E_HWTILEMODE tileMode, bool isDepth)
	: LatteTexture(dim, physAddress, physMipAddress, format, width, height, depth, pitch, mipLevels, swizzle, tileMode, isDepth), m_mtlr{metalRenderer}, m_format{format}, m_isDepth{isDepth}
{
    m_pixelFormat = GetMtlPixelFormat(format, isDepth);
    m_texture = CreateTexture();
}

LatteTextureMtl::~LatteTextureMtl()
{
	m_texture->release();
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

bool LatteTextureMtl::RequirePixelFormatViewUsage()
{
    if (m_hasPixelFormatViewUsage)
        return false;

    m_hasPixelFormatViewUsage = true;

    // Create a new texture with pixel format usage flag
    m_texture->release();
    m_texture = CreateTexture();

    // TODO: copy the contents from old texture to new texture

    return true;
}

MTL::Texture* LatteTextureMtl::CreateTexture()
{
    MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
    desc->setStorageMode(MTL::StorageModePrivate);
    desc->setCpuCacheMode(MTL::CPUCacheModeWriteCombined);

	sint32 effectiveBaseWidth = width;
	sint32 effectiveBaseHeight = height;
	sint32 effectiveBaseDepth = depth;
	if (overwriteInfo.hasResolutionOverwrite)
	{
		effectiveBaseWidth = overwriteInfo.width;
		effectiveBaseHeight = overwriteInfo.height;
		effectiveBaseDepth = overwriteInfo.depth;
	}
	effectiveBaseWidth = std::max(1, effectiveBaseWidth);
	effectiveBaseHeight = std::max(1, effectiveBaseHeight);
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
        cemu_assert_debug(effectiveBaseDepth % 6 == 0 && "cubemaps must have an array length multiple of 6");

        textureType = MTL::TextureTypeCubeArray;
        break;
    default:
        cemu_assert_unimplemented();
        textureType = MTL::TextureType2D;
        break;
    }
    desc->setTextureType(textureType);

	if (textureType == MTL::TextureType3D)
		desc->setDepth(effectiveBaseDepth);
	else if (textureType == MTL::TextureTypeCubeArray)
		desc->setArrayLength(effectiveBaseDepth / 6);
	else
		desc->setArrayLength(effectiveBaseDepth);

	desc->setPixelFormat(m_pixelFormat);

	MTL::TextureUsage usage = MTL::TextureUsageShaderRead;
	if (m_hasPixelFormatViewUsage)
	    usage |= MTL::TextureUsagePixelFormatView;
	if (!Latte::IsCompressedFormat(format))
		usage |= MTL::TextureUsageRenderTarget;
	desc->setUsage(usage);

	MTL::Texture* texture = m_mtlr->GetDevice()->newTexture(desc);
	desc->release();

	return texture;
}
