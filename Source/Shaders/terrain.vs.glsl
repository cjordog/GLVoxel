#version 460 core
#extension GL_ARB_explicit_uniform_location : enable
layout (location = 0) in uint inData;

layout (location = 0) uniform mat4 worldMat;
layout (location = 1) uniform mat4 viewMat;
layout (location = 2) uniform mat4 projMat;

out VS_OUT
{
	vec3 color;
	vec2 uv;
	vec3 normal;
	vec4 position;
} vs_out;

vec3 colorArray[] = 
{
	{ 1.0f, 0.0f, 0.0f },
	{ 0.7f, 0.39f, 0.11f },
	{ 0.0f, 0.8f, 0.1f },
	{ 0.7f, 0.7f, 0.7f },
	{ 0.76f, 0.7f, 0.5f },
	{ 0.1f, 0.4f, 0.8f },
};

vec3 blockNormals[] = {
	{ 1, 0, 0 },	// Right
	{ -1, 0, 0 },	// Left
	{ 0, 1, 0 },	// Top
	{ 0, -1, 0 },	// Bottom
	{ 0, 0, 1 },	// Front 
	{ 0, 0, -1 },	// Back
};

void main()
{
	//uint packedVertex = (0x3F & localVertexPos.x) | (0xFC0 & (localVertexPos.y << 6)) | (0x3F000 & (localVertexPos.z << 12)) | (0x1C0000 & (i << 18)) | (0x1FE00000 & (uint8_t(currentBlockType) << 21));
	vec3 localPos = vec3(uint(inData & 0x3Fu), (inData & 0xFC0u) >> 6u, (inData & 0x3F000u) >> 12u);
	uint normalIndex = (inData & 0x1C0000u) >> 18u;
	uint blockType = (inData & 0x1FE00000u) >> 21u;
	vs_out.position = worldMat * vec4(localPos, 1.0f);
    gl_Position = projMat * viewMat * vs_out.position;
	vs_out.color = colorArray[blockType];
	vs_out.uv = vec2(0, 0);
	// need to be transformed into worldspace, fine now since theres no rotation
	vs_out.normal = blockNormals[normalIndex];
}
