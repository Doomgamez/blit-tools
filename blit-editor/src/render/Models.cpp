#include "render/Models.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "core/Files.h"

// the base colour texture of a material, made into a GL texture
GLuint MaterialTexture(ModelCache& cache, TextureCache& textures, const cgltf_material* material, const std::string& folder)
{
    if (!material || !material->has_pbr_metallic_roughness)
    {
        return textures.white;
    }

    const cgltf_texture* texture = material->pbr_metallic_roughness.base_color_texture.texture;
    const cgltf_image* image = texture ? texture->image : nullptr;
    GLuint id = 0;
    int width = 0, height = 0;

    if (!image)
    {
        return textures.white;
    }

    if (image->buffer_view && image->buffer_view->buffer->data)
    {
        const unsigned char* data = (const unsigned char*)image->buffer_view->buffer->data + image->buffer_view->offset;
        id = LoadImageMemory(data, image->buffer_view->size, width, height);
    }
    else if (image->uri)
    {
        // the engine's archive is flat, so a model's images are found by name wherever they are
        std::string name = FileName(image->uri);
        std::string file = JoinPath(folder, name);

        if (!FileExists(file))
        {
            file = FindTextureFile(textures, FileStem(name));
        }

        if (!file.empty())
        {
            id = LoadImageTexture(file, width, height);
        }
    }

    if (!id)
    {
        return textures.white;
    }

    cache.textures.push_back(id);
    return id;
}

void ReadPrimitive(ModelCache& cache, TextureCache& textures, ModelData& model, const cgltf_primitive& primitive,
                   const Mat4& world, const std::string& folder)
{
    const cgltf_accessor *positions = nullptr, *normals = nullptr, *uvs = nullptr;

    if (primitive.type != cgltf_primitive_type_triangles)
    {
        return;
    }

    for (size_t a = 0; a < primitive.attributes_count; a++)
    {
        const cgltf_attribute& attribute = primitive.attributes[a];

        if (attribute.type == cgltf_attribute_type_position) positions = attribute.data;
        if (attribute.type == cgltf_attribute_type_normal) normals = attribute.data;
        if (attribute.type == cgltf_attribute_type_texcoord && attribute.index == 0) uvs = attribute.data;
    }

    if (!positions)
    {
        return;
    }

    ModelPart part;
    part.texture = MaterialTexture(cache, textures, primitive.material, folder);

    if (primitive.material && primitive.material->has_pbr_metallic_roughness)
    {
        for (int i = 0; i < 4; i++) part.color[i] = primitive.material->pbr_metallic_roughness.base_color_factor[i];
    }

    size_t count = primitive.indices ? primitive.indices->count : positions->count;
    Mat4 normalMatrix = Mat4Transpose(Mat4Inverse(world));

    for (size_t i = 0; i < count; i++)
    {
        size_t index = primitive.indices ? cgltf_accessor_read_index(primitive.indices, i) : i;
        DrawVertex vertex = {};
        float p[3] = { 0, 0, 0 }, n[3] = { 0, 1, 0 }, t[2] = { 0, 0 };

        cgltf_accessor_read_float(positions, index, p, 3);
        if (normals) cgltf_accessor_read_float(normals, index, n, 3);
        if (uvs) cgltf_accessor_read_float(uvs, index, t, 2);

        vertex.position = TransformPoint(world, { p[0], p[1], p[2] });
        vertex.normal = Normalize(TransformDirection(normalMatrix, { n[0], n[1], n[2] }));
        vertex.uv = { t[0], t[1] };
        for (int c = 0; c < 4; c++) vertex.color[c] = part.color[c];

        model.boundsMin = model.parts.empty() && part.vertices.empty() ? vertex.position : Min(model.boundsMin, vertex.position);
        model.boundsMax = model.parts.empty() && part.vertices.empty() ? vertex.position : Max(model.boundsMax, vertex.position);
        part.vertices.push_back(vertex);
    }

    if (!part.vertices.empty())
    {
        model.parts.push_back(std::move(part));
    }
}

const ModelData& GetModel(ModelCache& cache, TextureCache& textures, const std::string& file)
{
    auto found = cache.loaded.find(file);

    if (found != cache.loaded.end())
    {
        return found->second;
    }

    ModelData& model = cache.loaded[file];
    cgltf_options options = {};
    cgltf_data* data = nullptr;

    if (cgltf_parse_file(&options, file.c_str(), &data) != cgltf_result_success)
    {
        model.error = "can't read " + file;
        return model;
    }

    if (cgltf_load_buffers(&options, data, file.c_str()) != cgltf_result_success)
    {
        model.error = "can't read the buffers of " + file;
        cgltf_free(data);
        return model;
    }

    for (size_t n = 0; n < data->nodes_count; n++)
    {
        const cgltf_node& node = data->nodes[n];

        if (!node.mesh)
        {
            continue;
        }

        Mat4 world;
        cgltf_node_transform_world(&node, world.m);

        for (size_t p = 0; p < node.mesh->primitives_count; p++)
        {
            ReadPrimitive(cache, textures, model, node.mesh->primitives[p], world, ParentFolder(file));
        }
    }

    cgltf_free(data);
    model.ok = !model.parts.empty();

    if (!model.ok)
    {
        model.error = file + " has no triangles";
    }

    return model;
}

void FreeModels(ModelCache& cache)
{
    if (!cache.textures.empty())
    {
        glDeleteTextures((GLsizei)cache.textures.size(), cache.textures.data());
    }

    cache.textures.clear();
    cache.loaded.clear();
}
