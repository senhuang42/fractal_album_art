// gl.h — single point of OpenGL inclusion.
//
// On macOS (Apple Silicon included) GLFW_INCLUDE_GLCOREARB pulls in the system
// <OpenGL/gl3.h>, which exposes the full OpenGL 3.3 core API directly — no
// function loader (glad/glew) is required. Apple's GL runs on top of Metal and
// supports up to OpenGL 4.1 core.
#pragma once

#ifdef __APPLE__
#  define GLFW_INCLUDE_GLCOREARB
#  define GL_SILENCE_DEPRECATION
#endif
#include <GLFW/glfw3.h>
