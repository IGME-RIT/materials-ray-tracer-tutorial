/*
Title: Basic Ray Tracer
File Name: Main.cpp
Copyright � 2019
Original authors: Niko Procopi
Written under the supervision of David I. Schwartz, Ph.D., and
supported by a professional development seed grant from the B. Thomas
Golisano College of Computing & Information Sciences
(https://www.rit.edu/gccis) at the Rochester Institute of Technology.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <windows.h>
#include <time.h>

#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "FreeImage.h"

#define MAX_LIGHTS 5
#define MAX_TEXTURES 5
#define MAX_MESHES 10
#define MAX_TRIANGLES_PER_MESH 1486 // biggest mesh is 1486 triangles
#define NUM_TRIANGLES_IN_SCENE 4462 // This is calculated in the console window
#define MAX_TRIANGLES_PER_CHUNK 400 // We dont use 400, but this gives room for more

struct triangle {
	glm::vec4 pos[3];
	glm::vec4 uv[3];
	glm::vec4 normal[3];
	glm::vec4 color;
};

struct chunk
{
	glm::vec4 min;
	glm::vec4 max;

	int numTrianglesInThisChunk;
	int junk1;
	int junk2;
	int junk3;
	triangle collision[12];

	int triangleIndices[MAX_TRIANGLES_PER_CHUNK];
};

struct Mesh
{
	glm::vec4 min;
	glm::vec4 max;

	int numTriangles;
	int optimizationLevel; // 1 for single box, 2 for octants
	int boolUseEffects;
	int reflectionLevel;
	triangle collision[12];
	
	chunk chunk[8];
	triangle triangles[MAX_TRIANGLES_PER_MESH];
};

Mesh* meshes;

struct light {
	glm::vec4 pos;
	glm::vec4 color;
	float radius;
	float brightness;
	float junk1;
	float junk2;
};

GLuint trianglesCompToFrag;
int trianglesCompToFragSize = sizeof(Mesh) * MAX_MESHES;

GLuint triangleObjToComp;
int triangleObjToCompSize = sizeof(Mesh) * MAX_MESHES;

GLuint lightToFrag;
int lightToFragSize = sizeof(light) * MAX_LIGHTS;

GLuint matrixBuffer;
int matrixBufferSize = sizeof(glm::mat4x4) * MAX_MESHES;

// optimization
int numMeshesLev1 = 0;
int numMeshesLev2 = 0;

// This is your reference to your shader program.
// This will be assigned with glCreateProgram().
// This program will run on your GPU.
GLuint draw_program;
GLuint transform_program;

// These are your references to your actual compiled shaders
GLuint vertex_shader;
GLuint fragment_shader;
GLuint compute_shader;

// These are your uniform variables.
GLuint eye_loc;		// Specifies where cameraPos is in the GLSL shader
// The rays are the four corner rays of the camera view. See: https://camo.githubusercontent.com/21a84a8b21d6a4bc98b9992e8eaeb7d7acb1185d/687474703a2f2f63646e2e6c776a676c2e6f72672f7475746f7269616c732f3134313230385f676c736c5f636f6d707574652f726179696e746572706f6c6174696f6e2e706e67
GLuint ray00;
GLuint ray01;
GLuint ray10;
GLuint ray11;

// texture information
GLuint tex_loc[MAX_MESHES];
GLuint m_texture[MAX_TEXTURES];
GLuint sampler = 0;

// A variable used to describe the position of the camera.
glm::vec3 cameraPos;

// A reference to our window.
GLFWwindow* window;

// Variables you will need to calculate FPS.
int tempFrame = 0;
int totalFrame = 0;
double dtime = 0.0;
double timebase = 0.0;
double totalTime = 0.0;
int fps = 0;

int width = 1920; 
int height = 1080; 
int videoFPS = 60; 
int videoSeconds = 13;
int maxFrames = videoFPS * videoSeconds;

// This function takes in variables that define the perspective view of the camera, then outputs the four corner rays of the camera's view.
// It takes in a vec3 eye, which is the position of the camera.
// It also takes vec3 center, the position the camera's view is centered on.
// Then it will takes a vec3 up which is a vector that defines the upward direction. (So if you point it down, the camera view will be upside down.)
// Then it takes a float defining the verticle field of view angle. It also takes a float defining the ratio of the screen (in this case, 800/600 pixels).
// The last four parameters are actually just variables for this function to output data into. They should be pointers to pre-defined vec4 variables.
// For a visual reference, see this image: https://camo.githubusercontent.com/21a84a8b21d6a4bc98b9992e8eaeb7d7acb1185d/687474703a2f2f63646e2e6c776a676c2e6f72672f7475746f7269616c732f3134313230385f676c736c5f636f6d707574652f726179696e746572706f6c6174696f6e2e706e67
void calcCameraRays(glm::vec3 eye, glm::vec3 center, glm::vec3 up, float fov, float ratio)
{
	// Grab a ray from the camera position toward where the camera is to be centered on.
	glm::vec3 centerRay = center - eye;

	// w: Vector from center toward eye
	// u: Vector pointing directly right relative to the camera.
	// v: Vector pointing directly upward relative to the camera.

	// Create a w vector which is the opposite of that ray.
	glm::vec3 w = -centerRay;

	// Get the rightward (relative to camera) pointing vector by crossing up with w.
	glm::vec3 u = glm::cross(up, w);

	// Get the upward (relative to camera) pointing vector by crossing the rightward vector with your w vector.
	glm::vec3 v = glm::cross(w, u);

	// We create these two helper variables, as when we rotate the ray about it's relative Y axis (v), we will then need to rotate it about it's relative X axis (u).
	// This means that u has to be rotated by v too, otherwise the rotation will not be accurate. When the ray is rotated about v, so then are it's relative axes.
	glm::vec4 uRotateLeft = glm::vec4(u, 1.0f) * glm::rotate(glm::mat4(1), glm::radians(-fov * ratio / 2.0f), v);
	glm::vec4 uRotateRight = glm::vec4(u, 1.0f) * glm::rotate(glm::mat4(1), glm::radians(fov * ratio / 2.0f), v);

	// Now we simply take the ray and rotate it in each direction to create our four corner rays.
	glm::vec4 r00 = glm::vec4(centerRay, 1.0f) * glm::rotate(glm::mat4(1), glm::radians(-fov * ratio / 2.0f), v) * glm::rotate(glm::mat4(1), glm::radians(fov / 2.0f), glm::vec3(uRotateLeft));
	glm::vec4 r01 = glm::vec4(centerRay, 1.0f) * glm::rotate(glm::mat4(1), glm::radians(-fov * ratio / 2.0f), v) * glm::rotate(glm::mat4(1), glm::radians(-fov / 2.0f), glm::vec3(uRotateLeft));
	glm::vec4 r10 = glm::vec4(centerRay, 1.0f) * glm::rotate(glm::mat4(1), glm::radians(fov * ratio / 2.0f), v) * glm::rotate(glm::mat4(1), glm::radians(fov / 2.0f), glm::vec3(uRotateRight));
	glm::vec4 r11 = glm::vec4(centerRay, 1.0f) * glm::rotate(glm::mat4(1), glm::radians(fov * ratio / 2.0f), v) * glm::rotate(glm::mat4(1), glm::radians(-fov / 2.0f), glm::vec3(uRotateRight));

	// Now set the uniform variables in the shader to match our camera variables (cameraPos = eye, then four corner rays)
	glUniform3f(eye_loc, eye.x, eye.y, eye.z);
	glUniform3f(ray00, r00.x, r00.y, r00.z);
	glUniform3f(ray01, r01.x, r01.y, r01.z);
	glUniform3f(ray10, r10.x, r10.y, r10.z);
	glUniform3f(ray11, r11.x, r11.y, r11.z);
}

// This function runs every frame
void renderScene()
{
	// Used for FPS
	dtime = glfwGetTime();
	totalTime = dtime;

	// Every second, basically.
	if (dtime - timebase > 1)
	{
		// Calculate the FPS and set the window title to display it.
		fps = tempFrame / (int)(dtime - timebase);
		timebase = dtime;
		tempFrame = 0;

		std::string s = 
			"FPS: " + std::to_string(fps) + 
			" Frame: " + std::to_string(totalFrame) + 
			" / " + std::to_string(maxFrames);

		glfwSetWindowTitle(window, s.c_str());
	}

	// set camera position
	cameraPos = glm::vec3(
		0.0f,
		6.0f,
		10.0f
	);

	// There are two different ways of animating. We can 
	// animate with respect to the time elapsed in the program, or we can
	// animate with respect to the time elapsed in the video. 
	
	// If we want to test our animations, without waiting for the full 
	// video to render: lower the resolution, disable reflections, then we 
	// can render a real-time animation with totalTimeElapsedInProgram. This 
	// works, even if the computer renders less than 60 frames per second

	// After we have finished testing our animations, we can enable all of
	// our quality settings (reflections, resolution, etc), and then change
	// the elapsed time to totalTimeElapsedInVideo, and then all animations
	// should look correct in the final video.

	// Sometimes while prototyping, the real-time render will have a longer
	// duration than the actual rendered video, keep that in mind while testing

	float totalTimeElapsedInVideo = (float)totalFrame / videoFPS;
	float totalTimeElapsedInProgram = (float)totalTime;

	// choose which one you want here
	float time = totalTimeElapsedInVideo;

	//=================================================================

	// start using transform program
	glUseProgram(transform_program);

	glm::mat4x4 test[MAX_MESHES];
	
	// scale the floor
	test[0] = glm::mat4(1);
	test[0] = glm::translate(test[0],glm::vec3(0, -0.5, 0));
	test[0] = glm::scale(test[0], glm::vec3(2.5f));

	// move and rotate the cube
	test[1] = glm::mat4(1);
	test[1] = glm::translate(test[1], glm::vec3(5 * sin(time), 1.5, -5));
	test[1] = glm::rotate(test[1], -time, glm::vec3(0, 1, 0));
	test[1] = glm::scale(test[1], glm::vec3((3 + sin(time * 2)/2.0f)) * glm::vec3(1.25));

	// car
	test[2] = glm::mat4(1);
	test[2] = glm::translate(test[2], glm::vec3(-3, -0.25f, 0));
	test[2] = glm::rotate(test[2], time / 2, glm::vec3(0, 1, 0));

	// four wheels on the car
	glm::vec3 wheelPos[4];

	// Front Left
	wheelPos[0][0] = 0.870f;
	wheelPos[0][1] = 0.180f;
	wheelPos[0][2] = 1.530f;

	// Back left
	wheelPos[1][0] = 0.870f;
	wheelPos[1][1] = 0.180f;
	wheelPos[1][2] = -1.580f;

	// Back right
	wheelPos[2][0] = -0.870f;
	wheelPos[2][1] = 0.180f;
	wheelPos[2][2] = -1.580f;

	// Front right
	wheelPos[3][0] = -0.870f;
	wheelPos[3][1] = 0.180f;
	wheelPos[3][2] = 1.530f;

	// Move all 4 wheels
	for (int i = 0; i < 4; i++)
	{
		test[3 + i] = test[2];
		test[3 + i] = glm::translate(test[3 + i], wheelPos[i]);
		test[3 + i] = glm::rotate(test[3 + i], time * 3, glm::vec3(1, 0, 0));
	}
	
	// cat
	test[7] = glm::mat4(1);
	test[7] = glm::translate(test[7], glm::vec3(0, -0.5f, 2));
	test[7] = glm::rotate(test[7], -time, glm::vec3(0, 1, 0));
	test[7] = glm::scale(test[7], glm::vec3(2));

	// dog
	test[8] = glm::mat4(1);
	test[8] = glm::translate(test[8], glm::vec3(4, -0.5f, 0));
	test[8] = glm::rotate(test[8], -time, glm::vec3(0, 1, 0));
	test[8] = glm::scale(test[8], glm::vec3(2));

	// sky
	test[9] = glm::mat4(1);
	test[9] = glm::translate(test[9], cameraPos - glm::vec3(0, 10, 0));
	test[9] = glm::scale(test[9], glm::vec3(100));

	glBindBuffer(GL_UNIFORM_BUFFER, matrixBuffer);
	glBufferData(GL_UNIFORM_BUFFER, matrixBufferSize, test, GL_DYNAMIC_DRAW); // static because CPU won't touch it
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, trianglesCompToFrag);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, triangleObjToComp);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, matrixBuffer);
	glDispatchCompute(NUM_TRIANGLES_IN_SCENE + numMeshesLev1*12 + (numMeshesLev2+1)*8*12, 1, 1);

	//=================================================================

	// start using draw program
	glUseProgram(draw_program);

	light lights[MAX_LIGHTS];

	// white light
	lights[0].color = glm::vec4(1.0, 1.0, 1.0, 0.0);
	lights[0].radius = 7;
	lights[0].brightness = 1;

	lights[0].pos = glm::vec4(
		2 * sin(time),
		4,
		2 * cos(time),
		0
	);

	// red light
	lights[1].color = glm::vec4(1.0, 0.0, 0.0, 0.0);
	lights[1].radius = 4;
	lights[1].brightness = 2;

	lights[1].pos = glm::vec4(
		4 * cos(time),
		1,
		4,
		0
	);

	// blue light
	lights[2].color = glm::vec4(0.0, 0.0, 1.0, 0.0);
	lights[2].radius = 4;
	lights[2].brightness = 2;

	lights[2].pos = glm::vec4(
		-6,
		1,
		4 * cos(time),
		0
	);

	// yellow light
	lights[3].color = glm::vec4(1.0, 1.0, 0.0, 0.0);
	lights[3].radius = 3;
	lights[3].brightness = 1;

	lights[3].pos = glm::vec4(
		-4 * cos(time),
		1,
		-8,
		0
	);

	// blue light
	lights[4].color = glm::vec4(0.0, 1.0, 0.0, 0.0);
	lights[4].radius = 4;
	lights[4].brightness = 2;

	lights[4].pos = glm::vec4(
		6,
		1,
		-4 * cos(time),
		0
	);

	glBindBuffer(GL_UNIFORM_BUFFER, lightToFrag); // 'lights' is a pointer
	glBufferData(GL_UNIFORM_BUFFER, lightToFragSize, lights, GL_DYNAMIC_DRAW); // static because CPU won't touch it
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, trianglesCompToFrag);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, lightToFrag);

	// Call the function we created to calculate the corner rays.
	// We use the camera position, the focus position, and the up direction (just like glm::lookAt)
	// We use Field of View, and aspect ratio (just like glm::perspective)
	calcCameraRays(cameraPos, glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 45.0f, (float)width / height);

	// Draw an image on the screen
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	// help us keep track of FPS
	tempFrame++;
	totalFrame++;
}

// This method reads the text from a file.
// Realistically, we wouldn't want plain text shaders hardcoded in, we'd rather read them in from a separate file so that the shader code is separated.
std::string readShader(std::string fileName)
{
	std::string shaderCode;
	std::string line;

	// We choose ifstream and std::ios::in because we are opening the file for input into our program.
	// If we were writing to the file, we would use ofstream and std::ios::out.
	std::ifstream file(fileName, std::ios::binary);

	// This checks to make sure that we didn't encounter any errors when getting the file.
	if (!file.good())
	{
		std::cout << "Can't read file: " << fileName.data() << std::endl;

		// Return so we don't error out.
		return "";
	}

	// Get size of file
	file.seekg(0, std::ios::end);					
	shaderCode.resize((unsigned int)file.tellg());	
	file.seekg(0, std::ios::beg);					

	// Dump the file into our array
	file.read(&shaderCode[0], shaderCode.size());

	// close the file
	file.close();

	return shaderCode;
}

// This method will consolidate some of the shader code we've written to return a GLuint to the compiled shader.
// It only requires the shader source code and the shader type.
GLuint createShader(std::string sourceCode, GLenum shaderType)
{
	// glCreateShader, creates a shader given a type (such as GL_VERTEX_SHADER) and returns a GLuint reference to that shader.
	GLuint shader = glCreateShader(shaderType);
	const char *shader_code_ptr = sourceCode.c_str(); // We establish a pointer to our shader code string
	const int shader_code_size = sourceCode.size();   // And we get the size of that string.

	// glShaderSource replaces the source code in a shader object
	// It takes the reference to the shader (a GLuint), a count of the number of elements in the string array (in case you're passing in multiple strings), a pointer to the string array
	// that contains your source code, and a size variable determining the length of the array.
	glShaderSource(shader, 1, &shader_code_ptr, &shader_code_size);
	glCompileShader(shader); // This just compiles the shader, given the source code.

	GLint isCompiled = 0;

	// Check the compile status to see if the shader compiled correctly.
	glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);

	if (isCompiled == GL_FALSE)
	{
		char infolog[1024];
		glGetShaderInfoLog(shader, 1024, NULL, infolog);

		if (shaderType == GL_VERTEX_SHADER)
			printf("Vertex Shader\n");

		if (shaderType == GL_FRAGMENT_SHADER)
			printf("Fragment Shader\n");

		if (shaderType == GL_COMPUTE_SHADER)
			printf("Compute Shader\n");

		// Print the compile error.
		std::cout << "The shader failed to compile with the error:" << std::endl << infolog << std::endl;

		// Provide the infolog in whatever manor you deem best.
		// Exit with failure.
		glDeleteShader(shader); // Don't leak the shader.

		// NOTE: I almost always put a break point here, so that instead of the program continuing with a deleted/failed shader, it stops and gives me a chance to look at what may
		// have gone wrong. You can check the console output to see what the error was, and usually that will point you in the right direction.
	}

	return shader;
}

void loadOBJ(char* path, Mesh* m)
{
	// Part 1
	// Initialize variables and pointers

	FILE *f = fopen(path, "r");
	//delete path;

	float x[3];
	unsigned short y[9];

	std::vector<float>pos;
	std::vector<float>uvs;
	std::vector<float>norms;
	std::vector<unsigned short>faces;
	char line[100];


	// Part 2
	// Fill vectors for pos, uvs, norms, and faces

	while (fgets(line, sizeof(line), f))
	{
		if (sscanf(line, "v %f %f %f", &x[0], &x[1], &x[2]) == 3)
			for (int i = 0; i < 3; i++)
				pos.push_back(x[i]);

		if (sscanf(line, "vt %f %f", &x[0], &x[1]) == 2)
			for (int i = 0; i < 2; i++)
				uvs.push_back(x[i]);

		if (sscanf(line, "vn %f %f %f", &x[0], &x[1], &x[2]) == 3)
			for (int i = 0; i < 3; i++)
				norms.push_back(x[i]);

		if (sscanf(line, "f %hu/%hu/%hu %hu/%hu/%hu %hu/%hu/%hu", &y[0], &y[1], &y[2], &y[3], &y[4], &y[5], &y[6], &y[7], &y[8]) == 9)
			for (int i = 0; i < 9; i++)
				faces.push_back(y[i] - 1);
	}


	// Part 3
	// Initialize more variables and pointers

	int numVerts = 3 * (int)faces.size() / 9;

	m->numTriangles = numVerts / 3;

	// Part 4
	// Build final Vertex Buffer

	// for every triangle
	for (int i = 0; i < (int)numVerts / 3; i++)
	{
		// for every point
		for (int j = 0; j < 3; j++)
		{
			// X, Y, and Z
			for (int k = 0; k < 3; k++)
			{
				int coordIndex = 3 * faces[9 * i + 3 * j + 0] + k;
				m->triangles[i].pos[j][k] = pos[coordIndex];
			}

			for (int k = 0; k < 2; k++)
			{
				int uvIndex = 2 * faces[9 * i + 3 * j + 1] + k;
				m->triangles[i].uv[j][k] = uvs[uvIndex];
			}

			for (int k = 0; k < 3; k++)
			{
				int normalIndex = 3 * faces[9 * i + 3 * j + 2] + k;
				m->triangles[i].normal[j][k] = norms[normalIndex];
			}

			m->triangles[i].pos[j][3] = 1.0f;
			m->triangles[i].normal[j][3] = 1.0f;
		}

		m->triangles[i].color = glm::vec4(1.0, 1.0, 1.0, 1.0);
	}

	fclose(f);

}

void GetTrianglesInChunk(Mesh* m, int chunkIndex)
{
	m->chunk[chunkIndex].numTrianglesInThisChunk = 0;

	// Check every triangle in mesh
	for (int i = 0; i < m->numTriangles; i++)
	{
		// check every point in every triangle
		for (int j = 0; j < 3; j++)
		{
			// If any point of the triangle is in this chunk

			if (
				(m->triangles[i].pos[j].x <= m->chunk[chunkIndex].max.x) &&
				(m->triangles[i].pos[j].x >= m->chunk[chunkIndex].min.x) &&

				(m->triangles[i].pos[j].y <= m->chunk[chunkIndex].max.y) &&
				(m->triangles[i].pos[j].y >= m->chunk[chunkIndex].min.y) &&

				(m->triangles[i].pos[j].z <= m->chunk[chunkIndex].max.z) &&
				(m->triangles[i].pos[j].z >= m->chunk[chunkIndex].min.z)
			   )
			{
				m->chunk[chunkIndex].triangleIndices[m->chunk[chunkIndex].numTrianglesInThisChunk++] = i;

				// skip to next triangle
				j = 3;
			}
		}
	}
}

void MakeBox(triangle* t, glm::vec4 min, glm::vec4 max)
{
	// -x side part 1
	t[0].pos[0] = glm::vec4(min.x, min.y, min.z, 1.0);
	t[0].pos[1] = glm::vec4(min.x, min.y, max.z, 1.0);
	t[0].pos[2] = glm::vec4(min.x, max.y, max.z, 1.0);

	// -x side part 2
	t[1].pos[0] = glm::vec4(min.x, min.y, min.z, 1.0);
	t[1].pos[1] = glm::vec4(min.x, max.y, min.z, 1.0);
	t[1].pos[2] = glm::vec4(min.x, max.y, max.z, 1.0);

	// +x side part 1
	t[2].pos[0] = glm::vec4(max.x, min.y, min.z, 1.0);
	t[2].pos[1] = glm::vec4(max.x, min.y, max.z, 1.0);
	t[2].pos[2] = glm::vec4(max.x, max.y, max.z, 1.0);

	// +x side part 2
	t[3].pos[0] = glm::vec4(max.x, min.y, min.z, 1.0);
	t[3].pos[1] = glm::vec4(max.x, max.y, min.z, 1.0);
	t[3].pos[2] = glm::vec4(max.x, max.y, max.z, 1.0);

	// -y side part 1
	t[4].pos[0] = glm::vec4(min.x, min.y, min.z, 1.0);
	t[4].pos[1] = glm::vec4(min.x, min.y, max.z, 1.0);
	t[4].pos[2] = glm::vec4(max.x, min.y, max.z, 1.0);

	// -y side part 2
	t[5].pos[0] = glm::vec4(min.x, min.y, min.z, 1.0);
	t[5].pos[1] = glm::vec4(max.x, min.y, min.z, 1.0);
	t[5].pos[2] = glm::vec4(max.x, min.y, max.z, 1.0);

	// +y side part 1
	t[6].pos[0] = glm::vec4(min.x, max.y, min.z, 1.0);
	t[6].pos[1] = glm::vec4(min.x, max.y, max.z, 1.0);
	t[6].pos[2] = glm::vec4(max.x, max.y, max.z, 1.0);

	// +y side part 2
	t[7].pos[0] = glm::vec4(min.x, max.y, min.z, 1.0);
	t[7].pos[1] = glm::vec4(max.x, max.y, min.z, 1.0);
	t[7].pos[2] = glm::vec4(max.x, max.y, max.z, 1.0);

	// -z side part 1
	t[8].pos[0] = glm::vec4(min.x, min.y, min.z, 1.0);
	t[8].pos[1] = glm::vec4(min.x, max.y, min.z, 1.0);
	t[8].pos[2] = glm::vec4(max.x, max.y, min.z, 1.0);

	// -z side part 2
	t[9].pos[0] = glm::vec4(min.x, min.y, min.z, 1.0);
	t[9].pos[1] = glm::vec4(max.x, min.y, min.z, 1.0);
	t[9].pos[2] = glm::vec4(max.x, max.y, min.z, 1.0);

	// +z side part 1
	t[10].pos[0] = glm::vec4(min.x, min.y, max.z, 1.0);
	t[10].pos[1] = glm::vec4(min.x, max.y, max.z, 1.0);
	t[10].pos[2] = glm::vec4(max.x, max.y, max.z, 1.0);

	// +z side part 2
	t[11].pos[0] = glm::vec4(min.x, min.y, max.z, 1.0);
	t[11].pos[1] = glm::vec4(max.x, min.y, max.z, 1.0);
	t[11].pos[2] = glm::vec4(max.x, max.y, max.z, 1.0);
}

void OptimizeMesh(Mesh* m, int meshIndex)
{
	// add 1 level of optimization if we have 50+ triangles
	m->optimizationLevel += m->numTriangles >= 50;

	// add 1 level of optimization if we have 350+ triangles
	m->optimizationLevel += m->numTriangles >= 350;

	// If this mesh is too small for optimization, quit the function
	if (m->optimizationLevel == 0)
	{
		printf("Mesh %d, optimization level %d, triangles %d\n",
			meshIndex, 0, m->numTriangles);

		return;
	}

	// count how many meshes use lev1 optimization
	numMeshesLev1++;

	// Set min and max positions to the first point, to give
	// us something to start with
	m->min = m->triangles->pos[0];
	m->max = m->triangles->pos[0];

	// loop through all triangles, find min and max
	// position of the entire mesh, for meshBox
	for (int i = 0; i < m->numTriangles; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			if (m->triangles[i].pos[j].x < m->min.x) m->min.x = m->triangles[i].pos[j].x;
			if (m->triangles[i].pos[j].x > m->max.x) m->max.x = m->triangles[i].pos[j].x;

			if (m->triangles[i].pos[j].y < m->min.y) m->min.y = m->triangles[i].pos[j].y;
			if (m->triangles[i].pos[j].y > m->max.y) m->max.y = m->triangles[i].pos[j].y;

			if (m->triangles[i].pos[j].z < m->min.z) m->min.z = m->triangles[i].pos[j].z;
			if (m->triangles[i].pos[j].z > m->max.z) m->max.z = m->triangles[i].pos[j].z;
		}
	}

	// make box based on min and max
	MakeBox(&m->collision[0], m->min, m->max);

	// If you dont need the mesh chopped into 4 chunks, quit now
	if (m->optimizationLevel == 1)
	{
		printf("Mesh %d, optimization level %d, triangles %d\n",
			meshIndex, 1, m->numTriangles);

		return;
	}

	// count how many meshes use lev2 optimization
	numMeshesLev2++;

	// calculate chunk min and max

	// get the midpoint of the box
	glm::vec4 midpoint = glm::vec4(
		m->max.x - ((m->max.x - m->min.x) / 2.0f),
		m->max.y - ((m->max.y - m->min.y) / 2.0f),
		m->max.z - ((m->max.z - m->min.z) / 2.0f),
		1.0f
	);

	int id = 0;

	// 0: -x, -y, -z
	m->chunk[id].min = glm::vec4(m->min.x, m->min.y, m->min.z, 1.0f);
	m->chunk[id].max = glm::vec4(midpoint.x, midpoint.y, midpoint.z, 1.0f);

	id++;

	// 1: -x, +y, -z
	m->chunk[id].min = glm::vec4(m->min.x, midpoint.y, m->min.z, 1.0f);
	m->chunk[id].max = glm::vec4(midpoint.x, m->max.y, midpoint.z, 1.0f);

	id++;

	// 2: +x, -y, -z
	m->chunk[id].min = glm::vec4(midpoint.x, m->min.y, m->min.z, 1.0f);
	m->chunk[id].max = glm::vec4(m->max.x, midpoint.y, midpoint.z, 1.0f);

	id++;

	// 3: +x, +y, -z
	m->chunk[id].min = glm::vec4(midpoint.x, midpoint.y, m->min.z, 1.0f);
	m->chunk[id].max = glm::vec4(m->max.x, m->max.y, midpoint.z, 1.0f);

	id++;

	// 4: -x, -y, +z
	m->chunk[id].min = glm::vec4(m->min.x, m->min.y, midpoint.z, 1.0f);
	m->chunk[id].max = glm::vec4(midpoint.x, midpoint.y, m->max.z, 1.0f);

	id++;

	// 5: -x, +y, +z
	m->chunk[id].min = glm::vec4(m->min.x, midpoint.y, midpoint.z, 1.0f);
	m->chunk[id].max = glm::vec4(midpoint.x, m->max.y, m->max.z, 1.0f);

	id++;

	// 6: +x, -y, +z
	m->chunk[id].min = glm::vec4(midpoint.x, m->min.y, midpoint.z, 1.0f);
	m->chunk[id].max = glm::vec4(m->max.x, midpoint.y, m->max.z, 1.0f);

	id++;

	// 7: +x, +y, +z
	m->chunk[id].min = glm::vec4(midpoint.x, midpoint.y, midpoint.z, 1.0f);
	m->chunk[id].max = glm::vec4(m->max.x, m->max.y, m->max.z, 1.0f);

	for (int i = 0; i < 8; i++)
	{
		GetTrianglesInChunk(m, i);
		MakeBox(&m->chunk[i].collision[0], m->chunk[i].min, m->chunk[i].max);
		printf("Chunk %d: %d\n", i, m->chunk[i].numTrianglesInThisChunk);
	}

	printf("Mesh %d, optimization level %d, triangles %d\n",
		meshIndex, 2, m->numTriangles);
}

void LoadTexture(char* file, int index)
{
	// Load the file.
	FIBITMAP* bitmap = FreeImage_Load(FreeImage_GetFileType(file), file);
	// Convert the file to 32 bits so we can use it.
	FIBITMAP* bitmap32 = FreeImage_ConvertTo32Bits(bitmap);

	// Create an OpenGL texture.
	glGenTextures(1, &m_texture[index]);
	glActiveTexture(GL_TEXTURE0 + m_texture[index]);
	glBindTexture(GL_TEXTURE_2D, m_texture[index]);

	// common parameters for all textures
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// make the sampler if it does not exist
	if (sampler == 0)
		glGenSamplers(1, &sampler);

	// bind the sampler to the texture
	glBindSampler(m_texture[index], sampler);

	// GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT = 34047
	// GL_TEXTURE_MAX_ANISOTROPY_EXT = 34046

	GLfloat maxAnisotropy = 0.0f;
	glGetFloatv(34047, &maxAnisotropy);

	// Trilinear Mipmapping
	glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(sampler, 34046, (GLint)maxAnisotropy);

	// Fill our openGL side texture object.
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, FreeImage_GetWidth(bitmap32), FreeImage_GetHeight(bitmap32),
		0, GL_BGRA, GL_UNSIGNED_BYTE, static_cast<void*>(FreeImage_GetBits(bitmap32)));
	glGenerateMipmap(GL_TEXTURE_2D);

	// We can unload the images now that the texture data has been buffered with opengl
	FreeImage_Unload(bitmap);
	FreeImage_Unload(bitmap32);
}

// Initialization code
void init()
{
	glewExperimental = GL_TRUE;
	// Initializes the glew library
	glewInit();

	// Read in the shader code from a file.
	std::string vertShader = readShader("../Assets/VertexShader.glsl");
	std::string fragShader = readShader("../Assets/FragmentShader.glsl");
	std::string compShader = readShader("../Assets/Compute.glsl");

	// createShader consolidates all of the shader compilation code
	vertex_shader = createShader(vertShader, GL_VERTEX_SHADER);
	fragment_shader = createShader(fragShader, GL_FRAGMENT_SHADER);
	compute_shader = createShader(compShader, GL_COMPUTE_SHADER);

	// A shader is a program that runs on your GPU instead of your CPU. In this sense, OpenGL refers to your groups of shaders as "programs".
	// Using glCreateProgram creates a shader program and returns a GLuint reference to it.
	draw_program = glCreateProgram();
	glAttachShader(draw_program, vertex_shader);		// This attaches our vertex shader to our program.
	glAttachShader(draw_program, fragment_shader);	// This attaches our fragment shader to our program.
	glLinkProgram(draw_program);					// Link the program
	// End of shader and program creation

	// Tell our code to use the program
	glUseProgram(draw_program);

	// This gets us a reference to the uniform variables in the vertex shader, which are called by the same name here as in the shader.
	// We're using these variables to define the camera. The eye is the camera position, and teh rays are the four corner rays of what the camera sees.
	// Only 2 parameters required: A reference to the shader program and the name of the uniform variable within the shader code.
	eye_loc = glGetUniformLocation(draw_program, "eye");
	ray00 = glGetUniformLocation(draw_program, "ray00");
	ray01 = glGetUniformLocation(draw_program, "ray01");
	ray10 = glGetUniformLocation(draw_program, "ray10");
	ray11 = glGetUniformLocation(draw_program, "ray11");

	char* word = (char*)malloc(100);

	for (int i = 0; i < MAX_MESHES; i++)
	{
		sprintf(word, "textureTest[%d]", i);
		tex_loc[i] = glGetUniformLocation(draw_program, word);
	}

	delete word;

	glEnable(GL_TEXTURE_2D);

	// Load Texture ========================================

	LoadTexture((char*)"../Assets/texture.jpg", 0);
	LoadTexture((char*)"../Assets/CarColor.png", 1);
	LoadTexture((char*)"../Assets/CatColor.png", 2);
	LoadTexture((char*)"../Assets/DogColor.png", 3);
	LoadTexture((char*)"../Assets/night1.png", 4);

	// =====================================================

	transform_program = glCreateProgram();
	glAttachShader(transform_program, compute_shader);
	glLinkProgram(transform_program);					// Link the program
	// End of shader and program creation

	glGenBuffers(1, &matrixBuffer);
	glBindBuffer(GL_UNIFORM_BUFFER, matrixBuffer);
	glBufferData(GL_UNIFORM_BUFFER, matrixBufferSize, nullptr, GL_DYNAMIC_DRAW); // static because CPU won't touch it
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	meshes = new Mesh[MAX_MESHES];

	meshes[0].numTriangles = 2;
	meshes[0].triangles[0].pos[0] = glm::vec4(-5.0, 0.0, 5.0, 1.0); 
	meshes[0].triangles[0].pos[1] = glm::vec4(-5.0, 0.0, -5.0, 1.0);
	meshes[0].triangles[0].pos[2] = glm::vec4(5.0, 0.0, -5.0, 1.0);
	meshes[0].triangles[0].uv[0] = glm::vec4(0, 1, 1, 1);
	meshes[0].triangles[0].uv[1] = glm::vec4(0, 0, 1, 1);
	meshes[0].triangles[0].uv[2] = glm::vec4(1, 0, 1, 1);
	meshes[0].triangles[0].normal[0] = glm::vec4(0.0, 1.0, 0.0, 1.0); 
	meshes[0].triangles[0].color = glm::vec4(1.0, 1.0, 1.0, 1.0);

	meshes[0].triangles[1].pos[0] = glm::vec4(-5.0, 0.0, 5.0, 1.0);
	meshes[0].triangles[1].pos[1] = glm::vec4(5.0, 0.0, -5.0, 1.0);
	meshes[0].triangles[1].pos[2] = glm::vec4(5.0, 0.0, 5.0, 1.0);
	meshes[0].triangles[1].uv[0] = glm::vec4(0, 1, 1, 1);
	meshes[0].triangles[1].uv[1] = glm::vec4(1, 0, 1, 1);
	meshes[0].triangles[1].uv[2] = glm::vec4(1, 1, 1, 1);
	meshes[0].triangles[1].normal[0] = glm::vec4(0.0, 1.0, 0.0, 1.0);
	meshes[0].triangles[1].color = glm::vec4(1.0, 1.0, 1.0, 1.0);

	// Mesh 0 is a plane
	// It should have one normal per triangle
	// dulicate the first normal we give it
	for (int i = 0; i < meshes[0].numTriangles; i++)
	{
		meshes[0].triangles[i].normal[1] = meshes[0].triangles[i].normal[0];
		meshes[0].triangles[i].normal[2] = meshes[0].triangles[i].normal[0];
	}

	meshes[1].numTriangles = 12;
	meshes[1].triangles[0].pos[0] = glm::vec4(-0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[0].pos[1] = glm::vec4(0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[0].pos[2] = glm::vec4(-0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[0].uv[0] = glm::vec4(0, 0, 1, 1);
	meshes[1].triangles[0].uv[1] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[0].uv[2] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[0].normal[0] = glm::vec4(0.0, 0.0, -1.0, 1.0);
	meshes[1].triangles[0].color = glm::vec4(1.0, 0.5, 0.2, 1.0);
		   
	meshes[1].triangles[1].pos[0] = glm::vec4(0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[1].pos[1] = glm::vec4(0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[1].pos[2] = glm::vec4(-0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[1].uv[0] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[1].uv[1] = glm::vec4(1, 1, 1, 1);
	meshes[1].triangles[1].uv[2] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[1].normal[0] = glm::vec4(0.0, 0.0, -1.0, 1.0);
	meshes[1].triangles[1].color = glm::vec4(1.0, 0.5, 0.2, 1.0);
		   
	meshes[1].triangles[2].pos[0] = glm::vec4(-0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[2].pos[1] = glm::vec4(-0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[2].pos[2] = glm::vec4(0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[2].uv[0] = glm::vec4(0, 0, 1, 1);
	meshes[1].triangles[2].uv[1] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[2].uv[2] = glm::vec4(1, 1, 1, 1);
	meshes[1].triangles[2].normal[0] = glm::vec4(0.0, 0.0, 1.0, 1.0);
	meshes[1].triangles[2].color = glm::vec4(1.0, 0.5, 0.2, 1.0);
		   
	meshes[1].triangles[3].pos[0] = glm::vec4(-0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[3].pos[1] = glm::vec4(0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[3].pos[2] = glm::vec4(0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[3].uv[0] = glm::vec4(0, 0, 1, 1);
	meshes[1].triangles[3].uv[1] = glm::vec4(1, 1, 1, 1);
	meshes[1].triangles[3].uv[2] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[3].normal[0] = glm::vec4(0.0, 0.0, 1.0, 1.0);
	meshes[1].triangles[3].color = glm::vec4(1.0, 0.5, 0.2, 1.0);
		   
	meshes[1].triangles[4].pos[0] = glm::vec4(0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[4].pos[1] = glm::vec4(0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[4].pos[2] = glm::vec4(0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[4].uv[0] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[4].uv[1] = glm::vec4(1, 1, 1, 1);
	meshes[1].triangles[4].uv[2] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[4].normal[0] = glm::vec4(1.0, 0.0, 0.0, 1.0);
	meshes[1].triangles[4].color = glm::vec4(1.0, 0.5, 0.2, 1.0);
		   
	meshes[1].triangles[5].pos[0] = glm::vec4(0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[5].pos[1] = glm::vec4(0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[5].pos[2] = glm::vec4(0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[5].uv[0] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[5].uv[1] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[5].uv[2] = glm::vec4(0, 0, 1, 1);
	meshes[1].triangles[5].normal[0] = glm::vec4(1.0, 0.0, 0.0, 1.0);
	meshes[1].triangles[5].color = glm::vec4(1.0, 0.5, 0.2, 1.0);

	meshes[1].triangles[6].pos[0] = glm::vec4(-0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[6].pos[1] = glm::vec4(-0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[6].pos[2] = glm::vec4(-0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[6].uv[0] = glm::vec4(0, 0, 1, 1);
	meshes[1].triangles[6].uv[1] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[6].uv[2] = glm::vec4(1, 1, 1, 1);
	meshes[1].triangles[6].normal[0] = glm::vec4(-1.0, 0.0, 0.0, 1.0);
	meshes[1].triangles[6].color = glm::vec4(1.0, 0.5, 0.2, 1.0);

	meshes[1].triangles[7].pos[0] = glm::vec4(-0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[7].pos[1] = glm::vec4(-0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[7].pos[2] = glm::vec4(-0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[7].uv[0] = glm::vec4(0, 0, 1, 1);
	meshes[1].triangles[7].uv[1] = glm::vec4(1, 1, 1, 1);
	meshes[1].triangles[7].uv[2] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[7].normal[0] = glm::vec4(-1.0, 0.0, 0.0, 1.0);
	meshes[1].triangles[7].color = glm::vec4(1.0, 0.5, 0.2, 1.0);

	meshes[1].triangles[8].pos[0] = glm::vec4(-0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[8].pos[1] = glm::vec4(-0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[8].pos[2] = glm::vec4(0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[8].uv[0] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[8].uv[1] = glm::vec4(0, 0, 1, 1);
	meshes[1].triangles[8].uv[2] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[8].normal[0] = glm::vec4(0.0, 1.0, 0.0, 1.0);
	meshes[1].triangles[8].color = glm::vec4(1.0, 0.5, 0.2, 1.0);

	meshes[1].triangles[9].pos[0] = glm::vec4(-0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[9].pos[1] = glm::vec4(0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[9].pos[2] = glm::vec4(0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[9].uv[0] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[9].uv[1] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[9].uv[2] = glm::vec4(1, 1, 1, 1);
	meshes[1].triangles[9].normal[0] = glm::vec4(0.0, 1.0, 0.0, 1.0);
	meshes[1].triangles[9].color = glm::vec4(1.0, 0.5, 0.2, 1.0);

	meshes[1].triangles[10].pos[0] = glm::vec4(-0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[10].pos[1] = glm::vec4(-0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[10].pos[2] = glm::vec4(0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[10].uv[0] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[10].uv[1] = glm::vec4(0, 0, 1, 1);
	meshes[1].triangles[10].uv[2] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[10].normal[0] = glm::vec4(0.0, -1.0, 0.0, 1.0);
	meshes[1].triangles[10].color = glm::vec4(1.0, 0.5, 0.2, 1.0);

	meshes[1].triangles[11].pos[0] = glm::vec4(-0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[11].pos[1] = glm::vec4(0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[11].pos[2] = glm::vec4(0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[11].uv[0] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[11].uv[1] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[11].uv[2] = glm::vec4(1, 1, 1, 1);
	meshes[1].triangles[11].normal[0] = glm::vec4(0.0, -1.0, 0.0, 1.0);
	meshes[1].triangles[11].color = glm::vec4(1.0, 0.5, 0.2, 1.0);

	// Mesh 1 is a cube
	// It should have one normal per triangle
	// dulicate the first normal we give it
	for (int i = 0; i < meshes[1].numTriangles; i++)
	{
		meshes[1].triangles[i].normal[1] = meshes[1].triangles[i].normal[0];
		meshes[1].triangles[i].normal[2] = meshes[1].triangles[i].normal[0];
	}

	// Mesh 2 is a car
	// It will have one normal per vertex
	loadOBJ((char*)"../Assets/GreenCar14.3Dobj", &meshes[2]);
	loadOBJ((char*)"../Assets/wheel.3Dobj", &meshes[3]);
	
	// copy one wheel to make 4 wheels
	for (int i = 0; i < 3; i++)
		memcpy(&meshes[4+i], &meshes[3], sizeof(Mesh));

	loadOBJ((char*)"../Assets/cat.3Dobj", &meshes[7]);
	loadOBJ((char*)"../Assets/dog.3Dobj", &meshes[8]);
	loadOBJ((char*)"../Assets/Skybox.3Dobj", &meshes[9]);

	// Give Template texture to quad
	glUniform1i(tex_loc[0], m_texture[0]);

	// Give Template texture to cube
	glUniform1i(tex_loc[1], m_texture[0]);

	// Give Car texture to car
	glUniform1i(tex_loc[2], m_texture[1]);

	// Give Car texture to wheel
	for (int i = 0; i < 4; i++)
		glUniform1i(tex_loc[3 + i], m_texture[1]);

	// Give Cat texture to cat
	glUniform1i(tex_loc[7], m_texture[2]);

	// Give Dog texture to dog
	glUniform1i(tex_loc[8], m_texture[3]);

	// skybox texture
	glUniform1i(tex_loc[9], m_texture[4]);

	// set ray tracing properties to default values
	for (int i = 0; i < MAX_MESHES; i++)
	{
		meshes[i].boolUseEffects = 1;
		meshes[i].reflectionLevel = 2;
	}

	// Change properties based on individual meshes

	// sky
	meshes[9].boolUseEffects = 0;
	meshes[9].reflectionLevel = 0;

	// animals
	meshes[7].reflectionLevel = 0; // cat
	meshes[8].reflectionLevel = 0; // dog

	// car wheels will probably only reflect ground
	for (int i = 3; i < 7; i++)
		meshes[i].reflectionLevel = 1;

// By default, this should be 0. By setting it to 1, you disable
// lighting and relfection, then it's easier to change the scene,
// for adding objects, changing animaitons, etc
#define DEBUG_RAYTRACE 0

#if DEBUG_RAYTRACE
	
	// Disable all lighting and reflection
	for (int i = 0; i < MAX_MESHES; i++)
	{
		meshes[i].boolUseEffects = 0;
		meshes[i].reflectionLevel = 0;
	}
#endif

	for (int i = 0; i < MAX_MESHES; i++)
	{
		OptimizeMesh(&meshes[i], i);
	}

	int totalTri = 0;
	int biggestMesh = 0;
	
	for (int i = 0; i < MAX_MESHES; i++)
	{
		int n = meshes[i].numTriangles;

		if (biggestMesh < n)
			biggestMesh = n;

		totalTri += n;
	}

	printf("\n");
	printf("Num Meshes: %d\n", MAX_MESHES);
	printf("Max Triangles Per Mesh: %d\n", biggestMesh);
	printf("Total triangles in scene: %d\n", totalTri);
	printf("Lev1: %d\n", numMeshesLev1);
	printf("Lev2: %d\n", numMeshesLev2);

	// This sends our OBJ data to the Compute Shader
	// This data will be constant, and it will never be modified
	glGenBuffers(1, &triangleObjToComp);
	glBindBuffer(GL_UNIFORM_BUFFER, triangleObjToComp);
	glBufferData(GL_UNIFORM_BUFFER, triangleObjToCompSize, meshes, GL_STATIC_DRAW); // static because CPU won't touch it
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	// This sends our OBJ data to the Fragment Shader
	// Some of this data will not be modified, like the number
	// of triangles per mesh, and the color of each mesh, but
	// the vertices will be modified and overwritten by the 
	// compute shader, then send the modifications to the fragment shader
	glGenBuffers(1, &trianglesCompToFrag);
	glBindBuffer(GL_UNIFORM_BUFFER, trianglesCompToFrag);
	glBufferData(GL_UNIFORM_BUFFER, trianglesCompToFragSize, meshes, GL_STATIC_DRAW); // static because CPU won't touch it
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glGenBuffers(1, &lightToFrag);
	glBindBuffer(GL_UNIFORM_BUFFER, lightToFrag);
	glBufferData(GL_UNIFORM_BUFFER, lightToFrag, nullptr, GL_DYNAMIC_DRAW); // static because CPU won't touch it
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void window_size_callback(GLFWwindow* window, int w, int h)
{
	width = w;
	height = h;
	glViewport(0, 0, width, height);
}

int main(int argc, char **argv)
{
	// Initializes the GLFW library
	glfwInit();

	// Creates a window given (width, height, title, monitorPtr, windowPtr).
	// Don't worry about the last two, as they have to do with controlling which monitor to display on and having a reference to other windows. Leaving them as nullptr is fine.
	window = glfwCreateWindow(width, height, "", nullptr, nullptr);

	// This allows us to resize the window when we want to
	glfwSetWindowSizeCallback(window, window_size_callback);

	// Makes the OpenGL context current for the created window.
	glfwMakeContextCurrent(window);

	// Sets the number of screen updates to wait before swapping the buffers.
	glfwSwapInterval(1);

	// Initializes most things needed before the main loop
	init();

	// Make the BYTE array, factor of 3 because it's RGB.
	// This will hold each screenshot
	unsigned char* pixels = new unsigned char[3 * width * height];

	// Create a place for fileName
	char* fileName = (char*)malloc(100);

	// I finally made a boolean for this
	// because I got tired of commenting
	// and uncommenting the code to export
	// the frames and video files
	bool saveVideo = true;

	if (saveVideo)
	{
		// This creates the folder, only if it does
		// not already exist, called "exportedFrames"
		CreateDirectoryA("exportedFrames", NULL);
	}

	// record what time the rendering started
	clock_t start = clock();

	// continue rendering until the desired
	// number of frames are hit

	while (totalFrame != maxFrames)
	{
		// Call the render function.
		renderScene();

		// Swaps the back buffer to the front buffer
		// Remember, you're rendering to the back buffer, then once rendering is complete, you're moving the back buffer to the front so it can be displayed.
		glfwSwapBuffers(window);

		// Checks to see if any events are pending and then processes them.
		glfwPollEvents();

		// If you don't want to save screenshots,
		// then use "continue" to restart the loop
		if (!saveVideo)
			continue;

		// get the image that was rendered
		// We use BGR format, because BMP images use BGR
		glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, pixels);

		// make the name of the current file
		sprintf(fileName, "exportedFrames/%d.png", totalFrame);

		// Convert to FreeImage format & save to file
		FIBITMAP* image = FreeImage_ConvertFromRawBits(pixels, width, height, 3 * width, 24, 0xFF0000, 0x00FF00, 0x0000FF, false);
		FreeImage_Save(FIF_PNG, image, fileName, 0);
		FreeImage_Unload(image);
	}

	// record what time the rendering ended
	clock_t end = clock();

	// how many seconds it took to render
	float totalTime = (float)(end - start) / 1000.0f;

	// print statistics
	printf("\n%d frames rendered in %f seconds, %f FPS\n\n", maxFrames, totalTime, (float)maxFrames / totalTime);

	// After the program is over, cleanup your data!
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	glDeleteProgram(draw_program);
	delete[] pixels;

	// Frees up GLFW memory
	glfwTerminate();
	
	// make space for a command
	char* command = (char*)malloc(1000);

	// build the command with proper FPS
	sprintf(command, "ffmpeg -r %d -i exportedFrames/%%d.png -q 0 test.avi", videoFPS);

	// give the command to build the video
	if(saveVideo)
		system(command);

	return 0;
}
