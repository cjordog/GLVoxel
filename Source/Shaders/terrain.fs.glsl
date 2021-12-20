#version 460 core
#extension GL_ARB_explicit_uniform_location : enable
out vec4 FragColor;

in vec3 outCol;
in vec2 outUV;
in vec3 outNorm;

vec3 sunDir = { 0.1f, -1.0f, 0.2f };

void main()
{
	const float ambientStrength = 0.3f;
	const vec3 ambientColor = vec3(1.0f, 0.0f, 0.0f);
	const vec3 diffuseColor = vec3(1.0f, 0.0f, 0.0f);
	sunDir = normalize(sunDir);

	vec3 diffuse = clamp(dot(-sunDir, outNorm), 0.0f, 1.0f) * diffuseColor * (1.0f - ambientStrength);
	vec3 ambient = ambientColor * ambientStrength;
	FragColor = vec4(ambient + diffuse, 1.0f);
} 
