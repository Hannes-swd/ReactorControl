#pragma once

#include "raylib.h"
#include "../core/gameconfig.h"

#include <string>
#include <unordered_map>
#include <vector>

// Laedt Gruppen-Platzierungen aus einer JSON-Datei (siehe GameConfig::PlacementsJsonPath) und
// zeichnet sie an ihrer Gridposition. Eine Gruppe ist ein Button (3D-Modell, .glb etc.) auf
// einer Button-Zeile plus ein Textfeld, das automatisch in der Label-Zeile direkt darunter
// erscheint (siehe GameConfig::IsButtonRow) - exakt dasselbe Grid, das TableCursor beim Hover
// einfaerbt. So landet ein Button garantiert in der sichtbaren Gridzelle und nicht an einer
// frei geschaetzten Weltkoordinate. Pflichtfelder pro Gruppe sind "text" (das Textfeld) und
// "button" mit "model", "section", "row" (muss eine Button-Zeile sein) und "col"; alles
// andere hat einen automatischen Default:
// - "button.name" ist ein freier Bezeichner der Gruppe (z.B. fuer spaetere Interaktionslogik),
//   wird nur gespeichert und aktuell nicht weiter ausgewertet.
// - Modelle koennen in Blender beliebig gross exportiert sein: pro Modell wird anhand seiner
//   Bounding Box automatisch ein Normalisierungsfaktor berechnet, der die groesste Ausdehnung
//   auf die Kantenlaenge einer Button-Zelle bringt (siehe GameConfig::ButtonCellSize).
//   "button.scale" (Default 1.0) skaliert relativ dazu - 1.0 "fuellt genau ein Kaestchen".
// - Die Neigung zur (moeglicherweise geneigten) Tischflaeche wird automatisch aus deren
//   Normale berechnet, dafuer muss nichts angegeben werden.
// - "button.rotation" (Default 0) ist optional die Blickrichtung/Ausrichtung des Objekts auf
//   dem Grid, in Grad um seine eigene Hochachse, bevor es auf die Flaeche gekippt wird.
// - "button.height" (Default 0) ist optional ein Abstand von der Tischflaeche entlang deren
//   Normale.
//
// Optional koennen mehrere nebeneinanderliegende Gruppen per "frames" zu einem Block
// zusammengefasst werden, der einen Rahmen aussen herum bekommt (Button- und Label-Zeile
// eingeschlossen):
//   "frames": [ { "buttons": ["pumpe1_schluessel", "pumpe2_schluessel"] } ]
// Referenziert werden die "button.name"-Werte der jeweiligen Gruppen - diese muessen also
// gesetzt sein. Alle referenzierten Buttons einer Frame muessen auf derselben "section"
// liegen; die umschlossene Flaeche ergibt sich automatisch aus deren minimaler/maximaler
// Zeile/Spalte.
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

    // Zeichnet alle Button-Modelle und Textfelder. Muss innerhalb von BeginMode3D stehen.
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
        std::string name; // button.name aus der JSON, frei fuer spaetere Interaktionslogik
    };

    // Textfeld einer Gruppe: flaches Quad, das - wie die Button-Modelle - schraeg in der
    // Label-Zeile auf der Pultflaeche liegt. corners sind in Zeichenreihenfolge fuer einen
    // Triangle-Fan/Quad: oben-links, oben-rechts, unten-rechts, unten-links.
    struct LabelPlacement {
        Texture2D texture;
        Vector3 corners[4];
    };

    // Rahmen um eine Gruppe von Buttons (siehe "frames" in der JSON): reine Aussenlinie, kein
    // gefuelltes Quad, daher nur die vier Eckpunkte (Zeichenreihenfolge wie LabelPlacement).
    struct FramePlacement {
        Vector3 corners[4];
    };

    LoadedModel& GetOrLoadModel(const std::string& path, Shader shader);

    // Rendert `text` auf eine Textur und berechnet die vier Eckpunkte des Quads, mittig in der
    // Label-Zeile (row, col) auf `surface`, seitenverhaeltnis-treu eingepasst (kein Verzerren).
    static LabelPlacement BuildLabel(const GameConfig::TableSurface& surface, int row, int col, const std::string& text);

    // Berechnet die vier Eckpunkte eines Rahmens auf `surface`, der den Bruchteils-Bereich
    // [tuStart,tuEnd] x [tvStart,tvEnd] umschliesst (gleiche tu/tv-Konvention wie
    // GameConfig::RowBoundaryFraction).
    static FramePlacement BuildFrame(const GameConfig::TableSurface& surface, float tuStart, float tuEnd, float tvStart, float tvEnd);

    // unordered_map: Referenzen/Pointer auf Elemente bleiben beim Einfuegen weiterer Eintraege
    // gueltig, daher duerfen Placement-Eintraege sicher auf hier gespeicherte LoadedModels zeigen.
    std::unordered_map<std::string, LoadedModel> loadedModels;
    std::vector<Placement> placements;
    std::vector<LabelPlacement> labelPlacements;
    std::vector<FramePlacement> framePlacements;
};
