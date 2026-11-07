#pragma once

// the viewport's camera: a flying perspective one, three orthographic views of a 3D level, or the 2D view
//
//   perspective   hold right mouse to look, WASD / QE to fly, wheel moves forward, middle mouse pans
//   orthographic  right or middle mouse pans, wheel zooms at the cursor

#include "core/Math.h"

enum class ViewKind { Perspective, Top, Front, Side, TwoD, Count };

struct Camera
{
    ViewKind kind = ViewKind::Perspective;
    Vec3 position{ 8, 6, 8 };
    float yaw = -135.0f;            // degrees, 0 looks along -Z
    float pitch = -25.0f;
    float fov = 70.0f;
    float speed = 8.0f;             // metres per second when flying

    Vec3 center{ 0, 0, 0 };         // what the orthographic views look at
    float zoom = 0.02f;             // world units per pixel in the orthographic views
};

const char* ViewTitle(ViewKind kind);

Vec3 CameraForward(const Camera& camera);
Vec3 CameraRight(const Camera& camera);
Mat4 CameraView(const Camera& camera);
Mat4 CameraProjection(const Camera& camera, float width, float height);
bool IsOrthographic(const Camera& camera);
// the plane the grid lies in: its normal axis (0 x, 1 y, 2 z)
int GridAxis(const Camera& camera);

// where an orthographic camera sits and which way is up on screen
void OrthoFrame(const Camera& camera, Vec3& eye, Vec3& up);

// the ray through a pixel of the viewport (0,0 is its top left)
Ray CameraRay(const Camera& camera, float x, float y, float width, float height);
// a pixel's place on the grid plane through the origin
Vec3 PointOnGrid(const Camera& camera, float x, float y, float width, float height);

// looks at a box: puts it in the middle and fits it in the view
void FrameBox(Camera& camera, Vec3 lo, Vec3 hi);
// the default view of a level
void ResetCamera(Camera& camera, bool is2D);
