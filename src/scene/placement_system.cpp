#include "placement_system.h"
#include "../core/gameconfig.h"
#include "../external/json.hpp"

#include <raymath.h>

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

PlacementSystem::~PlacementSystem() {
    for (auto& [path, loaded] : loadedModels) {
        UnloadModel(loaded.model);
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
    if (data.is_discarded() || !data.contains("objects")) {
        return;
    }

    for (const auto& entry : data["objects"]) {
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

        // autoScale bringt die groesste Rohabmessung auf 1 Einheit, GridCellSize dann auf die
        // tatsaechliche Zellenkante (~0.2). scale=1.0 fuellt damit genau ein Kaestchen.
        float scale = entry.value("scale", 1.0f) * loaded.autoScale * GameConfig::GridCellSize(surface);

        // Offset entlang der Flaechen-Normalen, damit der tiefste Punkt des ausgerichteten
        // Modells exakt auf der Tischflaeche aufsteht (statt auf world-Y = 0, was bei
        // geneigten/verdrehten Flaechen daneben laege).
        float groundOffset = -RotatedMinAlongAxis(loaded.bounds, orientation, normal) * scale;
        float height = entry.value("height", 0.0f) + groundOffset;

        Vector3 cellCenter = GameConfig::GridCellCenter(surface, entry["row"].get<int>(), entry["col"].get<int>());
        Vector3 position = Vector3Add(cellCenter, Vector3Scale(normal, height));

        float facingDegrees = entry.value("rotation", 0.0f);
        Matrix rotation = MatrixMultiply(MatrixRotateY(facingDegrees * DEG2RAD), orientation);

        placements.push_back(Placement{ &loaded, position, scale, rotation });
    }
}

void PlacementSystem::Draw() const {
    for (const auto& placement : placements) {
        Model model = placement.loadedModel->model; // Kopie: transform-Aenderung bleibt lokal fuer diesen Draw-Call
        model.transform = MatrixMultiply(
            MatrixScale(placement.scale, placement.scale, placement.scale), placement.rotation);
        DrawModel(model, placement.position, 1.0f, WHITE);
    }
}
