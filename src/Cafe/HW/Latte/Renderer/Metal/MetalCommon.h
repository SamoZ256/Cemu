#pragma once

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Metal/MTLPixelFormat.hpp"

struct MetalPixelFormatSupport
{
	bool m_supportsR8Unorm_sRGB;
	bool m_supportsRG8Unorm_sRGB;
	bool m_supportsPacked16BitFormats;
	bool m_supportsDepth24Unorm_Stencil8;

	MetalPixelFormatSupport() = default;
	MetalPixelFormatSupport(MTL::Device* device)
	{
        m_supportsR8Unorm_sRGB = device->supportsFamily(MTL::GPUFamilyApple1);
        m_supportsRG8Unorm_sRGB = device->supportsFamily(MTL::GPUFamilyApple1);
        m_supportsPacked16BitFormats = device->supportsFamily(MTL::GPUFamilyApple1);
        m_supportsDepth24Unorm_Stencil8 = device->depth24Stencil8PixelFormatSupported();
	}
};

// TODO: don't define a new struct for this
struct MetalQueryRange
{
    uint32 begin;
	uint32 end;
};

#define MAX_MTL_BUFFERS 31
// Buffer indices 28-30 are reserved for the helper shaders
#define GET_MTL_VERTEX_BUFFER_INDEX(index) (MAX_MTL_BUFFERS - index - 4)

#define MAX_MTL_TEXTURES 31
#define MAX_MTL_SAMPLERS 16

#define GET_HELPER_BUFFER_BINDING(index) (28 + index)
#define GET_HELPER_TEXTURE_BINDING(index) (29 + index)
#define GET_HELPER_SAMPLER_BINDING(index) (14 + index)

constexpr uint32 INVALID_UINT32 = std::numeric_limits<uint32>::max();
constexpr size_t INVALID_OFFSET = std::numeric_limits<size_t>::max();

inline size_t Align(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

//inline std::string GetColorAttachmentTypeStr(uint32 index)
//{
//    return "COLOR_ATTACHMENT" + std::to_string(index) + "_TYPE";
//}

// Cast from const char* to NS::String*
inline NS::String* ToNSString(const char* str)
{
    return NS::String::string(str, NS::ASCIIStringEncoding);
}

// Cast from std::string to NS::String*
inline NS::String* ToNSString(const std::string& str)
{
    return ToNSString(str.c_str());
}

inline NS::String* GetLabel(const std::string& label, const void* identifier)
{
    return ToNSString(label + " (" + std::to_string(reinterpret_cast<uintptr_t>(identifier)) + ")");
}

constexpr MTL::RenderStages ALL_MTL_RENDER_STAGES = MTL::RenderStageVertex | MTL::RenderStageObject | MTL::RenderStageMesh | MTL::RenderStageFragment;

inline bool IsValidDepthTextureType(Latte::E_DIM dim)
{
    return (dim == Latte::E_DIM::DIM_2D || dim == Latte::E_DIM::DIM_2D_MSAA || dim == Latte::E_DIM::DIM_2D_ARRAY || dim == Latte::E_DIM::DIM_2D_ARRAY_MSAA || dim == Latte::E_DIM::DIM_CUBEMAP);
}

inline bool CommandBufferCompleted(MTL::CommandBuffer* commandBuffer)
{
    auto status = commandBuffer->status();
    return (status == MTL::CommandBufferStatusCompleted || status == MTL::CommandBufferStatusError);
}

enum class MetalPixelFormatLayout
{
    Invalid,

    ABGR4,
    B5G6R5,
    BGR5A1,
    A1BGR5,
    R8,
    RG8,
    RGBA8,
    RGB10A2,
    BGR10A2,
    RGBA16,
    R16,
    RG16,
    R32,
    RG11B10,
    RG32,
    RGBA32,
    BC1,
    BC2,
    BC3,
    BC4,
    BC5,
    D24S8,
    D32S8,
};

inline MetalPixelFormatLayout GetPixelFormatLayout(MTL::PixelFormat format)
{
    switch (format)
    {
    case MTL::PixelFormatABGR4Unorm:
        return MetalPixelFormatLayout::ABGR4;
    case MTL::PixelFormatB5G6R5Unorm:
        return MetalPixelFormatLayout::B5G6R5;
    case MTL::PixelFormatBGR5A1Unorm:
        return MetalPixelFormatLayout::BGR5A1;
    case MTL::PixelFormatA1BGR5Unorm:
        return MetalPixelFormatLayout::A1BGR5;
    case MTL::PixelFormatR8Unorm:
        return MetalPixelFormatLayout::R8;
    case MTL::PixelFormatR8Snorm:
        return MetalPixelFormatLayout::R8;
    case MTL::PixelFormatR8Uint:
        return MetalPixelFormatLayout::R8;
    case MTL::PixelFormatR8Sint:
        return MetalPixelFormatLayout::R8;
    case MTL::PixelFormatRG8Unorm:
        return MetalPixelFormatLayout::RG8;
    case MTL::PixelFormatRG8Snorm:
        return MetalPixelFormatLayout::RG8;
    case MTL::PixelFormatRG8Uint:
        return MetalPixelFormatLayout::RG8;
    case MTL::PixelFormatRG8Sint:
        return MetalPixelFormatLayout::RG8;
    case MTL::PixelFormatRGBA8Unorm:
        return MetalPixelFormatLayout::RGBA8;
    case MTL::PixelFormatRGBA8Snorm:
        return MetalPixelFormatLayout::RGBA8;
    case MTL::PixelFormatRGBA8Uint:
        return MetalPixelFormatLayout::RGBA8;
    case MTL::PixelFormatRGBA8Sint:
        return MetalPixelFormatLayout::RGBA8;
    case MTL::PixelFormatRGBA8Unorm_sRGB:
        return MetalPixelFormatLayout::RGBA8;
    case MTL::PixelFormatRGB10A2Unorm:
        return MetalPixelFormatLayout::RGB10A2;
    case MTL::PixelFormatRGBA16Snorm:
        return MetalPixelFormatLayout::RGBA16;
    case MTL::PixelFormatRGB10A2Uint:
        return MetalPixelFormatLayout::RGB10A2;
    case MTL::PixelFormatRGBA16Sint:
        return MetalPixelFormatLayout::RGBA16;
    case MTL::PixelFormatBGR10A2Unorm:
        return MetalPixelFormatLayout::BGR10A2;
    case MTL::PixelFormatR16Unorm:
        return MetalPixelFormatLayout::R16;
    case MTL::PixelFormatR16Snorm:
        return MetalPixelFormatLayout::R16;
    case MTL::PixelFormatR16Uint:
        return MetalPixelFormatLayout::R16;
    case MTL::PixelFormatR16Sint:
        return MetalPixelFormatLayout::R16;
    case MTL::PixelFormatR16Float:
        return MetalPixelFormatLayout::R16;
    case MTL::PixelFormatRG16Unorm:
        return MetalPixelFormatLayout::R16;
    case MTL::PixelFormatRG16Snorm:
        return MetalPixelFormatLayout::RG16;
    case MTL::PixelFormatRG16Uint:
        return MetalPixelFormatLayout::RG16;
    case MTL::PixelFormatRG16Sint:
        return MetalPixelFormatLayout::RG16;
    case MTL::PixelFormatRG16Float:
        return MetalPixelFormatLayout::RG16;
    case MTL::PixelFormatRGBA16Unorm:
        return MetalPixelFormatLayout::RGBA16;
    case MTL::PixelFormatRGBA16Uint:
        return MetalPixelFormatLayout::RGBA16;
    case MTL::PixelFormatRGBA16Float:
        return MetalPixelFormatLayout::RGBA16;
    case MTL::PixelFormatR32Float:
        return MetalPixelFormatLayout::R32;
    case MTL::PixelFormatRG11B10Float:
        return MetalPixelFormatLayout::RG11B10;
    case MTL::PixelFormatR32Uint:
        return MetalPixelFormatLayout::R32;
    case MTL::PixelFormatR32Sint:
        return MetalPixelFormatLayout::R32;
    case MTL::PixelFormatRG32Uint:
        return MetalPixelFormatLayout::RG32;
    case MTL::PixelFormatRG32Sint:
        return MetalPixelFormatLayout::RG32;
    case MTL::PixelFormatRG32Float:
        return MetalPixelFormatLayout::RG32;
    case MTL::PixelFormatRGBA32Uint:
        return MetalPixelFormatLayout::RGBA32;
    case MTL::PixelFormatRGBA32Sint:
        return MetalPixelFormatLayout::RGBA32;
    case MTL::PixelFormatRGBA32Float:
        return MetalPixelFormatLayout::RGBA32;
    case MTL::PixelFormatBC1_RGBA:
        return MetalPixelFormatLayout::BC1;
    case MTL::PixelFormatBC1_RGBA_sRGB:
        return MetalPixelFormatLayout::BC1;
    case MTL::PixelFormatBC2_RGBA:
        return MetalPixelFormatLayout::BC2;
    case MTL::PixelFormatBC2_RGBA_sRGB:
        return MetalPixelFormatLayout::BC2;
    case MTL::PixelFormatBC3_RGBA:
        return MetalPixelFormatLayout::BC3;
    case MTL::PixelFormatBC3_RGBA_sRGB:
        return MetalPixelFormatLayout::BC3;
    case MTL::PixelFormatBC4_RUnorm:
        return MetalPixelFormatLayout::BC4;
    case MTL::PixelFormatBC4_RSnorm:
        return MetalPixelFormatLayout::BC4;
    case MTL::PixelFormatBC5_RGUnorm:
        return MetalPixelFormatLayout::BC5;
    case MTL::PixelFormatBC5_RGSnorm:
        return MetalPixelFormatLayout::BC5;
    case MTL::PixelFormatDepth24Unorm_Stencil8:
        return MetalPixelFormatLayout::D24S8;
    case MTL::PixelFormatDepth32Float:
        return MetalPixelFormatLayout::R32; // TODO: correct?
    case MTL::PixelFormatDepth32Float_Stencil8:
        return MetalPixelFormatLayout::D32S8;
    case MTL::PixelFormatDepth16Unorm:
        return MetalPixelFormatLayout::R16; // TODO: correct?
    default:
        return MetalPixelFormatLayout::Invalid;
    }
}

inline bool PixelFormatsCompatible(MTL::PixelFormat format1, MTL::PixelFormat format2)
{
    if (format1 == format2)
        return true;

    return GetPixelFormatLayout(format1) == GetPixelFormatLayout(format2);
}
