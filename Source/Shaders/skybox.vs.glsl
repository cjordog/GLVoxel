#version 460 core
#extension GL_ARB_explicit_uniform_location : enable
layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

layout (location = 0) uniform mat4 viewMat;
layout (location = 1) uniform mat4 projMat;

void main()
{
    TexCoords = aPos;
    // trick to always have a depth of 1 after perspective division, so we can render after everything else
    gl_Position = (projMat * viewMat * vec4(aPos, 1.0)).xyww; 
} 