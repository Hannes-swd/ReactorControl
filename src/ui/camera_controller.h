#pragma once

#include "raylib.h"
#include "../core/enums.h"

// Steuert die feste Kamera-Position und die 3 wechselbaren Blickrichtungen
// (links/mitte/rechts) ueber die Pfeiltasten. Keine freie Kamera-Bewegung.
class CameraController {
public:
    CameraController();

    // Liest Pfeiltasten-Input und aktualisiert die Blickrichtung entsprechend.
    // An den Raendern (links/rechts) bewegt nur die Taste Richtung Mitte etwas,
    // die Taste zum Rand hin macht nichts (kein zyklisches Umschalten).
    void HandleInput();

    const Camera3D& GetCamera() const { return camera; }
    ViewSide GetCurrentView() const { return currentView; }

private:
    void UpdateCameraTarget();

    Camera3D camera;
    ViewSide currentView;
};
