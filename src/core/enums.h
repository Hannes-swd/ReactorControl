#pragma once

// Die drei moeglichen Blickrichtungen des Spielers am Pult (fest, keine freie Kamera).
// Reihenfolge entspricht der tatsaechlichen Anordnung auf dem Bildschirm (links -> rechts),
// damit sie direkt als Array-Index fuer GameConfig::ViewTargets verwendet werden kann.
enum class ViewSide {
    Left = 0,
    Center = 1,
    Right = 2,
};

constexpr int ViewSideCount = 3;
