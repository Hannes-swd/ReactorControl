#pragma once

#include "raylib.h"

#include <string>
#include <unordered_map>
#include <vector>

// Laedt Objekt-Platzierungen aus einer JSON-Datei (siehe GameConfig::PlacementsJsonPath) und
// zeichnet die referenzierten Modelle (.glb etc.) an ihrer Gridposition. Eine Gridzelle ist
// exakt 1x1 Welteinheiten gross. Modelle koennen in Blender beliebig gross exportiert sein:
// pro Modell wird anhand seiner Bounding Box automatisch ein Normalisierungsfaktor berechnet,
// der die groesste Ausdehnung auf 1 Einheit skaliert. scale=1.0 in der JSON entspricht also
// immer "fuellt eine Zelle", scale=0.5 der Haelfte davon - unabhaengig von der Rohgroesse.
class PlacementSystem {
public:
    PlacementSystem() = default;
    ~PlacementSystem();

    PlacementSystem(const PlacementSystem&) = delete;
    PlacementSystem& operator=(const PlacementSystem&) = delete;

    // Parst die JSON-Datei und laedt alle referenzierten Modelle (einmal pro Datei, auch wenn
    // sie mehrfach platziert werden). shader wird auf alle geladenen Materialien angewendet,
    // damit platzierte Objekte gleich beleuchtet werden wie der Rest der Szene. Fehlende Datei
    // oder fehlende Einzel-Modelle werden uebersprungen statt abzustuerzen.
    void LoadFromJson(const char* jsonPath, Shader shader);

    // Zeichnet alle platzierten Objekte. Muss innerhalb von BeginMode3D stehen.
    void Draw() const;

private:
    struct LoadedModel {
        Model model;
        float autoScale; // 1 / groesste Bounding-Box-Ausdehnung (rotationsunabhaengig)
        BoundingBox bounds; // lokale, unskalierte Bounding Box (fuer Boden-Ausgleich)
    };

    struct Placement {
        LoadedModel* loadedModel;
        Vector3 position;
        float scale;
        // rotationX/Z: Korrektur-Tilt, damit das Modell richtig zur Pultneigung liegt.
        // rotationY: Blickrichtung/Ausrichtung auf dem Grid.
        float rotationX;
        float rotationY;
        float rotationZ;
    };

    LoadedModel& GetOrLoadModel(const std::string& path, Shader shader);

    // unordered_map: Referenzen/Pointer auf Elemente bleiben beim Einfuegen weiterer Eintraege
    // gueltig, daher duerfen Placement-Eintraege sicher auf hier gespeicherte LoadedModels zeigen.
    std::unordered_map<std::string, LoadedModel> loadedModels;
    std::vector<Placement> placements;
};
