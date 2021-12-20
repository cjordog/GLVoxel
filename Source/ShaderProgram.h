#pragma once

#include "Common.h"

class ShaderProgram
{
public:
	ShaderProgram() = default;
	ShaderProgram(const char* vShader, const char* fShader);

	void Use();

private:
	enum ShaderType {
		Vertex = 0,
		Fragment = 1,
	};
	uint CompileShader(const char* shaderPath, ShaderType shaderType);
	
	uint m_id = 0;
};