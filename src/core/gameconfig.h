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

// --- HUD: flache Anzeige oben rechts ueber der 3D-Szene (siehe ui/hud.h) --------------------
// Angezeigt werden zwei ganz normale globale Lua-Variablen: der aktuelle Kontostand und der
// Betrag, den der Spieler erreichen muss. Das Skript setzt sie (Vorgabewerte in
// ScriptVariablesFile), der HUD liest sie nur - die Anzeige braucht also keine eigene Logik
// und aendert sich automatisch mit, sobald ein Skript die Werte anpasst.
constexpr const char* HudMoneyVariable = "geld";
constexpr const char* HudTargetVariable = "ziel";

// raylibs Standardschrift kennt nur ASCII, ein "€" wuerde als leeres Kaestchen erscheinen.
constexpr const char* CurrencySuffix = " EUR";
constexpr char ThousandsSeparator = '.';

// Auf dem Balken steht der Zielbetrag, direkt links daneben der aktuelle Kontostand - mehr
// nicht, kein Rahmen und keine weiteren Beschriftungen.
constexpr const char* HudTargetLabel = "ZIEL ";

constexpr int HudMargin = 20;   // Abstand zum Fensterrand
constexpr int HudGap = 18;      // Abstand zwischen Kontostand und Balken
constexpr int HudBarWidth = 300;
constexpr int HudBarHeight = 26;
constexpr float HudBarBorderThickness = 1.5f;

constexpr int HudMoneyFontSize = 32;
constexpr int HudTargetFontSize = 16;

// Dunkle Schrift und heller Balkenhintergrund, weil der HUD in der oberen Bildschirmecke vor
// dem hellen Hintergrund der Szene liegt (ClearBackground(RAYWHITE) in main.cpp).
constexpr Color HudMoneyColor = { 25, 30, 36, 255 };
constexpr Color HudDebtColor = { 200, 45, 45, 255 };   // Kontostand im Minus
constexpr Color HudBarTextColor = { 25, 30, 36, 255 };
constexpr Color HudBarBackColor = { 226, 229, 233, 255 };
constexpr Color HudBarBorderColor = { 120, 128, 138, 255 };
constexpr Color HudBarColor = { 70, 200, 130, 255 };
constexpr Color HudBarDoneColor = { 245, 190, 45, 255 }; // Balken, sobald das Ziel erreicht ist

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

// Ein Element darf mehrere Button-Zellen breit (colSpan) und hoch (rowSpan) sein. Die Hoehe
// wird dabei in BUTTON-Zeilen gezaehlt, nicht in Rohzeilen: rowSpan=2 belegt die Button-Zeilen
// row und row+2 und schluckt die dazwischenliegende Label-Zeile row+1 mit. Die Label-Zeile
// UNTER dem Block gehoert nicht mehr dazu - dort landet das Textfeld des Elements. Als
// Rohzeilen-Bereich belegt ein Block also [row, LabelRowBelow(row, rowSpan)).
inline int LabelRowBelow(int row, int rowSpan) { return row + 2 * rowSpan - 1; }

// Ausdehnung eines solchen Blocks entlang uAxis (Zeilenrichtung), in Welteinheiten. rowSpan
// Zeilenpaare abzueglich der einen Label-Zeile, die unten nicht mehr dazugehoert - haengt
// deshalb nur von rowSpan ab, nicht davon, in welcher Zeile der Block beginnt.
inline float ButtonBlockHeight(const TableSurface& surface, int rowSpan) {
    int pairs = surface.rows / 2;
    float totalWeight = static_cast<float>(pairs) * RowPairWeight;
    float weight = static_cast<float>(rowSpan) * RowPairWeight - LabelRowWeight;
    return Vector3Length(surface.uAxis) * (weight / totalWeight);
}

// Ausdehnung entlang vAxis (Spaltenrichtung), in Welteinheiten.
inline float ButtonBlockWidth(const TableSurface& surface, int colSpan) {
    return CellWidth(surface) * static_cast<float>(colSpan);
}

// Kantenlaenge fuer gleichmaessige Skalierung: die kleinere der beiden Blockkanten, damit ein
// darauf skaliertes Modell garantiert in beide Richtungen in den Block passt (siehe
// PlacementSystem, "fit" im Typ - "stretch" nutzt stattdessen beide Kanten einzeln).
inline float ButtonBlockSize(const TableSurface& surface, int colSpan, int rowSpan) {
    return fminf(ButtonBlockHeight(surface, rowSpan), ButtonBlockWidth(surface, colSpan));
}

inline float ButtonCellSize(const TableSurface& surface) { return ButtonBlockSize(surface, 1, 1); }

// Weltposition der Mitte eines colSpan x rowSpan grossen Blocks, dessen linke obere Button-Zelle
// (row, col) ist. row/col sind also immer die Ecke, an der das Element im Grid haengt.
inline Vector3 GridBlockCenter(const TableSurface& surface, int row, int col, int colSpan, int rowSpan) {
    float tuStart = RowBoundaryFraction(surface, row);
    float tuEnd = RowBoundaryFraction(surface, LabelRowBelow(row, rowSpan));
    float tu = (tuStart + tuEnd) * 0.5f;
    float tv = (static_cast<float>(col) + static_cast<float>(colSpan) * 0.5f) / static_cast<float>(surface.cols);
    return Vector3Add(surface.origin,
        Vector3Add(Vector3Scale(surface.uAxis, tu), Vector3Scale(surface.vAxis, tv)));
}

// Weltposition der Zellmitte (row, col) - Sonderfall einer 1x1 grossen Platzierung. Gilt auch
// fuer Label-Zeilen, fuer die rowSpan immer 1 ist.
inline Vector3 GridCellCenter(const TableSurface& surface, int row, int col) {
    return GridBlockCenter(surface, row, col, 1, 1);
}

// --- Sperrzonen an den Pultecken --------------------------------------------------------------
//
// Die drei Pultflaechen sind Parallelogramme, die in der DRAUFSICHT ueberlappen: die Ecken der
// beiden Fluegel ragen ein Stueck ueber die Mittelflaeche und umgekehrt. In diesem Bereich
// liegen Gridzellen unter dem Nachbartisch oder ueber dessen Kante - platzierte Elemente
// stecken dort ineinander oder schweben in der Luft.
//
// Wo genau das ist, wird nicht von Hand eingetragen, sondern hier ausgerechnet: eine Zelle ist
// gesperrt, wenn ihr Mittelpunkt in der Draufsicht auch auf einer ANDEREN Flaeche liegt. Aendert
// sich die Tischgeometrie, verschiebt sich die Sperrzone automatisch mit.

// Zerlegt den Punkt p in der Draufsicht (nur X/Z) in die Flaechenkoordinaten a (entlang uAxis)
// und b (entlang vAxis). false, wenn die Flaeche in der Draufsicht entartet ist.
inline bool PlanCoords(const TableSurface& surface, Vector3 p, float& a, float& b) {
    float ux = surface.uAxis.x, uz = surface.uAxis.z;
    float vx = surface.vAxis.x, vz = surface.vAxis.z;
    float det = ux * vz - uz * vx;
    if (fabsf(det) < 1e-6f) {
        return false;
    }
    float dx = p.x - surface.origin.x;
    float dz = p.z - surface.origin.z;
    a = (dx * vz - dz * vx) / det;
    b = (ux * dz - uz * dx) / det;
    return true;
}

// true, wenn p in der Draufsicht innerhalb dieser Flaeche liegt.
inline bool CoversInPlan(const TableSurface& surface, Vector3 p) {
    float a = 0.0f, b = 0.0f;
    if (!PlanCoords(surface, p, a, b)) {
        return false;
    }
    return a >= 0.0f && a <= 1.0f && b >= 0.0f && b <= 1.0f;
}

// true, wenn die Zelle (row, col) auf `section` von einer anderen Pultflaeche ueberdeckt wird
// und deshalb nicht belegt werden darf.
inline bool CellBlocked(size_t section, int row, int col) {
    if (section >= TableSurfaceCount) {
        return true;
    }
    Vector3 p = GridCellCenter(TableSurfaces[section], row, col);
    for (size_t other = 0; other < TableSurfaceCount; other++) {
        if (other != section && CoversInPlan(TableSurfaces[other], p)) {
            return true;
        }
    }
    return false;
}

// Farbe, in der gesperrte Zellen im Grid markiert werden (siehe TableCursor).
constexpr Color BlockedCellColor = { 200, 60, 50, 255 };

// Darstellung der Gruppen-Textfelder (siehe PlacementSystem): der Text wird einmalig beim
// Laden auf eine Textur gerendert und dann als flaches Quad IN der Label-Zeile gezeichnet -
// liegt also genau wie die Button-Modelle schraeg auf der geneigten Pultflaeche, statt immer
// gerade auf dem Bildschirm zu stehen.
constexpr int LabelFontSizePx = 96; // Render-Aufloesung der Text-Textur in Pixeln
constexpr int LabelTexturePadding = 6;
constexpr int LabelBoldRadius = 3; // "Fake Bold": Text wird (2*r+1)^2-fach mit Versatz gezeichnet
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

// Displays (siehe PlacementSystem, "screen" im Typ): die Bildschirmflaeche wird NICHT in der
// JSON vermessen, sondern in Blender mitmodelliert - eine eigene Flaeche im .glb, deren
// Material diese Basisfarbe traegt. raylibs Modell-Lader wirft Mesh- und Materialnamen weg
// (siehe raylib.h: weder Mesh noch Material haben ein Namensfeld), die Basisfarbe aus dem glTF
// ueberlebt dagegen - deshalb die Farbe als Markierung statt eines Namens. Pro Typ
// ueberschreibbar ("marker"), falls das Modell schon eine andere Bildschirmfarbe hat.
constexpr Color ScreenMarkerColor = { 255, 0, 255, 255 };

// Aufloesung der Display-Textur in Pixeln, falls der Typ nichts anderes angibt. In genau diesen
// Koordinaten zeichnet auch das Lua-Skript (0,0 = links oben), voellig unabhaengig davon, wo und
// wie gross die Flaeche in der 3D-Welt liegt.
constexpr int ScreenDefaultWidth = 256;
constexpr int ScreenDefaultHeight = 256;
constexpr Color ScreenClearColor = BLACK;

// Anteil JEDER Kante der Bildschirmflaeche, der im Modell vom Gehaeuse verdeckt wird. Die
// Flaeche im .glb reicht in der Regel unter den Rahmen; ohne Korrektur spannt sich die Textur
// ueber die GESAMTE Flaeche und wird oben, unten und seitlich angeschnitten. Ein Inset > 0
// staucht sie stattdessen in die sichtbare Oeffnung, sodass der volle Displayinhalt zu sehen
// ist. Pro Typ ueberschreibbar ("inset" unter "screen", siehe placement_system.h).
constexpr float ScreenDefaultInset = 0.0f;
constexpr float ScreenMaxInset = 0.4f; // mehr wuerde die Oeffnung auf null zusammenziehen

// Ueberabtastung der Displaytexturen: das Renderziel ist um diesen Faktor groesser als die
// Aufloesung, in der das Skript zeichnet. Beim Verkleinern auf die Bildschirmgroesse mittelt
// die Grafikkarte dann ueber mehrere Texel statt eines herauszugreifen - dasselbe Prinzip wie
// bei den Beschriftungen (siehe BuildLabel), nur dass eine Displaytextur jeden Frame neu
// entsteht und deshalb keine Mipmaps bekommen kann. Das Skript merkt davon nichts, es zeichnet
// weiter in der logischen Aufloesung (screen.width/height).
constexpr int ScreenSupersample = 3;

// Strichstaerke fuer screen.line(). Fest, weil 1px auf einer Displaytextur, die im Spiel stark
// verkleinert dargestellt wird, praktisch verschwindet.
constexpr float ScreenLineThickness = 2.0f;

// Sieben-Segment-Anzeige (screen.seg7): Verhaeltnisse relativ zur Ziffernhoehe. Nicht leuchtende
// Segmente werden schwach mitgezeichnet, wie bei einer echten Anzeige.
constexpr float SegmentThicknessRatio = 1.0f / 7.0f;
constexpr float SegmentWidthRatio = 0.55f;
constexpr float SegmentGapRatio = 1.2f; // Abstand zwischen zwei Ziffern, in Segmentdicken
constexpr float SegmentOffBrightness = 0.15f;

} // namespace GameConfig
