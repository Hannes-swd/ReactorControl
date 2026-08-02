#pragma once

#include "raylib.h"

// Zeigt live an, wo die Maus (die "Hand" des Spielers) auf einer der drei geneigten
// Pult-Oberflaechen steht: das getroffene Gitter-Kaestchen wird eingefaerbt und ein
// Kreis markiert zusaetzlich den exakten Trefferpunkt, jeden Frame, ganz ohne Klick.
// Die duennen Referenz-Gitterlinien lassen sich per TAB ein-/ausblenden (Hover-Markierung
// und Cursor-Kreis bleiben davon unberuehrt).
class TableCursor {
public:
    void Update(const Camera3D& camera);
    void Draw() const;

    // Fuer Klick-Erkennung ausserhalb der Klasse (siehe main.cpp): ob und welche Button-Zelle
    // diesen Frame gehovert wird. Nur gueltig, wenn IsVisible() true ist.
    bool IsVisible() const { return visible; }
    int HoverSection() const { return hoverSection; }
    int HoverRow() const { return hoverRow; }
    int HoverCol() const { return hoverCol; }

private:
    Vector3 CornerPoint(int section, float fu, float fv) const;

    bool visible = false;
    bool debugMode = true;
    int hoverSection = -1;
    int hoverRow = -1;
    int hoverCol = -1;
    Vector3 point{};
    Vector3 normal{};
};
