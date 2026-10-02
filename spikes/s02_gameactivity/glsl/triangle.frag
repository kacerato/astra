#version 450
// Spike S-04: ver triangle.vert para o layout.
layout(set = 0, binding = 0) uniform texture2D gTexture;
layout(set = 0, binding = 201) uniform sampler gSampler;
layout(std140, set = 1, binding = 0) uniform FrameBlock
{
    mat4 transform;
    vec4 tint;
} gFrame;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(sampler2D(gTexture, gSampler), inTexCoord) * gFrame.tint;
}
