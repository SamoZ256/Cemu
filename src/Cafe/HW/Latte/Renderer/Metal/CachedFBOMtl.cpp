#include "Cafe/HW/Latte/Renderer/Metal/CachedFBOMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"
#include "Metal/MTLRenderPass.hpp"

CachedFBOMtl::~CachedFBOMtl()
{
}

MTL::RenderPassDescriptor* CachedFBOMtl::GetRenderPassDescriptor(bool& doesClear)
{
	auto renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();

	for (int i = 0; i < 8; ++i)
	{
		const auto& buffer = colorBuffer[i];
		auto textureView = (LatteTextureViewMtl*)buffer.texture;
		if (!textureView)
		{
			continue;
		}
		doesClear = doesClear || textureView->FlushRegionAtRenderPassBegin(renderPassDescriptor, i);
	}

	// setup depth attachment
	if (depthBuffer.texture)
	{
		auto textureView = static_cast<LatteTextureViewMtl*>(depthBuffer.texture);
		doesClear = doesClear|| textureView->FlushRegionAtRenderPassBegin(renderPassDescriptor, 0, depthBuffer.hasStencil);
	}

	return renderPassDescriptor;
}
