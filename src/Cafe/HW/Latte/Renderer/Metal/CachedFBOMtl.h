#pragma once

#include <Metal/Metal.hpp>

#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/Core/LatteCachedFBO.h"

class CachedFBOMtl : public LatteCachedFBO
{
public:
	CachedFBOMtl(uint64 key) : LatteCachedFBO(key) {}

	~CachedFBOMtl();

	MTL::RenderPassDescriptor* GetRenderPassDescriptor(bool& doesClear);
};
