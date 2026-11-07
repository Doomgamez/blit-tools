#pragma once

// OpenGL 2.1 with framebuffer objects, GLSL 120: what every GPU the engine runs on has

#include <string>
#include <vector>

#include <glad/gl.h>

#include "core/Math.h"

struct Shader
{
    GLuint program = 0;

    GLint Uniform(const char* name) const { return glGetUniformLocation(program, name); }
};

bool CompileStage(GLuint stage, const char* source, std::string& error);
// attributes are bound to locations in the order given, 0, 1, 2...
bool CompileShader(Shader& shader, const char* vertex, const char* fragment, const std::vector<const char*>& attributes,
                   std::string& error);

// a colour texture and a depth buffer the viewport draws into, shown with ImGui::Image
struct Framebuffer
{
    GLuint framebuffer = 0;
    GLuint color = 0;
    GLuint depth = 0;
    int width = 0;
    int height = 0;
};

// makes it, or remakes it at a new size, false if the driver can't
bool ResizeFramebuffer(Framebuffer& target, int width, int height);
void DestroyFramebuffer(Framebuffer& target);

// a texture from RGBA pixels, repeating and mipmapped
GLuint MakeTexture(const unsigned char* rgba, int width, int height, bool smooth);

// what everything in the viewport is drawn with
struct DrawVertex
{
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    float color[4];
};

struct LineVertex
{
    Vec3 position;
    float color[4];
};
