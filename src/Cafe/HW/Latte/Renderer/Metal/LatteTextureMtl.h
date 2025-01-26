#pragma once

#include <Metal/Metal.hpp>

#include "Cafe/HW/Latte/Core/LatteTexture.h"
#include "HW/Latte/ISA/LatteReg.h"

struct MetalDepthPrepassInfo
{
    bool isDepthPrepass = false;
    bool needsClear = false;
    Latte::E_COMPAREFUNC depthFunc = Latte::E_COMPAREFUNC::ALWAYS;
};

class LatteTextureMtl : public LatteTexture
{
public:
	LatteTextureMtl(class MetalRenderer* mtlRenderer, Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels,
		uint32 swizzle, Latte::E_HWTILEMODE tileMode, bool isDepth);
	~LatteTextureMtl();

	MTL::Texture* GetTexture() const {
	    return m_texture;
	}

	void AllocateOnHost() override;

	void NotifyIsDepthPrepass();

	void InitializeDepthPrepass();

	void NotifyDepthPrepassCleared();

	MetalDepthPrepassInfo GetDepthPrepassInfo() const {
        return m_depthPrepassInfo;
    }

protected:
	LatteTextureView* CreateView(Latte::E_DIM dim, Latte::E_GX2SURFFMT format, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount) override;

private:
	class MetalRenderer* m_mtlr;

	MTL::Texture* m_texture;

	MetalDepthPrepassInfo m_depthPrepassInfo{};
};
