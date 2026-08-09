#include "hud.h"

#include "../core/gameconfig.h"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

// Geldbetrag als lesbarer Text: auf ganze Einheiten gerundet, mit Tausenderpunkten und
// Waehrung ("1.250 EUR"). Nachkommastellen waeren bei einem Kontostand, der sich staendig
// aendert, nur Unruhe in der Anzeige.
std::string FormatAmount(double value) {
    long long rounded = static_cast<long long>(std::llround(value));
    bool negative = rounded < 0;

    // Erst nach der Vorzeichenpruefung den Betrag bilden, damit die Punkte nicht im "-" landen.
    std::string digits = std::to_string(negative ? -rounded : rounded);
    for (int pos = static_cast<int>(digits.size()) - 3; pos > 0; pos -= 3) {
        digits.insert(static_cast<size_t>(pos), 1, GameConfig::ThousandsSeparator);
    }
    if (negative) {
        digits.insert(0, 1, '-');
    }
    return digits + GameConfig::CurrencySuffix;
}

// Anteil des Ziels, der schon erreicht ist (0..1). Ein Ziel von 0 (oder kleiner) gilt als
// bereits erfuellt, solange nichts im Minus steht - sonst waere die Division sinnlos.
float TargetFraction(double money, double target) {
    if (target <= 0.0) {
        return money >= 0.0 ? 1.0f : 0.0f;
    }
    return std::clamp(static_cast<float>(money / target), 0.0f, 1.0f);
}

} // namespace

void Hud::Draw(double money, double target) const {
    const Rectangle bar = {
        static_cast<float>(GetScreenWidth() - GameConfig::HudMargin - GameConfig::HudBarWidth),
        static_cast<float>(GameConfig::HudMargin),
        static_cast<float>(GameConfig::HudBarWidth),
        static_cast<float>(GameConfig::HudBarHeight),
    };

    // Kontostand links neben dem Balken, rechtsbuendig an dessen linker Kante: dadurch bleibt
    // beim Wechsel der Stellenzahl die Kante zwischen Zahl und Balken stehen. Senkrecht auf
    // Balkenmitte ausgerichtet.
    const std::string amount = FormatAmount(money);
    const int amountWidth = MeasureText(amount.c_str(), GameConfig::HudMoneyFontSize);
    DrawText(amount.c_str(),
             static_cast<int>(bar.x) - GameConfig::HudGap - amountWidth,
             static_cast<int>(bar.y + (bar.height - GameConfig::HudMoneyFontSize) * 0.5f),
             GameConfig::HudMoneyFontSize,
             money < 0.0 ? GameConfig::HudDebtColor : GameConfig::HudMoneyColor);

    // Balken: der leere Teil bleibt sichtbar, damit man sieht, wie weit es noch ist. Der
    // gefuellte Teil waechst von links und wird gold, sobald das Ziel erreicht ist.
    // Kanten bewusst eckig - das passt zur nuechternen Anmutung einer Reaktorsteuerung.
    const float fraction = TargetFraction(money, target);
    DrawRectangleRec(bar, GameConfig::HudBarBackColor);
    if (fraction > 0.0f) {
        Rectangle filled = bar;
        filled.width = bar.width * fraction;
        DrawRectangleRec(filled,
                         money >= target ? GameConfig::HudBarDoneColor : GameConfig::HudBarColor);
    }
    DrawRectangleLinesEx(bar, GameConfig::HudBarBorderThickness, GameConfig::HudBarBorderColor);

    // Zielbetrag mittig auf dem Balken - er beschriftet damit genau das, was sich fuellt.
    const std::string goal = GameConfig::HudTargetLabel + FormatAmount(target);
    const int goalWidth = MeasureText(goal.c_str(), GameConfig::HudTargetFontSize);
    DrawText(goal.c_str(),
             static_cast<int>(bar.x + (bar.width - static_cast<float>(goalWidth)) * 0.5f),
             static_cast<int>(bar.y + (bar.height - GameConfig::HudTargetFontSize) * 0.5f),
             GameConfig::HudTargetFontSize, GameConfig::HudBarTextColor);
}
