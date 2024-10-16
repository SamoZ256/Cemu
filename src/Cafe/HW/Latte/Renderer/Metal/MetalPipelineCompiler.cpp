#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalPipelineCompiler.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/CachedFBOMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/RendererShaderMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"

#include "Cafe/HW/Latte/Core/FetchShader.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "HW/Latte/Renderer/RendererShader.h"
#include <chrono>

extern std::atomic_int g_compiling_pipelines;
extern std::atomic_int g_compiling_pipelines_async;
extern std::atomic_uint64_t g_compiling_pipelines_syncTimeSum;

static void rectsEmulationGS_outputSingleVertex(std::string& gsSrc, const LatteDecompilerShader* vertexShader, LatteShaderPSInputTable* psInputTable, sint32 vIdx, const LatteContextRegister& latteRegister)
{
	auto parameterMask = vertexShader->outputParameterMask;
	for (uint32 i = 0; i < 32; i++)
	{
		if ((parameterMask & (1 << i)) == 0)
			continue;
		sint32 vsSemanticId = psInputTable->getVertexShaderOutParamSemanticId(latteRegister.GetRawView(), i);
		if (vsSemanticId < 0)
			continue;
		// make sure PS has matching input
		if (!psInputTable->hasPSImportForSemanticId(vsSemanticId))
			continue;
		gsSrc.append(fmt::format("out.passParameterSem{} = objectPayload.vertexOut[{}].passParameterSem{};\r\n", vsSemanticId, vIdx, vsSemanticId));
	}
	gsSrc.append(fmt::format("out.position = objectPayload.vertexOut[{}].position;\r\n", vIdx));
	gsSrc.append(fmt::format("mesh.set_vertex({}, out);\r\n", vIdx));
}

static void rectsEmulationGS_outputGeneratedVertex(std::string& gsSrc, const LatteDecompilerShader* vertexShader, LatteShaderPSInputTable* psInputTable, const char* variant, const LatteContextRegister& latteRegister)
{
	auto parameterMask = vertexShader->outputParameterMask;
	for (uint32 i = 0; i < 32; i++)
	{
		if ((parameterMask & (1 << i)) == 0)
			continue;
		sint32 vsSemanticId = psInputTable->getVertexShaderOutParamSemanticId(latteRegister.GetRawView(), i);
		if (vsSemanticId < 0)
			continue;
		// make sure PS has matching input
		if (!psInputTable->hasPSImportForSemanticId(vsSemanticId))
			continue;
		gsSrc.append(fmt::format("out.passParameterSem{} = gen4thVertex{}(objectPayload.vertexOut[0].passParameterSem{}, objectPayload.vertexOut[1].passParameterSem{}, objectPayload.vertexOut[2].passParameterSem{});\r\n", vsSemanticId, variant, vsSemanticId, vsSemanticId, vsSemanticId));
	}
	gsSrc.append(fmt::format("out.position = gen4thVertex{}(objectPayload.vertexOut[0].position, objectPayload.vertexOut[1].position, objectPayload.vertexOut[2].position);\r\n", variant));
	gsSrc.append(fmt::format("mesh.set_vertex(3, out);\r\n"));
}

static void rectsEmulationGS_outputVerticesCode(std::string& gsSrc, const LatteDecompilerShader* vertexShader, LatteShaderPSInputTable* psInputTable, sint32 p0, sint32 p1, sint32 p2, sint32 p3, const char* variant, const LatteContextRegister& latteRegister)
{
	sint32 pList[4] = { p0, p1, p2, p3 };
	for (sint32 i = 0; i < 4; i++)
	{
		if (pList[i] == 3)
			rectsEmulationGS_outputGeneratedVertex(gsSrc, vertexShader, psInputTable, variant, latteRegister);
		else
			rectsEmulationGS_outputSingleVertex(gsSrc, vertexShader, psInputTable, pList[i], latteRegister);
	}
	gsSrc.append(fmt::format("mesh.set_index(0, {});\r\n", pList[0]));
	gsSrc.append(fmt::format("mesh.set_index(1, {});\r\n", pList[1]));
	gsSrc.append(fmt::format("mesh.set_index(2, {});\r\n", pList[2]));
	gsSrc.append(fmt::format("mesh.set_index(3, {});\r\n", pList[1]));
	gsSrc.append(fmt::format("mesh.set_index(4, {});\r\n", pList[2]));
	gsSrc.append(fmt::format("mesh.set_index(5, {});\r\n", pList[3]));
}

static RendererShaderMtl* rectsEmulationGS_generate(MetalRenderer* metalRenderer, const LatteDecompilerShader* vertexShader, const LatteContextRegister& latteRegister)
{
	std::string gsSrc;
	gsSrc.append("#include <metal_stdlib>\r\n");
	gsSrc.append("using namespace metal;\r\n");

	LatteShaderPSInputTable* psInputTable = LatteSHRC_GetPSInputTable();

	// inputs & outputs
	std::string vertexOutDefinition = "struct VertexOut {\r\n";
	vertexOutDefinition += "float4 position;\r\n";
	std::string geometryOutDefinition = "struct GeometryOut {\r\n";
	geometryOutDefinition += "float4 position [[position]];\r\n";
	auto parameterMask = vertexShader->outputParameterMask;
	for (sint32 f = 0; f < 2; f++)
	{
		for (uint32 i = 0; i < 32; i++)
		{
			if ((parameterMask & (1 << i)) == 0)
				continue;
			sint32 vsSemanticId = psInputTable->getVertexShaderOutParamSemanticId(latteRegister.GetRawView(), i);
			if (vsSemanticId < 0)
				continue;
			auto psImport = psInputTable->getPSImportBySemanticId(vsSemanticId);
			if (psImport == nullptr)
				continue;

			if (f == 0)
			{
				vertexOutDefinition += fmt::format("float4 passParameterSem{};\r\n", vsSemanticId);
			}
			else
			{
				geometryOutDefinition += fmt::format("float4 passParameterSem{}", vsSemanticId);

    			geometryOutDefinition += fmt::format(" [[user(locn{})]]", psInputTable->getPSImportLocationBySemanticId(vsSemanticId));
    			if (psImport->isFlat)
    				geometryOutDefinition += " [[flat]]";
    			if (psImport->isNoPerspective)
    				geometryOutDefinition += " [[center_no_perspective]]";
                geometryOutDefinition += ";\r\n";
			}
		}
	}
	vertexOutDefinition += "};\r\n";
	geometryOutDefinition += "};\r\n";

	gsSrc.append(vertexOutDefinition);
	gsSrc.append(geometryOutDefinition);

	gsSrc.append("struct ObjectPayload {\r\n");
	gsSrc.append("VertexOut vertexOut[3];\r\n");
	gsSrc.append("};\r\n");

	// gen function
	gsSrc.append("float4 gen4thVertexA(float4 a, float4 b, float4 c)\r\n");
	gsSrc.append("{\r\n");
	gsSrc.append("return b - (c - a);\r\n");
	gsSrc.append("}\r\n");

	gsSrc.append("float4 gen4thVertexB(float4 a, float4 b, float4 c)\r\n");
	gsSrc.append("{\r\n");
	gsSrc.append("return c - (b - a);\r\n");
	gsSrc.append("}\r\n");

	gsSrc.append("float4 gen4thVertexC(float4 a, float4 b, float4 c)\r\n");
	gsSrc.append("{\r\n");
	gsSrc.append("return c + (b - a);\r\n");
	gsSrc.append("}\r\n");

	// main
	gsSrc.append("using MeshType = mesh<GeometryOut, void, 4, 2, topology::triangle>;\r\n");
	gsSrc.append("[[mesh, max_total_threads_per_threadgroup(1)]]\r\n");
	gsSrc.append("void main0(MeshType mesh, const object_data ObjectPayload& objectPayload [[payload]])\r\n");
	gsSrc.append("{\r\n");
	gsSrc.append("GeometryOut out;\r\n");

	// there are two possible winding orders that need different triangle generation:
	// 0 1
	// 2 3
	// and
	// 0 1
	// 3 2
	// all others are just symmetries of these cases

	// we can determine the case by comparing the distance 0<->1 and 0<->2

	gsSrc.append("float dist0_1 = length(objectPayload.vertexOut[1].position.xy - objectPayload.vertexOut[0].position.xy);\r\n");
	gsSrc.append("float dist0_2 = length(objectPayload.vertexOut[2].position.xy - objectPayload.vertexOut[0].position.xy);\r\n");
	gsSrc.append("float dist1_2 = length(objectPayload.vertexOut[2].position.xy - objectPayload.vertexOut[1].position.xy);\r\n");

	// emit vertices
	gsSrc.append("if(dist0_1 > dist0_2 && dist0_1 > dist1_2)\r\n");
	gsSrc.append("{\r\n");
	// p0 to p1 is diagonal
	rectsEmulationGS_outputVerticesCode(gsSrc, vertexShader, psInputTable, 2, 1, 0, 3, "A", latteRegister);
	gsSrc.append("} else if ( dist0_2 > dist0_1 && dist0_2 > dist1_2 ) {\r\n");
	// p0 to p2 is diagonal
	rectsEmulationGS_outputVerticesCode(gsSrc, vertexShader, psInputTable, 1, 2, 0, 3, "B", latteRegister);
	gsSrc.append("} else {\r\n");
	// p1 to p2 is diagonal
	rectsEmulationGS_outputVerticesCode(gsSrc, vertexShader, psInputTable, 0, 1, 2, 3, "C", latteRegister);
	gsSrc.append("}\r\n");

	gsSrc.append("mesh.set_primitive_count(2);\r\n");

	gsSrc.append("}\r\n");

	auto mtlShader = new RendererShaderMtl(metalRenderer, RendererShader::ShaderType::kGeometry, 0, 0, false, false, gsSrc);
	mtlShader->PreponeCompilation(true);

	return mtlShader;
}

#define INVALID_TITLE_ID 0xFFFFFFFFFFFFFFFF

uint64 s_cacheTitleId = INVALID_TITLE_ID;

extern std::atomic_int g_compiled_shaders_total;
extern std::atomic_int g_compiled_shaders_async;

template<typename T>
void SetFragmentState(T* desc, const MetalAttachmentsInfo& lastUsedAttachmentsInfo, const MetalAttachmentsInfo& activeAttachmentsInfo, const LatteContextRegister& lcr, bool& fbosMatch)
{
	// Rasterization
	bool rasterizationEnabled = !lcr.PA_CL_CLIP_CNTL.get_DX_RASTERIZATION_KILL();

	// HACK
	// TODO: include this in the hash?
	if (!lcr.PA_CL_VTE_CNTL.get_VPORT_X_OFFSET_ENA())
		rasterizationEnabled = true;

	// Culling both front and back faces effectively disables rasterization
	const auto& polygonControlReg = lcr.PA_SU_SC_MODE_CNTL;
	uint32 cullFront = polygonControlReg.get_CULL_FRONT();
	uint32 cullBack = polygonControlReg.get_CULL_BACK();
	if (cullFront && cullBack)
	    rasterizationEnabled = false;

	// TODO: check if the pixel shader is valid as well?
	if (!rasterizationEnabled/* || !pixelShaderMtl*/)
	{
	    desc->setRasterizationEnabled(false);
		return;
	}

    // Color attachments
    fbosMatch = true;
	const Latte::LATTE_CB_COLOR_CONTROL& colorControlReg = lcr.CB_COLOR_CONTROL;
	uint32 blendEnableMask = colorControlReg.get_BLEND_MASK();
	uint32 renderTargetMask = lcr.CB_TARGET_MASK.get_MASK();
	for (uint8 i = 0; i < LATTE_NUM_COLOR_TARGET; i++)
	{
	    Latte::E_GX2SURFFMT format = lastUsedAttachmentsInfo.colorFormats[i];
		if (format == Latte::E_GX2SURFFMT::INVALID_FORMAT)
		    continue;

		MTL::PixelFormat pixelFormat = GetMtlPixelFormat(format, false);
		auto colorAttachment = desc->colorAttachments()->object(i);
		colorAttachment->setPixelFormat(pixelFormat);

		// Disable writes if not in the active FBO
		if (activeAttachmentsInfo.colorFormats[i] == Latte::E_GX2SURFFMT::INVALID_FORMAT)
        {
            colorAttachment->setWriteMask(MTL::ColorWriteMaskNone);
            fbosMatch = false;
            continue;
        }

		colorAttachment->setWriteMask(GetMtlColorWriteMask((renderTargetMask >> (i * 4)) & 0xF));

		// Blending
		bool blendEnabled = ((blendEnableMask & (1 << i))) != 0;
		// Only float data type is blendable
		if (blendEnabled && GetMtlPixelFormatInfo(format, false).dataType == MetalDataType::FLOAT)
		{
       		colorAttachment->setBlendingEnabled(true);

       		const auto& blendControlReg = lcr.CB_BLENDN_CONTROL[i];

       		auto rgbBlendOp = GetMtlBlendOp(blendControlReg.get_COLOR_COMB_FCN());
       		auto srcRgbBlendFactor = GetMtlBlendFactor(blendControlReg.get_COLOR_SRCBLEND());
       		auto dstRgbBlendFactor = GetMtlBlendFactor(blendControlReg.get_COLOR_DSTBLEND());

       		colorAttachment->setRgbBlendOperation(rgbBlendOp);
       		colorAttachment->setSourceRGBBlendFactor(srcRgbBlendFactor);
       		colorAttachment->setDestinationRGBBlendFactor(dstRgbBlendFactor);
       		if (blendControlReg.get_SEPARATE_ALPHA_BLEND())
       		{
       			colorAttachment->setAlphaBlendOperation(GetMtlBlendOp(blendControlReg.get_ALPHA_COMB_FCN()));
      		    colorAttachment->setSourceAlphaBlendFactor(GetMtlBlendFactor(blendControlReg.get_ALPHA_SRCBLEND()));
      		    colorAttachment->setDestinationAlphaBlendFactor(GetMtlBlendFactor(blendControlReg.get_ALPHA_DSTBLEND()));
       		}
       		else
       		{
           		colorAttachment->setAlphaBlendOperation(rgbBlendOp);
           		colorAttachment->setSourceAlphaBlendFactor(srcRgbBlendFactor);
           		colorAttachment->setDestinationAlphaBlendFactor(dstRgbBlendFactor);
       		}
		}
	}

	// Depth stencil attachment
	if (lastUsedAttachmentsInfo.depthFormat != Latte::E_GX2SURFFMT::INVALID_FORMAT)
	{
	    MTL::PixelFormat pixelFormat = GetMtlPixelFormat(lastUsedAttachmentsInfo.depthFormat, true);
        desc->setDepthAttachmentPixelFormat(pixelFormat);
        if (lastUsedAttachmentsInfo.hasStencil)
            desc->setStencilAttachmentPixelFormat(pixelFormat);
	}
}

MetalPipelineCompiler::~MetalPipelineCompiler()
{
    /*
    for (auto& pair : m_pipelineCache)
    {
        pair.second->release();
    }
    m_pipelineCache.clear();

    NS::Error* error = nullptr;
    m_binaryArchive->serializeToURL(m_binaryArchiveURL, &error);
    if (error)
    {
        cemuLog_log(LogType::Force, "error serializing binary archive: {}", error->localizedDescription()->utf8String());
        error->release();
    }
    m_binaryArchive->release();

    m_binaryArchiveURL->release();
    */
    m_pipelineDescriptor->release();
}

void MetalPipelineCompiler::InitFromState(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, const MetalAttachmentsInfo& lastUsedAttachmentsInfo, const MetalAttachmentsInfo& activeAttachmentsInfo, const LatteContextRegister& lcr, bool& fbosMatch)
{
    // Check if the pipeline uses a geometry shader
    const LattePrimitiveMode primitiveMode = static_cast<LattePrimitiveMode>(lcr.VGT_PRIMITIVE_TYPE.get_PRIMITIVE_MODE());
    bool isPrimitiveRect = (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::RECTS);

    m_usesGeometryShader = (geometryShader != nullptr || isPrimitiveRect);

    // Shaders
    m_vertexShaderMtl = static_cast<RendererShaderMtl*>(vertexShader->shader);
    if (geometryShader)
        m_geometryShaderMtl = static_cast<RendererShaderMtl*>(geometryShader->shader);
    else if (isPrimitiveRect)
        m_geometryShaderMtl = rectsEmulationGS_generate(m_mtlr, vertexShader, lcr);
    else
        m_geometryShaderMtl = nullptr;
    m_pixelShaderMtl = static_cast<RendererShaderMtl*>(pixelShader->shader);

    if (m_usesGeometryShader)
        InitFromStateMesh(fetchShader, lastUsedAttachmentsInfo, activeAttachmentsInfo, lcr, fbosMatch);
    else
        InitFromStateRender(fetchShader, vertexShader, lastUsedAttachmentsInfo, activeAttachmentsInfo, lcr, fbosMatch);
}

MTL::RenderPipelineState* MetalPipelineCompiler::Compile(bool forceCompile, bool isRenderThread, bool showInOverlay)
{
    if (forceCompile)
	{
		// if some shader stages are not compiled yet, compile them now
		if (m_vertexShaderMtl && !m_vertexShaderMtl->IsCompiled())
			m_vertexShaderMtl->PreponeCompilation(isRenderThread);
		if (m_geometryShaderMtl && !m_geometryShaderMtl->IsCompiled())
			m_geometryShaderMtl->PreponeCompilation(isRenderThread);
		if (m_pixelShaderMtl && !m_pixelShaderMtl->IsCompiled())
			m_pixelShaderMtl->PreponeCompilation(isRenderThread);
	}
	else
	{
	    // fail early if some shader stages are not compiled
		if (m_vertexShaderMtl && !m_vertexShaderMtl->IsCompiled())
			return nullptr;
		if (m_geometryShaderMtl && !m_geometryShaderMtl->IsCompiled())
			return nullptr;
		if (m_pixelShaderMtl && !m_pixelShaderMtl->IsCompiled())
			return nullptr;
	}

	// Compile
    MTL::RenderPipelineState* pipeline = nullptr;
    NS::Error* error = nullptr;

    auto start = std::chrono::high_resolution_clock::now();
    if (m_usesGeometryShader)
    {
        auto desc = static_cast<MTL::MeshRenderPipelineDescriptor*>(m_pipelineDescriptor);

        // Shaders
        desc->setObjectFunction(m_vertexShaderMtl->GetFunction());
        desc->setMeshFunction(m_geometryShaderMtl->GetFunction());
        desc->setFragmentFunction(m_pixelShaderMtl->GetFunction());

#ifdef CEMU_DEBUG_ASSERT
        desc->setLabel(GetLabel("Mesh render pipeline state", desc));
#endif
       	pipeline = m_mtlr->GetDevice()->newRenderPipelineState(desc, MTL::PipelineOptionNone, nullptr, &error);
    }
    else
    {
        auto desc = static_cast<MTL::RenderPipelineDescriptor*>(m_pipelineDescriptor);

        // Shaders
        desc->setVertexFunction(m_vertexShaderMtl->GetFunction());
        desc->setFragmentFunction(m_pixelShaderMtl->GetFunction());

#ifdef CEMU_DEBUG_ASSERT
        desc->setLabel(GetLabel("Render pipeline state", desc));
#endif
       	pipeline = m_mtlr->GetDevice()->newRenderPipelineState(desc, MTL::PipelineOptionNone, nullptr, &error);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto creationDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

   	if (error)
   	{
       	cemuLog_log(LogType::Force, "error creating render pipeline state: {}", error->localizedDescription()->utf8String());
        error->release();
   	}
    else if (showInOverlay)
	{
		if (isRenderThread)
			g_compiling_pipelines_syncTimeSum += creationDuration;
		else
			g_compiling_pipelines_async++;
		g_compiling_pipelines++;
	}

    return pipeline;
}

void MetalPipelineCompiler::InitFromStateRender(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const MetalAttachmentsInfo& lastUsedAttachmentsInfo, const MetalAttachmentsInfo& activeAttachmentsInfo, const LatteContextRegister& lcr, bool& fbosMatch)
{
	// Render pipeline state
	MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();

    // Vertex descriptor
    if (!fetchShader->mtlFetchVertexManually)
    {
    	MTL::VertexDescriptor* vertexDescriptor = MTL::VertexDescriptor::alloc()->init();
    	for (auto& bufferGroup : fetchShader->bufferGroups)
    	{
    		std::optional<LatteConst::VertexFetchType2> fetchType;

    		uint32 minBufferStride = 0;
    		for (sint32 j = 0; j < bufferGroup.attribCount; ++j)
    		{
    			auto& attr = bufferGroup.attrib[j];

    			uint32 semanticId = vertexShader->resourceMapping.attributeMapping[attr.semanticId];
    			if (semanticId == (uint32)-1)
    				continue; // attribute not used?

    			auto attribute = vertexDescriptor->attributes()->object(semanticId);
    			attribute->setOffset(attr.offset);
    			attribute->setBufferIndex(GET_MTL_VERTEX_BUFFER_INDEX(attr.attributeBufferIndex));
    			attribute->setFormat(GetMtlVertexFormat(attr.format));

    			minBufferStride = std::max(minBufferStride, attr.offset + GetMtlVertexFormatSize(attr.format));

    			if (fetchType.has_value())
    				cemu_assert_debug(fetchType == attr.fetchType);
    			else
    				fetchType = attr.fetchType;

    			if (attr.fetchType == LatteConst::INSTANCE_DATA)
    			{
    				cemu_assert_debug(attr.aluDivisor == 1); // other divisor not yet supported
    			}
    		}

    		uint32 bufferIndex = bufferGroup.attributeBufferIndex;
    		uint32 bufferBaseRegisterIndex = mmSQ_VTX_ATTRIBUTE_BLOCK_START + bufferIndex * 7;
    		uint32 bufferStride = (lcr.GetRawView()[bufferBaseRegisterIndex + 2] >> 11) & 0xFFFF;

    		auto layout = vertexDescriptor->layouts()->object(GET_MTL_VERTEX_BUFFER_INDEX(bufferIndex));
    		if (bufferStride == 0)
    		{
    		    // Buffer stride cannot be zero, let's use the minimum stride
    			bufferStride = minBufferStride;

    			// Additionally, constant vertex function must be used
    			layout->setStepFunction(MTL::VertexStepFunctionConstant);
    			layout->setStepRate(0);
    		}
    		else
    		{
      		if (!fetchType.has_value() || fetchType == LatteConst::VertexFetchType2::VERTEX_DATA)
     			layout->setStepFunction(MTL::VertexStepFunctionPerVertex);
      		else if (fetchType == LatteConst::VertexFetchType2::INSTANCE_DATA)
     			layout->setStepFunction(MTL::VertexStepFunctionPerInstance);
      		else
      		{
      		    debug_printf("unimplemented vertex fetch type %u\n", (uint32)fetchType.value());
     			cemu_assert(false);
      		}
    		}
    		bufferStride = Align(bufferStride, 4);
    		layout->setStride(bufferStride);
    	}

        // TODO: don't always set the vertex descriptor?
    	desc->setVertexDescriptor(vertexDescriptor);
        vertexDescriptor->release();
    }

	SetFragmentState(desc, lastUsedAttachmentsInfo, activeAttachmentsInfo, lcr, fbosMatch);

	m_pipelineDescriptor = desc;

	//TryLoadBinaryArchive();

	// Load binary
	/*
    if (m_binaryArchive)
    {
        NS::Object* binArchives[] = {m_binaryArchive};
        auto binaryArchives = NS::Array::alloc()->init(binArchives, 1);
        desc->setBinaryArchives(binaryArchives);
        binaryArchives->release();
    }
    */

    /*
    NS::Error* error = nullptr;
#ifdef CEMU_DEBUG_ASSERT
    desc->setLabel(GetLabel("Cached render pipeline state", desc));
#endif
	pipeline = m_mtlr->GetDevice()->newRenderPipelineState(desc, MTL::PipelineOptionFailOnBinaryArchiveMiss, nullptr, &error);

	// Pipeline wasn't found in the binary archive, we need to compile it
	if (error)
	{
		desc->setBinaryArchives(nullptr);

        error->release();
        error = nullptr;
#ifdef CEMU_DEBUG_ASSERT
        desc->setLabel(GetLabel("New render pipeline state", desc));
#endif
	    pipeline = m_mtlr->GetDevice()->newRenderPipelineState(desc, &error);
		if (error)
		{
		    cemuLog_log(LogType::Force, "error creating render pipeline state: {}", error->localizedDescription()->utf8String());
			error->release();
		}
		else
		{
		    // Save binary
			if (m_binaryArchive)
			{
                NS::Error* error = nullptr;
                m_binaryArchive->addRenderPipelineFunctions(desc, &error);
                if (error)
                {
                    cemuLog_log(LogType::Force, "error saving render pipeline functions: {}", error->localizedDescription()->utf8String());
                    error->release();
                }
			}
		}
	}
	desc->release();

	return pipeline;
	*/
}

void MetalPipelineCompiler::InitFromStateMesh(const LatteFetchShader* fetchShader, const MetalAttachmentsInfo& lastUsedAttachmentsInfo, const MetalAttachmentsInfo& activeAttachmentsInfo, const LatteContextRegister& lcr, bool& fbosMatch)
{
	// Render pipeline state
	MTL::MeshRenderPipelineDescriptor* desc = MTL::MeshRenderPipelineDescriptor::alloc()->init();

	SetFragmentState(desc, lastUsedAttachmentsInfo, activeAttachmentsInfo, lcr, fbosMatch);

	m_pipelineDescriptor = desc;

	//TryLoadBinaryArchive();

	// Load binary
    // TODO: no binary archives? :(

    /*
    NS::Error* error = nullptr;
#ifdef CEMU_DEBUG_ASSERT
    desc->setLabel(GetLabel("Mesh pipeline state", desc));
#endif
	pipeline = m_mtlr->GetDevice()->newRenderPipelineState(desc, MTL::PipelineOptionNone, nullptr, &error);
	desc->release();
	if (error)
	{
    	cemuLog_log(LogType::Force, "error creating mesh render pipeline state: {}", error->localizedDescription()->utf8String());
        error->release();
	}

	return pipeline;
	*/
}

/*
void MetalPipelineCache::TryLoadBinaryArchive()
{
    if (m_binaryArchive || s_cacheTitleId == INVALID_TITLE_ID)
        return;

    // GPU name
    const char* deviceName1 = m_mtlr->GetDevice()->name()->utf8String();
    std::string deviceName;
    deviceName.assign(deviceName1);

    // Replace spaces with underscores
    for (auto& c : deviceName)
    {
        if (c == ' ')
            c = '_';
    }

    // OS version
    auto osVersion = NS::ProcessInfo::processInfo()->operatingSystemVersion();

    // Precompiled binaries cannot be shared between different devices or OS versions
    const std::string cacheFilename = fmt::format("{:016x}_mtl_pipelines.bin", s_cacheTitleId);
	const fs::path cachePath = ActiveSettings::GetCachePath("shaderCache/precompiled/{}/{}-{}-{}/{}", deviceName, osVersion.majorVersion, osVersion.minorVersion, osVersion.patchVersion, cacheFilename);

	// Create the directory if it doesn't exist
	std::filesystem::create_directories(cachePath.parent_path());

    m_binaryArchiveURL = NS::URL::fileURLWithPath(ToNSString((const char*)cachePath.generic_u8string().c_str()));

    MTL::BinaryArchiveDescriptor* desc = MTL::BinaryArchiveDescriptor::alloc()->init();
    desc->setUrl(m_binaryArchiveURL);

    NS::Error* error = nullptr;
    m_binaryArchive = m_mtlr->GetDevice()->newBinaryArchive(desc, &error);
    if (error)
    {
        desc->setUrl(nullptr);

        error->release();
        error = nullptr;
        m_binaryArchive = m_mtlr->GetDevice()->newBinaryArchive(desc, &error);
        if (error)
        {
            cemuLog_log(LogType::Force, "failed to create binary archive: {}", error->localizedDescription()->utf8String());
            error->release();
        }
    }
    desc->release();
}
*/
