#include "Cafe/HW/Latte/Renderer/Metal/RendererShaderMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
//#include "Cemu/FileCache/FileCache.h"
//#include "config/ActiveSettings.h"

#include "Cemu/Logging/CemuLogging.h"
#include "Common/precompiled.h"
#include "HW/Latte/Core/FetchShader.h"
#include "HW/Latte/ISA/RegDefines.h"

#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

extern std::atomic_int g_compiled_shaders_total;
extern std::atomic_int g_compiled_shaders_async;

bool RendererShaderMtl::s_glslangInitialized = false;

consteval TBuiltInResource GetDefaultBuiltInResource()
{
	TBuiltInResource defaultResource = {};
	defaultResource.maxLights = 32;
	defaultResource.maxClipPlanes = 6;
	defaultResource.maxTextureUnits = 32;
	defaultResource.maxTextureCoords = 32;
	defaultResource.maxVertexAttribs = 64;
	defaultResource.maxVertexUniformComponents = 4096;
	defaultResource.maxVaryingFloats = 64;
	defaultResource.maxVertexTextureImageUnits = 32;
	defaultResource.maxCombinedTextureImageUnits = 80;
	defaultResource.maxTextureImageUnits = 32;
	defaultResource.maxFragmentUniformComponents = 4096;
	defaultResource.maxDrawBuffers = 32;
	defaultResource.maxVertexUniformVectors = 128;
	defaultResource.maxVaryingVectors = 8;
	defaultResource.maxFragmentUniformVectors = 16;
	defaultResource.maxVertexOutputVectors = 16;
	defaultResource.maxFragmentInputVectors = 15;
	defaultResource.minProgramTexelOffset = -8;
	defaultResource.maxProgramTexelOffset = 7;
	defaultResource.maxClipDistances = 8;
	defaultResource.maxComputeWorkGroupCountX = 65535;
	defaultResource.maxComputeWorkGroupCountY = 65535;
	defaultResource.maxComputeWorkGroupCountZ = 65535;
	defaultResource.maxComputeWorkGroupSizeX = 1024;
	defaultResource.maxComputeWorkGroupSizeY = 1024;
	defaultResource.maxComputeWorkGroupSizeZ = 64;
	defaultResource.maxComputeUniformComponents = 1024;
	defaultResource.maxComputeTextureImageUnits = 16;
	defaultResource.maxComputeImageUniforms = 8;
	defaultResource.maxComputeAtomicCounters = 8;
	defaultResource.maxComputeAtomicCounterBuffers = 1;
	defaultResource.maxVaryingComponents = 60;
	defaultResource.maxVertexOutputComponents = 64;
	defaultResource.maxGeometryInputComponents = 64;
	defaultResource.maxGeometryOutputComponents = 128;
	defaultResource.maxFragmentInputComponents = 128;
	defaultResource.maxImageUnits = 8;
	defaultResource.maxCombinedImageUnitsAndFragmentOutputs = 8;
	defaultResource.maxCombinedShaderOutputResources = 8;
	defaultResource.maxImageSamples = 0;
	defaultResource.maxVertexImageUniforms = 0;
	defaultResource.maxTessControlImageUniforms = 0;
	defaultResource.maxTessEvaluationImageUniforms = 0;
	defaultResource.maxGeometryImageUniforms = 0;
	defaultResource.maxFragmentImageUniforms = 8;
	defaultResource.maxCombinedImageUniforms = 8;
	defaultResource.maxGeometryTextureImageUnits = 16;
	defaultResource.maxGeometryOutputVertices = 256;
	defaultResource.maxGeometryTotalOutputComponents = 1024;
	defaultResource.maxGeometryUniformComponents = 1024;
	defaultResource.maxGeometryVaryingComponents = 64;
	defaultResource.maxTessControlInputComponents = 128;
	defaultResource.maxTessControlOutputComponents = 128;
	defaultResource.maxTessControlTextureImageUnits = 16;
	defaultResource.maxTessControlUniformComponents = 1024;
	defaultResource.maxTessControlTotalOutputComponents = 4096;
	defaultResource.maxTessEvaluationInputComponents = 128;
	defaultResource.maxTessEvaluationOutputComponents = 128;
	defaultResource.maxTessEvaluationTextureImageUnits = 16;
	defaultResource.maxTessEvaluationUniformComponents = 1024;
	defaultResource.maxTessPatchComponents = 120;
	defaultResource.maxPatchVertices = 32;
	defaultResource.maxTessGenLevel = 64;
	defaultResource.maxViewports = 16;
	defaultResource.maxVertexAtomicCounters = 0;
	defaultResource.maxTessControlAtomicCounters = 0;
	defaultResource.maxTessEvaluationAtomicCounters = 0;
	defaultResource.maxGeometryAtomicCounters = 0;
	defaultResource.maxFragmentAtomicCounters = 8;
	defaultResource.maxCombinedAtomicCounters = 8;
	defaultResource.maxAtomicCounterBindings = 1;
	defaultResource.maxVertexAtomicCounterBuffers = 0;
	defaultResource.maxTessControlAtomicCounterBuffers = 0;
	defaultResource.maxTessEvaluationAtomicCounterBuffers = 0;
	defaultResource.maxGeometryAtomicCounterBuffers = 0;
	defaultResource.maxFragmentAtomicCounterBuffers = 1;
	defaultResource.maxCombinedAtomicCounterBuffers = 1;
	defaultResource.maxAtomicCounterBufferSize = 16384;
	defaultResource.maxTransformFeedbackBuffers = 4;
	defaultResource.maxTransformFeedbackInterleavedComponents = 64;
	defaultResource.maxCullDistances = 8;
	defaultResource.maxCombinedClipAndCullDistances = 8;
	defaultResource.maxSamples = 4;
	defaultResource.maxMeshOutputVerticesNV = 256;
	defaultResource.maxMeshOutputPrimitivesNV = 512;
	defaultResource.maxMeshWorkGroupSizeX_NV = 32;
	defaultResource.maxMeshWorkGroupSizeY_NV = 1;
	defaultResource.maxMeshWorkGroupSizeZ_NV = 1;
	defaultResource.maxTaskWorkGroupSizeX_NV = 32;
	defaultResource.maxTaskWorkGroupSizeY_NV = 1;
	defaultResource.maxTaskWorkGroupSizeZ_NV = 1;
	defaultResource.maxMeshViewCountNV = 4;

	defaultResource.limits = {};
	defaultResource.limits.nonInductiveForLoops = true;
	defaultResource.limits.whileLoops = true;
	defaultResource.limits.doWhileLoops = true;
	defaultResource.limits.generalUniformIndexing = true;
	defaultResource.limits.generalAttributeMatrixVectorIndexing = true;
	defaultResource.limits.generalVaryingIndexing = true;
	defaultResource.limits.generalSamplerIndexing = true;
	defaultResource.limits.generalVariableIndexing = true;
	defaultResource.limits.generalConstantMatrixVectorIndexing = true;
	return defaultResource;
};

void RendererShaderMtl::FinalizeGlslangIfNeeded()
{
    if (s_glslangInitialized)
    {
        glslang::FinalizeProcess();
        s_glslangInitialized = false;
    }
}

RendererShaderMtl::RendererShaderMtl(MetalRenderer* mtlRenderer, ShaderType type, uint64 baseHash, uint64 auxHash, bool isGameShader, bool isGfxPackShader, const std::string& mslCode)
	: RendererShader(type, baseHash, auxHash, isGameShader, isGfxPackShader), m_mtlr{mtlRenderer}
{
    NS::String* mslCodeNS;

    // Compile Vulkan GLSL to SPIR-V and then to MSL in case of gfx pack shaders
    if (isGfxPackShader)
        mslCodeNS = ToNSString(TranslateGlslToMsl(mslCode));
    else
        mslCodeNS = ToNSString(mslCode);

    NS::Error* error = nullptr;
	MTL::Library* library = m_mtlr->GetDevice()->newLibrary(mslCodeNS, nullptr, &error);
	if (error)
    {
        printf("failed to create library (error: %s) -> source:\n%s\n", error->localizedDescription()->utf8String(), mslCodeNS->utf8String());
        error->release();
        return;
    }
    m_function = library->newFunction(ToNSString("main0"));
    library->release();

	// Count shader compilation
	g_compiled_shaders_total++;
}

RendererShaderMtl::~RendererShaderMtl()
{
    if (m_function)
        m_function->release();
}

std::string RendererShaderMtl::TranslateGlslToMsl(const std::string& glslCode)
{
    // Initialize glslang
    if (!s_glslangInitialized)
    {
        glslang::InitializeProcess();
        s_glslangInitialized = true;
    }

    // TODO: make a setting for this?
#ifdef CEMU_DEBUG_ASSERT
    bool debugInfo = true;
#else
    bool debugInfo = false;
#endif

    EShLanguage state;
	switch (GetType())
	{
	case ShaderType::kVertex:
		state = EShLangVertex;
		break;
	case ShaderType::kFragment:
		state = EShLangFragment;
		break;
	case ShaderType::kGeometry:
		state = EShLangGeometry;
		break;
	default:
		cemu_assert_debug(false);
	}

	glslang::TShader Shader(state);
	const char* cstr = glslCode.c_str();
	Shader.setStrings(&cstr, 1);
	Shader.setEnvInput(glslang::EShSourceGlsl, state, glslang::EShClientVulkan, 100);
	Shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetClientVersion::EShTargetVulkan_1_1);

	Shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetLanguageVersion::EShTargetSpv_1_3);

	TBuiltInResource Resources = GetDefaultBuiltInResource();
	std::string PreprocessedGLSL;

	EShMessages messagesPreprocess;
	messagesPreprocess = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
    if (debugInfo)
        messagesPreprocess = (EShMessages)(messagesPreprocess | EShMsgDebugInfo);

	glslang::TShader::ForbidIncluder Includer;
	if (!Shader.preprocess(&Resources, 450, ENoProfile, false, false, messagesPreprocess, &PreprocessedGLSL, Includer))
	{
		cemuLog_log(LogType::Force, fmt::format("GLSL Preprocessing Failed For {:016x}_{:016x}: \"{}\"", m_baseHash, m_auxHash, Shader.getInfoLog()));
		return "";
	}

	EShMessages messagesParseLink;
	messagesParseLink = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
    if (debugInfo)
        messagesParseLink = (EShMessages)(messagesParseLink | EShMsgDebugInfo);

	const char* PreprocessedCStr = PreprocessedGLSL.c_str();
	Shader.setStrings(&PreprocessedCStr, 1);
	if (!Shader.parse(&Resources, 100, false, messagesParseLink))
	{
		cemuLog_log(LogType::Force, fmt::format("GLSL parsing failed for {:016x}_{:016x}: \"{}\"", m_baseHash, m_auxHash, Shader.getInfoLog()));
		cemuLog_logDebug(LogType::Force, "GLSL source:\n{}", glslCode);
		cemu_assert_debug(false);
		return "";
	}

	glslang::TProgram Program;
	Program.addShader(&Shader);

	if (!Program.link(messagesParseLink))
	{
		cemuLog_log(LogType::Force, fmt::format("GLSL linking failed for {:016x}_{:016x}: \"{}\"", m_baseHash, m_auxHash, Program.getInfoLog()));
		cemu_assert_debug(false);
		return "";
	}

	if (!Program.mapIO())
	{
		cemuLog_log(LogType::Force, fmt::format("GLSL linking failed for {:016x}_{:016x}: \"{}\"", m_baseHash, m_auxHash, Program.getInfoLog()));
		return "";
	}

	// temp storage for SPIR-V after translation
	std::vector<uint32> spirvBuffer;
	spv::SpvBuildLogger logger;

	glslang::SpvOptions spvOptions;
	spvOptions.disableOptimizer = false;
	spvOptions.generateDebugInfo = debugInfo;
	spvOptions.validate = false;
	spvOptions.optimizeSize = true;

	GlslangToSpv(*Program.getIntermediate(state), spirvBuffer, &logger, &spvOptions);

	return "SUCCESS";
}
