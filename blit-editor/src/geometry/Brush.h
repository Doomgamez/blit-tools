#pragma once

// brushes to polygons and texture coordinates, the same way homeinvasionsdl/tools/blitlevel/brush.c does it, so the
// viewport shows what the game will draw. keep the two in step

#include <vector>

#include "level/Level.h"

struct FacePolygon
{
    int face = -1;                  // index into the brush's faces
    std::vector<Vec3> points;       // counter-clockwise seen from outside, empty when the face doesn't touch the brush
};

// one polygon per face that has an area
std::vector<FacePolygon> BrushPolygons(const Brush& brush);

// the box around a brush's corners, false for a brush that encloses nothing
bool BrushBounds(const Brush& brush, Vec3& outMin, Vec3& outMax);
bool ElementBounds(const Element& element, Vec3& outMin, Vec3& outMax);

// the texture axes of a face (blitlevel's FaceAxes) and a point's UV on it, for a texture of this size
void FaceAxes(Vec3 normal, float rotation, Vec3& u, Vec3& v);
Vec2 FaceUV(const Face& face, Vec3 point, float texelsPerMetre, int textureWidth, int textureHeight);

// shapes, in the node's space
// a face whose plane goes through the point
Face MakeFace(Vec3 normal, Vec3 through, const std::string& texture);
Brush MakeBox(Vec3 boxMin, Vec3 boxMax, const std::string& texture);
// a ramp rising along +X
Brush MakeWedge(Vec3 boxMin, Vec3 boxMax, const std::string& texture);
// an upright cylinder with this many sides
Brush MakeCylinder(Vec3 boxMin, Vec3 boxMax, int sides, const std::string& texture);
// six walls of this thickness inside the box, for a room
std::vector<Brush> MakeHollow(Vec3 boxMin, Vec3 boxMax, float thickness, const std::string& texture);

// every plane through a matrix (moving, turning, scaling the brush)
void TransformBrush(Brush& brush, const Mat4& m);
// stretches the brush from one box to another
void FitBrush(Brush& brush, Vec3 fromMin, Vec3 fromMax, Vec3 toMin, Vec3 toMax);
void SetBrushTexture(Brush& brush, const std::string& texture);

// the nearest face the ray (in the brush's space) enters through, false when it misses
bool RayBrush(const Ray& ray, const Brush& brush, float& distance, int& face);
