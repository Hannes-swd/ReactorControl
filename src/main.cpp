#include "raylib.h"

#define RLIGHTS_IMPLEMENTATION
#include "core/enums.h"
#include "core/gameconfig.h"
#include "ui/camera_controller.h"
#include "ui/renderer.h"
#include "ui/table_cursor.h"

namespace {

const char* ViewSideName(ViewSide view) {
    switch (view) {
        case ViewSide::Left:   return "Links";
        case ViewSide::Center: return "Mitte";
        case ViewSide::Right:  return "Rechts";
    }
    return "";
}

} // namespace

int main() {
    InitWindow(GameConfig::ScreenWidth, GameConfig::ScreenHeight, "ReactorControl");
    SetTargetFPS(GameConfig::TargetFPS);

    CameraController cameraController;
    SceneRenderer sceneRenderer;
    TableCursor tableCursor;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            break;
        }

        cameraController.HandleInput();
        sceneRenderer.UpdateShaderCameraPosition(cameraController.GetCamera().position);
        tableCursor.Update(cameraController.GetCamera());

        BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginMode3D(cameraController.GetCamera());
        sceneRenderer.DrawScene();
        tableCursor.Draw();
        EndMode3D();

        if (!sceneRenderer.HasConsoleModel()) {
            DrawText("Kein Modell gefunden: assets/Desk.obj", 10, 40, 20, GRAY);
        }
        DrawFPS(10, 10);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
