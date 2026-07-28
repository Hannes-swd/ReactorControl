#include "camera_controller.h"
#include "../core/gameconfig.h"

CameraController::CameraController()
    : camera{ 0 }, currentView(GameConfig::InitialView) {
    camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
    camera.fovy = GameConfig::CameraFovY;
    camera.projection = CAMERA_PERSPECTIVE;
    UpdateCameraTarget();
}

void CameraController::HandleInput() {
    int view = static_cast<int>(currentView);

    if (IsKeyPressed(KEY_RIGHT)) {
        if (view < ViewSideCount - 1) {
            view++;
        }
    } else if (IsKeyPressed(KEY_LEFT)) {
        if (view > 0) {
            view--;
        }
    }

    currentView = static_cast<ViewSide>(view);
    UpdateCameraTarget();
}

void CameraController::UpdateCameraTarget() {
    camera.position = GameConfig::StandPosition;
    camera.target = GameConfig::ViewTargets[static_cast<int>(currentView)];
}
