// Spike S-07: Rml::RenderInterface sobre o The Forge 1.63 (Vulkan).
//
// Implementa o núcleo (geometria, texturas, scissor), máscara de recorte por stencil e transformações RCSS.
// Camadas, filtros e shaders da RmlUi 6 ficam para a fatia B do S-07 (a RmlUi desenha sem eles; os efeitos
// correspondentes não aparecem).
//
// Uso (thread de render = thread principal no spike):
//   init(...) uma vez; beginFrame(...) com o passe já aberto (cor + depth/stencil com stencil limpo em 0);
//   context->Render(); endFrame(). A pré-rotação da swapchain (S-05) entra na matriz e no scissor.
#pragma once

#include <RmlUi/Core/RenderInterface.h>

#include <cstdint>
#include <vector>

struct Renderer;
struct Cmd;
struct Shader;
struct Pipeline;
struct Buffer;
struct Texture;
struct Sampler;
struct DescriptorSet;

class ForgeRenderInterface final : public Rml::RenderInterface
{
public:
    static constexpr uint32_t kFrames = 2;
    static constexpr uint32_t kMaxTextures = 1024;
    static constexpr uint32_t kMaxDrawsPerFrame = 16384;

    // colorFormat/depthStencilFormat: TinyImageFormat do alvo onde a UI desenha.
    bool init(Renderer* renderer, uint32_t colorFormat, uint32_t depthStencilFormat);
    void exit();

    // targetW/H: tamanho da imagem (orientação nativa se pré-rotacionada); preRotationDeg: 0/90/180/270.
    void beginFrame(Cmd* cmd, uint32_t frameIndex, uint32_t targetW, uint32_t targetH, uint32_t preRotationDeg);
    void endFrame();

    // Tamanho lógico (o que o usuário vê): use para o Context da RmlUi.
    uint32_t logicalWidth() const { return mLogicalW; }
    uint32_t logicalHeight() const { return mLogicalH; }
    uint32_t drawsLastFrame() const { return mDrawsLastFrame; }
    uint32_t maskOpsLastFrame() const { return mMaskOpsLastFrame; }

    // Rml::RenderInterface
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
    Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;
    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;
    void EnableClipMask(bool enable) override;
    void RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation) override;
    void SetTransform(const Rml::Matrix4f* transform) override;

private:
    struct Geometry
    {
        Buffer*  vb = nullptr;
        Buffer*  ib = nullptr;
        uint32_t indexCount = 0;
    };
    struct TextureSlot
    {
        Texture* texture = nullptr;
        bool     used = false;
    };
    enum PipelineKind
    {
        kDraw,          // sem máscara
        kDrawClipped,   // stencil == ref
        kMaskReplace,   // grava ref no stencil onde a geometria cobre (sem cor)
        kMaskIncrement, // stencil == ref -> ref + 1 (interseção)
        kPipelineCount
    };

    uint32_t createTexture(uint32_t width, uint32_t height, const uint8_t* rgbaPremultiplied);
    void     draw(PipelineKind kind, const Geometry& g, Rml::Vector2f translation, uint32_t textureSlot);
    void     applyScissor();
    void     updateMatrix();
    void     flushDeferredReleases(uint32_t frameIndex);

    Renderer*      mRenderer = nullptr;
    Shader*        mShader = nullptr;
    Pipeline*      mPipelines[kPipelineCount] = {};
    Sampler*       mSampler = nullptr;
    DescriptorSet* mTextureSet = nullptr;
    DescriptorSet* mDrawSet = nullptr;
    Buffer*        mDrawBuffers[kFrames] = {};
    std::vector<TextureSlot> mTextures;
    uint32_t       mWhiteSlot = 0;
    Geometry*      mFullscreenQuad = nullptr;

    // Estado do frame.
    Cmd*      mCmd = nullptr;
    uint32_t  mFrameIndex = 0;
    float*    mDrawData = nullptr;
    uint32_t  mDrawCount = 0;
    uint32_t  mDrawsLastFrame = 0;
    uint32_t  mMaskOps = 0, mMaskOpsLastFrame = 0;
    uint32_t  mTargetW = 0, mTargetH = 0, mLogicalW = 0, mLogicalH = 0, mPreRotation = 0;
    float     mProjection[16] = {};   // pré-rotação * ortho (coluna-maior)
    float     mMatrix[16] = {};       // mProjection * transformação RCSS atual
    bool      mScissorEnabled = false;
    Rml::Rectanglei mScissor;
    bool      mClipMaskEnabled = false;
    uint32_t  mStencilRef = 0;
    Pipeline* mBoundPipeline = nullptr;
    uint32_t  mBoundTexture = UINT32_MAX;
    bool      mDrawSetBound = false;

    // Liberação adiada até a GPU terminar o frame que ainda pode usar o recurso.
    std::vector<Geometry*> mPendingGeometry[kFrames];
    std::vector<uint32_t>  mPendingTextures[kFrames]; // slots: só voltam a ficar livres depois de kFrames frames
};
