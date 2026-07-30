#pragma once

#include "raylib.h"
#include "../external/rlights.h"

// Laedt den Beleuchtungs-Shader, das Licht und das Steuerpult-Modell und zeichnet
// die 3D-Szene. Uebernimmt Besitz aller GPU-Ressourcen (Shader/Model) und gibt sie
// im Destruktor frei.
class SceneRenderer {
public:
    SceneRenderer();
    ~SceneRenderer();

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    // Muss einmal pro Frame vor BeginDrawing aufgerufen werden, damit der Shader die
    // aktuelle Kameraposition fuer die Lichtberechnung kennt.
    void UpdateShaderCameraPosition(const Vector3& cameraPosition);

    // Zeichnet die Szene (Pult-Modell + Licht-Kugel). Muss innerhalb von BeginMode3D stehen.
    void DrawScene() const;

    bool HasConsoleModel() const { return hasConsoleModel; }

    // Damit extern geladene Modelle (z.B. PlacementSystem) mit dem gleichen Licht
    // beleuchtet werden wie die Szene.
    Shader GetLightShader() const { return lightShader; }

private:
    Shader lightShader;
    Light lights[MAX_LIGHTS];
    bool hasConsoleModel;
    Model consoleModel;
};
