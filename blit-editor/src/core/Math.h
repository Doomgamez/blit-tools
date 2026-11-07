#pragma once

// vectors and 4x4 matrices for the editor. matrices are column-major float[16], what OpenGL and ImGuizmo take
// rotations are pitch / yaw / roll in degrees, applied Y * X * Z like the engine's Quat_FromEuler

#include <cmath>

constexpr float kPi = 3.14159265358979323846f;

struct Vec2
{
    float x = 0, y = 0;
};

struct Vec3
{
    float x = 0, y = 0, z = 0;

    Vec3() = default;
    Vec3(float ax, float ay, float az) : x(ax), y(ay), z(az) {}

    float& operator[](int i) { return (&x)[i]; }
    float operator[](int i) const { return (&x)[i]; }
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vec3 operator-(Vec3 a) { return { -a.x, -a.y, -a.z }; }
inline Vec3 operator*(Vec3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
inline Vec3 operator*(Vec3 a, Vec3 b) { return { a.x * b.x, a.y * b.y, a.z * b.z }; }
inline Vec3 operator/(Vec3 a, float s) { return { a.x / s, a.y / s, a.z / s }; }
inline Vec3& operator+=(Vec3& a, Vec3 b) { a = a + b; return a; }
inline float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 Cross(Vec3 a, Vec3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
inline float Length(Vec3 a) { return std::sqrt(Dot(a, a)); }
inline Vec3 Normalize(Vec3 a) { float l = Length(a); return l > 1e-12f ? a / l : Vec3(0, 0, 0); }
inline Vec3 Min(Vec3 a, Vec3 b) { return { std::fmin(a.x, b.x), std::fmin(a.y, b.y), std::fmin(a.z, b.z) }; }
inline Vec3 Max(Vec3 a, Vec3 b) { return { std::fmax(a.x, b.x), std::fmax(a.y, b.y), std::fmax(a.z, b.z) }; }

// rounds to the nearest multiple of step, step <= 0 leaves it
float Snap(float value, float step);
Vec3 Snap(Vec3 value, float step);

struct Mat4
{
    float m[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

    float& at(int row, int column) { return m[column * 4 + row]; }
    float at(int row, int column) const { return m[column * 4 + row]; }
};

Mat4 operator*(const Mat4& a, const Mat4& b);
Mat4 Mat4Translate(Vec3 t);
Mat4 Mat4Scale(Vec3 s);
Mat4 Mat4Euler(Vec3 degrees);
// translate * rotate * scale, a node's local matrix
Mat4 Mat4Compose(Vec3 position, Vec3 degrees, Vec3 scale);
Mat4 Mat4Inverse(const Mat4& a);
Mat4 Mat4Transpose(const Mat4& a);
Mat4 Mat4Perspective(float fovyDegrees, float aspect, float nearPlane, float farPlane);
Mat4 Mat4Ortho(float left, float right, float bottom, float top, float nearPlane, float farPlane);
Mat4 Mat4LookAt(Vec3 eye, Vec3 target, Vec3 up);

Vec3 TransformPoint(const Mat4& m, Vec3 p);
Vec3 TransformDirection(const Mat4& m, Vec3 d);
// a plane (normal, distance) through a matrix, the normal stays unit length
void TransformPlane(const Mat4& m, Vec3& normal, float& distance);

// the position, rotation (Y * X * Z degrees) and scale of a matrix without shear
void Mat4Decompose(const Mat4& m, Vec3& position, Vec3& degrees, Vec3& scale);

struct Ray
{
    Vec3 origin;
    Vec3 direction;     // unit length
};

// distance along the ray to the box, false when it misses
bool RayBox(const Ray& ray, Vec3 boxMin, Vec3 boxMax, float& distance);
Ray TransformRay(const Mat4& m, const Ray& ray);
