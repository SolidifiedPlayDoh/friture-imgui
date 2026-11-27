#pragma once
// Minimal OpenGL entry points for the few raw GL calls we make (textures).
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include "imgui.h"
#include "imgui_impl_opengl3_loader.h"
#endif
