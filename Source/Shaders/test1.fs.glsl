#version 460 core
#extension GL_ARB_explicit_uniform_location : enable
out vec4 FragColor;

in vec3 outCol;
in vec2 outUV;

layout (location = 512) uniform sampler2D t1;
layout (location = 513) uniform sampler2D t2;

void main()
{
//	FragColor = texture(t2, outUV);
FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
//	FragColor = mix(texture(t1, outUV), texture(t2, outUV), 0.5);
//    FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);
} 
