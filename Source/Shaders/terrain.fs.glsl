#version 460 core
#extension GL_ARB_explicit_uniform_location : enable

layout (location = 50) uniform vec3 cameraPos;

out vec4 FragColor;

in vec3 outCol;
in vec2 outUV;
in vec3 outNorm;
in vec4 outPos;

vec3 sunDir = { 0.1f, -1.0f, 0.2f };
float a = 1.0f;
float b = 0.1f;

// https://iquilezles.org/articles/fog/
vec3 ApplyFog( in vec3  rgb,      // original color of the pixel
               in float distance, // camera to point distance
               in vec3  rayOri,   // camera position
               in vec3  rayDir )  // camera to point vector
{
    //vec3  fogColor  = vec3(0.5,0.6,0.7);
    float heightFogAmount = clamp((a/b) * exp(-rayOri.y*b) * (1.0-exp( -distance*rayDir.y*b ))/rayDir.y, 0, 1);
	float distanceFogAmount = clamp(1.0 - exp( -distance*0.001f ), 0, 1);
    float sunAmount = max( dot( rayDir, -sunDir ), 0.0 );
    vec3  fogColor  = mix( vec3(0.5,0.6,0.7), // bluish
                           vec3(1.0,0.9,0.7), // yellowish
                           pow(sunAmount,8.0) );
    float fogAmount = max(heightFogAmount, distanceFogAmount);
    return mix( rgb, fogColor, fogAmount );
}

void main()
{
	const float ambientStrength = 0.3f;
	const vec3 ambientColor = outCol;
	const vec3 diffuseColor = outCol;
	sunDir = normalize(sunDir);

	vec3 diffuse = clamp(dot(-sunDir, outNorm), 0.0f, 1.0f) * diffuseColor * (1.0f - ambientStrength);
	vec3 ambient = ambientColor * ambientStrength;
	vec3 camToFrag = outPos.xyz - cameraPos;
	FragColor = vec4(ApplyFog(ambient + diffuse, length(camToFrag), cameraPos, normalize(camToFrag)), 1.0f);

} 
