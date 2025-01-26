#pragma once

#include <Metal/Metal.hpp>

#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/Core/LatteCachedFBO.h"

class CachedFBOMtl : public LatteCachedFBO
{
public:
	CachedFBOMtl(class MetalRenderer* metalRenderer, uint64 key);

	~CachedFBOMtl();

	MTL::RenderPassDescriptor* GetRenderPassDescriptor()
	{
	    return m_renderPassDescriptor;
	}

	void CheckForDepthPrepass();

	void CheckForDepthPrepassClear();

	void NotifyDepthPrepassCleared();

private:
    MTL::RenderPassDescriptor* m_renderPassDescriptor = nullptr;

    bool m_isDepthPrepass = false;
    bool m_needsClear = false;
};
