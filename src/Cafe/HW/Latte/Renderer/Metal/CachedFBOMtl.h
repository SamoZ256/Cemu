#pragma once

#include <Metal/Metal.hpp>

#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/Core/LatteCachedFBO.h"

class CachedFBOMtl : public LatteCachedFBO
{
public:
	CachedFBOMtl(class MetalRenderer* metalRenderer, uint64 key);

	~CachedFBOMtl();

	MTL::RenderPassDescriptor* GetRenderPassDescriptor();

private:
    MTL::RenderPassDescriptor* m_renderPassDescriptor = nullptr;

    bool m_colorTextureViewsUsages[8] = {false};
};
