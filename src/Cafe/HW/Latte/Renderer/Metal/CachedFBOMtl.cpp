#include "Cafe/HW/Latte/Renderer/Metal/CachedFBOMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"

CachedFBOMtl::CachedFBOMtl(class MetalRenderer* metalRenderer, uint64 key) : LatteCachedFBO(key)
{
	m_renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();

	bool hasAttachment = false;
	for (int i = 0; i < 8; ++i)
	{
		const auto& buffer = colorBuffer[i];
		auto textureView = (LatteTextureViewMtl*)buffer.texture;
		if (!textureView)
		{
			continue;
		}
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

	// HACK: setup a dummy color attachment to prevent Metal from discarding draws for streamout draws in Super Smash Bros. for Wii U (works fine on MoltenVK without this hack though)
	if (!hasAttachment)
	{
        auto colorAttachment = m_renderPassDescriptor->colorAttachments()->object(0);
    	colorAttachment->setTexture(metalRenderer->GetNullTexture2D());
    	colorAttachment->setLoadAction(MTL::LoadActionDontCare);
    	colorAttachment->setStoreAction(MTL::StoreActionDontCare);
	}

	// Visibility buffer
	m_renderPassDescriptor->setVisibilityResultBuffer(metalRenderer->GetOcclusionQueryResultBuffer());

	// Detect depth prepass
	if (metalRenderer->ClearDepthPrepass())
	{
        // Must have a depth buffer
        if (hasDepthBuffer())
    	{
    	    // Filter out shadow maps
    		if (depthBuffer.texture->baseTexture->width != depthBuffer.texture->baseTexture->height)
    		{
      		    bool hasColorBuffer = false;
     			for (uint32 i = 0; i < LATTE_NUM_COLOR_TARGET; i++)
                {
                    if (colorBuffer[i].texture)
                    {
                        hasColorBuffer = true;
                        break;
                    }
                }

                // Must not have a color buffer
                if (!hasColorBuffer)
                {
                    auto depthTexture = static_cast<LatteTextureMtl*>(depthBuffer.texture->baseTexture);
                    depthTexture->NotifyIsDepthPrepass();
                    m_isDepthPrepass = true;
                }
    		}
    	}
	}
}

CachedFBOMtl::~CachedFBOMtl()
{
	m_renderPassDescriptor->release();
}

void CachedFBOMtl::CheckForDepthPrepass()
{
    if (m_isDepthPrepass && hasDepthBuffer())
    {
        auto depthTexture = static_cast<LatteTextureMtl*>(depthBuffer.texture->baseTexture);
        depthTexture->InitializeDepthPrepass();
    }
}

void CachedFBOMtl::CheckForDepthPrepassClear()
{
    if (!m_isDepthPrepass && hasDepthBuffer())
    {
        auto depthTexture = static_cast<LatteTextureMtl*>(depthBuffer.texture->baseTexture);
        if (depthTexture->GetDepthPrepassInfo().needsClear)
        {
            m_renderPassDescriptor->depthAttachment()->setLoadAction(MTL::LoadActionClear);
            // TODO: set clear depth?
            m_needsClear = true;
        }
    }
}

void CachedFBOMtl::NotifyDepthPrepassCleared()
{
    if (m_needsClear)
    {
        auto depthTexture = static_cast<LatteTextureMtl*>(depthBuffer.texture->baseTexture);
        depthTexture->NotifyDepthPrepassCleared();
        m_renderPassDescriptor->depthAttachment()->setLoadAction(MTL::LoadActionLoad);
    }
}
