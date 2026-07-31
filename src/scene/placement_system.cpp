#include "placement_system.h"
#include "../core/gameconfig.h"
#include "../external/json.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cfloat>
#include <fstream>

using nlohmann::json;

namespace {

// Kleinste Projektion der Modell-Bounding-Box auf die gegebene Achse, nachdem die
// Flaechen-Ausrichtung (Neigung + Verdrehung, siehe GameConfig::SurfaceOrientation)
// angewendet wurde. Wird gebraucht, um das Modell danach korrekt auf die Tischflaeche zu
// setzen - eine Achsen-Bounding-Box laesst sich nicht einfach mitrotieren, deshalb alle 8
// Eckpunkte transformieren und das Minimum entlang der Flaechen-Normalen neu bestimmen.
float RotatedMinAlongAxis(const BoundingBox& bounds, const Matrix& orientation, const Vector3& axis) {
    float minProjection = FLT_MAX;
    for (int i = 0; i < 8; i++) {
        Vector3 corner = {
            (i & 1) ? bounds.max.x : bounds.min.x,
            (i & 2) ? bounds.max.y : bounds.min.y,
            (i & 4) ? bounds.max.z : bounds.min.z,
        };
        float projection = Vector3DotProduct(Vector3Transform(corner, orientation), axis);
        minProjection = std::min(minProjection, projection);
    }
    return minProjection;
}

} // namespace

// "right" (Bildschirmrichtung des Textes) liegt entlang -vAxis, weil die Spalten (cols) die
// breite Richtung sind und die Zeilen (rows) die schmale - der Text soll also quer ueber die
// Spaltenbreite laufen, nicht die Zeilenhoehe hinauf. "down" liegt entlang +uAxis, damit der
// Text so herum steht, wie ein Spieler ihn vor der geneigten Flaeche stehend liest (per
// Screenshot-Test ermittelt, siehe Kommentar in PlacementSystem::Draw).
PlacementSystem::LabelPlacement PlacementSystem::BuildLabel(
    const GameConfig::TableSurface& surface, int row, int col, const std::string& text) {
    int pixelWidth = MeasureText(text.c_str(), GameConfig::LabelFontSizePx) + 2 * GameConfig::LabelTexturePadding;
    int pixelHeight = GameConfig::LabelFontSizePx + 2 * GameConfig::LabelTexturePadding;

    Image image = GenImageColor(pixelWidth, pixelHeight, BLANK);
    // raylibs Standardfont hat keine fette Variante: stattdessen den Text mehrfach mit 1px
    // Versatz uebereinander zeichnen ("Fake Bold"), sonst sind die duennen Striche auf der
    // kleinen Label-Zelle kaum lesbar.
    for (int dx = -GameConfig::LabelBoldRadius; dx <= GameConfig::LabelBoldRadius; dx++) {
        for (int dy = -GameConfig::LabelBoldRadius; dy <= GameConfig::LabelBoldRadius; dy++) {
            ImageDrawText(&image, text.c_str(), GameConfig::LabelTexturePadding + dx,
                          GameConfig::LabelTexturePadding + dy, GameConfig::LabelFontSizePx, GameConfig::LabelTextColor);
        }
    }
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);

    float maxWidth = GameConfig::CellWidth(surface) * GameConfig::LabelCellFillRatio;
    float maxHeight = GameConfig::LabelCellHeight(surface) * GameConfig::LabelCellFillRatio;
    float scale = std::min(maxWidth / static_cast<float>(pixelWidth), maxHeight / static_cast<float>(pixelHeight));
    float halfWidth = static_cast<float>(pixelWidth) * scale * 0.5f;
    float halfHeight = static_cast<float>(pixelHeight) * scale * 0.5f;

    Vector3 right = Vector3Normalize(surface.vAxis);
    Vector3 down = Vector3Negate(Vector3Normalize(surface.uAxis));
    Vector3 normal = GameConfig::SurfaceNormal(surface);
    Vector3 center = Vector3Add(GameConfig::GridCellCenter(surface, row, col),
                                 Vector3Scale(normal, GameConfig::LabelSurfaceOffset));

    Vector3 rightOffset = Vector3Scale(right, halfWidth);
    Vector3 downOffset = Vector3Scale(down, halfHeight);

    LabelPlacement label{};
    label.texture = texture;
    label.corners[0] = Vector3Subtract(Vector3Subtract(center, rightOffset), downOffset); // oben-links
    label.corners[1] = Vector3Subtract(Vector3Add(center, rightOffset), downOffset);       // oben-rechts
    label.corners[2] = Vector3Add(Vector3Add(center, rightOffset), downOffset);            // unten-rechts
    label.corners[3] = Vector3Add(Vector3Subtract(center, rightOffset), downOffset);       // unten-links
    return label;
}

PlacementSystem::~PlacementSystem() {
    for (auto& [path, loaded] : loadedModels) {
        UnloadModel(loaded.model);
    }
    for (auto& label : labelPlacements) {
        UnloadTexture(label.texture);
    }
}

PlacementSystem::LoadedModel& PlacementSystem::GetOrLoadModel(const std::string& path, Shader shader) {
    auto it = loadedModels.find(path);
    if (it != loadedModels.end()) {
        return it->second;
    }

    Model model = LoadModel(path.c_str());
    for (int i = 0; i < model.materialCount; i++) {
        model.materials[i].shader = shader;
    }

    BoundingBox bounds = GetModelBoundingBox(model);
    Vector3 size = Vector3Subtract(bounds.max, bounds.min);
    float largestDimension = std::max({ size.x, size.y, size.z });
    float autoScale = largestDimension > 0.0001f ? 1.0f / largestDimension : 1.0f;

    return loadedModels.emplace(path, LoadedModel{ model, autoScale, bounds }).first->second;
}

void PlacementSystem::LoadFromJson(const char* jsonPath, Shader shader) {
    if (!FileExists(jsonPath)) {
        return;
    }

    std::ifstream file(jsonPath);
    json data = json::parse(file, nullptr, false);
    if (data.is_discarded() || !data.contains("groups")) {
        return;
    }

    for (const auto& group : data["groups"]) {
        if (!group.contains("text") || !group.contains("button")) {
            continue;
        }

        const auto& entry = group["button"];
        if (!entry.contains("model") || !entry.contains("section") || !entry.contains("row") ||
            !entry.contains("col")) {
            continue;
        }

        size_t section = entry["section"].get<size_t>();
        if (section >= GameConfig::TableSurfaceCount) {
            continue;
        }

        std::string modelPath = std::string(GameConfig::ModelsDirectory) + entry["model"].get<std::string>();
        if (!FileExists(modelPath.c_str())) {
            continue;
        }

        LoadedModel& loaded = GetOrLoadModel(modelPath, shader);

        const GameConfig::TableSurface& surface = GameConfig::TableSurfaces[section];
        Vector3 normal = GameConfig::SurfaceNormal(surface);
        Matrix orientation = GameConfig::SurfaceOrientation(surface);

        // autoScale bringt die groesste Rohabmessung auf 1 Einheit, ButtonCellSize dann auf
        // die tatsaechliche Zellenkante. scale=1.0 fuellt damit genau ein Kaestchen.
        float scale = entry.value("scale", 1.0f) * loaded.autoScale * GameConfig::ButtonCellSize(surface);

        // Offset entlang der Flaechen-Normalen, damit der tiefste Punkt des ausgerichteten
        // Modells exakt auf der Tischflaeche aufsteht (statt auf world-Y = 0, was bei
        // geneigten/verdrehten Flaechen daneben laege).
        float groundOffset = -RotatedMinAlongAxis(loaded.bounds, orientation, normal) * scale;
        float height = entry.value("height", 0.0f) + groundOffset;

        int row = entry["row"].get<int>();
        int col = entry["col"].get<int>();
        Vector3 cellCenter = GameConfig::GridCellCenter(surface, row, col);
        Vector3 position = Vector3Add(cellCenter, Vector3Scale(normal, height));

        float facingDegrees = entry.value("rotation", 0.0f);
        Matrix rotation = MatrixMultiply(MatrixRotateY(facingDegrees * DEG2RAD), orientation);

        std::string name = entry.value("name", std::string());
        placements.push_back(Placement{ &loaded, position, scale, rotation, name });

        // Textfeld faellt automatisch in die Label-Zeile direkt unter der Button-Zeile.
        labelPlacements.push_back(BuildLabel(surface, row + 1, col, group["text"].get<std::string>()));
    }
}

void PlacementSystem::Draw() const {
    for (const auto& placement : placements) {
        Model model = placement.loadedModel->model; // Kopie: transform-Aenderung bleibt lokal fuer diesen Draw-Call
        model.transform = MatrixMultiply(
            MatrixScale(placement.scale, placement.scale, placement.scale), placement.rotation);
        DrawModel(model, placement.position, 1.0f, WHITE);
    }

    // Textfelder unlit (ohne Beleuchtungs-Shader) zeichnen, damit sie unabhaengig vom
    // Lichteinfall immer gleich gut lesbar bleiben. raylib buendelt Quads intern und schickt
    // sie erst gesammelt an die GPU (siehe TableCursor::DrawTriangle3DBothSides) - ein
    // rlDisableBackfaceCulling() rund um den rlBegin/rlEnd-Block wirkt dadurch nicht
    // zuverlaessig auf die richtige Wicklung. Deshalb wird jedes Quad in beiden
    // Wicklungsrichtungen gezeichnet, statt auf Culling-Toggle zu vertrauen.
    for (const auto& label : labelPlacements) {
        rlSetTexture(label.texture.id);
        rlBegin(RL_QUADS);
        rlColor4ub(255, 255, 255, 255);
        rlTexCoord2f(0.0f, 0.0f); rlVertex3f(label.corners[0].x, label.corners[0].y, label.corners[0].z);
        rlTexCoord2f(0.0f, 1.0f); rlVertex3f(label.corners[3].x, label.corners[3].y, label.corners[3].z);
        rlTexCoord2f(1.0f, 1.0f); rlVertex3f(label.corners[2].x, label.corners[2].y, label.corners[2].z);
        rlTexCoord2f(1.0f, 0.0f); rlVertex3f(label.corners[1].x, label.corners[1].y, label.corners[1].z);

        rlTexCoord2f(0.0f, 0.0f); rlVertex3f(label.corners[0].x, label.corners[0].y, label.corners[0].z);
        rlTexCoord2f(1.0f, 0.0f); rlVertex3f(label.corners[1].x, label.corners[1].y, label.corners[1].z);
        rlTexCoord2f(1.0f, 1.0f); rlVertex3f(label.corners[2].x, label.corners[2].y, label.corners[2].z);
        rlTexCoord2f(0.0f, 1.0f); rlVertex3f(label.corners[3].x, label.corners[3].y, label.corners[3].z);
        rlEnd();
    }
    rlSetTexture(0);
}
