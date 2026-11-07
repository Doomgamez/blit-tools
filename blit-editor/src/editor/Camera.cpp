#include "editor/Camera.h"

#include <algorithm>

const char* ViewTitle(ViewKind kind)
{
    switch (kind)
    {
    case ViewKind::Perspective: return "Perspective";
    case ViewKind::Top: return "Top (XZ)";
    case ViewKind::Front: return "Front (XY)";
    case ViewKind::Side: return "Side (ZY)";
    case ViewKind::TwoD: return "2D";
    default: return "";
    }
}

Vec3 CameraForward(const Camera& camera)
{
    float yaw = camera.yaw * kPi / 180.0f, pitch = camera.pitch * kPi / 180.0f;
    return { std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch) };
}

Vec3 CameraRight(const Camera& camera)
{
    return Normalize(Cross(CameraForward(camera), { 0, 1, 0 }));
}

bool IsOrthographic(const Camera& camera)
{
    return camera.kind != ViewKind::Perspective;
}

int GridAxis(const Camera& camera)
{
    switch (camera.kind)
    {
    case ViewKind::Front: return 2;
    case ViewKind::Side: return 0;
    case ViewKind::TwoD: return 2;
    default: return 1;
    }
}

// where an orthographic camera sits and which way is up on screen
void OrthoFrame(const Camera& camera, Vec3& eye, Vec3& up)
{
    const float distance = 5000.0f;

    switch (camera.kind)
    {
    case ViewKind::Top: eye = camera.center + Vec3(0, distance, 0); up = { 0, 0, -1 }; break;
    case ViewKind::Front: eye = camera.center + Vec3(0, 0, distance); up = { 0, 1, 0 }; break;
    case ViewKind::Side: eye = camera.center + Vec3(distance, 0, 0); up = { 0, 1, 0 }; break;
    default: eye = camera.center - Vec3(0, 0, distance); up = { 0, -1, 0 }; break;  // 2D: from -Z, Y down the screen
    }
}

Mat4 CameraView(const Camera& camera)
{
    if (!IsOrthographic(camera))
    {
        return Mat4LookAt(camera.position, camera.position + CameraForward(camera), { 0, 1, 0 });
    }

    Vec3 eye, up;
    OrthoFrame(camera, eye, up);
    return Mat4LookAt(eye, camera.center, up);
}

Mat4 CameraProjection(const Camera& camera, float width, float height)
{
    width = std::max(width, 1.0f);
    height = std::max(height, 1.0f);

    if (!IsOrthographic(camera))
    {
        return Mat4Perspective(camera.fov, width / height, 0.05f, 2000.0f);
    }

    float w = width * camera.zoom * 0.5f, h = height * camera.zoom * 0.5f;
    return Mat4Ortho(-w, w, -h, h, 1.0f, 10000.0f);
}

Ray CameraRay(const Camera& camera, float x, float y, float width, float height)
{
    Mat4 inverse = Mat4Inverse(CameraProjection(camera, width, height) * CameraView(camera));
    float nx = x / std::max(width, 1.0f) * 2.0f - 1.0f, ny = 1.0f - y / std::max(height, 1.0f) * 2.0f;
    auto unproject = [&](float z)
    {
        float in[4] = { nx, ny, z, 1.0f }, out[4] = { 0, 0, 0, 0 };

        for (int r = 0; r < 4; r++)
        {
            for (int c = 0; c < 4; c++) out[r] += inverse.at(r, c) * in[c];
        }

        return Vec3(out[0] / out[3], out[1] / out[3], out[2] / out[3]);
    };

    Vec3 nearPoint = unproject(-1.0f), farPoint = unproject(1.0f);
    Ray ray;
    ray.origin = nearPoint;
    ray.direction = Normalize(farPoint - nearPoint);
    return ray;
}

Vec3 PointOnGrid(const Camera& camera, float x, float y, float width, float height)
{
    Ray ray = CameraRay(camera, x, y, width, height);
    int axis = GridAxis(camera);

    if (std::fabs(ray.direction[axis]) < 1e-6f)
    {
        return ray.origin + ray.direction * 10.0f;
    }

    float t = -ray.origin[axis] / ray.direction[axis];

    // looking away from the plane: somewhere in front instead
    if (t < 0.0f)
    {
        return ray.origin + ray.direction * 10.0f;
    }

    Vec3 point = ray.origin + ray.direction * t;
    point[axis] = 0.0f;
    return point;
}

void FrameBox(Camera& camera, Vec3 lo, Vec3 hi)
{
    Vec3 middle = (lo + hi) * 0.5f;
    float size = std::max(Length(hi - lo), 0.5f);

    if (IsOrthographic(camera))
    {
        camera.center = middle;
        camera.zoom = std::max(size / 600.0f, 0.0005f);
        return;
    }

    camera.position = middle - CameraForward(camera) * (size * 1.2f + 1.0f);
}

void ResetCamera(Camera& camera, bool is2D)
{
    camera = Camera();

    if (is2D)
    {
        camera.kind = ViewKind::TwoD;
        camera.center = { 320, 180, 0 };
        camera.zoom = 1.0f;
    }
}
