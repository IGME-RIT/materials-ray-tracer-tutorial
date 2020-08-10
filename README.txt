Documentation Author: Niko Procopi 2020

This tutorial was designed for Visual Studio 2019
If the solution does not compile, retarget the solution
to a different version of the Windows SDK. If you do not
have any version of the Windows SDK, it can be installed
from the Visual Studio Installer Tool

Welcome to the Ray Tracing Material Properties Tutorial!
Prerequesites: 
	Ray Tracing Octants

Change Mesh to have one variable for optimization, not two,
which requires changes in C++ and Shaders to use one variable

Change Mesh to have reflectivity level, so addReflectionToPixColor
does not take refleectivity level anymore, it grabs reflectivity from Mesh,
which it knows from hitInfo

We also change addLightColorToPixColor to only add specular
lighting to objects that have at least one level of reflectivity

We also add a bool to disable all ray tracing effects, which is used
on the skybox. For debugging purposes, you can disable all effects
on all objects, then it is easy to adjust the scene

Change C++ code in init() to add skybox,
and configure level of ray tracing effects,
also move texture uniforms to init(), not sure
why they weren't there before