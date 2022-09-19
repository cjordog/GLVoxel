cmake_minimum_required(VERSION 3.22.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

# set the project name
project(GLVoxel)

file(GLOB GLVOXEL_SRC CONFIGURE_DEPENDS "Source/*.h" "Source/*.cpp")
file(GLOB GLVOXEL_SHADERS CONFIGURE_DEPENDS "Source/Shaders/*.glsl")

# add the executable
add_executable(${PROJECT_NAME} ${GLVOXEL_SRC} ${GLVOXEL_SHADERS})

# FastNoise2 settings
set(FASTNOISE2_NOISETOOL OFF CACHE BOOL "Build Noise Tool" FORCE)

# add one subdirectory for each CMakeLists.txt
add_subdirectory(Libraries/)
add_subdirectory(Libraries/glfw/)
add_subdirectory(Libraries/glm/)
add_subdirectory(Libraries/FastNoise2/)
add_subdirectory(Libraries/tracy/)

target_include_directories(${PROJECT_NAME} PUBLIC Libraries/glfw/include)

# these are for things you want to include #include "Source/world.h" -> #include "world.h"
target_include_directories(${PROJECT_NAME} 
	PUBLIC Source/
	PUBLIC Source/Shaders/
	PUBLIC Libraries/glfw/include
	PUBLIC Libraries/FastNoise2/include
	PUBLIC Libraries/tracy/public/tracy
)

# link directories relative to build folder
target_link_directories(${PROJECT_NAME} 
	PRIVATE Libraries/glfw/src
	PRIVATE Libraries/glm/glm
	PRIVATE Libraries/FastNoise2/src
)


# link libraries
target_link_libraries(${PROJECT_NAME} glfw glm FastNoise2 imgui glad Tracy::TracyClient )

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT GLVoxel)

	#PRIVATE Libraries/tracy/public/common
	#PRIVATE Libraries/tracy/public/client
	#PRIVATE Libraries/tracy/public

