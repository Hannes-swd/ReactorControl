#pragma once

#include "raylib.h"
#include "enums.h"

// Statische Konfigurationswerte des Spiels (Fenster, Kamera-Positionen, Asset-Pfade, ...).
// Alles hier ist konstant zur Compile-Zeit, damit im Rest des Codes keine magischen Zahlen
// mehr vorkommen.
namespace GameConfig {

constexpr int ScreenWidth = 1600;
constexpr int ScreenHeight = 900;
constexpr int TargetFPS = 60;

// Spieler steht fest zwischen den Tischen (in der Oeffnung des Pults) und kann nur
// zwischen 3 Blickrichtungen wechseln (links/mitte/rechts) - keine freie Kamera.
// StandPosition.z = 0.591 = exakte Z-Mitte der Mittelbox (Z reicht von -3.397 bis 4.579).
constexpr Vector3 StandPosition = { -4.7f, 3.9f, 0.591f };

// Blickziel je Blickrichtung. Index entspricht ViewSide (links -> mitte -> rechts).
constexpr Vector3 ViewTargets[ViewSideCount] = {
    { -1.5f, 0.7f, -4.4f },   // ViewSide::Left   -> Mitte des linken Fluegels (Cube.002)
    {  0.0f, 0.7f,  0.591f }, // ViewSide::Center -> gleiche Z wie StandPosition, Blick ist gerade
    { -1.5f, 0.7f,  5.6f },   // ViewSide::Right  -> Mitte des rechten Fluegels (Cube.001)
};

constexpr ViewSide InitialView = ViewSide::Center;
constexpr float CameraFovY = 45.0f;

constexpr const char* ConsoleModelPath = "assets/Desk.obj";
constexpr const char* LightingShaderVsPath = "assets/shaders/glsl330/lighting.vs";
constexpr const char* LightingShaderFsPath = "assets/shaders/glsl330/lighting.fs";

constexpr Vector3 LightPosition = { 6.0f, 8.0f, 6.0f };
constexpr float AmbientColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };

constexpr float LightSphereRadius = 0.2f;
constexpr int LightSphereRings = 8;
constexpr int LightSphereSlices = 8;

} // namespace GameConfig
