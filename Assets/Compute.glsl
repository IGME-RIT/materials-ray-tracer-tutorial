/*
Title: Advanced Ray Tracer
File Name: Compute.glsl
Copyright © 2019
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

// Compute shaders are part of openGL core since version 4.3
#version 430

// This will just run once for each particle.
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

#define MAX_MESHES 10
#define MAX_TRIANGLES_PER_MESH 1486 // biggest mesh is 1486 triangles
#define NUM_TRIANGLES_IN_SCENE 4462 // This is calculated in the console window
#define MAX_TRIANGLES_PER_CHUNK 400

struct triangle 
{
	vec4 pos[3];
	vec4 uv[3];
	vec4 normal[3];
	vec4 color;
};

struct chunk
{
	vec4 min;
	vec4 max;

	int numTrianglesInThisChunk;
	int junk1;
	int junk2;
	int junk3;
	triangle collision[12];

	int triangleIndices[MAX_TRIANGLES_PER_CHUNK];
};

struct Mesh
{
	vec4 min;
	vec4 max;

	int numTriangles;
	int optimizationLevel; // 1 for single box, 2 for octants
	int boolUseEffects;
	int reflectionLevel;
	triangle collision[12];
	
	chunk c[8];
	triangle t[MAX_TRIANGLES_PER_MESH];
};

// A layout describing the vertex buffer.
layout(binding = 0) buffer b0
{
	Mesh m[MAX_MESHES];
} outBuffer;

layout (binding = 1) buffer b1
{
	Mesh m[MAX_MESHES];
} inGeometry;

layout (binding = 2) buffer b2
{
	mat4x4 m[MAX_MESHES];
} inMatrices;

// Declare main program function which is executed when
void main()
{
	// Get the index of this object into the buffer
	uint i = gl_GlobalInvocationID.x;

	int meshIndex = 0;
	uint count = i;


	// Geometry
	// --------------------------

	// count is the triangle index of the mesh
	// that is being processed

	while(count >= inGeometry.m[meshIndex].numTriangles)
	{
		count -= inGeometry.m[meshIndex].numTriangles;
		meshIndex++;

		if(meshIndex == MAX_MESHES) break;
	}

	if(meshIndex < MAX_MESHES)
	{
		for(int j = 0; j < 3; j++)
		{
			// multiply point by model matrix, and then export to fragment shader buffer
			vec4 point = inMatrices.m[meshIndex] * inGeometry.m[meshIndex].t[count].pos[j];
			outBuffer.m[meshIndex].t[count].pos[j] = point;

			// multiply point by model matrix, and then export to fragment shader buffer
			vec3 normal = mat3(inMatrices.m[meshIndex]) * inGeometry.m[meshIndex].t[count].normal[j].xyz;
			outBuffer.m[meshIndex].t[count].normal[j] = vec4(normalize(normal), 1);
		}

		// The color of each mesh, the UV coordinates, and the number of
		// triangles per mesh, are already in the output buffer. An 
		// explanation of how this is possible is written in the main.cpp
		// file in the init() function

		return;
	}

	// Lev1 mesh boxes
	// ----------------------------------------------


	// We have determined that this compute instance is not 
	// handling mesh geometry, so it must be handling the
	// collision boxes

	// reset mesh index array
	meshIndex = 0;

	// get the first mesh with a hitbox
	while(inGeometry.m[meshIndex].optimizationLevel == 0)
	{
		meshIndex++;
	}

	// 12 triangles for each mesh's hitbox
	while(count >= 12)
	{
		// If this mesh has a collision box
		if(inGeometry.m[meshIndex].optimizationLevel > 0)
		{
			count -= 12;
		}

		meshIndex++;

		if(meshIndex == MAX_MESHES) break;
	}

	if(meshIndex < MAX_MESHES)
	{
		for(int j = 0; j < 3; j++)
		{
			// multiply point by model matrix, and then export to fragment shader buffer
			vec4 point = inMatrices.m[meshIndex] * inGeometry.m[meshIndex].collision[count].pos[j];
			outBuffer.m[meshIndex].collision[count].pos[j] = point;
		}

		return;
	}

	// Lev2 division boxes
	// ----------------------------------------------

	// At this point, it must be doing octants

	// reset mesh index array
	meshIndex = 0;

	// get the first mesh with a hitbox
	while(inGeometry.m[meshIndex].optimizationLevel < 2)
	{
		meshIndex++;
	}

	// 12 triangles for each mesh's hitbox
	while(count >= 8*12)
	{
		// If this mesh has a collision box
		if(inGeometry.m[meshIndex].optimizationLevel == 2)
		{
			count -= 8*12;
		}

		meshIndex++;

		if(meshIndex == MAX_MESHES) break;
	}

	meshIndex--;

	if(meshIndex < MAX_MESHES)
	{
		int boxIndex = 0;

		// 12 triangles for each mesh's hitbox
		while(count >= 12)
		{
			count -= 12;
			boxIndex++;

			if(boxIndex == 8) break;
		}

		if(boxIndex < 8)
		{
			for(int j = 0; j < 3; j++)
			{
				// multiply point by model matrix, and then export to fragment shader buffer
				vec4 point = inMatrices.m[meshIndex] * inGeometry.m[meshIndex].c[boxIndex].collision[count].pos[j];
				outBuffer.m[meshIndex].c[boxIndex].collision[count].pos[j] = point;
			}
		}
	}

	// In theory, we'll never be here
}