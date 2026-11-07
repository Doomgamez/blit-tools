#include "core/Math.h"

#include <algorithm>

float Snap(float value, float step)
{
    return step > 0.0f ? std::round(value / step) * step : value;
}

Vec3 Snap(Vec3 value, float step)
{
    return { Snap(value.x, step), Snap(value.y, step), Snap(value.z, step) };
}

Mat4 operator*(const Mat4& a, const Mat4& b)
{
    Mat4 out;

    for (int row = 0; row < 4; row++)
    {
        for (int column = 0; column < 4; column++)
        {
            float sum = 0.0f;

            for (int k = 0; k < 4; k++)
            {
                sum += a.at(row, k) * b.at(k, column);
            }

            out.at(row, column) = sum;
        }
    }

    return out;
}

Mat4 Mat4Translate(Vec3 t)
{
    Mat4 out;
    out.at(0, 3) = t.x;
    out.at(1, 3) = t.y;
    out.at(2, 3) = t.z;
    return out;
}

Mat4 Mat4Scale(Vec3 s)
{
    Mat4 out;
    out.at(0, 0) = s.x;
    out.at(1, 1) = s.y;
    out.at(2, 2) = s.z;
    return out;
}

Mat4 Mat4Euler(Vec3 degrees)
{
    float x = degrees.x * kPi / 180.0f, y = degrees.y * kPi / 180.0f, z = degrees.z * kPi / 180.0f;
    float cx = std::cos(x), sx = std::sin(x), cy = std::cos(y), sy = std::sin(y), cz = std::cos(z), sz = std::sin(z);
    Mat4 out;

    // Ry * Rx * Rz
    out.at(0, 0) = cy * cz + sy * sx * sz; out.at(0, 1) = -cy * sz + sy * sx * cz; out.at(0, 2) = sy * cx;
    out.at(1, 0) = cx * sz;                out.at(1, 1) = cx * cz;                 out.at(1, 2) = -sx;
    out.at(2, 0) = -sy * cz + cy * sx * sz; out.at(2, 1) = sy * sz + cy * sx * cz; out.at(2, 2) = cy * cx;
    return out;
}

Mat4 Mat4Compose(Vec3 position, Vec3 degrees, Vec3 scale)
{
    return Mat4Translate(position) * Mat4Euler(degrees) * Mat4Scale(scale);
}

Mat4 Mat4Transpose(const Mat4& a)
{
    Mat4 out;

    for (int row = 0; row < 4; row++)
    {
        for (int column = 0; column < 4; column++)
        {
            out.at(row, column) = a.at(column, row);
        }
    }

    return out;
}

Mat4 Mat4Inverse(const Mat4& a)
{
    const float* m = a.m;
    float inv[16];

    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    Mat4 out;

    if (std::fabs(det) < 1e-20f)
    {
        return out;
    }

    for (int i = 0; i < 16; i++)
    {
        out.m[i] = inv[i] / det;
    }

    return out;
}

Mat4 Mat4Perspective(float fovyDegrees, float aspect, float nearPlane, float farPlane)
{
    float f = 1.0f / std::tan(fovyDegrees * kPi / 360.0f);
    Mat4 out;

    out.at(0, 0) = f / aspect;
    out.at(1, 1) = f;
    out.at(2, 2) = (farPlane + nearPlane) / (nearPlane - farPlane);
    out.at(2, 3) = 2.0f * farPlane * nearPlane / (nearPlane - farPlane);
    out.at(3, 2) = -1.0f;
    out.at(3, 3) = 0.0f;
    return out;
}

Mat4 Mat4Ortho(float left, float right, float bottom, float top, float nearPlane, float farPlane)
{
    Mat4 out;

    out.at(0, 0) = 2.0f / (right - left);
    out.at(1, 1) = 2.0f / (top - bottom);
    out.at(2, 2) = -2.0f / (farPlane - nearPlane);
    out.at(0, 3) = -(right + left) / (right - left);
    out.at(1, 3) = -(top + bottom) / (top - bottom);
    out.at(2, 3) = -(farPlane + nearPlane) / (farPlane - nearPlane);
    return out;
}

Mat4 Mat4LookAt(Vec3 eye, Vec3 target, Vec3 up)
{
    Vec3 f = Normalize(target - eye);
    Vec3 s = Normalize(Cross(f, up));
    Vec3 u = Cross(s, f);
    Mat4 out;

    out.at(0, 0) = s.x; out.at(0, 1) = s.y; out.at(0, 2) = s.z; out.at(0, 3) = -Dot(s, eye);
    out.at(1, 0) = u.x; out.at(1, 1) = u.y; out.at(1, 2) = u.z; out.at(1, 3) = -Dot(u, eye);
    out.at(2, 0) = -f.x; out.at(2, 1) = -f.y; out.at(2, 2) = -f.z; out.at(2, 3) = Dot(f, eye);
    return out;
}

Vec3 TransformPoint(const Mat4& m, Vec3 p)
{
    return { m.at(0, 0) * p.x + m.at(0, 1) * p.y + m.at(0, 2) * p.z + m.at(0, 3),
             m.at(1, 0) * p.x + m.at(1, 1) * p.y + m.at(1, 2) * p.z + m.at(1, 3),
             m.at(2, 0) * p.x + m.at(2, 1) * p.y + m.at(2, 2) * p.z + m.at(2, 3) };
}

Vec3 TransformDirection(const Mat4& m, Vec3 d)
{
    return { m.at(0, 0) * d.x + m.at(0, 1) * d.y + m.at(0, 2) * d.z,
             m.at(1, 0) * d.x + m.at(1, 1) * d.y + m.at(1, 2) * d.z,
             m.at(2, 0) * d.x + m.at(2, 1) * d.y + m.at(2, 2) * d.z };
}

void TransformPlane(const Mat4& m, Vec3& normal, float& distance)
{
    Vec3 point = TransformPoint(m, normal * distance);
    normal = Normalize(TransformDirection(Mat4Transpose(Mat4Inverse(m)), normal));
    distance = Dot(normal, point);
}

void Mat4Decompose(const Mat4& m, Vec3& position, Vec3& degrees, Vec3& scale)
{
    Vec3 columns[3];

    position = { m.at(0, 3), m.at(1, 3), m.at(2, 3) };

    for (int c = 0; c < 3; c++)
    {
        columns[c] = { m.at(0, c), m.at(1, c), m.at(2, c) };
        scale[c] = Length(columns[c]);
        columns[c] = scale[c] > 1e-12f ? columns[c] / scale[c] : columns[c];
    }

    // r(row, column) of Ry * Rx * Rz: r12 = -sin x, r02 / r22 give y, r10 / r11 give z
    float r02 = columns[2].x, r12 = columns[2].y, r22 = columns[2].z;
    float r10 = columns[0].y, r11 = columns[1].y, r00 = columns[0].x, r20 = columns[0].z;
    float sinX = std::clamp(-r12, -1.0f, 1.0f);

    if (std::fabs(sinX) > 0.9999f)
    {
        degrees = { std::asin(sinX), std::atan2(-r20, r00), 0.0f };
    }
    else
    {
        degrees = { std::asin(sinX), std::atan2(r02, r22), std::atan2(r10, r11) };
    }

    degrees = degrees * (180.0f / kPi);
}

bool RayBox(const Ray& ray, Vec3 boxMin, Vec3 boxMax, float& distance)
{
    float nearest = -1e30f, farthest = 1e30f;

    for (int axis = 0; axis < 3; axis++)
    {
        float origin = ray.origin[axis], direction = ray.direction[axis];

        if (std::fabs(direction) < 1e-12f)
        {
            if (origin < boxMin[axis] || origin > boxMax[axis])
            {
                return false;
            }

            continue;
        }

        float t1 = (boxMin[axis] - origin) / direction, t2 = (boxMax[axis] - origin) / direction;
        nearest = std::max(nearest, std::min(t1, t2));
        farthest = std::min(farthest, std::max(t1, t2));
    }

    if (farthest < std::max(nearest, 0.0f))
    {
        return false;
    }

    distance = nearest > 0.0f ? nearest : 0.0f;
    return true;
}

Ray TransformRay(const Mat4& m, const Ray& ray)
{
    Ray out;
    out.origin = TransformPoint(m, ray.origin);
    out.direction = TransformDirection(m, ray.direction);
    return out;
}
