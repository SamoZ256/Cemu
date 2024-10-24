#include "Cafe/HW/Latte/Renderer/Metal/MetalBinaryArchive.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"

#include "config/ActiveSettings.h"

MetalBinaryArchive::MetalBinaryArchive(class MetalRenderer* metalRenderer) : m_mtlr{metalRenderer}
{
    const char* deviceNameSrc = m_mtlr->GetDevice()->name()->utf8String();
    std::string deviceName;
    deviceName.assign(deviceNameSrc);

    // Replace spaces with underscores
    for (auto& c : deviceName)
    {
        if (c == ' ')
            c = '_';
    }

    // OS version
    auto osVersion = NS::ProcessInfo::processInfo()->operatingSystemVersion();

    // Precompiled binaries cannot be shared between different devices or OS versions
    m_archiveDir = ActiveSettings::GetCachePath("shaderCache/precompiled/{}/{}-{}-{}/", deviceName, osVersion.majorVersion, osVersion.minorVersion, osVersion.patchVersion);
    fs::create_directories(m_archiveDir);

    // Directory for temporary archives
    m_tmpArchiveDir = m_archiveDir / "tmp";
    fs::create_directories(m_tmpArchiveDir);
}

MetalBinaryArchive::~MetalBinaryArchive()
{
    SerializeOldSaveArchive();

    // TODO: combine new archives into a single one
}

void MetalBinaryArchive::SetTitleId(uint64 titleId)
{
    m_titleId = titleId;

    const std::string archiveFilename = fmt::format("{:016x}_mtl_pipelines.bin", titleId);
	m_finalArchivePath = m_archiveDir / archiveFilename;
}

void MetalBinaryArchive::LoadSerializedArchive()
{
    m_isLoading = true;

    if (!std::filesystem::exists(m_finalArchivePath))
    {
        CreateSaveArchive();
        return;
    }

    // Load the binary archive
    MTL::BinaryArchiveDescriptor* desc = MTL::BinaryArchiveDescriptor::alloc()->init();
    NS::URL* url = ToNSURL(m_finalArchivePath);
    desc->setUrl(url);
    url->release();

    NS::Error* error = nullptr;
    m_loadArchive = m_mtlr->GetDevice()->newBinaryArchive(desc, &error);
    if (error)
    {
        cemuLog_log(LogType::Force, "failed to load binary archive: {}", error->localizedDescription()->utf8String());
        error->release();
    }
    desc->release();

    // Create an array for the archive
    NS::Object* binArchives[] = {m_loadArchive};
    m_loadArchiveArray = NS::Array::alloc()->init(binArchives, 1);
}

void MetalBinaryArchive::CloseSerializedArchive()
{
    if (m_loadArchive)
        m_loadArchive->release();
    if (m_loadArchiveArray)
        m_loadArchiveArray->release();

    if (m_saveArchive)
    {
        SerializeArchive(m_saveArchive, m_finalArchivePath);
        m_saveArchive = nullptr;
    }

    m_isLoading = false;
}

void MetalBinaryArchive::LoadPipeline(MTL::RenderPipelineDescriptor* renderPipelineDescriptor) {
    if (m_loadArchiveArray)
        renderPipelineDescriptor->setBinaryArchives(m_loadArchiveArray);
}

// TODO: should be available since macOS 15.0
/*
void MetalBinaryArchive::LoadPipeline(MTL::MeshRenderPipelineDescriptor* renderPipelineDescriptor) {
    if (m_loadArchiveArray)
        renderPipelineDescriptor->setBinaryArchives(m_loadArchiveArray);
}
*/

void MetalBinaryArchive::SavePipeline(MTL::RenderPipelineDescriptor* renderPipelineDescriptor) {
    LoadSaveArchive();

    if (m_saveArchive)
    {
        NS::Error* error = nullptr;
        m_saveArchive->addRenderPipelineFunctions(renderPipelineDescriptor, &error);
        if (error)
        {
            cemuLog_log(LogType::Force, "error saving render pipeline functions: {}", error->localizedDescription()->utf8String());
            error->release();
        }
        m_pipelinesSerialized++;
    }
}

// TODO: should be available since macOS 15.0
/*
void MetalBinaryArchive::SavePipeline(MTL::MeshRenderPipelineDescriptor* renderPipelineDescriptor) {
    LoadSaveArchive();

    if (m_saveArchive)
    {
        NS::Error* error = nullptr;
        m_saveArchive->addMeshRenderPipelineFunctions(renderPipelineDescriptor, &error);
        if (error)
        {
            cemuLog_log(LogType::Force, "error saving mesh pipeline functions: {}", error->localizedDescription()->utf8String());
            error->release();
        }
        m_pipelinesSerialized++;
    }
}
*/

void MetalBinaryArchive::SerializeArchive(MTL::BinaryArchive* archive, const fs::path& path)
{
    NS::Error* error = nullptr;
    NS::URL* url = ToNSURL(path);
    archive->serializeToURL(url, &error);
    url->release();
    if (error)
    {
        cemuLog_log(LogType::Force, "failed to serialize binary archive: {}", error->localizedDescription()->utf8String());
        error->release();
    }
    archive->release();
}

void MetalBinaryArchive::SerializeOldSaveArchive()
{
    if (!m_saveArchive)
        return;

    SerializeArchive(m_saveArchive, GetTmpArchivePath(m_archiveIndex));

    m_archiveIndex++;
    m_pipelinesSerialized = 0;
}

void MetalBinaryArchive::CreateSaveArchive()
{
    MTL::BinaryArchiveDescriptor* desc = MTL::BinaryArchiveDescriptor::alloc()->init();

    NS::Error* error = nullptr;
    m_saveArchive = m_mtlr->GetDevice()->newBinaryArchive(desc, &error);
    if (error)
    {
        cemuLog_log(LogType::Force, "failed to create save binary archive: {}", error->localizedDescription()->utf8String());
        error->release();
    }
    desc->release();
}

void MetalBinaryArchive::LoadSaveArchive()
{
    if (m_isLoading || (m_saveArchive && m_pipelinesSerialized < SERIALIZE_TRESHOLD))
        return;

    // First, save the old archive to disk
    SerializeOldSaveArchive();

    // Create a new archive
    CreateSaveArchive();
}
