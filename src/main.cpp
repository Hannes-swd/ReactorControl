#include "raylib.h"

#include <string>

#define RLIGHTS_IMPLEMENTATION
#include "core/enums.h"
#include "core/gameconfig.h"
#include "scene/placement_system.h"
#include "script/script_system.h"
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
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(GameConfig::ScreenWidth, GameConfig::ScreenHeight, "ReactorControl");
    SetWindowMinSize(960, 540);
    SetTargetFPS(GameConfig::TargetFPS);

    CameraController cameraController;
    SceneRenderer sceneRenderer;
    TableCursor tableCursor;

    PlacementSystem placementSystem;
    placementSystem.LoadFromDirectory(GameConfig::PlacementsDirectory, sceneRenderer.GetLightShader());

    // Spiellogik liegt in Lua-Skripten (assets/scripts/*.lua) und wird zur Laufzeit beim
    // Speichern automatisch neu geladen - siehe script_system.h.
    ScriptSystem scriptSystem;
    scriptSystem.Load(GameConfig::ScriptsDirectory, placementSystem.GetRegistry());

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            break;
        }

        cameraController.HandleInput();
        sceneRenderer.UpdateShaderCameraPosition(cameraController.GetCamera().position);
        tableCursor.Update(cameraController.GetCamera());

        // Klick-Erkennung: jeden Frame neu ermitteln (nicht nur bei tatsaechlichem Klick),
        // damit ElementRegistry::WasClicked() zuverlaessig nach genau einem Frame wieder
        // false liefert - siehe element_registry.h.
        std::string clickedElementId;
        if (tableCursor.IsVisible() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            auto elementId = placementSystem.FindElementAt(
                tableCursor.HoverSection(), tableCursor.HoverRow(), tableCursor.HoverCol());
            if (elementId.has_value()) {
                clickedElementId = *elementId;
            }
        }
        placementSystem.GetRegistry().SetFrameClick(clickedElementId);

        // Nach SetFrameClick, damit die Skripte den Klick dieses Frames sehen (und den bereits
        // von der Registry ausgefuehrten Zustandswechsel/Trigger noch ueberschreiben koennen).
        scriptSystem.Update(GetFrameTime());

        BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginMode3D(cameraController.GetCamera());
        sceneRenderer.DrawScene();
        placementSystem.Draw();
        tableCursor.Draw();
        EndMode3D();

        if (!sceneRenderer.HasConsoleModel()) {
            DrawText("Kein Modell gefunden: assets/Desk.obj", 10, 40, 20, GRAY);
        }
        // Skriptfehler direkt im Fenster anzeigen, damit man beim Schreiben der Logik nicht in
        // der Konsole nachsehen muss - nach dem Speichern der korrigierten Datei ist sie weg.
        if (!scriptSystem.LastError().empty()) {
            DrawText(scriptSystem.LastError().c_str(), 10, GetScreenHeight() - 30, 18, RED);
        }
        DrawFPS(10, 10);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
