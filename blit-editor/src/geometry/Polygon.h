#pragma once

// clipping in doubles: neighbouring brushes have to meet without cracks. blitlevel's brush.c does the same

#include <vector>

#include "level/Level.h"

struct Vec3D
{
    double x = 0, y = 0, z = 0;
};

using PolygonD = std::vector<Vec3D>;

Vec3D AddD(Vec3D a, Vec3D b);
Vec3D SubD(Vec3D a, Vec3D b);
Vec3D ScaleD(Vec3D a, double s);
double DotD(Vec3D a, Vec3D b);
Vec3D CrossD(Vec3D a, Vec3D b);
double LengthD(Vec3D a);

constexpr double kWorldSize = 65536.0;      // the starting quad of each face, bigger than any level
constexpr double kClipEpsilon = 0.0005;

// a huge quad on the face's plane, counter-clockwise seen from outside
PolygonD BaseQuad(const Face& face);
// keeps the part behind the face's plane (inside the brush)
void ClipPolygon(PolygonD& poly, const Face& face);
// drops points that ended up on top of each other
void CleanPolygon(PolygonD& poly);
