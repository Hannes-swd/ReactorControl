#pragma once

#include "raylib.h"

#include <string>
#include <unordered_map>
#include <vector>

// Laedt Objekt-Platzierungen aus einer JSON-Datei (siehe GameConfig::PlacementsJsonPath) und
// zeichnet die referenzierten Modelle (.glb etc.) an ihrer Gridposition. Jede Platzierung
// referenziert eine der drei geneigten Pultflaechen aus GameConfig::TableSurfaces (section)
// sowie eine Zelle (row, col) auf deren Grid - exakt dasselbe Grid, das TableCursor beim
// Hover einfaerbt. So landet ein Objekt garantiert in der sichtbaren Gridzelle und nicht an
// einer frei geschaetzten Weltkoordinate. Pflichtfelder in der JSON sind nur "model",
// "section", "row" und "col"; alles andere hat einen automatischen Default:
// - Modelle koennen in Blender beliebig gross exportiert sein: pro Modell wird anhand seiner
//   Bounding Box automatisch ein Normalisierungsfaktor berechnet, der die groesste Ausdehnung
//   auf die Kantenlaenge einer Gridzelle (~0.2 Welteinheiten, siehe GameConfig::GridCellSize)
//   bringt. "scale" (Default 1.0) skaliert relativ dazu - 1.0 "fuellt genau ein Kaestchen".
// - Die Neigung zur (moeglicherweise geneigten) Tischflaeche wird automatisch aus deren
//   Normale berechnet, dafuer muss nichts angegeben werden.
// - "rotation" (Default 0) ist optional die Blickrichtung/Ausrichtung des Objekts auf dem
//   Grid, in Grad um seine eigene Hochachse, bevor es auf die Flaeche gekippt wird.
// - "height" (Default 0) ist optional ein Abstand von der Tischflaeche entlang deren Normale.
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
        // Fertige Rotationsmatrix: Blickrichtung (rotation) um die eigene Hochachse, gefolgt
        // von der automatisch berechneten Kippung zur Pultneigung. Wird einmalig beim Laden
        // bestimmt, da sich weder Platzierung noch Tischflaechen zur Laufzeit aendern.
        Matrix rotation;
    };

    LoadedModel& GetOrLoadModel(const std::string& path, Shader shader);

    // unordered_map: Referenzen/Pointer auf Elemente bleiben beim Einfuegen weiterer Eintraege
    // gueltig, daher duerfen Placement-Eintraege sicher auf hier gespeicherte LoadedModels zeigen.
    std::unordered_map<std::string, LoadedModel> loadedModels;
    std::vector<Placement> placements;
};
