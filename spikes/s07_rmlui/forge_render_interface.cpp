// Spike S-07: Rml::RenderInterface sobre o The Forge. Ver forge_render_interface.h.

#include "forge_render_interface.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "Common_3/Utilities/Interfaces/ILog.h"

#include "Common_3/Graphics/FSL/defaults.h"
#include "shaders/rmlui.srt.h"

#include <RmlUi/Core/Vertex.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Common_3/Utilities/Interfaces/IMemory.h"

namespace
{
// Coluna-maior: out = a * b.
void mul4(const float* a, const float* b, float* out)
{
    float r[16];
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row)
            r[c * 4 + row] = a[0 * 4 + row] * b[c * 4 + 0] + a[1 * 4 + row] * b[c * 4 + 1] + a[2 * 4 + row] * b[c * 4 + 2] +
                             a[3 * 4 + row] * b[c * 4 + 3];
    memcpy(out, r, sizeof(r));
}

void identity(float* m)
{
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

// Mesma convenção do resto da Astra no TF: o viewport do TF inverte Y, então o NDC +Y aponta para cima na tela
// (projeção estilo GL, como o backend GL3 da RmlUi).
void ortho(float l, float r, float b, float t, float n, float f, float* m)
{
    identity(m);
    m[0] = 2.0f / (r - l);
    m[5] = 2.0f / (t - b);
    m[10] = -2.0f / (f - n);
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
    m[14] = -(f + n) / (f - n);
}

void rotationZ(float radians, float* m)
{
    identity(m);
    const float c = cosf(radians), s = sinf(radians);
    m[0] = c;
    m[1] = s;
    m[4] = -s;
    m[5] = c;
}

Buffer* makeBuffer(DescriptorType type, const void* data, uint64_t size, const char* name)
{
    Buffer*        b = nullptr;
    BufferLoadDesc desc = {};
    desc.mDesc.mDescriptors = type;
    desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU; // geometria de UI: pequena e escrita uma vez
    desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    desc.mDesc.mSize = size;
    desc.mDesc.pName = name;
    desc.pData = data;
    desc.ppBuffer = &b;
    addResource(&desc, nullptr);
    return b;
}
} // namespace

bool ForgeRenderInterface::init(Renderer* renderer, uint32_t colorFormat, uint32_t depthStencilFormat)
{
    mRenderer = renderer;

    ShaderLoadDesc shaderDesc = {};
    shaderDesc.mVert.pFileName = "rmlui.vert";
    shaderDesc.mFrag.pFileName = "rmlui.frag";
    addShader(mRenderer, &shaderDesc, &mShader);
    if (!mShader)
        return false;

    SamplerDesc samplerDesc = { FILTER_LINEAR,
                                FILTER_LINEAR,
                                MIPMAP_MODE_NEAREST,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE };
    addSampler(mRenderer, &samplerDesc, &mSampler);

    DescriptorSetDesc texDesc = SRT_SET_DESC(RmlSrt, Persistent, kMaxTextures, 0);
    addDescriptorSet(mRenderer, &texDesc, &mTextureSet);
    DescriptorSetDesc drawDesc = SRT_SET_DESC(RmlSrt, PerFrame, kFrames, 0);
    addDescriptorSet(mRenderer, &drawDesc, &mDrawSet);

    for (uint32_t i = 0; i < kFrames; ++i)
    {
        BufferLoadDesc desc = {};
        desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
        desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        desc.mDesc.mStructStride = sizeof(float) * 4;
        desc.mDesc.mElementCount = kMaxDrawsPerFrame * 5;
        desc.mDesc.mSize = (uint64_t)desc.mDesc.mStructStride * desc.mDesc.mElementCount;
        desc.mDesc.pName = "RmlDraws";
        desc.ppBuffer = &mDrawBuffers[i];
        addResource(&desc, nullptr);
    }
    waitForAllResourceLoads();
    for (uint32_t i = 0; i < kFrames; ++i)
    {
        DescriptorData param = {};
        param.mIndex = SRT_RES_IDX(RmlSrt, PerFrame, gDraws);
        param.ppBuffers = &mDrawBuffers[i];
        updateDescriptorSet(mRenderer, i, mDrawSet, 1, &param);
    }

    mTextures.resize(kMaxTextures);
    const uint8_t white[4] = { 255, 255, 255, 255 };
    mWhiteSlot = createTexture(1, 1, white);

    // Quad de tela cheia em NDC, para "limpar" o stencil pela própria pipeline (o TF não tem clear no meio do passe).
    const Rml::Vertex quad[4] = {
        { { -1.0f, -1.0f }, {}, { 0, 0 } }, { { 1.0f, -1.0f }, {}, { 1, 0 } }, { { 1.0f, 1.0f }, {}, { 1, 1 } }, { { -1.0f, 1.0f }, {}, { 0, 1 } }
    };
    const int quadIdx[6] = { 0, 1, 2, 0, 2, 3 };
    mFullscreenQuad = (Geometry*)CompileGeometry({ quad, 4 }, { quadIdx, 6 });

    // Pipelines: mesma entrada de vértice e blend pré-multiplicado; muda o estado de stencil.
    VertexLayout layout = {};
    layout.mBindingCount = 1;
    layout.mBindings[0].mStride = sizeof(Rml::Vertex);
    layout.mAttribCount = 3;
    layout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
    layout.mAttribs[0].mFormat = TinyImageFormat_R32G32_SFLOAT;
    layout.mAttribs[0].mLocation = 0;
    layout.mAttribs[0].mOffset = offsetof(Rml::Vertex, position);
    layout.mAttribs[1].mSemantic = SEMANTIC_COLOR;
    layout.mAttribs[1].mFormat = TinyImageFormat_R8G8B8A8_UNORM;
    layout.mAttribs[1].mLocation = 1;
    layout.mAttribs[1].mOffset = offsetof(Rml::Vertex, colour);
    layout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
    layout.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
    layout.mAttribs[2].mLocation = 2;
    layout.mAttribs[2].mOffset = offsetof(Rml::Vertex, tex_coord);

    RasterizerStateDesc raster = {};
    raster.mCullMode = CULL_MODE_NONE;
    raster.mScissor = true;

    for (int kind = 0; kind < kPipelineCount; ++kind)
    {
        BlendStateDesc blend = {};
        blend.mSrcFactors[0] = BC_ONE;
        blend.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
        blend.mSrcAlphaFactors[0] = BC_ONE;
        blend.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
        blend.mBlendModes[0] = BM_ADD;
        blend.mBlendAlphaModes[0] = BM_ADD;
        blend.mColorWriteMasks[0] = (kind == kMaskReplace || kind == kMaskIncrement) ? COLOR_MASK_NONE : COLOR_MASK_ALL;
        blend.mRenderTargetMask = BLEND_STATE_TARGET_0;

        DepthStateDesc ds = {};
        ds.mDepthTest = false;
        ds.mDepthWrite = false;
        ds.mStencilTest = kind != kDraw;
        ds.mStencilReadMask = 0xFF;
        ds.mStencilWriteMask = (kind == kMaskReplace || kind == kMaskIncrement) ? 0xFF : 0x00;
        ds.mStencilFrontFunc = kind == kMaskReplace ? CMP_ALWAYS : CMP_EQUAL;
        ds.mStencilFrontFail = STENCIL_OP_KEEP;
        ds.mDepthFrontFail = STENCIL_OP_KEEP;
        ds.mStencilFrontPass = kind == kMaskReplace ? STENCIL_OP_REPLACE : kind == kMaskIncrement ? STENCIL_OP_INCR : STENCIL_OP_KEEP;
        ds.mStencilBackFunc = ds.mStencilFrontFunc;
        ds.mStencilBackFail = ds.mStencilFrontFail;
        ds.mDepthBackFail = ds.mDepthFrontFail;
        ds.mStencilBackPass = ds.mStencilFrontPass;

        TinyImageFormat color = (TinyImageFormat)colorFormat;
        PipelineDesc    desc = {};
        desc.mType = PIPELINE_TYPE_GRAPHICS;
        PIPELINE_LAYOUT_DESC(desc, SRT_LAYOUT_DESC(RmlSrt, Persistent), SRT_LAYOUT_DESC(RmlSrt, PerFrame), NULL, NULL);
        GraphicsPipelineDesc& gfx = desc.mGraphicsDesc;
        gfx.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        gfx.mRenderTargetCount = 1;
        gfx.pColorFormats = &color;
        gfx.mSampleCount = SAMPLE_COUNT_1;
        gfx.mDepthStencilFormat = (TinyImageFormat)depthStencilFormat;
        gfx.pShaderProgram = mShader;
        gfx.pVertexLayout = &layout;
        gfx.pRasterizerState = &raster;
        gfx.pBlendState = &blend;
        gfx.pDepthState = &ds;
        addPipeline(mRenderer, &desc, &mPipelines[kind]);
        if (!mPipelines[kind])
            return false;
    }
    return true;
}

void ForgeRenderInterface::exit()
{
    for (uint32_t f = 0; f < kFrames; ++f)
        flushDeferredReleases(f);
    ReleaseGeometry((Rml::CompiledGeometryHandle)mFullscreenQuad);
    for (uint32_t f = 0; f < kFrames; ++f)
        flushDeferredReleases(f);
    for (TextureSlot& t : mTextures)
        if (t.used)
            removeResource(t.texture);
    mTextures.clear();
    for (Pipeline*& p : mPipelines)
        removePipeline(mRenderer, p);
    for (Buffer*& b : mDrawBuffers)
        removeResource(b);
    removeDescriptorSet(mRenderer, mDrawSet);
    removeDescriptorSet(mRenderer, mTextureSet);
    removeSampler(mRenderer, mSampler);
    removeShader(mRenderer, mShader);
}

void ForgeRenderInterface::flushDeferredReleases(uint32_t frameIndex)
{
    for (Geometry* g : mPendingGeometry[frameIndex])
    {
        removeResource(g->vb);
        removeResource(g->ib);
        tf_delete(g);
    }
    mPendingGeometry[frameIndex].clear();
    for (uint32_t slot : mPendingTextures[frameIndex])
    {
        removeResource(mTextures[slot].texture);
        mTextures[slot] = {};
    }
    mPendingTextures[frameIndex].clear();
}

void ForgeRenderInterface::beginFrame(Cmd* cmd, uint32_t frameIndex, uint32_t targetW, uint32_t targetH, uint32_t preRotationDeg)
{
    mCmd = cmd;
    mFrameIndex = frameIndex % kFrames;
    // O chamador já esperou a fence deste slot de frame: o que foi liberado há kFrames frames pode sair agora.
    flushDeferredReleases(mFrameIndex);

    mTargetW = targetW;
    mTargetH = targetH;
    mPreRotation = preRotationDeg;
    const bool swapped = preRotationDeg == 90 || preRotationDeg == 270;
    mLogicalW = swapped ? targetH : targetW;
    mLogicalH = swapped ? targetW : targetH;

    float o[16], r[16];
    ortho(0.0f, (float)mLogicalW, (float)mLogicalH, 0.0f, -10000.0f, 10000.0f, o);
    rotationZ(-(float)preRotationDeg * 3.14159265358979f / 180.0f, r); // mesmo sentido validado no S-05
    mul4(r, o, mProjection);
    memcpy(mMatrix, mProjection, sizeof(mMatrix));

    BufferUpdateDesc update = { mDrawBuffers[mFrameIndex] };
    beginUpdateResource(&update);
    mDrawData = (float*)update.pMappedData;
    mDrawCount = 0;

    mScissorEnabled = false;
    mClipMaskEnabled = false;
    mStencilRef = 0;
    mBoundPipeline = nullptr;
    mBoundTexture = UINT32_MAX;
    mDrawSetBound = false; // o set dos draws só pode ser ligado depois de uma pipeline da UI (layout compatível)
    applyScissor();
}

void ForgeRenderInterface::endFrame()
{
    BufferUpdateDesc update = { mDrawBuffers[mFrameIndex] };
    update.pMappedData = mDrawData;
    endUpdateResource(&update);
    mDrawsLastFrame = mDrawCount;
    mMaskOpsLastFrame = mMaskOps;
    mMaskOps = 0;
    mCmd = nullptr;
}

Rml::CompiledGeometryHandle ForgeRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices)
{
    Geometry* g = tf_new(Geometry);
    g->vb = makeBuffer(DESCRIPTOR_TYPE_VERTEX_BUFFER, vertices.data(), vertices.size() * sizeof(Rml::Vertex), "RmlVB");
    g->ib = makeBuffer(DESCRIPTOR_TYPE_INDEX_BUFFER, indices.data(), indices.size() * sizeof(int), "RmlIB");
    g->indexCount = (uint32_t)indices.size();
    return (Rml::CompiledGeometryHandle)g;
}

void ForgeRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
{
    if (geometry)
        mPendingGeometry[mFrameIndex].push_back((Geometry*)geometry);
}

uint32_t ForgeRenderInterface::createTexture(uint32_t width, uint32_t height, const uint8_t* rgba)
{
    uint32_t slot = 0;
    while (slot < kMaxTextures && mTextures[slot].used)
        ++slot;
    if (slot == kMaxTextures)
    {
        LOGF(eERROR, "S07: limite de %u texturas da UI atingido", kMaxTextures);
        return UINT32_MAX;
    }

    // O TF cria image view 1D quando a altura é 1 (o shader espera 2D): duplica a linha.
    std::vector<uint8_t> twoRows;
    if (height == 1)
    {
        twoRows.assign(rgba, rgba + width * 4);
        twoRows.insert(twoRows.end(), rgba, rgba + width * 4);
        rgba = twoRows.data();
        height = 2;
    }

    TextureDesc desc = {};
    desc.mArraySize = 1;
    desc.mDepth = 1;
    desc.mMipLevels = 1;
    desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
    desc.mFormat = TinyImageFormat_R8G8B8A8_UNORM; // RGBA pré-multiplicado, como a RmlUi entrega
    desc.mWidth = width;
    desc.mHeight = height;
    desc.mSampleCount = SAMPLE_COUNT_1;
    desc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    desc.pName = "RmlTexture";
    Texture*        texture = nullptr;
    TextureLoadDesc load = {};
    load.pDesc = &desc;
    load.ppTexture = &texture;
    addResource(&load, nullptr);

    TextureUpdateDesc update = { texture, 0, 1, 0, 1, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
    beginUpdateResource(&update);
    TextureSubresourceUpdate sub = update.getSubresourceUpdateDesc(0, 0);
    for (uint32_t r = 0; r < sub.mRowCount; ++r)
        memcpy(sub.pMappedData + r * sub.mDstRowStride, rgba + r * width * 4, width * 4);
    endUpdateResource(&update);

    DescriptorData params[2] = {};
    params[0].mIndex = SRT_RES_IDX(RmlSrt, Persistent, gTexture);
    params[0].ppTextures = &texture;
    params[1].mIndex = SRT_RES_IDX(RmlSrt, Persistent, gSampler);
    params[1].ppSamplers = &mSampler;
    updateDescriptorSet(mRenderer, slot, mTextureSet, 2, params);

    mTextures[slot] = { texture, true };
    return slot;
}

Rml::TextureHandle ForgeRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions)
{
    const uint32_t slot = createTexture((uint32_t)dimensions.x, (uint32_t)dimensions.y, source.data());
    return slot == UINT32_MAX ? 0 : (Rml::TextureHandle)(slot + 1);
}

Rml::TextureHandle ForgeRenderInterface::LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source)
{
    // Imagens de arquivo entram pelo pipeline de assets da F3; no spike só fontes (GenerateTexture).
    (void)dimensions;
    LOGF(eWARNING, "S07: LoadTexture('%s') ainda não suportado no spike", source.c_str());
    return 0;
}

void ForgeRenderInterface::ReleaseTexture(Rml::TextureHandle texture)
{
    if (!texture)
        return;
    // O slot continua ocupado até sair da fila (kFrames frames): nenhum draw em voo vê o slot reaproveitado.
    mPendingTextures[mFrameIndex].push_back((uint32_t)texture - 1);
}

void ForgeRenderInterface::EnableScissorRegion(bool enable)
{
    mScissorEnabled = enable;
    applyScissor();
}

void ForgeRenderInterface::SetScissorRegion(Rml::Rectanglei region)
{
    mScissor = region;
    applyScissor();
}

void ForgeRenderInterface::applyScissor()
{
    if (!mCmd)
        return;
    if (!mScissorEnabled)
    {
        cmdSetScissor(mCmd, 0, 0, mTargetW, mTargetH);
        return;
    }
    // Retângulo lógico → imagem (com pré-rotação): passa os cantos pela projeção e pega a caixa envolvente.
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    const float xs[2] = { (float)mScissor.Left(), (float)mScissor.Right() };
    const float ys[2] = { (float)mScissor.Top(), (float)mScissor.Bottom() };
    for (float x : xs)
        for (float y : ys)
        {
            const float* m = mProjection;
            const float  nx = m[0] * x + m[4] * y + m[12];
            const float  ny = m[1] * x + m[5] * y + m[13];
            const float  px = (nx + 1.0f) * 0.5f * (float)mTargetW;
            const float  py = (1.0f - ny) * 0.5f * (float)mTargetH;
            minX = std::min(minX, px);
            minY = std::min(minY, py);
            maxX = std::max(maxX, px);
            maxY = std::max(maxY, py);
        }
    const uint32_t x0 = (uint32_t)std::clamp(std::lround(minX), 0l, (long)mTargetW);
    const uint32_t y0 = (uint32_t)std::clamp(std::lround(minY), 0l, (long)mTargetH);
    const uint32_t x1 = (uint32_t)std::clamp(std::lround(maxX), 0l, (long)mTargetW);
    const uint32_t y1 = (uint32_t)std::clamp(std::lround(maxY), 0l, (long)mTargetH);
    cmdSetScissor(mCmd, x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0);
}

void ForgeRenderInterface::SetTransform(const Rml::Matrix4f* transform)
{
    if (transform)
        mul4(mProjection, transform->data(), mMatrix);
    else
        memcpy(mMatrix, mProjection, sizeof(mMatrix));
}

void ForgeRenderInterface::draw(PipelineKind kind, const Geometry& g, Rml::Vector2f translation, uint32_t textureSlot)
{
    if (!mCmd || mDrawCount >= kMaxDrawsPerFrame)
        return;
    float* d = mDrawData + (size_t)mDrawCount * 20;
    memcpy(d, mMatrix, sizeof(mMatrix));
    d[16] = translation.x;
    d[17] = translation.y;
    d[18] = d[19] = 0.0f;

    if (mBoundPipeline != mPipelines[kind])
    {
        cmdBindPipeline(mCmd, mPipelines[kind]);
        mBoundPipeline = mPipelines[kind];
        if (!mDrawSetBound)
        {
            cmdBindDescriptorSet(mCmd, mFrameIndex, mDrawSet);
            mDrawSetBound = true;
        }
    }
    if (mBoundTexture != textureSlot)
    {
        cmdBindDescriptorSet(mCmd, textureSlot, mTextureSet);
        mBoundTexture = textureSlot;
    }
    const uint32_t stride = sizeof(Rml::Vertex);
    cmdBindVertexBuffer(mCmd, 1, (Buffer**)&g.vb, &stride, nullptr);
    cmdBindIndexBuffer(mCmd, g.ib, INDEX_TYPE_UINT32, 0);
    cmdDrawIndexedInstanced(mCmd, g.indexCount, 0, 1, 0, mDrawCount);
    ++mDrawCount;
}

void ForgeRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture)
{
    const uint32_t slot = texture ? (uint32_t)texture - 1 : mWhiteSlot;
    if (mClipMaskEnabled)
        cmdSetStencilReferenceValue(mCmd, mStencilRef);
    draw(mClipMaskEnabled ? kDrawClipped : kDraw, *(const Geometry*)geometry, translation, slot);
}

void ForgeRenderInterface::EnableClipMask(bool enable) { mClipMaskEnabled = enable; }

void ForgeRenderInterface::RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation)
{
    // Mesmo esquema do backend GL3 da RmlUi: Set/SetInverse reescrevem o stencil inteiro; Intersect incrementa.
    auto clearStencil = [&](uint32_t value) {
        float saved[16];
        memcpy(saved, mMatrix, sizeof(saved));
        identity(mMatrix);
        const bool scissor = mScissorEnabled;
        mScissorEnabled = false;
        applyScissor();
        cmdSetStencilReferenceValue(mCmd, value);
        draw(kMaskReplace, *mFullscreenQuad, { 0, 0 }, mWhiteSlot);
        mScissorEnabled = scissor;
        applyScissor();
        memcpy(mMatrix, saved, sizeof(saved));
    };
    const Geometry& g = *(const Geometry*)geometry;
    ++mMaskOps;
    switch (operation)
    {
    case Rml::ClipMaskOperation::Set:
        clearStencil(0);
        cmdSetStencilReferenceValue(mCmd, 1);
        draw(kMaskReplace, g, translation, mWhiteSlot);
        mStencilRef = 1;
        break;
    case Rml::ClipMaskOperation::SetInverse:
        clearStencil(1);
        cmdSetStencilReferenceValue(mCmd, 0);
        draw(kMaskReplace, g, translation, mWhiteSlot);
        mStencilRef = 1;
        break;
    case Rml::ClipMaskOperation::Intersect:
        cmdSetStencilReferenceValue(mCmd, mStencilRef);
        draw(kMaskIncrement, g, translation, mWhiteSlot);
        ++mStencilRef;
        break;
    }
}
