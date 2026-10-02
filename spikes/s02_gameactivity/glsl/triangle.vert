#version 450
// Spike S-04: GLSL escrito à mão (fora do FSL) com o mesmo layout de recursos que o FSL gera a partir de
// spikes/s01_forge_triangle/shaders/Global.srt.h:
//   set 0 (Persistent): binding 0 = gTexture, binding 1 + 200 = gSampler (o TF desloca samplers em 200)
//   set 1 (PerFrame):   binding 0 = gFrame (std140)
layout(std140, set = 1, binding = 0) uniform FrameBlock
{
    mat4 transform;
    vec4 tint;
} gFrame;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 0) out vec2 outTexCoord;

void main()
{
    gl_Position = gFrame.transform * vec4(inPosition, 0.0, 1.0);
    outTexCoord = inTexCoord;
}
