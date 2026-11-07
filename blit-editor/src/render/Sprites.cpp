#include "render/Sprites.h"

#include <algorithm>
#include <cmath>

#include "geometry/Brush.h"

void SpriteFrame(const Element& sprite, float pictureWidth, float pictureHeight, float out[4])
{
    bool region = sprite.useRegion && sprite.region[2] > 0 && sprite.region[3] > 0;
    float x = region ? sprite.region[0] : 0, y = region ? sprite.region[1] : 0;
    float w = region ? sprite.region[2] : pictureWidth, h = region ? sprite.region[3] : pictureHeight;
    int columns = std::max(1, sprite.columns), rows = std::max(1, sprite.rows);
    int frame = std::clamp(sprite.frame, 0, columns * rows - 1);

    out[2] = w / columns;
    out[3] = h / rows;
    out[0] = x + (frame % columns) * out[2];
    out[1] = y + (frame / columns) * out[3];
}

Vec2 SpriteTiles(const Element& sprite, Vec2 areaSize, Vec2 scale, const float frame[4])
{
    switch (sprite.fit)
    {
    case SpriteFit::Tiled:
        return { std::max(sprite.tiles.x, 0.01f), std::max(sprite.tiles.y, 0.01f) };
    case SpriteFit::Repeat:
        // the area as it is in the level, in frames of the picture's own size
        return { std::max(areaSize.x * std::fabs(scale.x) / std::max(frame[2], 1.0f), 0.01f),
                 std::max(areaSize.y * std::fabs(scale.y) / std::max(frame[3], 1.0f), 0.01f) };
    default:
        return { 1, 1 };
    }
}

SpriteLayout LayOutSprite(const Node& node, const Element& sprite, const TextureInfo& picture)
{
    SpriteLayout layout;
    bool any = false;

    SpriteFrame(sprite, (float)picture.width, (float)picture.height, layout.frame);

    for (const Element& element : node.elements)
    {
        Vec3 a, b;

        if (element.type == ElementType::Brush && ElementBounds(element, a, b))
        {
            layout.lo = any ? Min(layout.lo, a) : a;
            layout.hi = any ? Max(layout.hi, b) : b;
            any = true;
        }
    }

    if (!any)
    {
        layout.lo = { -layout.frame[2] / 2.0f, -layout.frame[3] / 2.0f, 0 };
        layout.hi = { layout.frame[2] / 2.0f, layout.frame[3] / 2.0f, 0 };
    }

    layout.tiles = SpriteTiles(sprite, { layout.hi.x - layout.lo.x, layout.hi.y - layout.lo.y },
                               { node.scale.x, node.scale.y }, layout.frame);
    return layout;
}

void SpriteArea(const Node& node, const Element& sprite, SceneContext& context, Vec3& lo, Vec3& hi)
{
    SpriteLayout layout = LayOutSprite(node, sprite, GetTexture(*context.textures, sprite.texture));
    lo = layout.lo;
    hi = layout.hi;
}

void AddSpriteTile(TriangleBatch& batch, const Element& sprite, const Mat4& world, const float corners[4],
                   const float uv[4], float z)
{
    // corners and uv: x0 y0 x1 y1, the top left of the tile is the top left of its part of the frame
    Vec3 points[4] = { { corners[0], corners[1], z }, { corners[2], corners[1], z }, { corners[2], corners[3], z },
                       { corners[0], corners[3], z } };
    Vec2 uvs[4] = { { uv[0], uv[1] }, { uv[2], uv[1] }, { uv[2], uv[3] }, { uv[0], uv[3] } };
    const int order[6] = { 0, 1, 2, 0, 2, 3 };

    for (int i : order)
    {
        DrawVertex v = {};
        v.position = TransformPoint(world, points[i]);
        // just in front of its box. what's on top is the node's z, the scene draws 2D in that order
        v.position.z -= 0.05f;
        v.normal = { 0, 0, -1 };
        v.uv = uvs[i];
        v.color[0] = sprite.color.x / 255.0f;
        v.color[1] = sprite.color.y / 255.0f;
        v.color[2] = sprite.color.z / 255.0f;
        v.color[3] = 1.0f;
        batch.vertices.push_back(v);
    }
}

void AddSprite(SceneMesh& mesh, const Node& node, const Element& element, const Mat4& world, SceneContext& context)
{
    const TextureInfo& picture = GetTexture(*context.textures, element.texture);
    TriangleBatch& batch = BatchFor(mesh, picture.id, true);
    SpriteLayout layout = LayOutSprite(node, element, picture);
    Vec2 tiles = layout.tiles;

    // too many to draw: fewer, bigger tiles, so it still looks tiled
    while (std::ceil(tiles.x) * std::ceil(tiles.y) > kMaxSpriteTiles) tiles = { tiles.x * 0.5f, tiles.y * 0.5f };

    float cellW = (layout.hi.x - layout.lo.x) / tiles.x, cellH = (layout.hi.y - layout.lo.y) / tiles.y;
    float pw = (float)std::max(picture.width, 1), ph = (float)std::max(picture.height, 1);
    const float* f = layout.frame;
    int across = (int)std::ceil(tiles.x - 1e-4f), down = (int)std::ceil(tiles.y - 1e-4f);

    for (int j = 0; j < down; j++)
    {
        for (int i = 0; i < across; i++)
        {
            // the last tile in a row or column shows only part of the frame
            float partX = std::min(1.0f, tiles.x - i), partY = std::min(1.0f, tiles.y - j);
            float corners[4] = { layout.lo.x + i * cellW, layout.lo.y + j * cellH, 0, 0 };
            corners[2] = corners[0] + cellW * partX;
            corners[3] = corners[1] + cellH * partY;

            float uv[4] = { f[0], f[1], f[0] + f[2] * partX, f[1] + f[3] * partY };
            if (element.flipX) { uv[0] = f[0] + f[2]; uv[2] = f[0] + f[2] * (1.0f - partX); }
            if (element.flipY) { uv[1] = f[1] + f[3]; uv[3] = f[1] + f[3] * (1.0f - partY); }
            uv[0] /= pw; uv[2] /= pw;
            uv[1] /= ph; uv[3] /= ph;

            AddSpriteTile(batch, element, world, corners, uv, layout.lo.z);
        }
    }
}
