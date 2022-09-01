#pragma once

#include "Common.h"

class RenderSettings
{
public:

	enum class DrawMode : uint
	{
		Triangles = 0,
		Wireframe,
	};

	static RenderSettings& Get();
	
	DrawMode m_drawMode = DrawMode::Triangles;
	bool greedyMesh = false;
	bool deleteMesh = false;
	bool mtEnabled = true;

// https://stackoverflow.com/questions/1008019/c-singleton-design-pattern
private:
	RenderSettings() {};                    // Constructor? (the {} brackets) are needed here.

public:
	RenderSettings(RenderSettings const&) = delete;
	void operator=(RenderSettings const&) = delete;

	// Note: Scott Meyers mentions in his Effective Modern
	//       C++ book, that deleted functions should generally
	//       be public as it results in better error messages
	//       due to the compilers behavior to check accessibility
	//       before deleted status
};