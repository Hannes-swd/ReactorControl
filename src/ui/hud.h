#pragma once

// Flache Anzeige (2D) oben rechts im Fenster: aktueller Kontostand, das zu erreichende Ziel
// und ein Fortschrittsbalken dazwischen. Anders als ein Display auf dem Pult (siehe
// placement_system.h) haengt der HUD nicht an einem 3D-Modell, sondern klebt am Bildschirm -
// er ist immer sichtbar, egal in welche Richtung der Spieler gerade schaut.
//
// Der HUD selbst kennt keine Spiellogik: er bekommt die beiden Zahlen von aussen uebergeben
// (main.cpp holt sie aus den globalen Lua-Variablen GameConfig::HudMoneyVariable/
// HudTargetVariable) und zeichnet sie nur. Wer Geld verdient oder verliert, entscheiden also
// allein die Skripte in assets/scripts/.
class Hud {
public:
    // Muss innerhalb von BeginDrawing, aber ausserhalb von BeginMode3D aufgerufen werden,
    // damit die Anzeige ueber der Szene liegt. Position richtet sich nach der aktuellen
    // Fenstergroesse (das Fenster ist skalierbar), der HUD bleibt so immer am rechten Rand.
    void Draw(double money, double target) const;
};
