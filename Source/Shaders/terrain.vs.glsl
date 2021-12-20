#version 460 core
#extension GL_ARB_explicit_uniform_location : enable
layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inCol;
layout (location = 2) in vec3 inNorm;

layout (location = 0) uniform mat4 worldViewMat;
layout (location = 1) uniform mat4 projMat;

out vec3 outCol;
out vec2 outUV;
out vec3 outNorm;

void main()
{
    gl_Position = projMat * worldViewMat * vec4(inPos, 1.0);
	outCol = inCol;
	outUV = vec2(0, 0);
	// need to be transformed into worldspace, fine now since theres no rotation
	outNorm = normalize(inNorm);
}
