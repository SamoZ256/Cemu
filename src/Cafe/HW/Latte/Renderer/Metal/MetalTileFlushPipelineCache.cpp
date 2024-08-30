#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalTileFlushPipelineCache.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Foundation/NSObject.hpp"
#include "HW/Latte/Core/LatteShader.h"
#include "HW/Latte/ISA/LatteReg.h"
#include "HW/Latte/Renderer/Metal/CachedFBOMtl.h"
#include "HW/Latte/Renderer/Metal/LatteToMtl.h"
#include "HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"

#include "HW/Latte/Core/FetchShader.h"
#include "HW/Latte/ISA/RegDefines.h"
#include "config/ActiveSettings.h"
#include <stdexcept>

inline std::string tileShaderGenerate(MetalDataType renderTargetDataTypes[LATTE_NUM_COLOR_TARGET])
{
	std::string src;
	src += "#include <metal_stdlib>\n";
	src += "using namespace metal;\n";

	src += "struct Imageblock {\n";

	std::string inputsStr;
	std::string blockStr;
	for (uint8 i = 0; i < LATTE_NUM_COLOR_TARGET; i++)
	{
	    MetalDataType dataType = renderTargetDataTypes[i];
	    if (dataType != MetalDataType::NONE)
		{
            std::string dataTypeStr;
            switch (dataType)
            {
            case MetalDataType::INT:
                dataTypeStr = "int";
                break;
            case MetalDataType::UINT:
                dataTypeStr = "uint";
                break;
            case MetalDataType::FLOAT:
                dataTypeStr = "float";
                break;
            }

            // Imageblock declaration
            src += dataTypeStr + "4 color" + std::to_string(i) + " [[color(" + std::to_string(i) + ")]];\n";

            // Inputs

            // Out
            // TODO: use the correct texture type
            inputsStr += ", texture2d<";
            inputsStr += dataTypeStr;
            inputsStr += ", access::write> out";
            inputsStr += std::to_string(i);
            inputsStr += " [[texture(";
            inputsStr += std::to_string(i);
            inputsStr += ")]]";

            // Block

            // Write to the attachment
            blockStr += "out" + std::to_string(i) + ".write(imageblk.color" + std::to_string(i) + ", tid);\n";
        }
	}

	src += "};\n";

	src += "kernel void kernelMain(ushort2 tid [[thread_position_in_grid]], ushort2 local_id [[thread_position_in_threadgroup]], imageblock<Imageblock, imageblock_layout_implicit> in"
	;
	src += inputsStr;
	src += ") {\n";
	src += "Imageblock imageblk = in.read(local_id);\n";
	src += blockStr;
	src += "}\n";

	return src;
}

MetalTileFlushPipelineCache::~MetalTileFlushPipelineCache()
{
    for (auto& pair : m_pipelineCache)
    {
        pair.second->release();
    }
    m_pipelineCache.clear();
}

MTL::RenderPipelineState* MetalTileFlushPipelineCache::GetRenderPipelineState(const class CachedFBOMtl* activeFBO, uint8 renderTargetMask)
{
    uint64 stateHash = 0;
    MetalDataType renderTargetDataTypes[LATTE_NUM_COLOR_TARGET] = {MetalDataType::NONE};
    for (int i = 0; i < LATTE_NUM_COLOR_TARGET; ++i)
	{
	    if (!(renderTargetMask & (1 << i)))
            continue;

		auto textureView = static_cast<LatteTextureViewMtl*>(activeFBO->colorBuffer[i].texture);
		if (!textureView)
		    throw;

		stateHash += textureView->GetRGBAView()->pixelFormat() + i * 31;
		stateHash = std::rotl<uint64>(stateHash, 7);

		renderTargetDataTypes[i] = GetMtlPixelFormatInfo(textureView->format, false).dataType;
		if (renderTargetDataTypes[i] == MetalDataType::NONE)
		    throw std::runtime_error("a: " + std::to_string(GetMtlPixelFormatInfo(textureView->format, false).pixelFormat));
	}

    auto& pipeline = m_pipelineCache[stateHash];
    if (pipeline)
        return pipeline;

    // Generate the tile function
    std::string tileFunctionSrc = tileShaderGenerate(renderTargetDataTypes);
    NS::Error* error = nullptr;
	MTL::Library* library = m_mtlr->GetDevice()->newLibrary(ToNSString(tileFunctionSrc), nullptr, &error);
	if (error)
    {
        printf("failed to create library (error: %s) -> source:\n%s\n", error->localizedDescription()->utf8String(), tileFunctionSrc.c_str());
        return nullptr;
    }
    MTL::Function* function = library->newFunction(ToNSString("kernelMain"));
    library->release();

	// Render pipeline state
	auto desc = MTL::TileRenderPipelineDescriptor::alloc()->init();
	desc->setTileFunction(function);
	desc->setThreadgroupSizeMatchesTileSize(true);
	// TODO: set max total threads per threadgroup?

	for (int i = 0; i < Latte::GPU_LIMITS::NUM_COLOR_ATTACHMENTS; ++i)
    {
        if (!(renderTargetMask & (1 << i)))
            continue;

        auto textureView = static_cast<LatteTextureViewMtl*>(activeFBO->colorBuffer[i].texture);
        if (!textureView)
            continue;

        auto colorAttachment = desc->colorAttachments()->object(i);
        colorAttachment->setPixelFormat(textureView->GetRGBAView()->pixelFormat());
    }

	// TODO: binary archive?

    error = nullptr;
#ifdef CEMU_DEBUG_ASSERT
    desc->setLabel(GetLabel("Tile flush render pipeline state", desc));
#endif
	pipeline = m_mtlr->GetDevice()->newRenderPipelineState(desc, MTL::PipelineOptionNone, nullptr, &error);
	desc->release();
	function->release();
	if (error)
    {
        printf("failed to create tile flush render pipeline state (error: %s)\n", error->localizedDescription()->utf8String());
        error->release();
        return nullptr;
    }

	return pipeline;
}
