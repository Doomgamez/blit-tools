#pragma once

// the sprite element as the game draws it (LevelSprite in level2d.bs, sized by homeinvasionsdl/tools/blitlevel/
// sprite.c): keep the three in step.
//
// the picture, or its region, is split into columns x rows frames, counted along the rows from the top left. the
// sprite shows one frame over its area: the node's brushes (a 2D box), else the frame's size in pixels. the node's
// scale stretches it. scaled shows the frame once, tiled splits the area into tiles.x by tiles.y, repeat tiles it at
// the frame's own size so a wider box gets more tiles. flips mirror each tile

#include "render/Scene.h"

struct SpriteLayout
{
    Vec3 lo, hi;            // the area, in the node's space
    float frame[4];         // the frame in the picture, pixels: x y width height
    Vec2 tiles{ 1, 1 };     // frames across and down the area, the last ones can be partial
};

// the frame of the picture the sprite shows, pixels
void SpriteFrame(const Element& sprite, float pictureWidth, float pictureHeight, float out[4]);
// how many frames fit across and down: 1 when scaled, its tiles when tiled, area / frame when repeated
Vec2 SpriteTiles(const Element& sprite, Vec2 areaSize, Vec2 scale, const float frame[4]);
SpriteLayout LayOutSprite(const Node& node, const Element& sprite, const TextureInfo& picture);

// at most this many tiles are drawn in the editor, a sprite that would need more is shown with fewer, bigger ones
constexpr int kMaxSpriteTiles = 4096;
