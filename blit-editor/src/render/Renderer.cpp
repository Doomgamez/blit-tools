#include "render/Renderer.h"

#include <algorithm>
#include <cstddef>

const char* const kLitVertex = R"(#version 120
attribute vec3 aPosition;
attribute vec3 aNormal;
attribute vec2 aUV;
attribute vec4 aColor;
uniform mat4 uViewProjection;
varying vec3 vPosition;
varying vec3 vNormal;
varying vec2 vUV;
varying vec4 vColor;
void main()
{
    vPosition = aPosition;
    vNormal = aNormal;
    vUV = aUV;
    vColor = aColor;
    gl_Position = uViewProjection * vec4(aPosition, 1.0);
}
)";

const char* const kLitFragment = R"(#version 120
uniform sampler2D uTexture;
uniform int uLighting;
uniform int uFlat;
uniform vec3 uShadow;
uniform vec3 uSun;
uniform vec3 uSunToward;
uniform int uLightCount;
uniform vec3 uLightPosition[8];
uniform vec3 uLightColor[8];
uniform float uLightRange[8];
varying vec3 vPosition;
varying vec3 vNormal;
varying vec2 vUV;
varying vec4 vColor;
void main()
{
    vec4 color = texture2D(uTexture, vUV) * vColor;
    if (uLighting == 1)
    {
        // the engine's basic.frag: the shadow color, the sun and the point lights, never brighter than the surface
        vec3 n = normalize(vNormal);
        vec3 light = uShadow + (uFlat == 1 ? uSun : uSun * max(dot(n, uSunToward), 0.0));
        for (int i = 0; i < 8; i++)
        {
            if (i >= uLightCount) break;
            vec3 toLight = uLightPosition[i] - vPosition;
            float d = uFlat == 1 ? length(toLight.xy) : length(toLight);
            float fade = clamp(1.0 - (d * d) / (uLightRange[i] * uLightRange[i]), 0.0, 1.0);
            float facing = uFlat == 1 ? 1.0 : max(dot(n, toLight / max(d, 0.0001)), 0.0);
            light += uLightColor[i] * fade * fade * facing;
        }
        color.rgb *= min(light, vec3(1.0));
    }
    gl_FragColor = color;
}
)";

const char* const kFlatVertex = R"(#version 120
attribute vec3 aPosition;
attribute vec4 aColor;
uniform mat4 uViewProjection;
varying vec4 vColor;
void main()
{
    vColor = aColor;
    gl_Position = uViewProjection * vec4(aPosition, 1.0);
}
)";

const char* const kFlatFragment = R"(#version 120
varying vec4 vColor;
void main()
{
    gl_FragColor = vColor;
}
)";

bool InitRenderer(Renderer& renderer)
{
    if (!CompileShader(renderer.lit, kLitVertex, kLitFragment, { "aPosition", "aNormal", "aUV", "aColor" }, renderer.error) ||
        !CompileShader(renderer.flat, kFlatVertex, kFlatFragment, { "aPosition", "aColor" }, renderer.error))
    {
        return false;
    }

    glGenBuffers(1, &renderer.triangleBuffer);
    glGenBuffers(1, &renderer.lineBuffer);
    return true;
}

void FreeRenderer(Renderer& renderer)
{
    glDeleteProgram(renderer.lit.program);
    glDeleteProgram(renderer.flat.program);
    glDeleteBuffers(1, &renderer.triangleBuffer);
    glDeleteBuffers(1, &renderer.lineBuffer);
}

void BeginFrame(Renderer&, Framebuffer& target, const DrawSettings& settings)
{
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glViewport(0, 0, target.width, target.height);
    glClearColor(settings.background[0], settings.background[1], settings.background[2], settings.background[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_SCISSOR_TEST);
}

void SetLights(Renderer& renderer, const SceneMesh& mesh, const DrawSettings& settings)
{
    // the lights nearest the camera, the engine itself has no such limit
    std::vector<const SceneLight*> lights;

    for (const SceneLight& light : mesh.lights) lights.push_back(&light);

    std::sort(lights.begin(), lights.end(), [&](const SceneLight* a, const SceneLight* b)
    {
        return Length(a->position - settings.eye) < Length(b->position - settings.eye);
    });

    int count = std::min((int)lights.size(), kMaxPreviewLights);
    float positions[kMaxPreviewLights * 3], colors[kMaxPreviewLights * 3], ranges[kMaxPreviewLights];

    for (int i = 0; i < count; i++)
    {
        for (int k = 0; k < 3; k++)
        {
            positions[i * 3 + k] = lights[i]->position[k];
            colors[i * 3 + k] = lights[i]->color[k] * lights[i]->intensity;
        }

        ranges[i] = std::max(lights[i]->range, 0.01f);
    }

    glUniform1i(renderer.lit.Uniform("uLightCount"), count);

    if (count)
    {
        glUniform3fv(renderer.lit.Uniform("uLightPosition"), count, positions);
        glUniform3fv(renderer.lit.Uniform("uLightColor"), count, colors);
        glUniform1fv(renderer.lit.Uniform("uLightRange"), count, ranges);
    }
}

void DrawBatch(Renderer& renderer, const TriangleBatch& batch)
{
    if (batch.vertices.empty())
    {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, batch.texture);
    glBindBuffer(GL_ARRAY_BUFFER, renderer.triangleBuffer);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(batch.vertices.size() * sizeof(DrawVertex)), batch.vertices.data(), GL_STREAM_DRAW);

    GLsizei stride = sizeof(DrawVertex);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(DrawVertex, position));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(DrawVertex, normal));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(DrawVertex, uv));
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(DrawVertex, color));
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)batch.vertices.size());
}

void DrawScene(Renderer& renderer, const SceneMesh& mesh, const DrawSettings& settings)
{
    glUseProgram(renderer.lit.program);
    glUniformMatrix4fv(renderer.lit.Uniform("uViewProjection"), 1, GL_FALSE, settings.viewProjection.m);
    glUniform1i(renderer.lit.Uniform("uTexture"), 0);
    glUniform1i(renderer.lit.Uniform("uLighting"), settings.lighting ? 1 : 0);
    glUniform1i(renderer.lit.Uniform("uFlat"), settings.flat ? 1 : 0);
    glUniform3f(renderer.lit.Uniform("uShadow"), settings.shadow.x, settings.shadow.y, settings.shadow.z);
    glUniform3f(renderer.lit.Uniform("uSun"), settings.sun.x, settings.sun.y, settings.sun.z);
    glUniform3f(renderer.lit.Uniform("uSunToward"), settings.sunToward.x, settings.sunToward.y, settings.sunToward.z);
    SetLights(renderer, mesh, settings);
    glActiveTexture(GL_TEXTURE0);

    for (GLuint a = 0; a < 4; a++) glEnableVertexAttribArray(a);

    // solids first, then volumes over them without hiding each other
    for (const TriangleBatch& batch : mesh.batches)
    {
        if (!batch.translucent) DrawBatch(renderer, batch);
    }

    glDepthMask(GL_FALSE);

    for (const TriangleBatch& batch : mesh.batches)
    {
        if (batch.translucent) DrawBatch(renderer, batch);
    }

    glDepthMask(GL_TRUE);

    for (GLuint a = 0; a < 4; a++) glDisableVertexAttribArray(a);
}

void DrawLines(Renderer& renderer, const std::vector<LineVertex>& lines, const DrawSettings& settings, bool depthTest)
{
    if (lines.empty())
    {
        return;
    }

    glUseProgram(renderer.flat.program);
    glUniformMatrix4fv(renderer.flat.Uniform("uViewProjection"), 1, GL_FALSE, settings.viewProjection.m);

    if (!depthTest) glDisable(GL_DEPTH_TEST);

    glBindBuffer(GL_ARRAY_BUFFER, renderer.lineBuffer);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(lines.size() * sizeof(LineVertex)), lines.data(), GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, position));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, color));
    glDrawArrays(GL_LINES, 0, (GLsizei)lines.size());
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    glEnable(GL_DEPTH_TEST);
}

void EndFrame(Renderer&)
{
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
