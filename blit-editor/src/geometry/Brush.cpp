#include "geometry/Brush.h"

#include <algorithm>

#include "geometry/Polygon.h"

std::vector<FacePolygon> BrushPolygons(const Brush& brush)
{
    std::vector<FacePolygon> out;

    for (size_t f = 0; f < brush.faces.size(); f++)
    {
        PolygonD poly = BaseQuad(brush.faces[f]);

        for (size_t other = 0; other < brush.faces.size() && poly.size() >= 3; other++)
        {
            if (other != f)
            {
                ClipPolygon(poly, brush.faces[other]);
            }
        }

        CleanPolygon(poly);

        if (poly.size() < 3)
        {
            continue;
        }

        FacePolygon polygon;
        polygon.face = (int)f;

        for (const Vec3D& p : poly)
        {
            polygon.points.push_back({ (float)p.x, (float)p.y, (float)p.z });
        }

        out.push_back(std::move(polygon));
    }

    return out;
}

bool BrushBounds(const Brush& brush, Vec3& outMin, Vec3& outMax)
{
    bool any = false;

    for (const FacePolygon& polygon : BrushPolygons(brush))
    {
        for (Vec3 p : polygon.points)
        {
            outMin = any ? Min(outMin, p) : p;
            outMax = any ? Max(outMax, p) : p;
            any = true;
        }
    }

    return any;
}

bool ElementBounds(const Element& element, Vec3& outMin, Vec3& outMax)
{
    bool any = false;

    for (const Brush& brush : element.brushes)
    {
        Vec3 lo, hi;

        if (BrushBounds(brush, lo, hi))
        {
            outMin = any ? Min(outMin, lo) : lo;
            outMax = any ? Max(outMax, hi) : hi;
            any = true;
        }
    }

    return any;
}

void FaceAxes(Vec3 n, float rotation, Vec3& u, Vec3& v)
{
    float ax = std::fabs(n.x), ay = std::fabs(n.y), az = std::fabs(n.z);
    float c = std::cos(rotation * kPi / 180.0f), s = std::sin(rotation * kPi / 180.0f);
    Vec3 baseU, baseV;

    if (ay >= ax && ay >= az)
    {
        baseU = { 1, 0, 0 };
        baseV = { 0, 0, 1 };
    }
    else if (ax >= az)
    {
        baseU = { 0, 0, n.x > 0 ? -1.0f : 1.0f };
        baseV = { 0, -1, 0 };
    }
    else
    {
        baseU = { n.z > 0 ? 1.0f : -1.0f, 0, 0 };
        baseV = { 0, -1, 0 };
    }

    u = baseU * c - baseV * s;
    v = baseU * s + baseV * c;
}

Vec2 FaceUV(const Face& face, Vec3 point, float texelsPerMetre, int textureWidth, int textureHeight)
{
    Vec3 u, v;
    float su = face.scale[0] != 0.0f ? face.scale[0] : 1.0f;
    float sv = face.scale[1] != 0.0f ? face.scale[1] : 1.0f;

    FaceAxes(face.normal, face.rotation, u, v);
    return { (Dot(point, u) * texelsPerMetre / su + face.offset[0]) / (float)std::max(textureWidth, 1),
             (Dot(point, v) * texelsPerMetre / sv + face.offset[1]) / (float)std::max(textureHeight, 1) };
}

Face MakeFace(Vec3 normal, Vec3 through, const std::string& texture)
{
    Face face;
    face.normal = Normalize(normal);
    face.distance = Dot(face.normal, through);
    face.texture = texture;
    return face;
}

Brush MakeBox(Vec3 lo, Vec3 hi, const std::string& texture)
{
    Brush brush;
    brush.faces = {
        MakeFace({ 1, 0, 0 }, hi, texture), MakeFace({ -1, 0, 0 }, lo, texture),
        MakeFace({ 0, 1, 0 }, hi, texture), MakeFace({ 0, -1, 0 }, lo, texture),
        MakeFace({ 0, 0, 1 }, hi, texture), MakeFace({ 0, 0, -1 }, lo, texture),
    };
    return brush;
}

Brush MakeWedge(Vec3 lo, Vec3 hi, const std::string& texture)
{
    Brush brush = MakeBox(lo, hi, texture);
    Vec3 size = hi - lo;

    // the top becomes a slope from the low edge at -X up to the high edge at +X
    brush.faces[2] = MakeFace({ -size.y, size.x, 0 }, lo, texture);
    return brush;
}

Brush MakeCylinder(Vec3 lo, Vec3 hi, int sides, const std::string& texture)
{
    Brush brush;
    Vec3 center = (lo + hi) * 0.5f;
    float rx = (hi.x - lo.x) * 0.5f, rz = (hi.z - lo.z) * 0.5f;

    sides = std::clamp(sides, 3, 64);
    brush.faces.push_back(MakeFace({ 0, 1, 0 }, hi, texture));
    brush.faces.push_back(MakeFace({ 0, -1, 0 }, lo, texture));

    for (int i = 0; i < sides; i++)
    {
        float angle = (i + 0.5f) * 2.0f * kPi / sides;
        Vec3 edge = { center.x + std::cos(angle) * rx, center.y, center.z + std::sin(angle) * rz };
        // the normal of an ellipse at that point
        Vec3 normal = { std::cos(angle) / std::max(rx, 1e-4f), 0, std::sin(angle) / std::max(rz, 1e-4f) };
        brush.faces.push_back(MakeFace(normal, edge, texture));
    }

    return brush;
}

std::vector<Brush> MakeHollow(Vec3 lo, Vec3 hi, float t, const std::string& texture)
{
    t = std::max(t, 0.001f);

    return {
        MakeBox({ lo.x, lo.y, lo.z }, { hi.x, lo.y + t, hi.z }, texture),               // floor
        MakeBox({ lo.x, hi.y - t, lo.z }, { hi.x, hi.y, hi.z }, texture),               // ceiling
        MakeBox({ lo.x, lo.y + t, lo.z }, { lo.x + t, hi.y - t, hi.z }, texture),       // -X
        MakeBox({ hi.x - t, lo.y + t, lo.z }, { hi.x, hi.y - t, hi.z }, texture),       // +X
        MakeBox({ lo.x + t, lo.y + t, lo.z }, { hi.x - t, hi.y - t, lo.z + t }, texture), // -Z
        MakeBox({ lo.x + t, lo.y + t, hi.z - t }, { hi.x - t, hi.y - t, hi.z }, texture), // +Z
    };
}

void TransformBrush(Brush& brush, const Mat4& m)
{
    for (Face& face : brush.faces)
    {
        TransformPlane(m, face.normal, face.distance);
    }
}

void FitBrush(Brush& brush, Vec3 fromMin, Vec3 fromMax, Vec3 toMin, Vec3 toMax)
{
    Vec3 scale;

    for (int axis = 0; axis < 3; axis++)
    {
        float from = fromMax[axis] - fromMin[axis];
        scale[axis] = from > 1e-6f ? (toMax[axis] - toMin[axis]) / from : 1.0f;
    }

    // fromMin goes to toMin, everything else scales away from it
    TransformBrush(brush, Mat4Translate(toMin) * Mat4Scale(scale) * Mat4Translate(-fromMin));
}

void SetBrushTexture(Brush& brush, const std::string& texture)
{
    for (Face& face : brush.faces)
    {
        face.texture = texture;
    }
}

bool RayBrush(const Ray& ray, const Brush& brush, float& distance, int& face)
{
    float enter = 0.0f, leave = 1e30f;
    int enterFace = -1;

    // a convex solid: the ray is inside between the last plane it enters and the first it leaves
    for (size_t i = 0; i < brush.faces.size(); i++)
    {
        const Face& f = brush.faces[i];
        float denominator = Dot(f.normal, ray.direction);
        float along = f.distance - Dot(f.normal, ray.origin);

        if (std::fabs(denominator) < 1e-9f)
        {
            if (along < 0.0f) return false; // parallel and outside
            continue;
        }

        float t = along / denominator;

        if (denominator < 0.0f)
        {
            if (t > enter) { enter = t; enterFace = (int)i; }
        }
        else
        {
            leave = std::min(leave, t);
        }

        if (enter > leave) return false;
    }

    if (enterFace < 0)
    {
        return false; // starts inside, nothing to click on
    }

    distance = enter;
    face = enterFace;
    return true;
}
