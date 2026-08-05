#pragma once

#include "raylib.h"
#include "../core/gameconfig.h"
#include "element_registry.h"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Laedt Typen und Element-Platzierungen aus ALLEN JSON-Dateien in einem Ordner (siehe
// GameConfig::PlacementsDirectory) und zeichnet sie an ihrer Gridposition. Die Aufteilung auf
// Dateien ist voellig frei (z.B. eine Datei pro Pult-Flaeche oder Button-Gruppe) - jede Datei
// darf eigene "types"/"elements"/"frames" enthalten, alle werden zusammengefuehrt, bevor
// irgendetwas ausgewertet wird (siehe LoadFromDirectory). So bleibt eine einzelne Datei bei
// vielen Buttons uebersichtlich, ohne dass Typen/Elemente/Frames dateiuebergreifend etwas
// Besonderes waeren. Ein Element ist ein Button/Schalter/Lampe/... (3D-Modell, .glb etc.) auf
// einer Button-Zeile plus ein Textfeld, das automatisch in der Label-Zeile direkt darunter
// erscheint (siehe GameConfig::IsButtonRow) - exakt dasselbe Grid, das TableCursor beim Hover
// einfaerbt. So landet ein Element garantiert in der sichtbaren Gridzelle und nicht an einer
// frei geschaetzten Weltkoordinate.
//
// Zustand/Klick eines Elements sind komplett von Rendering getrennt: PlacementSystem
// besitzt dafuer eine ElementRegistry (siehe element_registry.h), die weder Modelle noch
// Grid kennt. PlacementSystem uebersetzt beim Laden die JSON-Typen/Elemente in Registry-
// Eintraege UND in eigene Render-Daten (Modelle pro Zustand); beim Zeichnen fragt es die
// Registry nach dem aktuellen Zustand jeder Instanz.
//
// JSON-Struktur:
//   "types": [
//     { "id": "schluessel_1", "states": ["aus"], "models": { "aus": "schluessel.glb" } }
//   ]
//   Pflichtfelder pro Typ: "id", "states" (nicht-leere Liste geordneter Zustandsnamen),
//   "models" (Map Zustandsname -> Modell-Dateiname unter GameConfig::ModelsDirectory,
//   fuer jeden Zustand ein Eintrag). Fehlt ein Eintrag oder die Datei, wird der GESAMTE
//   Typ uebersprungen statt abzustuerzen. Optional mit Default:
//   - "size" ({ "cols": 2, "rows": 2 }, Default je 1) macht den Typ mehrere Button-Zellen
//     gross. "rows" zaehlt BUTTON-Zeilen: rows=2 belegt die Zeilen row und row+2 und schluckt
//     die Label-Zeile dazwischen (siehe GameConfig::LabelRowBelow). Die Groesse gehoert bewusst
//     zum Typ und nicht zum Element - sie steckt in der Form des Modells, und die Belegungs-
//     karte fuer Klicks (elementsByCell) bliebe sonst je nach Platzierung unterschiedlich.
//   - "fit" ("uniform" oder "stretch", Default "uniform") bestimmt, wie das Modell in den Block
//     skaliert wird. "uniform" behaelt die Proportionen und passt in die kleinere Blockkante;
//     "stretch" zieht Laengs- und Querrichtung getrennt auf die Blockkanten, sodass der Block
//     exakt ausgefuellt wird - fuer rechteckige Displaygehaeuse gedacht, deren Seitenverhaeltnis
//     man nicht in Blender treffen kann, weil es von cols/rows der Pultflaeche abhaengt.
//   - "screen" macht den Typ zu einem Display, siehe unten.
//
// Displays: ein Display ist ein ganz normales Element, dessen Modell zusaetzlich eine
// Bildschirmflaeche enthaelt. Wo diese Flaeche liegt, wird NICHT in der JSON vermessen, sondern
// in Blender modelliert: eine eigene Flaeche im .glb mit eigenem Material. Erkannt wird sie an
// der Basisfarbe dieses Materials (siehe GameConfig::ScreenMarkerColor) - raylib wirft Mesh- und
// Materialnamen beim Laden weg, die Basisfarbe ueberlebt. Die Flaeche braucht eine UV-Map, wie
// sie herum liegt ist aber egal: die Texturkoordinaten werden beim Laden aus der Geometrie neu
// berechnet (siehe PrepareScreenMesh), damit der Inhalt immer richtig herum steht.
//   "types": [
//     { "id": "display_gross", "states": ["an"], "models": { "an": "display.glb" },
//       "size": { "cols": 4, "rows": 2 }, "fit": "stretch",
//       "screen": { "width": 256, "height": 224, "marker": [0, 0, 0] } }
//   ]
//   Alle Felder unter "screen" sind optional ("width"/"height": Aufloesung der Displaytextur,
//   Default GameConfig::ScreenDefault...; "marker": Basisfarbe der Bildschirmflaeche als
//   [r,g,b], Default GameConfig::ScreenMarkerColor). Findet sich im Modell keine Flaeche mit
//   dieser Farbe, wird das Element ganz normal ohne Bildschirm gezeichnet.
//   Jede Display-INSTANZ bekommt eine eigene Textur - zwei Elemente desselben Typs zeigen also
//   Verschiedenes, obwohl sie sich Modell und Geometrie teilen. Der Inhalt wird von aussen
//   gezeichnet (siehe RenderDisplays); die Bildschirmflaeche wird unbeleuchtet gezeichnet, damit
//   sie unabhaengig vom Raumlicht leuchtet.
//
//   "elements": [
//     { "id": "notaus_taste", "type": "schluessel_1", "section": 0, "row": 6, "col": 20,
//       "text": "N1" }
//   ]
//   Pflichtfelder pro Element: "id" (eindeutiger Bezeichner, fuer Registry/Klicks/frames),
//   "type" (Referenz auf types[].id), "section", "row" (muss eine Button-Zeile sein),
//   "col", "text" (das Textfeld, faellt automatisch in die Label-Zeile drunter). Optional
//   mit automatischem Default:
//   - "state" (Default: erster Eintrag in types[].states) ist der Start-Zustandsname.
//   - Modelle koennen in Blender beliebig gross exportiert sein: pro Modell wird anhand
//     seiner Bounding Box automatisch ein Normalisierungsfaktor berechnet, der die groesste
//     Ausdehnung auf die Kantenlaenge einer Button-Zelle bringt (siehe
//     GameConfig::ButtonCellSize). "scale" (Default 1.0) skaliert relativ dazu.
//   - Die Neigung zur (moeglicherweise geneigten) Tischflaeche wird automatisch aus deren
//     Normale berechnet.
//   - "rotation" (Default 0) ist optional die Blickrichtung/Ausrichtung des Objekts auf dem
//     Grid, in Grad um seine eigene Hochachse, bevor es auf die Flaeche gekippt wird.
//   - "height" (Default 0) ist optional ein Abstand von der Tischflaeche entlang deren
//     Normale.
//
// Optional koennen mehrere nebeneinanderliegende Elemente per "frames" zu einem Block
// zusammengefasst werden, der einen Rahmen aussen herum bekommt (Button- und Label-Zeile
// eingeschlossen):
//   "frames": [ { "elements": ["pumpe1_schluessel", "pumpe2_schluessel"] } ]
// Referenziert werden die "id"-Werte der jeweiligen Elemente. Alle referenzierten Elemente
// einer Frame muessen auf derselben "section" liegen; die umschlossene Flaeche ergibt sich
// automatisch aus deren minimaler/maximaler Zeile/Spalte.
//
// Reaktorlogik/Trigger: ein Klick auf ein Element kann per "triggers" zusaetzlich andere
// Elemente auf einen bestimmten Zustand setzen, unabhaengig vom eigenen Zustandswechsel des
// geklickten Elements (siehe ElementRegistry::RegisterTrigger/SetFrameClick):
//   "triggers": [ { "on": "notaus_taste", "set": { "lampe_1": "aus" } } ]
// "on" ist die id des Elements, dessen Klick den Trigger ausloest; "set" ist eine Map von
// Ziel-Element-id auf den Zustandsnamen, der dabei gesetzt wird (beliebig viele Ziele pro
// Trigger, beliebig viele Trigger pro "on"-id). Ungueltige ids fuehren nur zu einem
// wirkungslosen Trigger, kein Absturz.
class PlacementSystem {
public:
    PlacementSystem() = default;
    ~PlacementSystem();

    PlacementSystem(const PlacementSystem&) = delete;
    PlacementSystem& operator=(const PlacementSystem&) = delete;

    // Parst alle *.json-Dateien direkt in dirPath (nicht rekursiv) und laedt alle
    // referenzierten Modelle (einmal pro Modell-Datei, auch wenn sie mehrfach platziert oder
    // von mehreren JSON-Dateien referenziert werden). Reihenfolge zwischen Dateien spielt keine
    // Rolle: erst werden alle "types" aus allen Dateien eingelesen, dann alle "elements", dann
    // alle "frames" - ein Element kann also z.B. in einer anderen Datei stehen als der Typ, den
    // es referenziert, oder als der Frame, der es einschliesst. shader wird auf alle geladenen
    // Materialien angewendet, damit platzierte Objekte gleich beleuchtet werden wie der Rest
    // der Szene. Fehlender Ordner, fehlende Einzel-Modelle oder unvollstaendige Eintraege
    // werden uebersprungen statt abzustuerzen.
    void LoadFromDirectory(const char* dirPath, Shader shader);

    // Zeichnet alle Element-Modelle (im jeweils aktuellen Zustand laut Registry) und
    // Textfelder. Muss innerhalb von BeginMode3D stehen.
    void Draw() const;

    // Fuellt die Textur jedes Displays neu: ruft fuer jedes Display drawContent(id, breite,
    // hoehe) auf, waehrend dessen Textur als Renderziel aktiv ist. drawContent zeichnet also mit
    // ganz normalen 2D-Aufrufen in Pixelkoordinaten (0,0 = links oben) und muss ueberhaupt
    // nichts ueber die 3D-Platzierung wissen.
    //
    // MUSS ausserhalb von BeginMode3D (und am besten vor BeginDrawing) aufgerufen werden -
    // BeginTextureMode darf nicht innerhalb eines 3D-Modus stehen. Deshalb ist das eine eigene
    // Funktion und nicht Teil von Draw().
    //
    // Der Inhalt kommt bewusst per Callback von aussen statt aus einem Zeiger auf das
    // Skriptsystem: PlacementSystem muss so nichts ueber Lua wissen und umgekehrt.
    void RenderDisplays(const std::function<void(const std::string& id, int width, int height)>& drawContent);

    // Zustand/Klicks aller geladenen Elemente - siehe element_registry.h. Bleibt fuer die
    // gesamte Laufzeit gueltig (nicht nur waehrend LoadFromJson), main.cpp/Interaktionscode
    // greift darueber lesend und schreibend zu.
    ElementRegistry& GetRegistry() { return registry; }
    const ElementRegistry& GetRegistry() const { return registry; }

    // Reverse-Lookup: welches Element (falls ueberhaupt eines) liegt in der Button-Zelle
    // (section, row, col)? Wird fuer Klick-Erkennung gebraucht (siehe TableCursor::Hover...).
    std::optional<std::string> FindElementAt(int section, int row, int col) const;

private:
    struct LoadedModel {
        Model model;
        float autoScale; // 1 / groesste Bounding-Box-Ausdehnung (rotationsunabhaengig)
        BoundingBox bounds; // lokale, unskalierte Bounding Box (fuer Boden-Ausgleich)
        Vector3 size; // Rohabmessungen der Bounding Box (fuer achsenweise Skalierung, "stretch")
        int screenMesh = -1; // Index des Bildschirm-Meshes, -1 = keines (siehe PrepareScreenMesh)
    };

    // Eine Display-Instanz: eigenes Renderziel, in das der Inhalt gezeichnet wird (siehe
    // RenderDisplays), und die id des Elements, dem es gehoert.
    struct DisplayScreen {
        std::string id;
        RenderTexture2D target;
    };

    // Render-Seite eines Typs: Modelle in derselben Reihenfolge wie states - Index i hier
    // gehoert zu Zustand states[i], genau wie in der parallel dazu in ElementRegistry
    // registrierten Zustandsliste. states wird hier zusaetzlich zur Registry gehalten, damit
    // LoadFromJson beim Verarbeiten der Elemente (Start-Zustand per Name -> Index) nicht bei
    // der Registry nachfragen muss, waehrend diese noch aufgebaut wird.
    struct TypeModels {
        std::vector<std::string> states;
        std::vector<LoadedModel*> modelsByState;
        int colSpan = 1;
        int rowSpan = 1;
        bool stretch = false; // "fit": "stretch" - Achsen einzeln auf die Blockkanten ziehen
        bool hasScreen = false;
        int screenWidth = GameConfig::ScreenDefaultWidth;
        int screenHeight = GameConfig::ScreenDefaultHeight;
    };

    struct Placement {
        std::string id; // Registry-Schluessel, bestimmt zusammen mit typeId das Modell
        std::string typeId;
        Vector3 position;
        // Fertige Modellmatrix ohne Verschiebung: Skalierung (bei "stretch" achsenweise
        // unterschiedlich), dann Blickrichtung um die eigene Hochachse, dann die automatisch
        // berechnete Kippung zur Pultneigung. Wird einmalig beim Laden bestimmt, da sich weder
        // Platzierung noch Tischflaechen zur Laufzeit aendern.
        Matrix transform;
        int screenIndex = -1; // Index in screens, -1 = dieses Element hat keinen Bildschirm
    };

    // Textfeld einer Gruppe: flaches Quad, das - wie die Element-Modelle - schraeg in der
    // Label-Zeile auf der Pultflaeche liegt. corners sind in Zeichenreihenfolge fuer einen
    // Triangle-Fan/Quad: oben-links, oben-rechts, unten-rechts, unten-links.
    struct LabelPlacement {
        Texture2D texture;
        Vector3 corners[4];
    };

    // Rahmen um eine Gruppe von Elementen (siehe "frames" in der JSON): reine Aussenlinie,
    // kein gefuelltes Quad, daher nur die vier Eckpunkte (Zeichenreihenfolge wie LabelPlacement).
    struct FramePlacement {
        Vector3 corners[4];
    };

    LoadedModel& GetOrLoadModel(const std::string& path, Shader shader);

    // Sucht im Modell die Bildschirmflaeche: das erste Mesh, dessen Material die Markierungsfarbe
    // traegt (siehe Klassenkommentar). Deren Texturkoordinaten werden dabei einmalig aus der
    // Geometrie neu berechnet, damit der Displayinhalt garantiert richtig herum steht, egal wie
    // die UV-Map in Blender liegt. Mehrfachaufrufe fuer dasselbe Modell tun nichts.
    static void PrepareScreenMesh(LoadedModel& loaded, Color marker);

    // Rendert `text` auf eine Textur und berechnet die vier Eckpunkte des Quads, mittig in der
    // Label-Zeile (row) unter den Spalten col..col+colSpan-1 auf `surface`, seitenverhaeltnis-treu
    // eingepasst (kein Verzerren).
    static LabelPlacement BuildLabel(const GameConfig::TableSurface& surface, int row, int col,
                                     int colSpan, const std::string& text);

    // Berechnet die vier Eckpunkte eines Rahmens auf `surface`, der den Bruchteils-Bereich
    // [tuStart,tuEnd] x [tvStart,tvEnd] umschliesst (gleiche tu/tv-Konvention wie
    // GameConfig::RowBoundaryFraction).
    static FramePlacement BuildFrame(const GameConfig::TableSurface& surface, float tuStart, float tuEnd, float tvStart, float tvEnd);

    // unordered_map: Referenzen/Pointer auf Elemente bleiben beim Einfuegen weiterer Eintraege
    // gueltig, daher duerfen TypeModels/Placement-Eintraege sicher auf hier gespeicherte
    // LoadedModels zeigen.
    std::unordered_map<std::string, LoadedModel> loadedModels;
    std::unordered_map<std::string, TypeModels> modelsByType;
    ElementRegistry registry;

    std::vector<Placement> placements;
    std::vector<LabelPlacement> labelPlacements;
    std::vector<FramePlacement> framePlacements;
    std::vector<DisplayScreen> screens;

    // Unbeleuchtetes Material, mit dem alle Bildschirmflaechen gezeichnet werden - beim Zeichnen
    // wird jeweils nur die Textur der aktuellen Instanz eingehaengt. Eines fuer alle reicht, weil
    // DrawMesh das Material sofort auswertet und nichts davon behaelt.
    Material screenMaterial{};
    bool screenMaterialLoaded = false;

    // section*1000000 + row*1000 + col -> Element-id, fuer FindElementAt. Nur Button-Zeilen
    // werden je platziert, row/col bleiben daher klein genug fuer diese simple Kodierung.
    std::unordered_map<long long, std::string> elementsByCell;
};
