#pragma once

#include "raylib.h"
#include "raymath.h"
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

// Platzierungssystem: JSON-Datei mit Grid-Platzierungen, Modelle liegen relativ zu ModelsDirectory.
// Eine Gridzelle ist immer exakt 1x1 Welteinheiten gross (siehe PlacementSystem).
constexpr const char* PlacementsJsonPath = "assets/placements.json";
constexpr const char* ModelsDirectory = "assets/models/";

constexpr Vector3 LightPosition = { 6.0f, 8.0f, 6.0f };
constexpr float AmbientColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };

constexpr float LightSphereRadius = 0.2f;
constexpr int LightSphereRings = 8;
constexpr int LightSphereSlices = 8;

// Die drei geneigten Pult-Oberflaechen (Mitte + linker/rechter Fluegel). Je Flaeche ist
// Origin die obere Ecke naeher am Spieler, UAxis/VAxis spannen die schraege Flaeche auf
// (aus den Top-Face-Vertices von Desk.obj abgeleitet, stehen senkrecht zueinander). Rows/
// Cols sind je Flaeche an deren Groesse angepasst, damit alle Zellen etwa gleich gross
// sind (~0.2 Einheiten).
struct TableSurface {
    Vector3 origin;
    Vector3 uAxis;
    Vector3 vAxis;
    int rows;
    int cols;
};

constexpr TableSurface TableSurfaces[] = {
    // Mitte (Cube, Top-Face v1/v5/v7/v3)
    { { -1.0f, 1.0f, -3.396848f }, { 2.0f, 1.327838f, 0.0f }, { 0.0f, 0.0f, 7.975538f }, 12, 40 },
    // Rechter Fluegel (Cube.001, Top-Face v9/v13/v15/v11)
    { { -0.438935f, 1.0f, 3.169075f }, { 1.438935f, 1.327838f, 1.409615f }, { -3.470372f, 0.0f, 3.527964f }, 12, 25 },
    // Linker Fluegel (Cube.002, Top-Face v17/v21/v23/v19)
    { { -3.972384f, 1.013700f, -5.481921f }, { 1.414213f, 1.327838f, -1.414213f }, { 3.499286f, 0.0f, 3.499286f }, 12, 25 },
};

constexpr size_t TableSurfaceCount = sizeof(TableSurfaces) / sizeof(TableSurfaces[0]);

// VAxis x UAxis zeigt bei allen drei Pult-Flaechen nach oben/aussen zum Spieler hin;
// die andere Reihenfolge zeigt ins Tischinnere. Gemeinsam genutzt von TableCursor (Hover-
// Markierung) und PlacementSystem (Grid-Platzierung), damit beide dieselbe Flaechen-
// Normale berechnen.
inline Vector3 SurfaceNormal(const TableSurface& surface) {
    return Vector3Normalize(Vector3CrossProduct(surface.vAxis, surface.uAxis));
}

// Kantenlaenge einer Gridzelle in Welteinheiten (~0.2). Rows/Cols sind ganzzahlig, deshalb
// sind die Zellen nicht exakt quadratisch - es wird die kleinere der beiden Kanten genommen,
// damit ein darauf skaliertes Modell garantiert in beide Richtungen in die Zelle passt.
inline float GridCellSize(const TableSurface& surface) {
    float uCell = Vector3Length(surface.uAxis) / static_cast<float>(surface.rows);
    float vCell = Vector3Length(surface.vAxis) / static_cast<float>(surface.cols);
    return fminf(uCell, vCell);
}

// Weltposition der Zellmitte (row, col) auf der gegebenen Pultflaeche.
inline Vector3 GridCellCenter(const TableSurface& surface, int row, int col) {
    float tu = (static_cast<float>(row) + 0.5f) / static_cast<float>(surface.rows);
    float tv = (static_cast<float>(col) + 0.5f) / static_cast<float>(surface.cols);
    return Vector3Add(surface.origin,
        Vector3Add(Vector3Scale(surface.uAxis, tu), Vector3Scale(surface.vAxis, tv)));
}

// Kreis, der der Maus (der "Hand" des Spielers) live auf der getroffenen Tischflaeche folgt.
constexpr float TableCursorRadius = 0.08f;
constexpr float TableCursorSurfaceOffset = 0.01f;
constexpr int TableCursorSegments = 24;

} // namespace GameConfig
