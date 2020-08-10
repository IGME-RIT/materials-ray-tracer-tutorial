/*
Title: Advanced Ray Tracer
File Name: FragmentShader.glsl
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

#version 430 // Identifies the version of the shader, this line must be on a separate line from the rest of the shader code

// The uniform variables, these storing the camera position and the four corner rays of the camera's view.
uniform vec3 eye;
uniform vec3 ray00;
uniform vec3 ray01;
uniform vec3 ray10;
uniform vec3 ray11;

// The input textureCoord relative to the quad as given by the Vertex Shader.
in vec2 textureCoord;

// The output of the Fragment Shader, AKA the pixel color.
out vec4 color;

struct light 
{
	vec4 pos;
	vec4 color;
	float radius;
	float brightness;
	float junk1;
	float junk2;
};

// Create some constants
#define MAX_SCENE_BOUNDS 100.0

#define MAX_LIGHTS 5
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

// texture that we will use
uniform sampler2D textureTest[MAX_MESHES];

// A layout describing the vertex buffer.
layout(binding = 0) buffer vertexBlock
{
	Mesh m[MAX_MESHES];
};

layout (binding = 1) buffer lightBlock
{
	light lights[MAX_LIGHTS];
};

struct hitinfo
{
	vec3 point;
	int m;
	int t;
};

// Determines whether or not a ray in a given direction hits a given triangle.
// Returns -1.0 if it does not; otherwise returns the value t at which the ray hits the triangle, which can be used to determine the point of collision.
// p is point on ray, d is ray direction, v0, v1, and v2 are points of the triangle.
float rayIntersectsTriangle(vec3 p, vec3 d, vec3 v0, vec3 v1, vec3 v2)
{
	vec3 e1,e2,h,s,q;
	float a,f,u,v, t;

	// Get two edges of triangle
	e1 = vec3(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
	e2 = vec3(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);
	
	// Cross ray direction with triangle edge
	h = cross(d, e2);
	
	// Dot the other triangle edge with the above cross product
	a = dot(e1, h);

	// If a is zero or realy close to zero, then there's no collision.
	if (a > -0.00001 && a < 0.00001)
	{
		return -1.0;
	}

	// Take the inverse of a.
	f = 1/a;
	
	// Get vector from first triangle vertex toward cameraPos (or in the scope of this function, the vec3 p that is a point on the ray direction)
	s = vec3(p.x - v0.x, p.y - v0.y, p.z - v0.z);
	
	// Dot your s value with your h value from earlier (cross(d, e2)), then multiply by the inverse of a.
	u = f * dot(s, h);

	// If this value is not between 0 and 1, then there's no collision.
	if (u < 0.0 || u > 1.0)
	{
		return -1.0;
	}

	// Cross your s value with edge 1 (e1).
	q = cross(s, e1);

	// Dot the ray direction with this new q value, and then multiply by the inverse of a.
	v = f * dot(d, q);

	// If v is less than 0, or u + v are greater than 1, then there's no collision.
	if (v < 0.0 || u + v > 1.0)
	{
		return -1.0;
	}

	// At this stage we can compute t to find out where the intersection point is on the line
	t = f * dot(e2, q);

	// If t is greater than zero
	if (t > 0.00001)
	{
		// The ray does intersect the triangle, and we return the t value.
		return t;
	}
	
	// Otherwise, there is a line intersection, but not a ray intersection, so we return -1.0.
	return -1.0;
}

bool intersectMeshBox(vec3 origin, vec3 dir, int meshIndex)
{
	for(int i = 0; i < 12; i++)
	{
		// Compute distance d using above function to determine how far along the ray the triangle collides.
		float d = rayIntersectsTriangle(origin, dir, 
			m[meshIndex].collision[i].pos[0].xyz, 
			m[meshIndex].collision[i].pos[1].xyz,
			m[meshIndex].collision[i].pos[2].xyz);

		// If t = -1.0 then there was no intersection
		if(d != -1.0)
		{
			return true;
		}
	}

	return false;
}

bool intersectChunkBox(vec3 origin, vec3 dir, int meshIndex, int chunkIndex)
{
	for(int i = 0; i < 12; i++)
	{
		// Compute distance d using above function to determine how far along the ray the triangle collides.
		float d = rayIntersectsTriangle(origin, dir, 
			m[meshIndex].c[chunkIndex].collision[i].pos[0].xyz, 
			m[meshIndex].c[chunkIndex].collision[i].pos[1].xyz,
			m[meshIndex].c[chunkIndex].collision[i].pos[2].xyz);

		// If t = -1.0 then there was no intersection
		if(d != -1.0)
		{
			return true;
		}
	}

	return false;
}

// Given an origin point, a direction, and a variable to pass information back out to, this will test a ray against every triangle in the scene.
// It will then return true or false, based on whether or not the ray collided with anything.
// If it did, then the hitinfo object will be filled with a point of collision and an index referring to which triangle it intersects with first.
bool intersectTriangles(vec3 origin, vec3 dir, out hitinfo info)
{
	// Start our variables for determining the closest triangle.
	// Smallest will be the smallest distance between the origin point and the point of collision.
	// Found just determines whether or not there was a collision at all.
	float smallest = MAX_SCENE_BOUNDS;
	bool found = false;
	float d = -1.0f;

	for(int i = 0; i < MAX_MESHES; i++)
	{
		// This is level 1 optimization, where it checks
		// the box around the entire mesh, but does NOT
		// check the dividing 8 boxes, which would be oct-tree
		
		bool checkMesh = false;

		// If this mesh has no meshBox,
		// due to being low-poly anyways
		if(m[i].optimizationLevel == 0)
		{
			checkMesh = true;
		}

		// If this is a high poly model
		else
		{
			checkMesh = intersectMeshBox(origin, dir, i);
		}

		// If this ray intersected the mesh's box
		if(checkMesh)
		{
			int triangleIndex;

			if(m[i].optimizationLevel == 2)
			{
				for(int boxID = 0; boxID < 8; boxID++)
				{
					bool checkBox = intersectChunkBox(origin, dir, i, boxID);

					if(checkBox)
					{
						// check all triangles in the mesh
						for(int j = 0; j < m[i].c[boxID].numTrianglesInThisChunk; j++)
						{
							triangleIndex = m[i].c[boxID].triangleIndices[j];
							triangle t = m[i].t[triangleIndex];

							// Optimization to see if the polygon is facing
							// a direction that the ray can hit
							
							if(
								(dot(t.normal[0].xyz, dir) > 0) &&
								(dot(t.normal[1].xyz, dir) > 0) &&
								(dot(t.normal[2].xyz, dir) > 0)
							)
								continue;

							// Compute distance d using above function to determine how far along the ray the triangle collides.
							d = rayIntersectsTriangle(origin, dir, t.pos[0].xyz, t.pos[1].xyz, t.pos[2].xyz);
						
							// If t = -1.0 then there was no intersection, we also ignore it if t is not < smallest, as that would mean we already found a triangle that 
							// was closer (and thus collides first).
							if(d != -1.0 && d < smallest)
							{
								// This t becomes the new smallest.
								smallest = d;

								// color can be found via index as can the normal
								// Thus, we just pass out a point of collision using t and the triangle index.
								info.point = origin + (dir * d);
								info.m = i;
								info.t = triangleIndex;

								// Make sure we set found to true, signifying that the ray collided with something.
								found = true;
							}
						}
					}
				}
			}

			else
			{
				// check all triangles in the mesh
				for(int j = 0; j < m[i].numTriangles; j++)
				{
					triangle t = m[i].t[j];
					triangleIndex = j;

					// Optimization to see if the polygon is facing
					// a direction that the ray can hit
					
					if(
						(dot(t.normal[0].xyz, dir) > 0) &&
						(dot(t.normal[1].xyz, dir) > 0) &&
						(dot(t.normal[2].xyz, dir) > 0)
					)
						continue;

					// Compute distance d using above function to determine how far along the ray the triangle collides.
					d = rayIntersectsTriangle(origin, dir, t.pos[0].xyz, t.pos[1].xyz, t.pos[2].xyz);

					// If t = -1.0 then there was no intersection, we also ignore it if t is not < smallest, as that would mean we already found a triangle that 
					// was closer (and thus collides first).
					if(d != -1.0 && d < smallest)
					{
						// This t becomes the new smallest.
						smallest = d;

						// color can be found via index as can the normal
						// Thus, we just pass out a point of collision using t and the triangle index.
						info.point = origin + (dir * d);
						info.m = i;
						info.t = triangleIndex;

						// Make sure we set found to true, signifying that the ray collided with something.
						found = true;
					}
				}
			}
		}
	}

	return found;
}

vec3 GetInterpolatedNormal(vec3 pointHit, vec3 p1, vec3 p2, vec3 p3, vec3 n1, vec3 n2, vec3 n3)
{
	// Given the 3 points on the triangle,
	// Given the 1 point inside the triangle,
	// Given the 3 normals on the triangle,
	// Find the interpolated normal for that point

	vec3 a = p1;
	vec3 b = p2;
	vec3 c = p3;
	vec3 p = pointHit;

	vec3 v0 = b - a;
	vec3 v1 = c - a; 
	vec3 v2 = p - a;

	float d00 = dot(v0, v0);
	float d01 = dot(v0, v1);
	float d11 = dot(v1, v1);
	float d20 = dot(v2, v0);
	float d21 = dot(v2, v1);
	float denom = d00 * d11 - d01 * d01;
	
	// Barycentric Coordinates of point
	float v = (d11 * d20 - d01 * d21) / denom;
	float w = (d00 * d21 - d01 * d20) / denom;
	float u = 1.0f - v - w;

	// Interpolate Normal
	// Interpolate Normal
	vec3 newNormal = 
		u*n1 + 
		v*n2 + 
		w*n3;

	// This is calculated incorrectly
	// It needs to be fixed so that
	// The car can look smooth
	return normalize(newNormal);
}

vec2 GetInterpolatedUV(vec3 pointHit, vec3 p1, vec3 p2, vec3 p3, vec2 t1, vec2 t2, vec2 t3)
{
	// Given the 3 points on the triangle,
	// Given the 1 point inside the triangle,
	// Given the 3 normals on the triangle,
	// Find the interpolated normal for that point

	vec3 a = p1;
	vec3 b = p2;
	vec3 c = p3;
	vec3 p = pointHit;

	vec3 v0 = b - a;
	vec3 v1 = c - a; 
	vec3 v2 = p - a;

	float d00 = dot(v0, v0);
	float d01 = dot(v0, v1);
	float d11 = dot(v1, v1);
	float d20 = dot(v2, v0);
	float d21 = dot(v2, v1);
	float denom = d00 * d11 - d01 * d01;
	
	// Barycentric Coordinates of point
	float v = (d11 * d20 - d01 * d21) / denom;
	float w = (d00 * d21 - d01 * d20) / denom;
	float u = 1.0f - v - w;

	// Interpolate UV
	vec2 newUV = 
		u*t1 + 
		v*t2 + 
		w*t3;

	// return the texture coordinate
	return newUV;
}

vec4 getSurfaceColor(hitinfo i)
{
	triangle t = m[i.m].t[i.t];

	vec4 triangleColor = vec4(t.color.xyz, 1);

	vec2 uv = GetInterpolatedUV(
		i.point,
		t.pos[0].xyz,
		t.pos[1].xyz,
		t.pos[2].xyz,
		vec2(t.uv[0]),
		vec2(t.uv[1]),
		vec2(t.uv[2])
	);

	return texture(textureTest[i.m], uv.xy) * triangleColor;
}

vec3 addLightColorToPixColor(light L, vec3 dirRayToPoint, hitinfo rayHitPoint)
{
	// get direction from point to light
	vec3 pointToLight = L.pos.xyz - rayHitPoint.point;
	
	// Get the distance from point on surface to light
	float dist = length(pointToLight);

	// if the pixel is outside the range of the 
	// light, return 0. Don't process the light 
	// if the light doesn't touch the pixel anyways
	if(dist > L.radius)
		return vec3(0);

	// normalize the distance, to get direction
	pointToLight = normalize(pointToLight);

	// Create a hitinfo object to store the collision.
	hitinfo lightHitPoint;

	// Now we check to see if any polygons are standing between the point
	// that the ray hit, and the light. If a polygon blocks this new ray from
	// the light, then don't light this pixel (shadow). Otherwise, light it.
	// If you do NOT want shadows, delete the if-statment
	if(intersectTriangles(L.pos.xyz, -pointToLight, lightHitPoint))
	{
		// If the distance from the point to the light is farther than the distance from the light to the first surface it hits
		if(dist - length(L.pos.xyz - lightHitPoint.point) > 0.1)
		{
			// Then this is in shadow, since the light is hitting another object first.
			return vec3(0);
		}
	}

	hitinfo i = rayHitPoint;
	triangle t = m[i.m].t[i.t];

	// Get the interpolated normal for the Point that is hit on the triangle by the ray
	// This normal will be interpolated between all three vertex normals
	vec3 normal = GetInterpolatedNormal(
		rayHitPoint.point, 
		t.pos[0].xyz,
		t.pos[1].xyz,
		t.pos[2].xyz,
		t.normal[0].xyz,
		t.normal[1].xyz,
		t.normal[2].xyz);

	// Get a reflection vector bouncing the light ray off the surface of the triangle.
	// Used for specular light calculations.
	vec3 reflectedRayToPoint = reflect(pointToLight, normal);

	// get the dot product, just like the basic tutorials
	float NdotL = dot(normal, pointToLight);

	// clamp the color
	NdotL = clamp(NdotL, 0.0, 1.0);

	// Formula for range-based attenuation
	float atten = 1.0 - (dist*dist) / (L.radius*L.radius);
	
	// clamp the attenuation
	atten = clamp(atten, 0.0, 1.0);

	// Get the final color of the light on the pixel
	float diffuse = NdotL;

	// 0 by default, for objects that aren't reflective
	float specular = 0.0f;
	
	// get reflectivity level from Mesh
	int maxBounces = m[rayHitPoint.m].reflectionLevel;
	
	// if the object is reflective in any way
	if(maxBounces != 0)
	{
		// Calculate specular and diffuse lighting normally.
		specular = max(0, pow(dot(reflectedRayToPoint, dirRayToPoint), 64));
	}

	// brightness of light
	vec3 brightness = L.brightness * L.color.xyz * atten;

	// color of surface
	vec4 surfaceColor = getSurfaceColor(rayHitPoint);

	// color of surface, multiplied by diffuse lighting
	vec3 finalColor = surfaceColor.xyz * brightness * diffuse;

	// if the object is reflective in any way
	if(maxBounces != 0)
	{
		// add specular lighting
		finalColor += brightness * specular;
	}

	// return the final color of lighting
	return finalColor;
}

vec3 addReflectionToPixColor(light L, vec3 dir, hitinfo rayHitPoint, out bool endEarly)
{
	// default
	endEarly = false;

	// Gets a vector in the direction of the reflected ray.
	vec3 reflectedRayToPoint;

	// We're doing another collision test here to get the reflection.
	// You can see how this starts to get intensive and can slow down your framerate, since every 
	// one of these intersectTriangles calls tests a ray against every triangle in the scene. 
	// There are ways to optimize this (spatial partitioning), but ultimately Ray Tracing is not a 
	// technique for real-time rendering.
	hitinfo reflectHit;
	
	// color that will be added for all reflections
	vec3 color = vec3(0);

	hitinfo h = rayHitPoint;

	// get reflectivity level from Mesh
	int maxBounces = m[h.m].reflectionLevel;

	for(int i = 0; i < maxBounces; i++)
	{
		triangle t = m[h.m].t[h.t];

		// Get the interpolated normal for the Point that is hit on the triangle by the ray
		// This normal will be interpolated between all three vertex normals
		vec3 normal = GetInterpolatedNormal(
			rayHitPoint.point, 
			t.pos[0].xyz,
			t.pos[1].xyz,
			t.pos[2].xyz,
			t.normal[0].xyz,
			t.normal[1].xyz,
			t.normal[2].xyz);

		// Gets a vector in the direction of the reflected ray.
		reflectedRayToPoint = reflect(dir, normal);

		// If the reflected vector hits a triangle.
		// Render the pixel of that triangle
		if(intersectTriangles(rayHitPoint.point, reflectedRayToPoint, reflectHit))
		{
			// If you are reflecting a surface that has no effects
			if(m[reflectHit.m].boolUseEffects == 0)
			{
				// dont calculate lighting, and dont 
				// calculate more reflection bounces

				// return color of that surface
				color += getSurfaceColor(reflectHit).xyz  * pow(0.5, i);
				
				// dont add more reflections
				endEarly = true;
				break;
			}

			// This is the lighting that is in the geometry that is reflected off of other geomtry
			color += addLightColorToPixColor(L, reflectedRayToPoint, reflectHit) * pow(0.5, i);

			if(m[reflectHit.m].reflectionLevel == 0)
			{
				break;
			}

			dir = reflectedRayToPoint;
			rayHitPoint = reflectHit;
		}

		// If we hit nothing
		// exit the loop
		else
		{
			break;
		}
	}
	
	// return final color
	// Skybox can be added to color after calculations
	return color;
}

// Trace a ray from an origin point in a given direction and calculate/return the color value of the point that ray hits.
vec4 trace(vec3 origin, vec3 dirEyeToTriangle)
{
	// Create object to get our hitinfo back out of the intersectTriangles function.
	hitinfo eyeHitTriangle;

	// If this ray intersects any of the triangles in the scene.
	if (intersectTriangles(origin, dirEyeToTriangle, eyeHitTriangle))
	{
		vec4 surfaceColor = getSurfaceColor(eyeHitTriangle);
		
		// If you dont want any effects on this object
		if(m[eyeHitTriangle.m].boolUseEffects == 0)
		{
			// return the color without reflection
			return surfaceColor;
		}

		// Create a pixColor variable, which will determine the output color of this pixel. Start with some ambient light.
		vec3 pixColor;
		
		// If you can reflect
		if(m[eyeHitTriangle.m].reflectionLevel != 0)
		{
			// set ambient occlusion low, and reflect skybox
			pixColor = surfaceColor.xyz * 0.1;
		}

		// If you can't reflect
		else
		{
			// fake ambient occlusion to match sky
			pixColor = surfaceColor.xyz * vec3(0.15, 0.15, 0.3);
		}

		bool endEarly;

		// Loop through each light. By default, we have 4 lights.
		// To use 4 lights, we have "j < MAX_LIGHTS" in our 'for' loop.
		// If you want to use less lights, you can use "j < 1"
		// or "j < 2", to reduce the amount of processing and boost FPS
		for(int j = 0; j < MAX_LIGHTS; j++)
		{
			// color of reflected light
			// This is a combination of the color of the polygon that the eye's ray hit,
			// and the lighting that effects this point (4 lights, shadows, specular, etc)
			// This function returns the geometry color
			vec3 lightColor = addLightColorToPixColor(lights[j], dirEyeToTriangle, eyeHitTriangle);
			
			vec3 reflection = vec3(0);

			if(!endEarly && m[eyeHitTriangle.m].reflectionLevel != 0)
			{
				// color of reflections
				// We get reflection level from the hitinfo
				reflection = addReflectionToPixColor(lights[j], dirEyeToTriangle, eyeHitTriangle, endEarly);

				// multiply reflection by color of the surface.
				// This makes sure that red light doesn't reflect on green surfaces,
				// and makes sure that blue light doesn't reflect on red surfaces, etc
				reflection *= surfaceColor.xyz;
			}

			// Level of Reflectivity:
			// 0.5 = half and half
			// 1.0 = perfect mirror, plus the ambient color
			// 0.0 = no reflection
			float reflectionLevel = 0.5;
	
			// blend the two colors together
			pixColor += mix(lightColor, reflection, reflectionLevel);
		}
		
		// Return the final pixel color.		
		return vec4(pixColor.rgb, 1.0);
	}

	// If the ray doesn't hit any triangles, then this ray sees nothing and thus:
	// Return 0, which can be replaced with skybox
	return vec4(vec3(0), 1.0);
}

void main(void)
{
	// Keep in mind, "textureCoord" does not actually mean textures being mapped onto the surface of geometry,
	// UV coordinates and textures will come in a future tutorial

	// This is easy. Using the textureCoord, you interpolate between the four corner rays to get a ray (dir) that goes through a point in the screen.
	// For your mental image, imagine this shader runes once for every single pixel on your screen.
	// Every time it runs, dir is the ray that goes from the camera's position, through the pixel that it is rendering. Thus, we are tracing a ray through every pixel 
	// on the screen to determine what to render.
	vec2 pos = textureCoord;
	vec3 dir = normalize(mix(mix(ray00, ray01, pos.y), mix(ray10, ray11, pos.y), pos.x));
	color = trace(eye, dir);
}