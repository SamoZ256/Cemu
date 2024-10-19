#include "Cafe/HW/Latte/Renderer/Metal/MetalPipelineCache.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/CachedFBOMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.h"

#include "Cafe/HW/Latte/Core/FetchShader.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/Core/LatteCachedFBO.h"
#include "Cafe/HW/Latte/Common/RegisterSerializer.h"
#include "Cafe/HW/Latte/Core/LatteShaderCache.h"
#include "Cemu/FileCache/FileCache.h"
#include "Common/precompiled.h"
#include "HW/Latte/Core/LatteShader.h"
#include "HW/Latte/ISA/LatteReg.h"
#include "HW/Latte/Renderer/Metal/LatteToMtl.h"
#include "HW/Latte/Renderer/Metal/MetalAttachmentsInfo.h"
#include "Metal/MTLRenderPipeline.hpp"
#include "util/helpers/helpers.h"
#include "config/ActiveSettings.h"
#include <openssl/sha.h>

MetalPipelineCache* g_mtlPipelineCache = nullptr;

MetalPipelineCache& MetalPipelineCache::GetInstance()
{
    return *g_mtlPipelineCache;
}

MetalPipelineCache::MetalPipelineCache(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer}
{
    g_mtlPipelineCache = this;
}

MetalPipelineCache::~MetalPipelineCache()
{
    for (auto& [key, value] : m_pipelineCache)
    {
        value->release();
    }
}

MTL::RenderPipelineState* MetalPipelineCache::GetRenderPipelineState(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, const MetalAttachmentsInfo& lastUsedAttachmentsInfo, const MetalAttachmentsInfo& activeAttachmentsInfo, const LatteContextRegister& lcr)
{
    uint64 hash = CalculatePipelineHash(fetchShader, vertexShader, geometryShader, pixelShader, lastUsedAttachmentsInfo, activeAttachmentsInfo, lcr);
    auto it = m_pipelineCache.find(hash);
    if (it != m_pipelineCache.end())
        return it->second;

    MetalPipelineCompiler compiler(m_mtlr);
    bool fbosMatch;
    compiler.InitFromState(fetchShader, vertexShader, geometryShader, pixelShader, lastUsedAttachmentsInfo, activeAttachmentsInfo, lcr, fbosMatch);
    MTL::RenderPipelineState* pipeline = compiler.Compile(false, true, true);

    // If FBOs don't match, it wouldn't be possible to reconstruct the pipeline from the cache
    if (fbosMatch)
        AddCurrentStateToCache(hash);

    m_pipelineCache.insert({hash, pipeline});

    return pipeline;
}

uint64 MetalPipelineCache::CalculatePipelineHash(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, const MetalAttachmentsInfo& lastUsedAttachmentsInfo, const MetalAttachmentsInfo& activeAttachmentsInfo, const LatteContextRegister& lcr)
{
    // Hash
    uint64 stateHash = 0;
    for (int i = 0; i < Latte::GPU_LIMITS::NUM_COLOR_ATTACHMENTS; ++i)
	{
	    Latte::E_GX2SURFFMT format = lastUsedAttachmentsInfo.colorFormats[i];
		if (format == Latte::E_GX2SURFFMT::INVALID_FORMAT)
            continue;

		stateHash += GetMtlPixelFormat(format, false) + i * 31;
		stateHash = std::rotl<uint64>(stateHash, 7);

		if (activeAttachmentsInfo.colorFormats[i] == Latte::E_GX2SURFFMT::INVALID_FORMAT)
		{
            stateHash += 1;
		    stateHash = std::rotl<uint64>(stateHash, 1);
		}
	}

	if (lastUsedAttachmentsInfo.depthFormat != Latte::E_GX2SURFFMT::INVALID_FORMAT)
	{
		stateHash += GetMtlPixelFormat(lastUsedAttachmentsInfo.depthFormat, true);
		stateHash = std::rotl<uint64>(stateHash, 7);

		if (activeAttachmentsInfo.depthFormat == Latte::E_GX2SURFFMT::INVALID_FORMAT)
		{
            stateHash += 1;
		    stateHash = std::rotl<uint64>(stateHash, 1);
		}
	}

	for (auto& group : fetchShader->bufferGroups)
	{
		uint32 bufferStride = group.getCurrentBufferStride(lcr.GetRawView());
		stateHash = std::rotl<uint64>(stateHash, 7);
		stateHash += bufferStride * 3;
	}

	stateHash += fetchShader->getVkPipelineHashFragment();
	stateHash = std::rotl<uint64>(stateHash, 7);

	stateHash += lcr.GetRawView()[mmVGT_STRMOUT_EN];
	stateHash = std::rotl<uint64>(stateHash, 7);

	if(lcr.PA_CL_CLIP_CNTL.get_DX_RASTERIZATION_KILL())
		stateHash += 0x333333;

	stateHash = (stateHash >> 8) + (stateHash * 0x370531ull) % 0x7F980D3BF9B4639Dull;

	uint32* ctxRegister = lcr.GetRawView();

	if (vertexShader)
		stateHash += vertexShader->baseHash;

	stateHash = std::rotl<uint64>(stateHash, 13);

	if (pixelShader)
		stateHash += pixelShader->baseHash + pixelShader->auxHash;

	stateHash = std::rotl<uint64>(stateHash, 13);

	uint32 polygonCtrl = lcr.PA_SU_SC_MODE_CNTL.getRawValue();
	stateHash += polygonCtrl;
	stateHash = std::rotl<uint64>(stateHash, 7);

	stateHash += ctxRegister[Latte::REGADDR::PA_CL_CLIP_CNTL];
	stateHash = std::rotl<uint64>(stateHash, 7);

	const auto colorControlReg = ctxRegister[Latte::REGADDR::CB_COLOR_CONTROL];
	stateHash += colorControlReg;

	stateHash += ctxRegister[Latte::REGADDR::CB_TARGET_MASK];

	const uint32 blendEnableMask = (colorControlReg >> 8) & 0xFF;
	if (blendEnableMask)
	{
		for (auto i = 0; i < 8; ++i)
		{
			if (((blendEnableMask & (1 << i))) == 0)
				continue;
			stateHash = std::rotl<uint64>(stateHash, 7);
			stateHash += ctxRegister[Latte::REGADDR::CB_BLEND0_CONTROL + i];
		}
	}

	// Mesh pipeline
	const LattePrimitiveMode primitiveMode = static_cast<LattePrimitiveMode>(LatteGPUState.contextRegister[mmVGT_PRIMITIVE_TYPE]);
    bool isPrimitiveRect = (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::RECTS);

    bool usesGeometryShader = (geometryShader != nullptr || isPrimitiveRect);

    if (usesGeometryShader)
    {
        stateHash += lcr.GetRawView()[mmVGT_PRIMITIVE_TYPE];
        stateHash = std::rotl<uint64>(stateHash, 7);
    }

	return stateHash;
}

struct
{
	uint32 pipelineLoadIndex;
	uint32 pipelineMaxFileIndex;

	std::atomic_uint32_t pipelinesQueued;
	std::atomic_uint32_t pipelinesLoaded;
} g_mtlCacheState;

uint32 MetalPipelineCache::BeginLoading(uint64 cacheTitleId)
{
	std::error_code ec;
	fs::create_directories(ActiveSettings::GetCachePath("shaderCache/transferable"), ec);
	const auto pathCacheFile = ActiveSettings::GetCachePath("shaderCache/transferable/{:016x}_mtlpipeline.bin", cacheTitleId);

	// init cache loader state
	g_mtlCacheState.pipelineLoadIndex = 0;
	g_mtlCacheState.pipelineMaxFileIndex = 0;
	g_mtlCacheState.pipelinesLoaded = 0;
	g_mtlCacheState.pipelinesQueued = 0;

	// start async compilation threads
	m_compilationCount.store(0);
	m_compilationQueue.clear();

	// get core count
	uint32 cpuCoreCount = GetPhysicalCoreCount();
	m_numCompilationThreads = std::clamp(cpuCoreCount, 1u, 8u);
	// TODO: uncomment?
	//if (VulkanRenderer::GetInstance()->GetDisableMultithreadedCompilation())
	//	m_numCompilationThreads = 1;

	for (uint32 i = 0; i < m_numCompilationThreads; i++)
	{
		std::thread compileThread(&MetalPipelineCache::CompilerThread, this);
		compileThread.detach();
	}

	// open cache file or create it
	cemu_assert_debug(s_cache == nullptr);
	s_cache = FileCache::Open(pathCacheFile, true, LatteShaderCache_getPipelineCacheExtraVersion(cacheTitleId));
	if (!s_cache)
	{
		cemuLog_log(LogType::Force, "Failed to open or create Metal pipeline cache file: {}", _pathToUtf8(pathCacheFile));
		return 0;
	}
	else
	{
		s_cache->UseCompression(false);
		g_mtlCacheState.pipelineMaxFileIndex = s_cache->GetMaximumFileIndex();
	}
	return s_cache->GetFileCount();
}

bool MetalPipelineCache::UpdateLoading(uint32& pipelinesLoadedTotal, uint32& pipelinesMissingShaders)
{
	pipelinesLoadedTotal = g_mtlCacheState.pipelinesLoaded;
	pipelinesMissingShaders = 0;
	while (g_mtlCacheState.pipelineLoadIndex <= g_mtlCacheState.pipelineMaxFileIndex)
	{
		if (m_compilationQueue.size() >= 50)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			return true; // queue up to 50 entries at a time
		}

		uint64 fileNameA, fileNameB;
		std::vector<uint8> fileData;
		if (s_cache->GetFileByIndex(g_mtlCacheState.pipelineLoadIndex, &fileNameA, &fileNameB, fileData))
		{
			// queue for async compilation
			g_mtlCacheState.pipelinesQueued++;
			m_compilationQueue.push(std::move(fileData));
			g_mtlCacheState.pipelineLoadIndex++;
			return true;
		}
		g_mtlCacheState.pipelineLoadIndex++;
	}
	if (g_mtlCacheState.pipelinesLoaded != g_mtlCacheState.pipelinesQueued)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		return true; // pipelines still compiling
	}
	return false; // done
}

void MetalPipelineCache::EndLoading()
{
	// shut down compilation threads
	uint32 threadCount = m_numCompilationThreads;
	m_numCompilationThreads = 0; // signal thread shutdown
	for (uint32 i = 0; i < threadCount; i++)
	{
		m_compilationQueue.push({}); // push empty workload for every thread. Threads then will shutdown after checking for m_numCompilationThreads == 0
	}
	// keep cache file open for writing of new pipelines
}

void MetalPipelineCache::Close()
{
    if(s_cache)
    {
        delete s_cache;
        s_cache = nullptr;
    }
}

struct CachedPipeline
{
	struct ShaderHash
	{
		uint64 baseHash;
		uint64 auxHash;
		bool isPresent{};

		void set(uint64 baseHash, uint64 auxHash)
		{
			this->baseHash = baseHash;
			this->auxHash = auxHash;
			this->isPresent = true;
		}
	};

	ShaderHash vsHash; // includes fetch shader
	ShaderHash gsHash;
	ShaderHash psHash;

	Latte::GPUCompactedRegisterState gpuState;
};

void MetalPipelineCache::LoadPipelineFromCache(std::span<uint8> fileData)
{
	static FSpinlock s_spinlockSharedInternal;

	// deserialize file
	LatteContextRegister* lcr = new LatteContextRegister();
	s_spinlockSharedInternal.lock();
	CachedPipeline* cachedPipeline = new CachedPipeline();
	s_spinlockSharedInternal.unlock();

	MemStreamReader streamReader(fileData.data(), fileData.size());
	if (!DeserializePipeline(streamReader, *cachedPipeline))
	{
		// failed to deserialize
		s_spinlockSharedInternal.lock();
		delete lcr;
		delete cachedPipeline;
		s_spinlockSharedInternal.unlock();
		return;
	}
	// restored register view from compacted state
	Latte::LoadGPURegisterState(*lcr, cachedPipeline->gpuState);

	LatteDecompilerShader* vertexShader = nullptr;
	LatteDecompilerShader* geometryShader = nullptr;
	LatteDecompilerShader* pixelShader = nullptr;
	// find vertex shader
	if (cachedPipeline->vsHash.isPresent)
	{
		vertexShader = LatteSHRC_FindVertexShader(cachedPipeline->vsHash.baseHash, cachedPipeline->vsHash.auxHash);
		if (!vertexShader)
		{
			cemuLog_logDebug(LogType::Force, "Vertex shader not found in cache");
			return;
		}
	}
	// find geometry shader
	if (cachedPipeline->gsHash.isPresent)
	{
		geometryShader = LatteSHRC_FindGeometryShader(cachedPipeline->gsHash.baseHash, cachedPipeline->gsHash.auxHash);
		if (!geometryShader)
		{
			cemuLog_logDebug(LogType::Force, "Geometry shader not found in cache");
			return;
		}
	}
	// find pixel shader
	if (cachedPipeline->psHash.isPresent)
	{
		pixelShader = LatteSHRC_FindPixelShader(cachedPipeline->psHash.baseHash, cachedPipeline->psHash.auxHash);
		if (!pixelShader)
		{
			cemuLog_logDebug(LogType::Force, "Pixel shader not found in cache");
			return;
		}
	}

	if (!pixelShader)
	{
		cemu_assert_debug(false);
		return;
	}

	MetalAttachmentsInfo attachmentsInfo(*lcr, pixelShader);

	// TODO: this shouldn't probably be called directly
	LatteShader_UpdatePSInputs(lcr->GetRawView());

	MTL::RenderPipelineState* pipeline = nullptr;
	// compile
	{
		MetalPipelineCompiler pp(m_mtlr);
		bool fbosMatch;
		pp.InitFromState(vertexShader->compatibleFetchShader, vertexShader, geometryShader, pixelShader, attachmentsInfo, attachmentsInfo, *lcr, fbosMatch);
		cemu_assert_debug(fbosMatch);
		//{
		//	s_spinlockSharedInternal.lock();
		//	delete lcr;
		//	delete cachedPipeline;
		//	s_spinlockSharedInternal.unlock();
		//	return;
		//}
		pipeline = pp.Compile(true, true, false);
		// destroy pp early
	}

	// on success, calculate pipeline hash and flag as present in cache
	if (pipeline)
	{
    	uint64 pipelineStateHash = CalculatePipelineHash(vertexShader->compatibleFetchShader, vertexShader, geometryShader, pixelShader, attachmentsInfo, attachmentsInfo, *lcr);
    	m_pipelineCacheLock.lock();
    	m_pipelineCache[pipelineStateHash] = pipeline;
    	m_pipelineCacheLock.unlock();
	}

	// clean up
	s_spinlockSharedInternal.lock();
	delete lcr;
	delete cachedPipeline;
	s_spinlockSharedInternal.unlock();
}

ConcurrentQueue<CachedPipeline*> g_mtlPipelineCachingQueue;

void MetalPipelineCache::AddCurrentStateToCache(uint64 pipelineStateHash)
{
	if (!m_pipelineCacheStoreThread)
	{
		m_pipelineCacheStoreThread = new std::thread(&MetalPipelineCache::WorkerThread, this);
		m_pipelineCacheStoreThread->detach();
	}
	// fill job structure with cached GPU state
	// for each cached pipeline we store:
	// - Active shaders (referenced by hash)
	// - An almost-complete register state of the GPU (minus some ALU uniform constants which aren't relevant)
	CachedPipeline* job = new CachedPipeline();
	auto vs = LatteSHRC_GetActiveVertexShader();
	auto gs = LatteSHRC_GetActiveGeometryShader();
	auto ps = LatteSHRC_GetActivePixelShader();
	if (vs)
		job->vsHash.set(vs->baseHash, vs->auxHash);
	if (gs)
		job->gsHash.set(gs->baseHash, gs->auxHash);
	if (ps)
		job->psHash.set(ps->baseHash, ps->auxHash);
	Latte::StoreGPURegisterState(LatteGPUState.contextNew, job->gpuState);
	// queue job
	g_mtlPipelineCachingQueue.push(job);
}

bool MetalPipelineCache::SerializePipeline(MemStreamWriter& memWriter, CachedPipeline& cachedPipeline)
{
	memWriter.writeBE<uint8>(0x01); // version
	uint8 presentMask = 0;
	if (cachedPipeline.vsHash.isPresent)
		presentMask |= 1;
	if (cachedPipeline.gsHash.isPresent)
		presentMask |= 2;
	if (cachedPipeline.psHash.isPresent)
		presentMask |= 4;
	memWriter.writeBE<uint8>(presentMask);
	if (cachedPipeline.vsHash.isPresent)
	{
		memWriter.writeBE<uint64>(cachedPipeline.vsHash.baseHash);
		memWriter.writeBE<uint64>(cachedPipeline.vsHash.auxHash);
	}
	if (cachedPipeline.gsHash.isPresent)
	{
		memWriter.writeBE<uint64>(cachedPipeline.gsHash.baseHash);
		memWriter.writeBE<uint64>(cachedPipeline.gsHash.auxHash);
	}
	if (cachedPipeline.psHash.isPresent)
	{
		memWriter.writeBE<uint64>(cachedPipeline.psHash.baseHash);
		memWriter.writeBE<uint64>(cachedPipeline.psHash.auxHash);
	}
	Latte::SerializeRegisterState(cachedPipeline.gpuState, memWriter);
	return true;
}

bool MetalPipelineCache::DeserializePipeline(MemStreamReader& memReader, CachedPipeline& cachedPipeline)
{
	// version
	if (memReader.readBE<uint8>() != 1)
	{
		cemuLog_log(LogType::Force, "Cached Metal pipeline corrupted or has unknown version");
		return false;
	}
	// shader hashes
	uint8 presentMask = memReader.readBE<uint8>();
	if (presentMask & 1)
	{
		uint64 baseHash = memReader.readBE<uint64>();
		uint64 auxHash = memReader.readBE<uint64>();
		cachedPipeline.vsHash.set(baseHash, auxHash);
	}
	if (presentMask & 2)
	{
		uint64 baseHash = memReader.readBE<uint64>();
		uint64 auxHash = memReader.readBE<uint64>();
		cachedPipeline.gsHash.set(baseHash, auxHash);
	}
	if (presentMask & 4)
	{
		uint64 baseHash = memReader.readBE<uint64>();
		uint64 auxHash = memReader.readBE<uint64>();
		cachedPipeline.psHash.set(baseHash, auxHash);
	}
	// deserialize GPU state
	if (!Latte::DeserializeRegisterState(cachedPipeline.gpuState, memReader))
	{
		return false;
	}
	cemu_assert_debug(!memReader.hasError());
	return true;
}

int MetalPipelineCache::CompilerThread()
{
	SetThreadName("plCacheCompiler");
	while (m_numCompilationThreads != 0)
	{
		std::vector<uint8> pipelineData = m_compilationQueue.pop();
		if(pipelineData.empty())
			continue;
		LoadPipelineFromCache(pipelineData);
		++g_mtlCacheState.pipelinesLoaded;
	}
	return 0;
}

void MetalPipelineCache::WorkerThread()
{
	SetThreadName("plCacheWriter");
	while (true)
	{
		CachedPipeline* job;
		g_mtlPipelineCachingQueue.pop(job);
		if (!s_cache)
		{
			delete job;
			continue;
		}
		// serialize
		MemStreamWriter memWriter(1024 * 4);
		SerializePipeline(memWriter, *job);
		auto blob = memWriter.getResult();
		// file name is derived from data hash
		uint8 hash[SHA256_DIGEST_LENGTH];
		SHA256(blob.data(), blob.size(), hash);
		uint64 nameA = *(uint64be*)(hash + 0);
		uint64 nameB = *(uint64be*)(hash + 8);
		s_cache->AddFileAsync({ nameA, nameB }, blob.data(), blob.size());
		delete job;
	}
}
