#pragma once

#include "raylib.h"
#include "raymath.h"
#include "enums.h"

#include <algorithm>

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

// Platzierungssystem: Ordner mit beliebig vielen JSON-Dateien (frei aufteilbar, z.B. pro
// Pult-Flaeche oder Button-Gruppe - siehe PlacementSystem), Modelle liegen relativ zu
// ModelsDirectory. Eine Gridzelle ist immer exakt 1x1 Welteinheiten gross (siehe PlacementSystem).
constexpr const char* PlacementsDirectory = "assets/placements/";
constexpr const char* ModelsDirectory = "assets/models/";

// Ordner mit den Lua-Skripten, die die Reaktor-/Button-Logik enthalten (siehe ScriptSystem).
// Beliebig viele Dateien, werden alle geladen und beim Speichern automatisch neu geladen.
constexpr const char* ScriptsDirectory = "assets/scripts/";

// Datei fuer eigene globale Variablen. Liegt im ScriptsDirectory und wird immer als erste
// Skriptdatei ausgefuehrt, damit alle anderen Skripte die Variablen von Anfang an kennen
// (siehe ScriptSystem). Sie muss nicht existieren - dann gibt es einfach keine Vorgaben.
constexpr const char* ScriptVariablesFile = "variables.lua";

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

// Rotationsmatrix, die ein Modell (lokal X=rechts/Y=oben/Z=vorne) exakt auf die Ausrichtung
// der Pultflaeche legt: Neigung UND Verdrehung. uAxis/vAxis stehen laut obigem Kommentar
// senkrecht zueinander und beide senkrecht zur Normalen, bilden also mit ihr bereits eine
// orthogonale Basis - es muss nur noch normalisiert werden. Dadurch braucht eine Platzierung
// (siehe PlacementSystem) keinen manuellen Korrektur-Winkel mehr, auch nicht auf den
// diagonal zur Haupttischachse stehenden Nebentischen.
inline Matrix SurfaceOrientation(const TableSurface& surface) {
    Vector3 right = Vector3Normalize(surface.uAxis);
    Vector3 forward = Vector3Normalize(surface.vAxis);
    Vector3 up = SurfaceNormal(surface);

    Matrix m = { 0 };
    m.m0 = right.x;   m.m1 = right.y;   m.m2 = right.z;
    m.m4 = up.x;      m.m5 = up.y;      m.m6 = up.z;
    m.m8 = forward.x; m.m9 = forward.y; m.m10 = forward.z;
    m.m15 = 1.0f;
    return m;
}

// Zeilen wechseln sich entlang der uAxis ab zwischen breiten "Button"-Zeilen (fuer 3D-Objekte,
// row 0, 2, 4, ...) und schmalen "Label"-Zeilen direkt darunter (fuer Textfelder, row 1, 3, 5,
// ...). surface.rows muss dafuer geradzahlig sein. ButtonRowWeight/LabelRowWeight bestimmen
// das Groessenverhaeltnis der beiden Zeilenarten zueinander (3:1 -> Button-Zeile ist 3x so
// hoch wie die Label-Zeile darunter). TableCursor (Hover/Gridlinien) und PlacementSystem
// (Platzierung) teilen sich diese Berechnung, damit beide exakt dieselben Zeilengrenzen sehen.
constexpr float ButtonRowWeight = 3.0f;
constexpr float LabelRowWeight = 1.0f;
constexpr float RowPairWeight = ButtonRowWeight + LabelRowWeight;

inline bool IsButtonRow(int row) { return (row % 2) == 0; }

// Bruchteil (0..1) entlang uAxis, an dem die Zeilengrenze VOR `row` liegt. row darf auch
// gleich surface.rows sein (untere Aussenkante der letzten Zeile).
inline float RowBoundaryFraction(const TableSurface& surface, int row) {
    int pairs = surface.rows / 2;
    float totalWeight = static_cast<float>(pairs) * RowPairWeight;
    int pairIndex = row / 2;
    float weight = static_cast<float>(pairIndex) * RowPairWeight;
    if (row % 2 != 0) {
        weight += ButtonRowWeight;
    }
    return weight / totalWeight;
}

// Kehrfunktion zu RowBoundaryFraction: liefert die Zeile, in deren Bereich der Bruchteil tu
// (0..1 entlang uAxis) faellt. Wird von TableCursor beim Hover gebraucht.
inline int RowFromFraction(const TableSurface& surface, float tu) {
    int pairs = surface.rows / 2;
    float totalWeight = static_cast<float>(pairs) * RowPairWeight;
    float weighted = Clamp(tu, 0.0f, 0.9999f) * totalWeight;
    int pairIndex = std::min(static_cast<int>(weighted / RowPairWeight), pairs - 1);
    float withinPair = weighted - static_cast<float>(pairIndex) * RowPairWeight;
    return pairIndex * 2 + (withinPair < ButtonRowWeight ? 0 : 1);
}

// Breite einer Gridspalte in Welteinheiten (entlang vAxis) - gleich fuer Button- und
// Label-Zeilen, da nur Zeilen (uAxis), nicht Spalten (vAxis), unterschiedlich hoch sind.
inline float CellWidth(const TableSurface& surface) {
    return Vector3Length(surface.vAxis) / static_cast<float>(surface.cols);
}

// Hoehe einer Label-Zeile in Welteinheiten (entlang uAxis).
inline float LabelCellHeight(const TableSurface& surface) {
    int pairs = surface.rows / 2;
    float totalWeight = static_cast<float>(pairs) * RowPairWeight;
    return Vector3Length(surface.uAxis) * (LabelRowWeight / totalWeight);
}

// Kantenlaenge einer Button-Zelle in Welteinheiten. Es wird die kleinere der beiden Kanten
// genommen, damit ein darauf skaliertes Modell garantiert in beide Richtungen in die Zelle
// passt.
inline float ButtonCellSize(const TableSurface& surface) {
    int pairs = surface.rows / 2;
    float totalWeight = static_cast<float>(pairs) * RowPairWeight;
    float uCell = Vector3Length(surface.uAxis) * (ButtonRowWeight / totalWeight);
    return fminf(uCell, CellWidth(surface));
}

// Weltposition der Zellmitte (row, col) auf der gegebenen Pultflaeche.
inline Vector3 GridCellCenter(const TableSurface& surface, int row, int col) {
    float tuStart = RowBoundaryFraction(surface, row);
    float tuEnd = RowBoundaryFraction(surface, row + 1);
    float tu = (tuStart + tuEnd) * 0.5f;
    float tv = (static_cast<float>(col) + 0.5f) / static_cast<float>(surface.cols);
    return Vector3Add(surface.origin,
        Vector3Add(Vector3Scale(surface.uAxis, tu), Vector3Scale(surface.vAxis, tv)));
}

// Darstellung der Gruppen-Textfelder (siehe PlacementSystem): der Text wird einmalig beim
// Laden auf eine Textur gerendert und dann als flaches Quad IN der Label-Zeile gezeichnet -
// liegt also genau wie die Button-Modelle schraeg auf der geneigten Pultflaeche, statt immer
// gerade auf dem Bildschirm zu stehen.
constexpr int LabelFontSizePx = 40; // Render-Aufloesung der Text-Textur in Pixeln
constexpr int LabelTexturePadding = 6;
constexpr int LabelBoldRadius = 2; // "Fake Bold": Text wird (2*r+1)^2-fach mit Versatz gezeichnet
constexpr float LabelCellFillRatio = 0.9f; // Text nutzt max. 90% der Label-Zelle, Rest Rand
constexpr float LabelSurfaceOffset = 0.015f; // Abstand von der Tischflaeche (gegen Z-Fighting)
constexpr Color LabelTextColor = { 20, 20, 20, 255 };

// Kreis, der der Maus (der "Hand" des Spielers) live auf der getroffenen Tischflaeche folgt.
constexpr float TableCursorRadius = 0.08f;
constexpr float TableCursorSurfaceOffset = 0.01f;
constexpr int TableCursorSegments = 24;

// Rahmen um gruppierte Buttons (siehe PlacementSystem, "frames" in der JSON).
constexpr float FrameSurfaceOffset = 0.02f;
constexpr Color FrameColor = YELLOW;

} // namespace GameConfig
