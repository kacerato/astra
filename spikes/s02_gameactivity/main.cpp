// Spike S-02 (docs/plano-mestre/22-ROADMAP-FASES-E-GATES.md §1):
// Dá para usar o renderer do The Forge com GameActivity e loop próprio (sem IApp/AndroidBase)?
//
// Aceite: 100 ciclos de surface perdida/recriada sem erro; IME funcionando (GameTextInput, sem tela
// cheia em paisagem); nenhuma dependência do app glue do TF (só tf_core + tf_graphics).
//
// Interação: toque curto abre/fecha o teclado. O texto digitado e a composição vão para o logcat
// (tag The-Forge, prefixo "S02:"). Com o teclado aberto o triângulo fica azulado.
//
// S-05 (pré-rotação), controlado por propriedades de depuração (adb shell setprop ...):
//   debug.astra.s02.prerotate  1 (padrão) = swapchain pré-rotacionada; 0 = rotação pelo compositor
//   debug.astra.s02.freeze     1 = congela o giro do triângulo (capturas comparáveis)
//
// S-04 (SPIR-V fora do FSL):
//   debug.astra.s02.shader     0 (padrão) = shaders FSL; 1 = SPIR-V compilado no PC a partir de glsl/*.vert|frag
//                              2 = o mesmo GLSL compilado no aparelho pelo glslang (shaders do usuário, doc 08 §4)
//
// S-08 (Luau no contexto de app, onde a política de memória executável difere da do adb shell):
//   debug.astra.s02.luau       1 = roda a suíte do S-08 uma vez ao iniciar (resultado no log, prefixo "S08:")
//
// S-07 (Astra UI / RmlUi):
//   debug.astra.s02.ui         1 = desenha o painel de UI por cima do triângulo e roda o benchmark do Inspector

#include <android/log.h>
#include <android/native_window.h>
#include <sys/system_properties.h>

#include <cmath>
#include <cstdlib>

#include <game-activity/GameActivity.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <game-text-input/gametextinput.h>

#include <string>
#include <vector>

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/OS/Interfaces/IOperatingSystem.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "Common_3/Utilities/Interfaces/IFileSystem.h"
#include "Common_3/Utilities/Interfaces/ILog.h"
#include "Common_3/Utilities/Interfaces/ITime.h"
#include "Common_3/Utilities/Math/MathTypes.h"
#include "Common_3/Utilities/RingBuffer.h"

#include "Common_3/Graphics/FSL/defaults.h"
#include "shaders/Global.srt.h"

#include "luau_suite.h" // spike S-08
#include "ui_spike.h"   // spike S-07

#include "Common_3/Utilities/Interfaces/IMemory.h"

extern WindowDesc gWindow; // forge_android_shim.cpp

namespace
{
constexpr const char* kAppName = "AstraSpikeS02";
constexpr uint32_t    kDataBufferCount = 2;

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

struct FrameData
{
    mat4 transform;
    vec4 tint;
};

int readDebugProp(const char* name, int fallback)
{
    char value[PROP_VALUE_MAX] = {};
    return __system_property_get(name, value) > 0 ? atoi(value) : fallback;
}

// Estado do spike. Tudo roda na thread nativa do GameActivity (android_main).
struct Spike
{
    android_app*    app = nullptr;
    ANativeActivity facade = {}; // ANativeActivity montada a partir do GameActivity (ver shim)

    Renderer*      renderer = nullptr;
    Queue*         queue = nullptr;
    GpuCmdRing     cmdRing = {};
    Semaphore*     imageAcquired = nullptr;
    Shader*        shader = nullptr;
    Pipeline*      pipeline = nullptr;
    TinyImageFormat pipelineFormat = TinyImageFormat_UNDEFINED;
    Buffer*        vertexBuffer = nullptr;
    Buffer*        frameBuffers[kDataBufferCount] = {};
    Texture*       texture = nullptr;
    Sampler*       sampler = nullptr;
    DescriptorSet* setPersistent = nullptr;
    DescriptorSet* setPerFrame = nullptr;
    SwapChain*     swapChain = nullptr;
    RenderTarget*  depth = nullptr; // S-05: anexo só no tile (TEXTURE_CREATION_FLAG_ON_TILE)
    TinyImageFormat depthFormat = TinyImageFormat_D24_UNORM_S8_UINT; // com stencil: máscara de recorte da UI (S-07)
    bool           uiEnabled = false;
    bool           uiReady = false;

    bool     resumed = false;
    bool     keyboardVisible = false;
    uint32_t frameIndex = 0;
    float    angle = 0.0f;
    uint64_t framesPresented = 0;
    uint32_t swapchainsCreated = 0;
    uint32_t surfacesLost = 0;
    float    secondsSinceReport = 0.0f;
    uint64_t framesAtLastReport = 0;

    std::string text;
    int32_t     composeStart = -1;
    int32_t     composeEnd = -1;

    HiresTimer timer = {};
};

Spike gSpike;

// Lido no máximo uma vez por segundo (propriedade de depuração, não é caminho quente de produção).
bool readFrozen(Spike& s)
{
    static float sinceRead = 1.0f;
    static bool  frozen = false;
    (void)s;
    sinceRead += 1.0f / 60.0f;
    if (sinceRead >= 1.0f)
    {
        frozen = readDebugProp("debug.astra.s02.freeze", 0) != 0;
        sinceRead = 0.0f;
    }
    return frozen;
}

// ---------------------------------------------------------------------------
// S-04: shader vindo de fora do FSL
// ---------------------------------------------------------------------------
bool readAsset(const char* path, std::vector<char>& out)
{
    FileStream fs = {};
    if (!fsOpenStreamFromPath(RD_OTHER_FILES, path, FM_READ, &fs))
        return false;
    out.resize((size_t)fsGetStreamFileSize(&fs));
    const bool ok = (size_t)fsReadFromStream(&fs, out.data(), (ssize_t)out.size()) == out.size();
    fsCloseStream(&fs);
    return ok;
}

// GLSL -> SPIR-V 1.0 (mesmo alvo do FSL) com a API C do glslang. Devolve as palavras SPIR-V, ou vazio + log.
std::vector<uint32_t> compileGlsl(const std::vector<char>& source, glslang_stage_t stage, const char* name)
{
    std::string     code(source.begin(), source.end());
    glslang_input_t input = {};
    input.language = GLSLANG_SOURCE_GLSL;
    input.stage = stage;
    input.client = GLSLANG_CLIENT_VULKAN;
    input.client_version = GLSLANG_TARGET_VULKAN_1_0;
    input.target_language = GLSLANG_TARGET_SPV;
    input.target_language_version = GLSLANG_TARGET_SPV_1_0;
    input.code = code.c_str();
    input.default_version = 450;
    input.default_profile = GLSLANG_NO_PROFILE;
    input.messages = GLSLANG_MSG_DEFAULT_BIT;
    input.resource = glslang_default_resource();

    std::vector<uint32_t> words;
    glslang_shader_t*     shader = glslang_shader_create(&input);
    glslang_program_t*    program = nullptr;
    if (!glslang_shader_preprocess(shader, &input) || !glslang_shader_parse(shader, &input))
    {
        LOGF(eERROR, "S04: %s não compilou: %s", name, glslang_shader_get_info_log(shader));
    }
    else
    {
        program = glslang_program_create();
        glslang_program_add_shader(program, shader);
        if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT))
        {
            LOGF(eERROR, "S04: %s não linkou: %s", name, glslang_program_get_info_log(program));
        }
        else
        {
            glslang_program_SPIRV_generate(program, stage);
            const uint32_t* ptr = glslang_program_SPIRV_get_ptr(program);
            words.assign(ptr, ptr + glslang_program_SPIRV_get_size(program));
        }
    }
    if (program)
        glslang_program_delete(program);
    glslang_shader_delete(shader);
    return words;
}

bool loadShader(Spike& s)
{
    const int mode = readDebugProp("debug.astra.s02.shader", 0);
    if (mode == 0)
    {
        ShaderLoadDesc shaderDesc = {};
        shaderDesc.mVert.pFileName = "triangle.vert";
        shaderDesc.mFrag.pFileName = "triangle.frag";
        addShader(s.renderer, &shaderDesc, &s.shader);
        LOGF(eINFO, "S04: shader FSL (CompiledShaders)");
        return s.shader != nullptr;
    }

    std::vector<uint32_t> vert, frag;
    std::vector<char>     v, f;
    if (mode == 1)
    {
        if (!readAsset("GLSL/triangle.vert.spv", v) || !readAsset("GLSL/triangle.frag.spv", f))
            return false;
        vert.resize(v.size() / 4);
        frag.resize(f.size() / 4);
        memcpy(vert.data(), v.data(), vert.size() * 4);
        memcpy(frag.data(), f.data(), frag.size() * 4);
        LOGF(eINFO, "S04: SPIR-V compilado no PC (vert %zu B, frag %zu B)", v.size(), f.size());
    }
    else
    {
        if (!readAsset("GLSL/triangle.vert", v) || !readAsset("GLSL/triangle.frag", f))
            return false;
        HiresTimer t;
        initHiresTimer(&t);
        glslang_initialize_process();
        const float initMs = (float)getHiresTimerUSec(&t, true) / 1000.0f;
        vert = compileGlsl(v, GLSLANG_STAGE_VERTEX, "triangle.vert");
        const float vertMs = (float)getHiresTimerUSec(&t, true) / 1000.0f;
        frag = compileGlsl(f, GLSLANG_STAGE_FRAGMENT, "triangle.frag");
        const float fragMs = (float)getHiresTimerUSec(&t, true) / 1000.0f;
        glslang_finalize_process();
        if (vert.empty() || frag.empty())
            return false;
        LOGF(eINFO, "S04: GLSL compilado no aparelho: init %.2f ms, vert %.2f ms (%zu B), frag %.2f ms (%zu B)", initMs, vertMs,
             vert.size() * 4, fragMs, frag.size() * 4);
    }

    BinaryShaderDesc desc = {};
    desc.mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG;
    desc.mVert.pName = "triangle.vert (GLSL)";
    desc.mVert.pByteCode = vert.data();
    desc.mVert.mByteCodeSize = (uint32_t)(vert.size() * 4);
    desc.mVert.pEntryPoint = "main";
    desc.mFrag.pName = "triangle.frag (GLSL)";
    desc.mFrag.pByteCode = frag.data();
    desc.mFrag.mByteCodeSize = (uint32_t)(frag.size() * 4);
    desc.mFrag.pEntryPoint = "main";
    addShaderBinary(s.renderer, &desc, &s.shader); // o TF só usa o bytecode para criar os VkShaderModule
    return s.shader != nullptr;
}

// ---------------------------------------------------------------------------
// Renderer (vive o processo inteiro; só a swapchain segue a surface)
// ---------------------------------------------------------------------------
bool initRendererObjects(Spike& s)
{
    RendererDesc settings = {};
    initGPUConfiguration(settings.pExtendedSettings);
    initRenderer(kAppName, &settings, &s.renderer);
    if (!s.renderer)
    {
        LOGF(eERROR, "S02: initRenderer falhou");
        return false;
    }
    setupGPUConfigurationPlatformParameters(s.renderer, settings.pExtendedSettings);
    LOGF(eINFO, "S02: GPU '%s'", s.renderer->pGpu->mGpuVendorPreset.mGpuName);

    QueueDesc queueDesc = {};
    queueDesc.mType = QUEUE_TYPE_GRAPHICS;
    initQueue(s.renderer, &queueDesc, &s.queue);

    GpuCmdRingDesc ringDesc = {};
    ringDesc.pQueue = s.queue;
    ringDesc.mPoolCount = kDataBufferCount;
    ringDesc.mCmdPerPoolCount = 1;
    ringDesc.mAddSyncPrimitives = true;
    initGpuCmdRing(s.renderer, &ringDesc, &s.cmdRing);

    initSemaphore(s.renderer, &s.imageAcquired);
    initResourceLoaderInterface(s.renderer);

    RootSignatureDesc rootDesc = {};
    INIT_RS_DESC(rootDesc, "default.rootsig", "compute.rootsig");
    initRootSignature(s.renderer, &rootDesc);

    SamplerDesc samplerDesc = { FILTER_LINEAR,       FILTER_LINEAR,       MIPMAP_MODE_LINEAR,
                                ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
    addSampler(s.renderer, &samplerDesc, &s.sampler);

    TextureLoadDesc textureDesc = {};
    textureDesc.pFileName = "astra_checker.ktx";
    textureDesc.mContainer = TEXTURE_CONTAINER_KTX;
    textureDesc.ppTexture = &s.texture;
    addResource(&textureDesc, nullptr);

    BufferLoadDesc vbDesc = {};
    vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    vbDesc.mDesc.mSize = sizeof(kTriangle);
    vbDesc.pData = kTriangle;
    vbDesc.ppBuffer = &s.vertexBuffer;
    addResource(&vbDesc, nullptr);

    BufferLoadDesc ubDesc = {};
    ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    ubDesc.mDesc.mSize = sizeof(FrameData);
    for (uint32_t i = 0; i < kDataBufferCount; ++i)
    {
        ubDesc.ppBuffer = &s.frameBuffers[i];
        addResource(&ubDesc, nullptr);
    }
    waitForAllResourceLoads();
    if (!s.texture)
        return false;

    if (!loadShader(s))
        return false;

    DescriptorSetDesc persistent = SRT_SET_DESC(SrtData, Persistent, 1, 0);
    addDescriptorSet(s.renderer, &persistent, &s.setPersistent);
    DescriptorSetDesc perFrame = SRT_SET_DESC(SrtData, PerFrame, kDataBufferCount, 0);
    addDescriptorSet(s.renderer, &perFrame, &s.setPerFrame);

    DescriptorData params[2] = {};
    params[0].mIndex = SRT_RES_IDX(SrtData, Persistent, gTexture);
    params[0].ppTextures = &s.texture;
    params[1].mIndex = SRT_RES_IDX(SrtData, Persistent, gSampler);
    params[1].ppSamplers = &s.sampler;
    updateDescriptorSet(s.renderer, 0, s.setPersistent, 2, params);
    for (uint32_t i = 0; i < kDataBufferCount; ++i)
    {
        DescriptorData frameParam = {};
        frameParam.mIndex = SRT_RES_IDX(SrtData, PerFrame, gFrame);
        frameParam.ppBuffers = &s.frameBuffers[i];
        updateDescriptorSet(s.renderer, i, s.setPerFrame, 1, &frameParam);
    }
    return true;
}

void initUi(Spike& s);

void addPipelineFor(Spike& s, TinyImageFormat format)
{
    if (s.pipeline && s.pipelineFormat == format)
        return;
    if (s.pipeline)
        removePipeline(s.renderer, s.pipeline);

    VertexLayout layout = {};
    layout.mBindingCount = 1;
    layout.mBindings[0].mStride = sizeof(Vertex);
    layout.mAttribCount = 2;
    layout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
    layout.mAttribs[0].mFormat = TinyImageFormat_R32G32_SFLOAT;
    layout.mAttribs[0].mLocation = 0;
    layout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
    layout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
    layout.mAttribs[1].mLocation = 1;
    layout.mAttribs[1].mOffset = offsetof(Vertex, u);

    RasterizerStateDesc raster = {};
    raster.mCullMode = CULL_MODE_NONE;
    DepthStateDesc depthState = {};
    depthState.mDepthTest = true;
    depthState.mDepthWrite = true;
    depthState.mDepthFunc = CMP_GEQUAL;

    PipelineDesc desc = {};
    desc.mType = PIPELINE_TYPE_GRAPHICS;
    PIPELINE_LAYOUT_DESC(desc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame), NULL, NULL);
    GraphicsPipelineDesc& gfx = desc.mGraphicsDesc;
    gfx.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
    gfx.mRenderTargetCount = 1;
    gfx.pColorFormats = &format;
    gfx.mSampleCount = SAMPLE_COUNT_1;
    gfx.pShaderProgram = s.shader;
    gfx.pVertexLayout = &layout;
    gfx.pRasterizerState = &raster;
    gfx.pDepthState = &depthState;
    gfx.mDepthStencilFormat = s.depthFormat;
    addPipeline(s.renderer, &desc, &s.pipeline);
    s.pipelineFormat = format;
}

// ---------------------------------------------------------------------------
// Swapchain ↔ surface
// ---------------------------------------------------------------------------
void destroySwapchain(Spike& s)
{
    if (!s.swapChain)
        return;
    waitQueueIdle(s.queue);
    removeRenderTarget(s.renderer, s.depth);
    s.depth = nullptr;
    removeSwapChain(s.renderer, s.swapChain);
    s.swapChain = nullptr;
}

void createSwapchain(Spike& s)
{
    ANativeWindow* window = s.app->window;
    if (!window || !s.renderer)
        return;
    gWindow.handle.window = window;

    SwapChainDesc desc = {};
    desc.mWindowHandle = gWindow.handle;
    desc.mPresentQueueCount = 1;
    desc.ppPresentQueues = &s.queue;
    desc.mWidth = (uint32_t)ANativeWindow_getWidth(window);
    desc.mHeight = (uint32_t)ANativeWindow_getHeight(window);
    desc.mImageCount = getRecommendedSwapchainImageCount(s.renderer, &gWindow.handle);
    desc.mColorFormat = getSupportedSwapchainFormat(s.renderer, &desc, COLOR_SPACE_SDR_SRGB);
    desc.mColorSpace = COLOR_SPACE_SDR_SRGB;
    desc.mEnableVsync = true;
    desc.mColorClearValue = { { 0.0033f, 0.0037f, 0.0044f, 1.0f } }; // bg.canvas #0B0C0E (linear)
    if (readDebugProp("debug.astra.s02.prerotate", 1))
        desc.mFlags = SWAP_CHAIN_CREATION_FLAG_PRE_ROTATION;
    addSwapChain(s.renderer, &desc, &s.swapChain);
    if (!s.swapChain)
    {
        LOGF(eERROR, "S02: addSwapChain falhou (%ux%u)", desc.mWidth, desc.mHeight);
        return;
    }
    // Depth que nunca sai do tile: memória lazily allocated + STORE_DONTCARE (o TF força o descarte quando
    // a memória lazily allocated é concedida).
    RenderTargetDesc depthDesc = {};
    depthDesc.mArraySize = 1;
    depthDesc.mDepth = 1;
    depthDesc.mWidth = s.swapChain->ppRenderTargets[0]->mWidth;
    depthDesc.mHeight = s.swapChain->ppRenderTargets[0]->mHeight;
    depthDesc.mFormat = s.depthFormat;
    depthDesc.mClearValue.depth = 0.0f;
    depthDesc.mClearValue.stencil = 0;
    depthDesc.mStartState = RESOURCE_STATE_DEPTH_WRITE;
    depthDesc.mSampleCount = SAMPLE_COUNT_1;
    depthDesc.mFlags = TEXTURE_CREATION_FLAG_ON_TILE;
    depthDesc.pName = "S02Depth";
    addRenderTarget(s.renderer, &depthDesc, &s.depth);
    if (s.swapchainsCreated == 0)
        LOGF(eINFO, "S02: depth ON_TILE %ux%u: memória lazily allocated %s", depthDesc.mWidth, depthDesc.mHeight,
             s.depth->pTexture->mLazilyAllocated ? "CONCEDIDA" : "não disponível (memória comum)");

    addPipelineFor(s, s.swapChain->ppRenderTargets[0]->mFormat);
    ++s.swapchainsCreated;
    if (s.uiEnabled && !s.uiReady)
        initUi(s);
    RenderTarget* rt = s.swapChain->ppRenderTargets[0];
    LOGF(eINFO, "S02: swapchain #%u janela %ux%u, imagem %ux%u, pré-rotação %u°, %u imagens, %s", s.swapchainsCreated, desc.mWidth,
         desc.mHeight, rt->mWidth, rt->mHeight, s.swapChain->mPreRotationDegrees, desc.mImageCount, TinyImageFormat_Name(desc.mColorFormat));
}

// A surface pode mudar de tamanho sem TERM/INIT (rotação, multi-janela, dobra).
void recreateSwapchainIfResized(Spike& s)
{
    if (!s.swapChain || !s.app->window)
        return;
    const uint32_t w = (uint32_t)ANativeWindow_getWidth(s.app->window);
    const uint32_t h = (uint32_t)ANativeWindow_getHeight(s.app->window);
    // Compara na orientação lógica (a imagem pré-rotacionada em 90/270 tem largura e altura trocadas).
    RenderTarget*  rt = s.swapChain->ppRenderTargets[0];
    const bool     swapped = s.swapChain->mPreRotationDegrees == 90 || s.swapChain->mPreRotationDegrees == 270;
    const uint32_t curW = swapped ? rt->mHeight : rt->mWidth;
    const uint32_t curH = swapped ? rt->mWidth : rt->mHeight;
    if (w != curW || h != curH)
    {
        LOGF(eINFO, "S02: surface mudou %ux%u -> %ux%u", curW, curH, w, h);
        destroySwapchain(s);
        createSwapchain(s);
    }
}

// ---------------------------------------------------------------------------
// Texto (GameTextInput)
// ---------------------------------------------------------------------------
void onTextState(void* ctx, const GameTextInputState* state)
{
    Spike& s = *(Spike*)ctx;
    if (s.uiReady)
    {
        std::string text(state->text_UTF8, (size_t)state->text_length);
        UiSpike::textInput(text.c_str(), state->selection.start, state->selection.end, state->composingRegion.start,
                           state->composingRegion.end);
    }
    s.text.assign(state->text_UTF8, (size_t)state->text_length);
    s.composeStart = state->composingRegion.start;
    s.composeEnd = state->composingRegion.end;
    LOGF(eINFO, "S02: texto='%s' (%d bytes) seleção[%d,%d) composição[%d,%d)", s.text.c_str(), state->text_length,
         state->selection.start, state->selection.end, s.composeStart, s.composeEnd);
}

void toggleKeyboard(Spike& s)
{
    GameActivity* act = s.app->activity;
    if (s.keyboardVisible)
    {
        GameActivity_hideSoftInput(act, 0);
        return;
    }
    // Sem a tela cheia de extração do teclado em paisagem (lição da Astra 1: escondia Aplicar/Cancelar).
    GameActivity_setImeEditorInfo(act, TYPE_CLASS_TEXT, IME_ACTION_DONE, IME_FLAG_NO_EXTRACT_UI);
    GameActivity_showSoftInput(act, 0);
}

// ---------------------------------------------------------------------------
// S-07: UI
// ---------------------------------------------------------------------------
void initUi(Spike& s)
{
    GameActivity*    act = s.app->activity;
    const float      dpRatio = (float)AConfiguration_getDensity(s.app->config) / 160.0f;
    UiKeyboardBridge keyboard;
    keyboard.show = [act](const char* text, int selStart, int selEnd) {
        GameTextInputState state = {};
        state.text_UTF8 = text;
        state.text_length = (int32_t)strlen(text);
        state.selection = { selStart, selEnd };
        state.composingRegion = { -1, -1 };
        GameActivity_setTextInputState(act, &state);
        GameActivity_setImeEditorInfo(act, TYPE_CLASS_TEXT, IME_ACTION_DONE, IME_FLAG_NO_EXTRACT_UI);
        GameActivity_showSoftInput(act, 0);
    };
    keyboard.hide = [act]() { GameActivity_hideSoftInput(act, 0); };
    s.uiReady = UiSpike::init(s.renderer, s.swapChain->ppRenderTargets[0]->mFormat, s.depthFormat, "/system/fonts/Roboto-Regular.ttf",
                              dpRatio, keyboard);
    if (s.uiReady)
        UiSpike::runInspectorBenchmark(300);
}

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------
void onAppCmd(android_app* app, int32_t cmd)
{
    Spike& s = *(Spike*)app->userData;
    switch (cmd)
    {
    case APP_CMD_INIT_WINDOW:
        LOGF(eINFO, "S02: INIT_WINDOW");
        createSwapchain(s);
        break;
    case APP_CMD_TERM_WINDOW:
        ++s.surfacesLost;
        LOGF(eINFO, "S02: TERM_WINDOW (#%u)", s.surfacesLost);
        destroySwapchain(s);
        gWindow.handle.window = nullptr;
        break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
    case APP_CMD_CONTENT_RECT_CHANGED:
        recreateSwapchainIfResized(s);
        break;
    case APP_CMD_RESUME:
        s.resumed = true;
        break;
    case APP_CMD_PAUSE:
        s.resumed = false;
        break;
    case APP_CMD_SOFTWARE_KB_VIS_CHANGED:
        s.keyboardVisible = app->softwareKeyboardVisible;
        LOGF(eINFO, "S02: teclado %s", s.keyboardVisible ? "visível" : "oculto");
        break;
    case APP_CMD_WINDOW_INSETS_CHANGED:
    {
        ARect ime = {};
        GameActivity_getWindowInsets(app->activity, GAMECOMMON_INSETS_TYPE_IME, &ime);
        LOGF(eINFO, "S02: insets do teclado (l=%d t=%d r=%d b=%d)", ime.left, ime.top, ime.right, ime.bottom);
        break;
    }
    case APP_CMD_EDITOR_ACTION:
        LOGF(eINFO, "S02: ação do editor %d, texto final='%s'", app->editorAction, s.text.c_str());
        if (s.uiReady)
            UiSpike::editorDone();
        GameActivity_hideSoftInput(app->activity, 0);
        break;
    default:
        break;
    }
}

void processInput(Spike& s)
{
    android_input_buffer* input = android_app_swap_input_buffers(s.app);
    if (!input)
        return;
    for (uint64_t i = 0; i < input->motionEventsCount; ++i)
    {
        const GameActivityMotionEvent& e = input->motionEvents[i];
        const int32_t action = e.action & AMOTION_EVENT_ACTION_MASK;
        if (s.uiReady)
        {
            const int32_t idx = (e.action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
            if (action == AMOTION_EVENT_ACTION_MOVE)
            {
                for (uint32_t p = 0; p < e.pointerCount; ++p)
                    UiSpike::touch(1, e.pointers[p].id, GameActivityPointerAxes_getX(&e.pointers[p]), GameActivityPointerAxes_getY(&e.pointers[p]));
            }
            else
            {
                const int kind = (action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_POINTER_DOWN) ? 0
                                 : (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_POINTER_UP)   ? 2
                                 : action == AMOTION_EVENT_ACTION_CANCEL                                              ? 3
                                                                                                                      : -1;
                if (kind >= 0)
                    UiSpike::touch(kind, e.pointers[idx].id, GameActivityPointerAxes_getX(&e.pointers[idx]),
                                   GameActivityPointerAxes_getY(&e.pointers[idx]));
            }
            continue;
        }
        // Toque curto (< 300 ms) com um dedo abre/fecha o teclado.
        if (action == AMOTION_EVENT_ACTION_UP && e.pointerCount == 1 && (e.eventTime - e.downTime) < 300000000LL)
            toggleKeyboard(s);
    }
    android_app_clear_motion_events(input);
    android_app_clear_key_events(input);
}

void drawFrame(Spike& s, float dt)
{
    if (!s.swapChain || !s.resumed)
        return;

    if (!readFrozen(s))
        s.angle += dt * 0.6f;
    // Tamanho lógico (orientação que o usuário vê) e rotação do clip-space quando a swapchain é pré-rotacionada.
    RenderTarget*  rt0 = s.swapChain->ppRenderTargets[0];
    const uint32_t preRot = s.swapChain->mPreRotationDegrees;
    const bool     swapped = preRot == 90 || preRot == 270;
    const float    w = (float)(swapped ? rt0->mHeight : rt0->mWidth);
    const float    h = (float)(swapped ? rt0->mWidth : rt0->mHeight);
    // Sentido validado por captura: com o viewport do TF (y invertido no Vulkan), a pré-rotação de N graus da tela
    // corresponde a girar o clip-space em -N graus (doc 24: comparação com/sem pré-rotação, diferença ~0).
    const mat4     preRotation = mat4::rotationZ(-(float)preRot * (float)M_PI / 180.0f);
    FrameData      frame;
    frame.transform = preRotation * mat4::scale(w < h ? vec3(1.0f, w / h, 1.0f) : vec3(h / w, 1.0f, 1.0f)) * mat4::rotationZ(s.angle);
    frame.tint = s.keyboardVisible ? vec4(0.55f, 0.75f, 1.0f, 1.0f) : vec4(1.0f);

    if (s.uiReady)
        UiSpike::update((uint32_t)w, (uint32_t)h);

    uint32_t imageIndex = 0;
    acquireNextImage(s.renderer, s.swapChain, s.imageAcquired, nullptr, &imageIndex);
    if (imageIndex == (uint32_t)-1)
    {
        LOGF(eINFO, "S02: swapchain desatualizada no acquire; recriando");
        destroySwapchain(s);
        createSwapchain(s);
        return;
    }
    RenderTarget* target = s.swapChain->ppRenderTargets[imageIndex];

    GpuCmdRingElement elem = getNextGpuCmdRingElement(&s.cmdRing, true, 1);
    FenceStatus       fenceStatus;
    getFenceStatus(s.renderer, elem.pFence, &fenceStatus);
    if (fenceStatus == FENCE_STATUS_INCOMPLETE)
        waitForFences(s.renderer, 1, &elem.pFence);

    BufferUpdateDesc update = { s.frameBuffers[s.frameIndex] };
    beginUpdateResource(&update);
    memcpy(update.pMappedData, &frame, sizeof(frame));
    endUpdateResource(&update);

    resetCmdPool(s.renderer, elem.pCmdPool);
    Cmd* cmd = elem.pCmds[0];
    beginCmd(cmd);
    RenderTargetBarrier barrier = { target, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET };
    cmdResourceBarrier(cmd, 0, nullptr, 0, nullptr, 1, &barrier);
    BindRenderTargetsDesc bind = {};
    bind.mRenderTargetCount = 1;
    bind.mRenderTargets[0] = { target, LOAD_ACTION_CLEAR };
    bind.mDepthStencil = { s.depth, LOAD_ACTION_CLEAR, LOAD_ACTION_CLEAR, STORE_ACTION_DONTCARE, STORE_ACTION_DONTCARE };
    cmdBindRenderTargets(cmd, &bind);
    cmdSetViewport(cmd, 0.0f, 0.0f, (float)target->mWidth, (float)target->mHeight, 0.0f, 1.0f);
    cmdSetScissor(cmd, 0, 0, target->mWidth, target->mHeight);
    const uint32_t stride = sizeof(Vertex);
    cmdBindPipeline(cmd, s.pipeline);
    cmdBindDescriptorSet(cmd, 0, s.setPersistent);
    cmdBindDescriptorSet(cmd, s.frameIndex, s.setPerFrame);
    cmdBindVertexBuffer(cmd, 1, &s.vertexBuffer, &stride, nullptr);
    cmdDraw(cmd, 3, 0);
    if (s.uiReady)
        UiSpike::render(cmd, s.frameIndex, target->mWidth, target->mHeight, preRot);
    cmdBindRenderTargets(cmd, nullptr);
    barrier = { target, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
    cmdResourceBarrier(cmd, 0, nullptr, 0, nullptr, 1, &barrier);
    endCmd(cmd);

    FlushResourceUpdateDesc flush = {};
    flushResourceUpdates(&flush);
    Semaphore* waits[2] = { flush.pOutSubmittedSemaphore, s.imageAcquired };
    QueueSubmitDesc submit = {};
    submit.mCmdCount = 1;
    submit.ppCmds = &cmd;
    submit.mSignalSemaphoreCount = 1;
    submit.ppSignalSemaphores = &elem.pSemaphore;
    submit.mWaitSemaphoreCount = 2;
    submit.ppWaitSemaphores = waits;
    submit.pSignalFence = elem.pFence;
    queueSubmit(s.queue, &submit);

    QueuePresentDesc present = {};
    present.mIndex = (uint8_t)imageIndex;
    present.mWaitSemaphoreCount = 1;
    present.ppWaitSemaphores = &elem.pSemaphore;
    present.pSwapChain = s.swapChain;
    present.mSubmitDone = true;
    queuePresent(s.queue, &present);

    // Com pré-rotação, SUBOPTIMAL/OUT_OF_DATE indica que a orientação da tela mudou (ex.: giro de 180° entre as
    // duas paisagens, que não gera mudança de configuração). Sem pré-rotação o Android pode devolver SUBOPTIMAL
    // em todo frame, então o sinal só é usado no modo pré-rotacionado.
    if (s.swapChain->mSuboptimal && (s.swapChain->ppRenderTargets[0] && s.swapChain->mVk.pDesc->mFlags & SWAP_CHAIN_CREATION_FLAG_PRE_ROTATION))
    {
        LOGF(eINFO, "S02: swapchain subótima (pré-rotação %u°); recriando", s.swapChain->mPreRotationDegrees);
        destroySwapchain(s);
        createSwapchain(s);
        s.frameIndex = (s.frameIndex + 1) % kDataBufferCount;
        ++s.framesPresented;
        return;
    }

    if (s.framesPresented == 0)
        LOGF(eINFO, "S02: primeiro frame apresentado (%ux%u)", target->mWidth, target->mHeight);
    ++s.framesPresented;

    // Resumo a cada 10 s de render (não por frame): prova que o loop segue vivo após os ciclos de surface.
    s.secondsSinceReport += dt;
    if (s.secondsSinceReport >= 10.0f)
    {
        LOGF(eINFO, "S02: %llu frames em %.1f s (total %llu)", (unsigned long long)(s.framesPresented - s.framesAtLastReport),
             s.secondsSinceReport, (unsigned long long)s.framesPresented);
        s.framesAtLastReport = s.framesPresented;
        s.secondsSinceReport = 0.0f;
    }
    s.frameIndex = (s.frameIndex + 1) % kDataBufferCount;
}

void shutdown(Spike& s)
{
    if (s.uiReady)
    {
        waitQueueIdle(s.queue);
        UiSpike::exit();
    }
    destroySwapchain(s);
    if (s.renderer)
    {
        waitQueueIdle(s.queue);
        removePipeline(s.renderer, s.pipeline);
        removeDescriptorSet(s.renderer, s.setPerFrame);
        removeDescriptorSet(s.renderer, s.setPersistent);
        removeShader(s.renderer, s.shader);
        for (uint32_t i = 0; i < kDataBufferCount; ++i)
            removeResource(s.frameBuffers[i]);
        removeResource(s.vertexBuffer);
        removeResource(s.texture);
        removeSampler(s.renderer, s.sampler);
        exitGpuCmdRing(s.renderer, &s.cmdRing);
        exitSemaphore(s.renderer, s.imageAcquired);
        exitRootSignature(s.renderer);
        exitResourceLoaderInterface(s.renderer);
        exitQueue(s.renderer, s.queue);
        exitRenderer(s.renderer);
        exitGPUConfiguration();
        s.renderer = nullptr;
    }
    LOGF(eINFO, "S02: encerrado; frames=%llu swapchains=%u surfaces perdidas=%u", (unsigned long long)s.framesPresented,
         s.swapchainsCreated, s.surfacesLost);
    exitLog();
    exitFileSystem();
    exitMemAlloc();
}
} // namespace

// Ponto de entrada chamado pelo glue do GameActivity numa thread nativa própria.
extern "C" void android_main(android_app* app)
{
    Spike& s = gSpike;
    s = Spike{};
    s.app = app;
    app->userData = &s;
    app->onAppCmd = onAppCmd;

    if (!initMemAlloc(kAppName))
        return;

    // Fachada ANativeActivity: o FileSystem e o Swappy do TF só leem estes campos.
    GameActivity* act = app->activity;
    s.facade.vm = act->vm;
    s.facade.clazz = act->javaGameActivity;
    s.facade.internalDataPath = act->internalDataPath;
    s.facade.externalDataPath = act->externalDataPath;
    s.facade.sdkVersion = act->sdkVersion;
    s.facade.assetManager = act->assetManager;
    s.facade.obbPath = act->obbPath;

    gWindow.handle.type = WINDOW_HANDLE_TYPE_ANDROID;
    gWindow.handle.activity = &s.facade;
    gWindow.handle.configuration = app->config;

    FileSystemInitDesc fsDesc = {};
    fsDesc.pPlatformData = &s.facade;
    fsDesc.pAppName = kAppName;
    if (!initFileSystem(&fsDesc))
        return;
    initLog(kAppName, DEFAULT_LOG_LEVEL);
    LOGF(eINFO, "S02: android_main (GameActivity, SDK %d)", act->sdkVersion);
    if (readDebugProp("debug.astra.s02.luau", 0))
        runLuauSuite([](const char* line) { LOGF(eINFO, "%s", line); });

    s.uiEnabled = readDebugProp("debug.astra.s02.ui", 0) != 0;
    const bool rendererOk = initRendererObjects(s);
    if (rendererOk && !(s.renderer->pGpu->mFormatCaps[TinyImageFormat_D24_UNORM_S8_UINT] & FORMAT_CAP_DEPTH_STENCIL))
        s.depthFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
    if (rendererOk)
        LOGF(eINFO, "S02: depth/stencil %s, UI %s", TinyImageFormat_Name(s.depthFormat), s.uiEnabled ? "ligada" : "desligada");
    if (!rendererOk)
        LOGF(eERROR, "S02: renderer não inicializou; o loop segue só para encerrar limpo");

    initHiresTimer(&s.timer);
    while (!app->destroyRequested)
    {
        // Bloqueia sem surface ou em pausa (0 frames de GPU); com surface ativa, só esvazia a fila.
        const int timeoutMs = (s.swapChain && s.resumed) ? 0 : -1;
        int                  events = 0;
        android_poll_source* source = nullptr;
        if (ALooper_pollOnce(timeoutMs, nullptr, &events, (void**)&source) >= 0 && source)
            source->process(app, source);

        processInput(s);
        if (app->textInputState)
        {
            GameActivity_getTextInputState(act, onTextState, &s);
            app->textInputState = 0;
        }

        const float dt = getHiresTimerSeconds(&s.timer, true);
        if (rendererOk)
            drawFrame(s, dt > 0.1f ? 0.1f : dt);
    }
    shutdown(s);
}
