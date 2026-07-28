#include "raylib.h"

#define RLIGHTS_IMPLEMENTATION
#include "external/rlights.h"

int main() {
    const int screenWidth = 1600;
    const int screenHeight = 900;

    InitWindow(screenWidth, screenHeight, "ReactorControl");
    SetTargetFPS(60);

    // Spieler steht fest zwischen den Tischen (in der Oeffnung des Pults) und kann nur
    // zwischen 3 Blickrichtungen wechseln (links/mitte/rechts) - keine freie Kamera.
    // standPosition.z = 0.591 = exakte Z-Mitte der Mittelbox (Z reicht von -3.397 bis 4.579).
    // Reihenfolge im Array entspricht der tatsaechlichen Blickrichtung auf dem Bildschirm
    // (links -> mitte -> rechts), damit die Pfeiltasten am Rand direkt zur anderen Seite
    // umschalten koennen, ohne ueber die Mitte zu muessen.
    Vector3 standPosition = Vector3{ -4.7f, 3.9f, 0.591f };
    Vector3 viewTargets[3] = {
        Vector3{ -1.5f, 0.7f, -4.4f },   // links  -> Mitte des linken Fluegels (Cube.002)
        Vector3{  0.0f, 0.7f,  0.591f }, // mitte  -> gleiche Z wie standPosition, damit der Blick gerade ist
        Vector3{ -1.5f, 0.7f,  5.6f },   // rechts -> Mitte des rechten Fluegels (Cube.001)
    };
    int currentView = 1; // Start: Mitte

    Camera3D camera = { 0 };
    camera.position = standPosition;
    camera.target = viewTargets[currentView];
    camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Beleuchtungs-Shader (raylib Standard "lighting" Shader), damit die 3D-Modelle
    // mit Licht/Schatten sichtbar werden statt nur flach eingefaerbt zu sein.
    Shader lightShader = LoadShader("assets/shaders/glsl330/lighting.vs", "assets/shaders/glsl330/lighting.fs");
    lightShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(lightShader, "viewPos");

    int ambientLoc = GetShaderLocation(lightShader, "ambient");
    float ambient[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
    SetShaderValue(lightShader, ambientLoc, ambient, SHADER_UNIFORM_VEC4);

    Light lights[MAX_LIGHTS] = { 0 };
    lights[0] = CreateLight(LIGHT_POINT, Vector3{ 6.0f, 8.0f, 6.0f }, Vector3{ 0.0f, 0.0f, 0.0f }, WHITE, lightShader);

    // Steuerpult-Modell aus Blender (assets/Desk.obj). Wird nur geladen, wenn die
    // Datei vorhanden ist, damit das Projekt auch ohne Modell weiter startet.
    bool hasConsoleModel = FileExists("assets/Desk.obj");
    Model consoleModel = { 0 };
    if (hasConsoleModel) {
        consoleModel = LoadModel("assets/Desk.obj");
        for (int i = 0; i < consoleModel.materialCount; i++) {
            consoleModel.materials[i].shader = lightShader;
        }
    }

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            break;
        }

        // Pfeiltasten wechseln die Blickrichtung (links/mitte/rechts) schrittweise, nicht
        // zyklisch: an den Raendern (links/rechts) bewegt nur die Taste in Richtung Mitte
        // etwas, die Taste zum Rand hin macht nichts. Die Position bleibt fest.
        if (IsKeyPressed(KEY_RIGHT)) {
            if (currentView < 2) {
                currentView++;
            }
        } else if (IsKeyPressed(KEY_LEFT)) {
            if (currentView > 0) {
                currentView--;
            }
        }
        camera.position = standPosition;
        camera.target = viewTargets[currentView];

        float cameraPos[3] = { camera.position.x, camera.position.y, camera.position.z };
        SetShaderValue(lightShader, lightShader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

        BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginMode3D(camera);

        if (hasConsoleModel) {
            DrawModel(consoleModel, Vector3{ 0.0f, 0.0f, 0.0f }, 1.0f, WHITE);
        }

        DrawSphereEx(lights[0].position, 0.2f, 8, 8, YELLOW);

        EndMode3D();

        if (!hasConsoleModel) {
            DrawText("Kein Modell gefunden: assets/Desk.obj", 10, 40, 20, GRAY);
        }
        const char* viewNames[3] = { "Links", "Mitte", "Rechts" };
        DrawFPS(10, 10);

        EndDrawing();
    }

    if (hasConsoleModel) {
        UnloadModel(consoleModel);
    }
    UnloadShader(lightShader);

    CloseWindow();

    return 0;
}
