#include "placement_system.h"
#include "../core/gameconfig.h"
#include "../external/json.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cfloat>
#include <climits>
#include <fstream>
#include <optional>

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

// Kodiert eine Button-Zelle als einzelnen Schluessel fuer elementsByCell. row/col bleiben
// laut GameConfig::TableSurfaces deutlich unter 1000, section unter 1000000/1000.
long long CellKey(int section, int row, int col) {
    return static_cast<long long>(section) * 1000000 + static_cast<long long>(row) * 1000 + col;
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

PlacementSystem::FramePlacement PlacementSystem::BuildFrame(
    const GameConfig::TableSurface& surface, float tuStart, float tuEnd, float tvStart, float tvEnd) {
    Vector3 normal = GameConfig::SurfaceNormal(surface);
    auto corner = [&](float fu, float fv) {
        Vector3 onSurface = Vector3Add(surface.origin,
            Vector3Add(Vector3Scale(surface.uAxis, fu), Vector3Scale(surface.vAxis, fv)));
        return Vector3Add(onSurface, Vector3Scale(normal, GameConfig::FrameSurfaceOffset));
    };

    FramePlacement frame{};
    frame.corners[0] = corner(tuStart, tvStart);
    frame.corners[1] = corner(tuStart, tvEnd);
    frame.corners[2] = corner(tuEnd, tvEnd);
    frame.corners[3] = corner(tuEnd, tvStart);
    return frame;
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

void PlacementSystem::LoadFromDirectory(const char* dirPath, Shader shader) {
    if (!DirectoryExists(dirPath)) {
        return;
    }

    // Alle *.json-Dateien direkt im Ordner einmalig einlesen und parsen (nicht rekursiv,
    // ungueltige/leere Dateien werden verworfen), damit die drei Durchgaenge unten beliebig
    // ueber Dateigrenzen hinweg aufeinander verweisen koennen (z.B. ein Element in
    // rechts.json, dessen Typ in types.json steht, referenziert von einem Frame in
    // rechts.json) - Lesereihenfolge der Dateien spielt dafuer keine Rolle.
    std::vector<json> files;
    FilePathList filePaths = LoadDirectoryFilesEx(dirPath, ".json", false);
    for (unsigned int i = 0; i < filePaths.count; i++) {
        std::ifstream file(filePaths.paths[i]);
        json data = json::parse(file, nullptr, false);
        if (!data.is_discarded()) {
            files.push_back(std::move(data));
        }
    }
    UnloadDirectoryFiles(filePaths);

    // Durchgang 1: "types" aus allen Dateien. Ein Typ landet nur dann in modelsByType
    // (Render-Seite) UND in der Registry (Logik-Seite), wenn ALLE seine Zustaende ein
    // existierendes Modell haben - sonst wuerde ein spaeterer Klick auf einen unvollstaendig
    // geladenen Zustand zeigen. Elemente, die auf einen uebersprungenen Typ verweisen, werden
    // in Durchgang 2 ebenfalls uebersprungen.
    for (const auto& data : files) {
        if (!data.contains("types")) {
            continue;
        }
        for (const auto& typeEntry : data["types"]) {
            if (!typeEntry.contains("id") || !typeEntry.contains("states") || !typeEntry.contains("models")) {
                continue;
            }

            std::string typeId = typeEntry["id"].get<std::string>();
            std::vector<std::string> states = typeEntry["states"].get<std::vector<std::string>>();
            if (states.empty()) {
                continue;
            }

            const auto& modelsJson = typeEntry["models"];
            std::vector<LoadedModel*> modelsByState;
            modelsByState.reserve(states.size());
            bool allModelsFound = true;
            for (const std::string& state : states) {
                if (!modelsJson.contains(state)) {
                    allModelsFound = false;
                    break;
                }
                std::string modelPath = std::string(GameConfig::ModelsDirectory) + modelsJson[state].get<std::string>();
                if (!FileExists(modelPath.c_str())) {
                    allModelsFound = false;
                    break;
                }
                modelsByState.push_back(&GetOrLoadModel(modelPath, shader));
            }
            if (!allModelsFound) {
                continue;
            }

            registry.RegisterType(typeId, states);
            modelsByType[typeId] = TypeModels{ std::move(states), std::move(modelsByState) };
        }
    }

    // Merkt sich section/row/col jedes Elements, damit "frames" (Durchgang 3) per id darauf
    // verweisen kann, ohne die Grid-Position ein zweites Mal in der JSON angeben zu muessen.
    // Rein lokal, im Gegensatz zu elementsByCell (Member, fuer Klick-Lookup zur Laufzeit).
    struct ElementRef {
        size_t section;
        int row;
        int col;
    };
    std::unordered_map<std::string, ElementRef> elementRefs;

    // Durchgang 2: "elements" aus allen Dateien.
    for (const auto& data : files) {
        if (!data.contains("elements")) {
            continue;
        }
        for (const auto& group : data["elements"]) {
            if (!group.contains("id") || !group.contains("type") || !group.contains("text") ||
                !group.contains("section") || !group.contains("row") || !group.contains("col")) {
                continue;
            }

            std::string id = group["id"].get<std::string>();
            std::string typeId = group["type"].get<std::string>();
            auto typeIt = modelsByType.find(typeId);
            if (typeIt == modelsByType.end()) {
                continue; // Typ existiert nicht oder wurde oben wegen fehlender Modelle uebersprungen
            }
            const TypeModels& typeModels = typeIt->second;

            size_t section = group["section"].get<size_t>();
            if (section >= GameConfig::TableSurfaceCount) {
                continue;
            }

            // Start-Zustand: per Name aus "state" (Default: erster Zustand des Typs).
            std::string initialStateName = group.value("state", typeModels.states.front());
            auto stateIt = std::find(typeModels.states.begin(), typeModels.states.end(), initialStateName);
            int initialStateIndex = (stateIt != typeModels.states.end())
                ? static_cast<int>(std::distance(typeModels.states.begin(), stateIt))
                : 0;

            const GameConfig::TableSurface& surface = GameConfig::TableSurfaces[section];
            Vector3 normal = GameConfig::SurfaceNormal(surface);
            Matrix orientation = GameConfig::SurfaceOrientation(surface);

            // Skalierung/Boden-Ausgleich werden einmalig anhand des Start-Zustand-Modells
            // berechnet und fuer alle Zustaende dieser Instanz beibehalten - setzt voraus, dass
            // alle Modelle eines Typs (z.B. die Farbvarianten eines Tasters) aehnlich grosse
            // Rohabmessungen haben. Bei sehr unterschiedlich grossen Zustandsmodellen muesste das
            // stattdessen pro Zustand in Draw() neu berechnet werden.
            const LoadedModel& initialModel = *typeModels.modelsByState[initialStateIndex];

            // autoScale bringt die groesste Rohabmessung auf 1 Einheit, ButtonCellSize dann auf
            // die tatsaechliche Zellenkante. scale=1.0 fuellt damit genau ein Kaestchen.
            float scale = group.value("scale", 1.0f) * initialModel.autoScale * GameConfig::ButtonCellSize(surface);

            // Offset entlang der Flaechen-Normalen, damit der tiefste Punkt des ausgerichteten
            // Modells exakt auf der Tischflaeche aufsteht (statt auf world-Y = 0, was bei
            // geneigten/verdrehten Flaechen daneben laege).
            float groundOffset = -RotatedMinAlongAxis(initialModel.bounds, orientation, normal) * scale;
            float height = group.value("height", 0.0f) + groundOffset;

            int row = group["row"].get<int>();
            int col = group["col"].get<int>();
            Vector3 cellCenter = GameConfig::GridCellCenter(surface, row, col);
            Vector3 position = Vector3Add(cellCenter, Vector3Scale(normal, height));

            float facingDegrees = group.value("rotation", 0.0f);
            Matrix rotation = MatrixMultiply(MatrixRotateY(facingDegrees * DEG2RAD), orientation);

            registry.RegisterInstance(id, typeId, initialStateIndex);
            elementRefs[id] = ElementRef{ section, row, col };
            elementsByCell[CellKey(static_cast<int>(section), row, col)] = id;
            placements.push_back(Placement{ id, typeId, position, scale, rotation });

            // Textfeld faellt automatisch in die Label-Zeile direkt unter der Button-Zeile.
            labelPlacements.push_back(BuildLabel(surface, row + 1, col, group["text"].get<std::string>()));
        }
    }

    // Durchgang 3: "frames" aus allen Dateien - fasst mehrere (per id referenzierte)
    // Elemente, die auf derselben Flaeche nebeneinander liegen, zu einem Block zusammen und
    // zeichnet einen Rahmen aussen herum (Button- UND die direkt darunterliegende Label-Zeile
    // eingeschlossen). Alle referenzierten Elemente muessen existieren und auf derselben
    // section liegen, sonst wird die Frame uebersprungen.
    for (const auto& data : files) {
        if (!data.contains("frames")) {
            continue;
        }
        for (const auto& frameEntry : data["frames"]) {
            if (!frameEntry.contains("elements")) {
                continue;
            }

            std::optional<size_t> frameSection;
            int rowMin = INT_MAX, rowMax = INT_MIN, colMin = INT_MAX, colMax = INT_MIN;
            bool valid = true;

            for (const auto& elementId : frameEntry["elements"]) {
                auto it = elementRefs.find(elementId.get<std::string>());
                if (it == elementRefs.end()) {
                    valid = false;
                    break;
                }
                if (!frameSection.has_value()) {
                    frameSection = it->second.section;
                } else if (*frameSection != it->second.section) {
                    valid = false;
                    break;
                }
                rowMin = std::min(rowMin, it->second.row);
                rowMax = std::max(rowMax, it->second.row);
                colMin = std::min(colMin, it->second.col);
                colMax = std::max(colMax, it->second.col);
            }
            if (!valid || !frameSection.has_value()) {
                continue;
            }

            const GameConfig::TableSurface& surface = GameConfig::TableSurfaces[*frameSection];
            // +2 statt +1, damit die Label-Zeile direkt unter der untersten Button-Zeile mit
            // eingeschlossen wird.
            float tuStart = GameConfig::RowBoundaryFraction(surface, rowMin);
            float tuEnd = GameConfig::RowBoundaryFraction(surface, rowMax + 2);
            float tvStart = static_cast<float>(colMin) / surface.cols;
            float tvEnd = static_cast<float>(colMax + 1) / surface.cols;

            framePlacements.push_back(BuildFrame(surface, tuStart, tuEnd, tvStart, tvEnd));
        }
    }

    // Durchgang 4: "triggers" aus allen Dateien - reine Weiterleitung an die Registry (siehe
    // ElementRegistry::RegisterTrigger), PlacementSystem wertet sie selbst nicht aus.
    for (const auto& data : files) {
        if (!data.contains("triggers")) {
            continue;
        }
        for (const auto& triggerEntry : data["triggers"]) {
            if (!triggerEntry.contains("on") || !triggerEntry.contains("set")) {
                continue;
            }
            std::string onId = triggerEntry["on"].get<std::string>();
            for (const auto& [targetId, targetState] : triggerEntry["set"].items()) {
                registry.RegisterTrigger(onId, targetId, targetState.get<std::string>());
            }
        }
    }
}

std::optional<std::string> PlacementSystem::FindElementAt(int section, int row, int col) const {
    auto it = elementsByCell.find(CellKey(section, row, col));
    if (it == elementsByCell.end()) {
        return std::nullopt;
    }
    return it->second;
}

void PlacementSystem::Draw() const {
    for (const auto& placement : placements) {
        const TypeModels& typeModels = modelsByType.at(placement.typeId);
        int stateIndex = registry.GetStateIndex(placement.id);
        if (stateIndex < 0 || static_cast<size_t>(stateIndex) >= typeModels.modelsByState.size()) {
            continue;
        }

        Model model = typeModels.modelsByState[stateIndex]->model; // Kopie: transform-Aenderung bleibt lokal fuer diesen Draw-Call
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

    for (const auto& frame : framePlacements) {
        DrawLine3D(frame.corners[0], frame.corners[1], GameConfig::FrameColor);
        DrawLine3D(frame.corners[1], frame.corners[2], GameConfig::FrameColor);
        DrawLine3D(frame.corners[2], frame.corners[3], GameConfig::FrameColor);
        DrawLine3D(frame.corners[3], frame.corners[0], GameConfig::FrameColor);
    }
}
