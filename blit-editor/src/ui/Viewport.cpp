// the viewport: the level drawn into a framebuffer shown as an image, the camera's controls, the grid

#include <algorithm>

#include <imgui.h>
#include <ImGuizmo.h>

#include "level/LevelLighting.h"
#include "ui/Editor.h"
#include "ui/Viewport.h"

void AddGridLines(Editor& editor, std::vector<LineVertex>& lines)
{
    const Camera& camera = editor.camera;
    int axis = GridAxis(camera);
    int a = axis == 0 ? 2 : 0, b = axis == 1 ? 2 : 1;   // the two axes the grid spans
    float step = GridStep(editor);
    float w = (float)editor.viewport.width, h = (float)editor.viewport.height;
    Vec3 middle;
    float extent;

    if (IsOrthographic(camera))
    {
        // no denser than a line every 6 pixels
        while (step / camera.zoom < 6.0f) step *= 8.0f;
        middle = camera.center;
        extent = std::max(w, h) * camera.zoom * 0.5f + step;
    }
    else
    {
        middle = camera.position;
        extent = std::min(step * 64.0f, 200.0f);
    }

    float startA = Snap(middle[a] - extent, step), startB = Snap(middle[b] - extent, step);
    int count = std::min((int)(extent * 2.0f / step) + 2, 1000);
    const float minor[4] = { 0.5f, 0.5f, 0.55f, 0.18f }, major[4] = { 0.6f, 0.6f, 0.65f, 0.35f };
    const float axisColors[3][4] = { { 0.85f, 0.3f, 0.3f, 0.8f }, { 0.3f, 0.85f, 0.3f, 0.8f }, { 0.3f, 0.45f, 0.95f, 0.8f } };

    for (int i = 0; i < count; i++)
    {
        float va = startA + step * i, vb = startB + step * i;
        bool majorA = std::fabs(std::fmod(std::round(va / step), 8.0f)) < 0.5f;
        bool majorB = std::fabs(std::fmod(std::round(vb / step), 8.0f)) < 0.5f;
        Vec3 p0, p1, q0, q1;

        p0[a] = va; p0[b] = startB; p1[a] = va; p1[b] = startB + step * (count - 1);
        q0[b] = vb; q0[a] = startA; q1[b] = vb; q1[a] = startA + step * (count - 1);

        AddLine(lines, p0, p1, std::fabs(va) < step * 0.01f ? axisColors[b] : majorA ? major : minor);
        AddLine(lines, q0, q1, std::fabs(vb) < step * 0.01f ? axisColors[a] : majorB ? major : minor);
    }
}

void MoveCamera(Editor& editor, ImVec2 mouse, ImVec2 size)
{
    ImGuiIO& io = ImGui::GetIO();
    Camera& camera = editor.camera;
    ImVec2 delta = io.MouseDelta;

    if (!IsOrthographic(camera))
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            camera.yaw += delta.x * 0.2f;
            camera.pitch = std::clamp(camera.pitch - delta.y * 0.2f, -89.0f, 89.0f);

            Vec3 move;
            if (ImGui::IsKeyDown(ImGuiKey_W)) move += CameraForward(camera);
            if (ImGui::IsKeyDown(ImGuiKey_S)) move += -CameraForward(camera);
            if (ImGui::IsKeyDown(ImGuiKey_D)) move += CameraRight(camera);
            if (ImGui::IsKeyDown(ImGuiKey_A)) move += -CameraRight(camera);
            if (ImGui::IsKeyDown(ImGuiKey_E)) move += Vec3(0, 1, 0);
            if (ImGui::IsKeyDown(ImGuiKey_Q)) move += Vec3(0, -1, 0);

            float speed = camera.speed * (io.KeyShift ? 3.0f : 1.0f);
            camera.position += move * (speed * io.DeltaTime);

            // the wheel while flying sets how fast
            if (io.MouseWheel != 0.0f) camera.speed = std::clamp(camera.speed * (io.MouseWheel > 0 ? 1.25f : 0.8f), 0.5f, 200.0f);
        }
        else if (io.MouseWheel != 0.0f)
        {
            camera.position += CameraForward(camera) * (io.MouseWheel * camera.speed * 0.25f);
        }

        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
            Vec3 up = Cross(CameraRight(camera), CameraForward(camera));
            float scale = camera.speed * 0.004f;
            camera.position += CameraRight(camera) * (-delta.x * scale) + up * (delta.y * scale);
        }

        return;
    }

    // orthographic: drag to pan, the wheel zooms around the point under the mouse
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle))
    {
        Ray a = CameraRay(camera, mouse.x - delta.x, mouse.y - delta.y, size.x, size.y);
        Ray b = CameraRay(camera, mouse.x, mouse.y, size.x, size.y);
        camera.center = camera.center - (b.origin - a.origin);
    }

    if (io.MouseWheel != 0.0f)
    {
        Ray before = CameraRay(camera, mouse.x, mouse.y, size.x, size.y);
        float limit = editor.doc.level.is2D ? 50.0f : 2.0f;
        camera.zoom = std::clamp(camera.zoom * (io.MouseWheel > 0 ? 0.8f : 1.25f), 0.0005f, limit);
        Ray after = CameraRay(camera, mouse.x, mouse.y, size.x, size.y);
        camera.center = camera.center + (before.origin - after.origin);
    }
}

void RenderLevel(Editor& editor, const Mat4& view, const Mat4& projection)
{
    SceneContext context = MakeSceneContext(editor);
    float size = editor.doc.level.is2D ? 8.0f : 0.2f;

    if (editor.sceneOptions.iconSize != size)
    {
        editor.sceneOptions.iconSize = size;
        editor.scene.version = -1;
    }

    if (editor.scene.version != editor.doc.version)
    {
        BuildScene(editor.scene, editor.doc.level, context, editor.sceneOptions);
        editor.scene.version = editor.doc.version;
    }

    BuildSelectionLines(editor.overlay, editor.doc.level, editor.faceMode ? std::vector<int>() : editor.doc.selection,
                        context, editor.sceneOptions);
    AddFaceOutlines(editor, editor.overlay);

    std::vector<LineVertex> grid;
    AddGridLines(editor, grid);

    DrawSettings settings;
    settings.viewProjection = projection * view;
    settings.eye = editor.doc.level.is2D ? editor.camera.center : editor.camera.position;

    // the level's own lighting, what the game will show
    LevelLighting lighting = ReadLevelLighting(editor.doc.level);
    Vec3 toward = Normalize(lighting.sunDirection * -1.0f);
    settings.lighting = editor.lighting;
    settings.flat = editor.doc.level.is2D;
    settings.shadow = lighting.shadow / 255.0f;
    settings.sun = lighting.sun ? lighting.sunColor / 255.0f * lighting.sunIntensity : Vec3(0, 0, 0);
    settings.sunToward = Length(toward) > 0 ? toward : Vec3(0, 1, 0);

    BeginFrame(editor.renderer, editor.viewport, settings);
    DrawLines(editor.renderer, grid, settings, true);
    DrawScene(editor.renderer, editor.scene, settings);
    DrawLines(editor.renderer, editor.scene.lines, settings, true);
    DrawLines(editor.renderer, editor.overlay, settings, false);
    EndFrame(editor.renderer);
}

void DrawViewport(Editor& editor)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    bool open = ImGui::Begin("Viewport");
    ImGui::PopStyleVar();

    if (!open)
    {
        editor.viewportHovered = false;
        ImGui::End();
        return;
    }

    DrawToolbar(editor);

    ImVec2 size = ImGui::GetContentRegionAvail();
    size.y -= ImGui::GetTextLineHeightWithSpacing();
    size.x = std::max(size.x, 32.0f);
    size.y = std::max(size.y, 32.0f);
    ResizeFramebuffer(editor.viewport, (int)size.x, (int)size.y);

    Mat4 view = CameraView(editor.camera);
    Mat4 projection = CameraProjection(editor.camera, size.x, size.y);
    RenderLevel(editor, view, projection);

    ImVec2 origin = ImGui::GetCursorScreenPos();
    // the framebuffer is upside down for ImGui
    ImGui::Image((ImTextureID)(intptr_t)editor.viewport.color, size, ImVec2(0, 1), ImVec2(1, 0));
    editor.viewportHovered = ImGui::IsItemHovered();
    editor.viewportFocused = ImGui::IsWindowFocused();

    ImVec2 mouse = ImVec2(ImGui::GetIO().MousePos.x - origin.x, ImGui::GetIO().MousePos.y - origin.y);
    Ray ray = CameraRay(editor.camera, mouse.x, mouse.y, size.x, size.y);

    HandleViewportDrops(editor, ray, mouse, size);
    DrawClassPictures(editor, view, projection, origin, size);
    DrawTextNodes(editor, view, projection, origin, size);

    bool canvas = UsesCanvas(editor);
    bool gizmo = !canvas && DrawGizmo(editor, view, projection, origin, size);

    if (editor.viewportHovered)
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
        {
            ImGui::SetWindowFocus();
        }

        // a right click that cancels a canvas drag doesn't pan
        if (!CanvasBusy(editor)) MoveCamera(editor, mouse, size);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !gizmo && !canvas)
        {
            ClickSelect(editor, ray, ImGui::GetIO().KeyCtrl);
        }
    }

    if (editor.doc.level.is2D && editor.camera.kind == ViewKind::TwoD)
    {
        DrawCanvas(editor, origin, size);
    }

    // the status line: where the mouse is on the grid, what the editor last did
    Vec3 at = PointOnGrid(editor.camera, mouse.x, mouse.y, size.x, size.y);
    ImGui::TextDisabled("%s   %.2f %.2f %.2f   %s", ViewTitle(editor.camera.kind), at.x, at.y, at.z, editor.status.c_str());
    ImGui::End();
}
