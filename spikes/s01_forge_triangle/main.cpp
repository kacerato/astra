// Spike S-01 (docs/plano-mestre/22-ROADMAP-FASES-E-GATES.md §1):
// O The Forge 1.63 (Vulkan) compila por CMake + NDK r28+ com páginas de 16 KB e roda no aparelho e no host?
//
// Aceite: triângulo + textura carregada pelo Resource Loader (arquivo KTX) + shader FSL, no Android e no
// Windows, sem erro da camada de validação. Código descartável: usa o IApp do TF de propósito (o S-02 é que
// testa o loop próprio com GameActivity).

#include "Common_3/Application/Interfaces/IApp.h"
#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "Common_3/Utilities/Interfaces/IFileSystem.h"
#include "Common_3/Utilities/Interfaces/ILog.h"
#include "Common_3/Utilities/Interfaces/ITime.h"
#include "Common_3/Utilities/Math/MathTypes.h"
#include "Common_3/Utilities/RingBuffer.h"

#include "Common_3/Graphics/FSL/defaults.h"
#include "shaders/Global.srt.h"

#include "Common_3/Utilities/Interfaces/IMemory.h"

namespace
{
constexpr uint32_t kDataBufferCount = 2;

// Mesma ordem do VSInput em triangle.vert.fsl: posição (float2) + UV (float2).
struct Vertex
{
    float x, y;
    float u, v;
};

constexpr Vertex kTriangle[] = {
    { 0.0f, 0.75f, 0.5f, 0.0f },
    { -0.65f, -0.5f, 0.0f, 1.0f },
    { 0.65f, -0.5f, 1.0f, 1.0f },
};

// Mesmo layout de FrameData em resources.h.fsl.
struct FrameData
{
    mat4 transform;
    vec4 tint;
};

Renderer*      gRenderer = nullptr;
Queue*         gGraphicsQueue = nullptr;
GpuCmdRing     gCmdRing = {};
SwapChain*     gSwapChain = nullptr;
Semaphore*     gImageAcquired = nullptr;
Shader*        gShader = nullptr;
Pipeline*      gPipeline = nullptr;
Buffer*        gVertexBuffer = nullptr;
Buffer*        gFrameBuffers[kDataBufferCount] = {};
Texture*       gTexture = nullptr;
Sampler*       gSampler = nullptr;
DescriptorSet* gSetPersistent = nullptr;
DescriptorSet* gSetPerFrame = nullptr;

uint32_t  gFrameIndex = 0;
uint64_t  gFramesPresented = 0;
float     gAngle = 0.0f;
float     gSecondsSinceReport = 0.0f;
uint64_t  gFramesAtLastReport = 0;
FrameData gFrame = {};
} // namespace

class AstraSpikeS01 final : public IApp
{
public:
    const char* GetName() override { return "AstraSpikeS01"; }

    bool Init() override
    {
        RendererDesc settings = {};
        initGPUConfiguration(settings.pExtendedSettings);
        initRenderer(GetName(), &settings, &gRenderer);
        if (!gRenderer)
        {
            ShowUnsupportedMessage(getUnsupportedGPUMsg());
            return false;
        }
        setupGPUConfigurationPlatformParameters(gRenderer, settings.pExtendedSettings);
        LOGF(eINFO, "S01: GPU '%s' (vendor 0x%x, model 0x%x)", gRenderer->pGpu->mGpuVendorPreset.mGpuName,
             gRenderer->pGpu->mGpuVendorPreset.mVendorId, gRenderer->pGpu->mGpuVendorPreset.mModelId);

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        initQueue(gRenderer, &queueDesc, &gGraphicsQueue);

        GpuCmdRingDesc ringDesc = {};
        ringDesc.pQueue = gGraphicsQueue;
        ringDesc.mPoolCount = kDataBufferCount;
        ringDesc.mCmdPerPoolCount = 1;
        ringDesc.mAddSyncPrimitives = true;
        initGpuCmdRing(gRenderer, &ringDesc, &gCmdRing);

        initSemaphore(gRenderer, &gImageAcquired);
        initResourceLoaderInterface(gRenderer);

        RootSignatureDesc rootDesc = {};
        INIT_RS_DESC(rootDesc, "default.rootsig", "compute.rootsig");
        initRootSignature(gRenderer, &rootDesc);

        SamplerDesc samplerDesc = { FILTER_LINEAR,
                                    FILTER_LINEAR,
                                    MIPMAP_MODE_LINEAR,
                                    ADDRESS_MODE_REPEAT,
                                    ADDRESS_MODE_REPEAT,
                                    ADDRESS_MODE_REPEAT };
        addSampler(gRenderer, &samplerDesc, &gSampler);

        // Textura vinda de arquivo pelo Resource Loader (KTX forçado também no Windows,
        // onde o padrão do TF seria DDS).
        TextureLoadDesc textureDesc = {};
        textureDesc.pFileName = "astra_checker.ktx"; // o RL usa o nome como veio (o comentário do header está desatualizado)
        textureDesc.mContainer = TEXTURE_CONTAINER_KTX;
        textureDesc.ppTexture = &gTexture;
        addResource(&textureDesc, nullptr);

        BufferLoadDesc vbDesc = {};
        vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        vbDesc.mDesc.mSize = sizeof(kTriangle);
        vbDesc.pData = kTriangle;
        vbDesc.ppBuffer = &gVertexBuffer;
        addResource(&vbDesc, nullptr);

        BufferLoadDesc ubDesc = {};
        ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubDesc.mDesc.mSize = sizeof(FrameData);
        ubDesc.mDesc.pName = "S01FrameData";
        for (uint32_t i = 0; i < kDataBufferCount; ++i)
        {
            ubDesc.ppBuffer = &gFrameBuffers[i];
            addResource(&ubDesc, nullptr);
        }

        waitForAllResourceLoads();
        if (!gTexture)
        {
            LOGF(eERROR, "S01: falha ao carregar astra_checker.ktx pelo Resource Loader");
            return false;
        }
        LOGF(eINFO, "S01: textura %ux%u, %u mips, formato %s", gTexture->mWidth, gTexture->mHeight, gTexture->mMipLevels,
             TinyImageFormat_Name((TinyImageFormat)gTexture->mFormat));

        gFrameIndex = 0;
        return true;
    }

    void Exit() override
    {
        for (uint32_t i = 0; i < kDataBufferCount; ++i)
            removeResource(gFrameBuffers[i]);
        removeResource(gVertexBuffer);
        removeResource(gTexture);
        removeSampler(gRenderer, gSampler);

        exitGpuCmdRing(gRenderer, &gCmdRing);
        exitSemaphore(gRenderer, gImageAcquired);
        exitRootSignature(gRenderer);
        exitResourceLoaderInterface(gRenderer);
        exitQueue(gRenderer, gGraphicsQueue);
        exitRenderer(gRenderer);
        exitGPUConfiguration();
        gRenderer = nullptr;
        LOGF(eINFO, "S01: encerrado após %llu frames apresentados", (unsigned long long)gFramesPresented);
    }

    bool Load(ReloadDesc* reload) override
    {
        LOGF(eINFO, "S01: Load(tipo 0x%x) %dx%d", (unsigned)reload->mType, mSettings.mWidth, mSettings.mHeight);
        if (reload->mType & RELOAD_TYPE_SHADER)
        {
            ShaderLoadDesc shaderDesc = {};
            shaderDesc.mVert.pFileName = "triangle.vert";
            shaderDesc.mFrag.pFileName = "triangle.frag";
            addShader(gRenderer, &shaderDesc, &gShader);
            if (!gShader)
                return false;

            DescriptorSetDesc persistent = SRT_SET_DESC(SrtData, Persistent, 1, 0);
            addDescriptorSet(gRenderer, &persistent, &gSetPersistent);
            DescriptorSetDesc perFrame = SRT_SET_DESC(SrtData, PerFrame, kDataBufferCount, 0);
            addDescriptorSet(gRenderer, &perFrame, &gSetPerFrame);
        }

        if (reload->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            if (!addSwapChain())
                return false;
        }

        if (reload->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
            addPipeline();

        DescriptorData persistentParams[2] = {};
        persistentParams[0].mIndex = SRT_RES_IDX(SrtData, Persistent, gTexture);
        persistentParams[0].ppTextures = &gTexture;
        persistentParams[1].mIndex = SRT_RES_IDX(SrtData, Persistent, gSampler);
        persistentParams[1].ppSamplers = &gSampler;
        updateDescriptorSet(gRenderer, 0, gSetPersistent, 2, persistentParams);
        for (uint32_t i = 0; i < kDataBufferCount; ++i)
        {
            DescriptorData frameParam = {};
            frameParam.mIndex = SRT_RES_IDX(SrtData, PerFrame, gFrame);
            frameParam.ppBuffers = &gFrameBuffers[i];
            updateDescriptorSet(gRenderer, i, gSetPerFrame, 1, &frameParam);
        }
        return gPipeline != nullptr;
    }

    void Unload(ReloadDesc* reload) override
    {
        waitQueueIdle(gGraphicsQueue);
        if (reload->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
            removePipeline(gRenderer, gPipeline);
        if (reload->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
            removeSwapChain(gRenderer, gSwapChain);
        if (reload->mType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSet(gRenderer, gSetPerFrame);
            removeDescriptorSet(gRenderer, gSetPersistent);
            removeShader(gRenderer, gShader);
        }
    }

    void Update(float deltaTime) override
    {
        gAngle += deltaTime * 0.6f;
        // Mantém o triângulo com proporção correta e dentro da menor dimensão (retrato ou paisagem).
        const float w = (float)(mSettings.mWidth > 0 ? mSettings.mWidth : 1);
        const float h = (float)(mSettings.mHeight > 0 ? mSettings.mHeight : 1);
        const vec3  fit = w < h ? vec3(1.0f, w / h, 1.0f) : vec3(h / w, 1.0f, 1.0f);
        gFrame.transform = mat4::scale(fit) * mat4::rotationZ(gAngle);
        gFrame.tint = vec4(1.0f, 1.0f, 1.0f, 1.0f);

        // Resumo a cada 10 s (não por frame): permite conferir no logcat que o loop segue vivo
        // depois de pausa/retomada e rotação.
        gSecondsSinceReport += deltaTime;
        if (gSecondsSinceReport >= 10.0f)
        {
            LOGF(eINFO, "S01: %llu frames em %.1f s (total %llu)", (unsigned long long)(gFramesPresented - gFramesAtLastReport),
                 gSecondsSinceReport, (unsigned long long)gFramesPresented);
            gFramesAtLastReport = gFramesPresented;
            gSecondsSinceReport = 0.0f;
        }
    }

    void Draw() override
    {
        if ((bool)gSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
        {
            waitQueueIdle(gGraphicsQueue);
            ::toggleVSync(gRenderer, &gSwapChain);
        }

        uint32_t imageIndex = 0;
        acquireNextImage(gRenderer, gSwapChain, gImageAcquired, nullptr, &imageIndex);
        RenderTarget* target = gSwapChain->ppRenderTargets[imageIndex];

        GpuCmdRingElement elem = getNextGpuCmdRingElement(&gCmdRing, true, 1);
        FenceStatus       fenceStatus;
        getFenceStatus(gRenderer, elem.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            waitForFences(gRenderer, 1, &elem.pFence);

        BufferUpdateDesc frameUpdate = { gFrameBuffers[gFrameIndex] };
        beginUpdateResource(&frameUpdate);
        memcpy(frameUpdate.pMappedData, &gFrame, sizeof(gFrame));
        endUpdateResource(&frameUpdate);

        resetCmdPool(gRenderer, elem.pCmdPool);
        Cmd* cmd = elem.pCmds[0];
        beginCmd(cmd);

        RenderTargetBarrier barrier = { target, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET };
        cmdResourceBarrier(cmd, 0, nullptr, 0, nullptr, 1, &barrier);

        BindRenderTargetsDesc bind = {};
        bind.mRenderTargetCount = 1;
        bind.mRenderTargets[0] = { target, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bind);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)target->mWidth, (float)target->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, target->mWidth, target->mHeight);

        const uint32_t stride = sizeof(Vertex);
        cmdBindPipeline(cmd, gPipeline);
        cmdBindDescriptorSet(cmd, 0, gSetPersistent);
        cmdBindDescriptorSet(cmd, gFrameIndex, gSetPerFrame);
        cmdBindVertexBuffer(cmd, 1, &gVertexBuffer, &stride, nullptr);
        cmdDraw(cmd, 3, 0);
        cmdBindRenderTargets(cmd, nullptr);

        barrier = { target, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
        cmdResourceBarrier(cmd, 0, nullptr, 0, nullptr, 1, &barrier);
        endCmd(cmd);

        FlushResourceUpdateDesc flush = {};
        flushResourceUpdates(&flush);
        Semaphore* waitSemaphores[2] = { flush.pOutSubmittedSemaphore, gImageAcquired };

        QueueSubmitDesc submit = {};
        submit.mCmdCount = 1;
        submit.ppCmds = &cmd;
        submit.mSignalSemaphoreCount = 1;
        submit.ppSignalSemaphores = &elem.pSemaphore;
        submit.mWaitSemaphoreCount = TF_ARRAY_COUNT(waitSemaphores);
        submit.ppWaitSemaphores = waitSemaphores;
        submit.pSignalFence = elem.pFence;
        queueSubmit(gGraphicsQueue, &submit);

        QueuePresentDesc present = {};
        present.mIndex = (uint8_t)imageIndex;
        present.mWaitSemaphoreCount = 1;
        present.ppWaitSemaphores = &elem.pSemaphore;
        present.pSwapChain = gSwapChain;
        present.mSubmitDone = true;
        queuePresent(gGraphicsQueue, &present);

        if (gFramesPresented == 0)
            LOGF(eINFO, "S01: primeiro frame apresentado (%ux%u)", target->mWidth, target->mHeight);
        ++gFramesPresented;
        gFrameIndex = (gFrameIndex + 1) % kDataBufferCount;
    }

private:
    bool addSwapChain()
    {
        SwapChainDesc desc = {};
        desc.mWindowHandle = pWindow->handle;
        desc.mPresentQueueCount = 1;
        desc.ppPresentQueues = &gGraphicsQueue;
        desc.mWidth = mSettings.mWidth;
        desc.mHeight = mSettings.mHeight;
        desc.mImageCount = getRecommendedSwapchainImageCount(gRenderer, &pWindow->handle);
        desc.mColorFormat = getSupportedSwapchainFormat(gRenderer, &desc, COLOR_SPACE_SDR_SRGB);
        desc.mColorSpace = COLOR_SPACE_SDR_SRGB;
        desc.mEnableVsync = mSettings.mVSyncEnabled;
        // bg.canvas #0B0C0E da marca, em valor linear.
        desc.mColorClearValue = { { 0.0033f, 0.0037f, 0.0044f, 1.0f } };
        ::addSwapChain(gRenderer, &desc, &gSwapChain);
        if (gSwapChain)
            LOGF(eINFO, "S01: swapchain %ux%u, %u imagens, formato %s", desc.mWidth, desc.mHeight, desc.mImageCount,
                 TinyImageFormat_Name(desc.mColorFormat));
        return gSwapChain != nullptr;
    }

    void addPipeline()
    {
        VertexLayout layout = {};
        layout.mBindingCount = 1;
        layout.mBindings[0].mStride = sizeof(Vertex);
        layout.mAttribCount = 2;
        layout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        layout.mAttribs[0].mFormat = TinyImageFormat_R32G32_SFLOAT;
        layout.mAttribs[0].mLocation = 0;
        layout.mAttribs[0].mOffset = 0;
        layout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
        layout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
        layout.mAttribs[1].mLocation = 1;
        layout.mAttribs[1].mOffset = offsetof(Vertex, u);

        RasterizerStateDesc raster = {};
        raster.mCullMode = CULL_MODE_NONE;

        PipelineDesc desc = {};
        desc.mType = PIPELINE_TYPE_GRAPHICS;
        PIPELINE_LAYOUT_DESC(desc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame), NULL, NULL);
        GraphicsPipelineDesc& gfx = desc.mGraphicsDesc;
        gfx.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        gfx.mRenderTargetCount = 1;
        gfx.pColorFormats = &gSwapChain->ppRenderTargets[0]->mFormat;
        gfx.mSampleCount = gSwapChain->ppRenderTargets[0]->mSampleCount;
        gfx.mSampleQuality = gSwapChain->ppRenderTargets[0]->mSampleQuality;
        gfx.pShaderProgram = gShader;
        gfx.pVertexLayout = &layout;
        gfx.pRasterizerState = &raster;
        ::addPipeline(gRenderer, &desc, &gPipeline);
    }
};

DEFINE_APPLICATION_MAIN(AstraSpikeS01)
