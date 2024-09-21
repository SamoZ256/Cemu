#pragma once

#include <Metal/Metal.hpp>
#include <unordered_map>

#include "Cafe/HW/Latte/Core/LatteTexture.h"

#define RGBA_SWIZZLE 0x06880000
#define INVALID_SWIZZLE 0xFFFFFFFF

struct MetalTextureViewCache
{
    bool m_mirror;

    MTL::Texture* m_rgbaView = nullptr;
	struct {
	    uint32 key;
	    MTL::Texture* texture;
	} m_viewCache[4] = {{INVALID_SWIZZLE, nullptr}, {INVALID_SWIZZLE, nullptr}, {INVALID_SWIZZLE, nullptr}, {INVALID_SWIZZLE, nullptr}};
	std::unordered_map<uint32, MTL::Texture*> m_fallbackViewCache;

	MetalTextureViewCache(bool mirror) : m_mirror{mirror} {}

	void Clear();

	void CreateRGBAView(class LatteTextureViewMtl* view);

	MTL::Texture* GetView(class LatteTextureViewMtl* view, uint32 gpuSamplerSwizzle);
};

class LatteTextureViewMtl : public LatteTextureView
{
public:
    friend struct MetalTextureViewCache;

	LatteTextureViewMtl(class MetalRenderer* mtlRenderer, class LatteTextureMtl* texture, Latte::E_DIM dim, Latte::E_GX2SURFFMT format, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount);
	~LatteTextureViewMtl();

    MTL::Texture* GetSwizzledView(uint32 gpuSamplerSwizzle, bool mirror);

    MTL::Texture* GetRGBAView(bool mirror = false)
    {
        return GetSwizzledView(RGBA_SWIZZLE, mirror);
    }

private:
	class MetalRenderer* m_mtlr;

	class LatteTextureMtl* m_baseTexture;

	MetalTextureViewCache m_cache;
	MetalTextureViewCache m_cacheMirror;

    MTL::Texture* CreateSwizzledView(uint32 gpuSamplerSwizzle, bool mirror);
};
