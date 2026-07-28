#include "renderer.h"
#include "../core/gameconfig.h"

SceneRenderer::SceneRenderer()
    : lightShader{ 0 }, lights{ 0 }, hasConsoleModel(false), consoleModel{ 0 } {
    // Beleuchtungs-Shader (raylib Standard "lighting" Shader), damit die 3D-Modelle
    // mit Licht/Schatten sichtbar werden statt nur flach eingefaerbt zu sein.
    lightShader = LoadShader(GameConfig::LightingShaderVsPath, GameConfig::LightingShaderFsPath);
    lightShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(lightShader, "viewPos");

    int ambientLoc = GetShaderLocation(lightShader, "ambient");
    float ambient[4] = {
        GameConfig::AmbientColor[0], GameConfig::AmbientColor[1],
        GameConfig::AmbientColor[2], GameConfig::AmbientColor[3],
    };
    SetShaderValue(lightShader, ambientLoc, ambient, SHADER_UNIFORM_VEC4);

    lights[0] = CreateLight(LIGHT_POINT, GameConfig::LightPosition, Vector3{ 0.0f, 0.0f, 0.0f }, WHITE, lightShader);

    // Steuerpult-Modell aus Blender (assets/Desk.obj). Wird nur geladen, wenn die
    // Datei vorhanden ist, damit das Projekt auch ohne Modell weiter startet.
    hasConsoleModel = FileExists(GameConfig::ConsoleModelPath);
    if (hasConsoleModel) {
        consoleModel = LoadModel(GameConfig::ConsoleModelPath);
        for (int i = 0; i < consoleModel.materialCount; i++) {
            consoleModel.materials[i].shader = lightShader;
        }
    }
}

SceneRenderer::~SceneRenderer() {
    if (hasConsoleModel) {
        UnloadModel(consoleModel);
    }
    UnloadShader(lightShader);
}

void SceneRenderer::UpdateShaderCameraPosition(const Vector3& cameraPosition) {
    float cameraPos[3] = { cameraPosition.x, cameraPosition.y, cameraPosition.z };
    SetShaderValue(lightShader, lightShader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);
}

void SceneRenderer::DrawScene() const {
    if (hasConsoleModel) {
        DrawModel(consoleModel, Vector3{ 0.0f, 0.0f, 0.0f }, 1.0f, WHITE);
    }
    DrawSphereEx(lights[0].position, GameConfig::LightSphereRadius,
                 GameConfig::LightSphereRings, GameConfig::LightSphereSlices, YELLOW);
}
