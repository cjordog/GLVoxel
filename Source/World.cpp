#include <fstream>
#include <sstream>
#include <iostream>
#include <glad/glad.h>
#include "World.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

ShaderProgram World::shaderProgram1;

static float vertices[] = {
	// positions          // colors           // texture coords
	 0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f,   // top right
	 0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f,   // bottom right
	-0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,   // bottom left
	-0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f    // top left 
};

static uint indices[] = {
	0, 1, 3, // first triangle
	1, 2, 3  // second triangle
};

World::World()
	: m_camera(glm::vec3(0, 0, -10), 0, 90.0f),
	m_voxelScene()
{

}

bool World::Init()
{
	//glGenVertexArrays(1, &VAO);
	//glGenBuffers(1, &VBO);
	//glGenBuffers(1, &EBO);

	//glBindVertexArray(VAO);
	//glBindBuffer(GL_ARRAY_BUFFER, VBO);
	//glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	//glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	//glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	//glEnableVertexAttribArray(0);
	//glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	//glEnableVertexAttribArray(1);
	//glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	//glEnableVertexAttribArray(2);

	testTex1 = LoadTexture("uv-test.png", PNG);
	testTex2 = LoadTexture("woodbox.jpg", JPG);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	return true;
}

bool World::InitShared()
{
	shaderProgram1 = ShaderProgram("test1.vs.glsl", "test1.fs.glsl");
	VoxelScene::InitShared();
	return true;
}

void World::Render()
{
	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//shaderProgram1.Use();

	//glBindVertexArray(VAO);
	//glBindBuffer(GL_ARRAY_BUFFER, VBO);
	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

	//glActiveTexture(GL_TEXTURE0);
	//glBindTexture(GL_TEXTURE_2D, testTex1);
	//glActiveTexture(GL_TEXTURE1);
	//glBindTexture(GL_TEXTURE_2D, testTex2);

	//glUniform1i(512, 0);
	//glUniform1i(513, 1);

	//glm::mat4 Projection = glm::perspective(glm::radians(45.0f), 4.0f / 3.0f, 0.1f, 100.f);
	////glm::mat4 View = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -10.0f));
	////glm::mat4 Model = glm::rotate(glm::mat4(1.0f), 30.0f, glm::vec3(0.0f, 1.0f, 0.0f));
	//glm::mat4 Model = glm::mat4(1.0f);

	//glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(m_camera.GetViewMatrix() * Model));
	//glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(Projection));

	//glBindVertexArray(VAO);
	//glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	m_voxelScene.Render(&m_camera);
}

void World::Update(InputData* inputData)
{
	m_camera.Transform(inputData->m_moveInput * 0.05f, -inputData->m_mouseInput.y * 0.5f, inputData->m_mouseInput.x * 0.5f);
	m_voxelScene.Update(m_camera.GetPosition());
}

uint World::LoadTexture(const char* image, ImageFormat fmt)
{
	uint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// load and generate the texture
	int width, height, nrChannels;
	char imgPath[128];
	strcpy_s(imgPath, "../Assets/Textures/");
	strcat_s(imgPath, image);

	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(imgPath, &width, &height, &nrChannels, 0);
	if (data)
	{
		switch (fmt)
		{
			case PNG:
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
				break;
			case JPG:
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
				break;
			default:
				break;
		}
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load texture" << std::endl;
	}
	stbi_image_free(data);
	return texture;
}
