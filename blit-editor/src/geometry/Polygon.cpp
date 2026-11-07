#include "geometry/Polygon.h"

#include <cmath>

Vec3D AddD(Vec3D a, Vec3D b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vec3D SubD(Vec3D a, Vec3D b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
Vec3D ScaleD(Vec3D a, double s) { return { a.x * s, a.y * s, a.z * s }; }
double DotD(Vec3D a, Vec3D b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3D CrossD(Vec3D a, Vec3D b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
double LengthD(Vec3D a) { return std::sqrt(DotD(a, a)); }

PolygonD BaseQuad(const Face& face)
{
    Vec3D n = { face.normal.x, face.normal.y, face.normal.z };
    Vec3D up = std::fabs(n.y) < 0.9 ? Vec3D{ 0, 1, 0 } : Vec3D{ 1, 0, 0 };
    Vec3D u = CrossD(up, n);
    Vec3D center = ScaleD(n, face.distance);

    u = ScaleD(u, 1.0 / LengthD(u));
    Vec3D v = CrossD(n, u);

    return {
        AddD(center, AddD(ScaleD(u, -kWorldSize), ScaleD(v, -kWorldSize))),
        AddD(center, AddD(ScaleD(u, kWorldSize), ScaleD(v, -kWorldSize))),
        AddD(center, AddD(ScaleD(u, kWorldSize), ScaleD(v, kWorldSize))),
        AddD(center, AddD(ScaleD(u, -kWorldSize), ScaleD(v, kWorldSize))),
    };
}

void ClipPolygon(PolygonD& poly, const Face& face)
{
    Vec3D normal = { face.normal.x, face.normal.y, face.normal.z };
    double distance = face.distance;
    PolygonD out;

    for (size_t i = 0; i < poly.size(); i++)
    {
        Vec3D a = poly[i];
        Vec3D b = poly[(i + 1) % poly.size()];
        double da = DotD(normal, a) - distance;
        double db = DotD(normal, b) - distance;

        if (da <= kClipEpsilon)
        {
            out.push_back(a);
        }

        if (((da > kClipEpsilon) != (db > kClipEpsilon)) && std::fabs(da - db) > 1e-12)
        {
            double t = da / (da - db);

            if ((da < -kClipEpsilon || da > kClipEpsilon) && (db < -kClipEpsilon || db > kClipEpsilon))
            {
                out.push_back(AddD(a, ScaleD(SubD(b, a), t)));
            }
        }
    }

    poly = out;
}

void CleanPolygon(PolygonD& poly)
{
    PolygonD out;

    for (size_t i = 0; i < poly.size(); i++)
    {
        if (LengthD(SubD(poly[i], poly[(i + 1) % poly.size()])) > kClipEpsilon)
        {
            out.push_back(poly[i]);
        }
    }

    poly = out;
}
