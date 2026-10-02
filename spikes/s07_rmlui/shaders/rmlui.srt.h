// Spike S-07: recursos dos shaders da Astra UI (RmlUi) no The Forge.
//   Persistent (set 0): uma entrada por textura (atlas de fonte, imagens); trocada por draw com cmdBindDescriptorSet.
//   PerFrame   (set 1): dados de todos os draws do frame (5 x float4 por draw: 4 colunas da matriz + translação).
#pragma once

BEGIN_SRT_NO_AB(RmlSrt)
    BEGIN_SRT_SET(Persistent)
        DECL_TEXTURE(Persistent, Tex2D(float4), gTexture)
        DECL_SAMPLER(Persistent, SamplerState, gSampler)
    END_SRT_SET(Persistent)
    BEGIN_SRT_SET(PerFrame)
        DECL_BUFFER(PerFrame, Buffer(float4), gDraws)
    END_SRT_SET(PerFrame)
END_SRT(RmlSrt)
