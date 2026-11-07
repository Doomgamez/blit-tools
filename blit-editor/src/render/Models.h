#pragma once

// glTF models for prop_model previews, read with cgltf. kept on the CPU as triangles in the model's space,
// the scene mesh puts them where their nodes are

#include <map>
#include <string>
#include <vector>

#include "render/Gpu.h"
#include "render/Textures.h"

struct ModelPart
{
    GLuint texture = 0;             // the base colour image, the white texture when it has none
    float color[4] = { 1, 1, 1, 1 };
    std::vector<DrawVertex> vertices;   // triangles, three per face
};

struct ModelData
{
    bool ok = false;
    std::string error;
    Vec3 boundsMin{ -0.5f, -0.5f, -0.5f };
    Vec3 boundsMax{ 0.5f, 0.5f, 0.5f };
    std::vector<ModelPart> parts;
};

struct ModelCache
{
    std::map<std::string, ModelData> loaded;   // by file
    std::vector<GLuint> textures;              // what the models made, freed with them
};

// the model in file, loaded once. images named by uri are looked for next to it and in the texture folders
const ModelData& GetModel(ModelCache& cache, TextureCache& textures, const std::string& file);
void FreeModels(ModelCache& cache);

// the loader's steps
struct cgltf_material;
struct cgltf_primitive;
GLuint MaterialTexture(ModelCache& cache, TextureCache& textures, const cgltf_material* material, const std::string& folder);
void ReadPrimitive(ModelCache& cache, TextureCache& textures, ModelData& model, const cgltf_primitive& primitive,
                   const Mat4& world, const std::string& folder);
