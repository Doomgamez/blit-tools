#include "render/Gpu.h"

bool CompileStage(GLuint stage, const char* source, std::string& error)
{
    GLint ok = 0;

    glShaderSource(stage, 1, &source, nullptr);
    glCompileShader(stage);
    glGetShaderiv(stage, GL_COMPILE_STATUS, &ok);

    if (!ok)
    {
        char log[1024];
        glGetShaderInfoLog(stage, sizeof(log), nullptr, log);
        error = log;
    }

    return ok != 0;
}

bool CompileShader(Shader& shader, const char* vertex, const char* fragment, const std::vector<const char*>& attributes,
                   std::string& error)
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    GLint ok = 0;

    if (!CompileStage(vs, vertex, error) || !CompileStage(fs, fragment, error))
    {
        glDeleteShader(vs);
        glDeleteShader(fs);
        return false;
    }

    shader.program = glCreateProgram();
    glAttachShader(shader.program, vs);
    glAttachShader(shader.program, fs);

    for (size_t i = 0; i < attributes.size(); i++)
    {
        glBindAttribLocation(shader.program, (GLuint)i, attributes[i]);
    }

    glLinkProgram(shader.program);
    glGetProgramiv(shader.program, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);

    if (!ok)
    {
        char log[1024];
        glGetProgramInfoLog(shader.program, sizeof(log), nullptr, log);
        error = log;
        return false;
    }

    return true;
}

bool ResizeFramebuffer(Framebuffer& target, int width, int height)
{
    width = width < 1 ? 1 : width;
    height = height < 1 ? 1 : height;

    if (target.framebuffer && target.width == width && target.height == height)
    {
        return true;
    }

    DestroyFramebuffer(target);
    target.width = width;
    target.height = height;

    glGenTextures(1, &target.color);
    glBindTexture(GL_TEXTURE_2D, target.color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenRenderbuffers(1, &target.depth);
    glBindRenderbuffer(GL_RENDERBUFFER, target.depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

    glGenFramebuffers(1, &target.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.color, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target.depth);

    bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return complete;
}

void DestroyFramebuffer(Framebuffer& target)
{
    if (target.framebuffer) glDeleteFramebuffers(1, &target.framebuffer);
    if (target.color) glDeleteTextures(1, &target.color);
    if (target.depth) glDeleteRenderbuffers(1, &target.depth);
    target = Framebuffer();
}

GLuint MakeTexture(const unsigned char* rgba, int width, int height, bool smooth)
{
    GLuint texture = 0;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return texture;
}
