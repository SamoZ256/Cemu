#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"

#include "Cafe/HW/Latte/Core/LatteTexture.h"
#include "HW/Latte/ISA/LatteReg.h"
#include "Metal/MTLRenderPass.hpp"

union MetalTextureClearInfo
{
    struct
    {
        float r, g, b, a;
    } color;
    struct
    {
        bool clearDepth;
        bool clearStencil;
        float depthValue;
        uint32 stencilValue;
    } depthStencil;
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

	Latte::E_GX2SURFFMT GetFormat() const {
        return m_format;
    }

    bool IsDepth() const {
        return m_isDepth;
    }

	void AllocateOnHost() override;

	void RegisterClear(const MetalTextureClearInfo& clearInfo, uint32 mip, uint32 slice)
	{
	    m_clearInfos[PackUint32x2(mip, slice)] = clearInfo;
	}

	void Flush(uint32 firstMip = 0, uint32 firstSlice = 0);

	void FlushRegion(uint32 firstMip, uint32 mipCount, uint32 firstSlice, uint32 sliceCount);

	// Returns whether a clear needs to be performed
	bool FlushRegionAtRenderPassBegin(uint32 mip, uint32 slice, MTL::RenderPassDescriptor* renderPassDescriptor, uint32 colorAttachmentIndex = 0, bool hasStencil = true);

protected:
	LatteTextureView* CreateView(Latte::E_DIM dim, Latte::E_GX2SURFFMT format, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount) override;

private:
	class MetalRenderer* m_mtlr;

	MTL::Texture* m_texture;

	Latte::E_GX2SURFFMT m_format;
	bool m_isDepth;

	std::map<uint64, MetalTextureClearInfo> m_clearInfos;

    void SetupAttachment(const MetalTextureClearInfo& clearInfo, MTL::RenderPassDescriptor* renderPassDescriptor, uint32 colorAttachmentIndex, uint32 mip, uint32 slice);

    void FlushPart(uint32 mip, uint32 slice);
};
