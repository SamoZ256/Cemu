#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"

class MetalBinaryArchive
{
public:
    static constexpr uint32 SERIALIZE_TRESHOLD = 64;

    MetalBinaryArchive(class MetalRenderer* metalRenderer);
    ~MetalBinaryArchive();

    void SetTitleId(uint64 titleId);

    void LoadSerializedArchive();
    void CloseSerializedArchive();

    // For pipeline compiler
    void LoadPipeline(MTL::RenderPipelineDescriptor* renderPipelineDescriptor);
    //void LoadPipeline(MTL::MeshRenderPipelineDescriptor* renderPipelineDescriptor);
    void SavePipeline(MTL::RenderPipelineDescriptor* renderPipelineDescriptor);
    //void SavePipeline(MTL::MeshRenderPipelineDescriptor* renderPipelineDescriptor);

private:
    class MetalRenderer* m_mtlr;

    uint64 m_titleId;

    bool m_isLoading = false;

    // Paths
    fs::path m_archiveDir;
    fs::path m_tmpArchiveDir;
    fs::path m_finalArchivePath;

    // Binary archives
    MTL::BinaryArchive* m_loadArchive = nullptr;
    NS::Array* m_loadArchiveArray = nullptr;
    MTL::BinaryArchive* m_saveArchive = nullptr;

    uint32 m_pipelinesSerialized = 0;
    uint32 m_archiveIndex = 0;

    void SerializeOldArchive();
    void createSaveArchive();
    void LoadSaveArchive();

    fs::path GetTmpArchivePath(uint32 index)
    {
        return m_tmpArchiveDir / fs::path("archive" + std::to_string(index) + ".bin");
    }
};
