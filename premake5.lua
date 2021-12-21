-- The name of your workspace will be used, for example, to name the Visual Studio .sln file generated by Premake.
workspace "GLVoxelProj"
	
	location "Generated"

    configurations { "Debug", "Release" }
    startproject "GLVoxel"

    flags { "MultiProcessorCompile" }

    filter "configurations:Debug"
        defines { "DEBUG", "DEBUG_SHADER" }
        optimize "Off"
        symbols "On"

    filter "configurations:Release"
        defines { "RELEASE" }
        optimize "Speed"
        flags { "LinkTimeOptimization" }

--https://github.com/HectorPeeters/opengl_premake_boilerplate
project "GLVoxel"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
	architecture "x86_64"
--	entrypoint "main"

    includedirs { "Source/*.h", "Libraries/glad/include/", "Libraries/glfw/include/", "Libraries/glm/", "Libraries/imgui/", "Libraries/imgui/examples" }
    
    vpaths { 
        ["Headers"] = "Source/**.h",
        ["Source"] = "Source/**.cpp",
        ["Shaders"] = "Source/Shaders/**.glsl"
    }

    files { "Source/**" }

    links { "GLFW", "GLM", "GLAD", "ImGui" }

    targetdir "Generated/bin/%{cfg.buildcfg}"
    objdir "Generated/obj/%{cfg.buildcfg}"
  --  abspath "J:/Projects/GLVoxel"

    -- postbuildcommands { '{COPY} "J:/Projects/GLVoxel/Source/Shaders/" "J:/Projects/GLVoxel/%{cfg.targetdir}"' }

    filter "system:linux"
        links { "dl", "pthread" }

        defines { "_GL" }

    filter "system:windows"
        defines { "_WINDOWS" }

    filter {}

include "Libraries/glfw.lua"
include "Libraries/glad.lua"
include "Libraries/glm.lua"
include "Libraries/imgui.lua"