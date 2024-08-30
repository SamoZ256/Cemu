#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"

#include "Cafe/HW/Latte/ISA/LatteReg.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "HW/Latte/Renderer/Metal/CachedFBOMtl.h"
#include "HW/Latte/Renderer/Metal/LatteToMtl.h"

// TODO: the hash should also include tetxure types
class MetalTileFlushPipelineCache
{
public:
    MetalTileFlushPipelineCache(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer} {}
    ~MetalTileFlushPipelineCache();

    MTL::RenderPipelineState* GetRenderPipelineState(const class CachedFBOMtl* activeFBO, uint8 renderTargetMask);

private:
    class MetalRenderer* m_mtlr;

    std::map<uint64, MTL::RenderPipelineState*> m_pipelineCache;
};
