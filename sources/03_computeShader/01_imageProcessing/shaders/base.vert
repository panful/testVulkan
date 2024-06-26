#version 450

layout(location = 0) in vec2 inPos;
layout(location = 2) in vec2 inTexCoord;

layout(location = 1) out vec2 fragTexCoord;

layout(binding = 9) uniform UniformBufferObject{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() 
{
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos, 0.0, 1.0);

    fragTexCoord = inTexCoord;
}
