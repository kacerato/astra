// Spike S-01: layout de recursos (SRT) compartilhado entre C++ e FSL.
#pragma once

BEGIN_SRT_NO_AB(SrtData)
    BEGIN_SRT_SET(Persistent)
        DECL_TEXTURE(Persistent, Tex2D(float4), gTexture)
        DECL_SAMPLER(Persistent, SamplerState, gSampler)
    END_SRT_SET(Persistent)
    BEGIN_SRT_SET(PerFrame)
        DECL_CBUFFER(PerFrame, CBUFFER(FrameData), gFrame)
    END_SRT_SET(PerFrame)
END_SRT(SrtData)
