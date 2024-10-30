#include "Cafe/HW/Latte/Renderer/Metal/CachedFBOMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"
#include "HW/Latte/Core/LatteConst.h"

CachedFBOMtl::CachedFBOMtl(class MetalRenderer* metalRenderer, uint64 key) : LatteCachedFBO(key)
{
	m_renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();

	bool hasAttachment = false;
	for (int i = 0; i < 8; ++i)
	{
		auto textureView = static_cast<LatteTextureViewMtl*>(colorBuffer[i].texture);
		if (!textureView)
			continue;

		auto colorAttachment = m_renderPassDescriptor->colorAttachments()->object(i);
		colorAttachment->setTexture(textureView->GetRGBAView());
		colorAttachment->setLoadAction(MTL::LoadActionLoad);
		colorAttachment->setStoreAction(MTL::StoreActionStore);

		hasAttachment = true;
	}

	// setup depth attachment
	if (depthBuffer.texture)
	{
		auto textureView = static_cast<LatteTextureViewMtl*>(depthBuffer.texture);
		auto depthAttachment = m_renderPassDescriptor->depthAttachment();
		depthAttachment->setTexture(textureView->GetRGBAView());
		depthAttachment->setLoadAction(MTL::LoadActionLoad);
		depthAttachment->setStoreAction(MTL::StoreActionStore);

		// setup stencil attachment
		if (depthBuffer.hasStencil && GetMtlPixelFormatInfo(depthBuffer.texture->format, true).hasStencil)
		{
		    auto stencilAttachment = m_renderPassDescriptor->stencilAttachment();
            stencilAttachment->setTexture(textureView->GetRGBAView());
            stencilAttachment->setLoadAction(MTL::LoadActionLoad);
            stencilAttachment->setStoreAction(MTL::StoreActionStore);
		}

		hasAttachment = true;
	}

	// HACK: setup a dummy color attachment to prevent Metal from discarding draws for stremout draws in Super Smash Bros. for Wii U (works fine on MoltenVK without this hack though)
	if (!hasAttachment)
	{
        auto colorAttachment = m_renderPassDescriptor->colorAttachments()->object(0);
    	colorAttachment->setTexture(metalRenderer->GetNullTexture2D());
    	colorAttachment->setLoadAction(MTL::LoadActionDontCare);
    	colorAttachment->setStoreAction(MTL::StoreActionDontCare);
	}

	// Visibility buffer
	m_renderPassDescriptor->setVisibilityResultBuffer(metalRenderer->GetOcclusionQueryResultBuffer());
}

CachedFBOMtl::~CachedFBOMtl()
{
	m_renderPassDescriptor->release();
}

MTL::RenderPassDescriptor* CachedFBOMtl::GetRenderPassDescriptor()
{
    // Check if any of the color texture views has changed
    for (uint8 i = 0; i < LATTE_NUM_COLOR_TARGET; i++)
    {
        auto textureView = static_cast<LatteTextureViewMtl*>(colorBuffer[i].texture);
        if (!textureView)
            continue;

        if (!m_colorTextureViewsUsages[i] && textureView->HasPixelFormatViewUsage())
        {
            m_renderPassDescriptor->colorAttachments()->object(i)->setTexture(textureView->GetRGBAView());
            m_colorTextureViewsUsages[i] = true;
        }
    }

    return m_renderPassDescriptor;
}
