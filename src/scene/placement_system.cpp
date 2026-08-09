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

// Alle Button-Zellen, die ein colSpan x rowSpan grosses Element ab (row, col) belegt. Die
// Label-Zeilen zwischen den Button-Zeilen sind nicht dabei - sie sind ohnehin nicht anklickbar
// (siehe TableCursor::Update).
std::vector<std::pair<int, int>> BlockCells(int row, int col, int colSpan, int rowSpan) {
    std::vector<std::pair<int, int>> cells;
    cells.reserve(static_cast<size_t>(colSpan) * static_cast<size_t>(rowSpan));
    for (int r = 0; r < rowSpan; r++) {
        for (int c = 0; c < colSpan; c++) {
            cells.emplace_back(row + 2 * r, col + c);
        }
    }
    return cells;
}

// "size"/"fit"/"screen" eines Typs sind alle optional - fehlt der Eintrag oder hat er den
// falschen Typ, bleibt der Default stehen, statt dass der Typ wegfaellt.
int ReadPositiveInt(const json& parent, const char* key, int fallback) {
    if (!parent.contains(key) || !parent[key].is_number_integer()) {
        return fallback;
    }
    int value = parent[key].get<int>();
    return value > 0 ? value : fallback;
}

// Markierungsfarbe eines Bildschirms: [r, g, b] mit Werten 0..255.
Color ReadMarkerColor(const json& screenJson) {
    if (!screenJson.contains("marker") || !screenJson["marker"].is_array() ||
        screenJson["marker"].size() < 3) {
        return GameConfig::ScreenMarkerColor;
    }
    const auto& marker = screenJson["marker"];
    return Color{
        static_cast<unsigned char>(std::clamp(marker[0].get<int>(), 0, 255)),
        static_cast<unsigned char>(std::clamp(marker[1].get<int>(), 0, 255)),
        static_cast<unsigned char>(std::clamp(marker[2].get<int>(), 0, 255)),
        255,
    };
}

// Der Alphawert bleibt aussen vor: in Blender laesst er sich leicht versehentlich veraendern und
// sagt ueber die Markierung nichts aus.
bool SameRGB(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

} // namespace

// "right" (Bildschirmrichtung des Textes) liegt entlang -vAxis, weil die Spalten (cols) die
// breite Richtung sind und die Zeilen (rows) die schmale - der Text soll also quer ueber die
// Spaltenbreite laufen, nicht die Zeilenhoehe hinauf. "down" liegt entlang +uAxis, damit der
// Text so herum steht, wie ein Spieler ihn vor der geneigten Flaeche stehend liest (per
// Screenshot-Test ermittelt, siehe Kommentar in PlacementSystem::Draw).
PlacementSystem::LabelPlacement PlacementSystem::BuildLabel(
    const GameConfig::TableSurface& surface, int row, int col, int colSpan, const std::string& text) {
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
    // Ohne Mipmaps greift die GPU aus der stark verkleinerten Textur pro Bildschirmpixel nur
    // ein einziges Texel heraus - duenne Striche fallen dadurch zufaellig ganz weg und der
    // Text wirkt zerfressen. Mipmaps mitteln stattdessen ueber alle Texel, die auf das Pixel
    // fallen; TRILINEAR blendet zusaetzlich zwischen den Stufen, damit der Uebergang beim
    // Naeherkommen nicht springt.
    GenTextureMipmaps(&texture);
    SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
    // Reicht allein aber nicht: die Quads liegen flach auf dem geneigten Pult und werden
    // senkrecht viel staerker gestaucht als waagerecht. OpenGL waehlt die Mipmap-Stufe nach
    // der STAERKEREN Stauchung - waagerecht ist sie damit viel zu grob und die Buchstaben
    // verschmieren seitlich. Anisotropes Filtern tastet stattdessen mehrfach entlang der
    // gestauchten Richtung ab und behaelt die Schaerfe in der anderen Richtung.
    SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_16X);
    UnloadImage(image);

    // Ein mehrere Spalten breites Element bekommt auch ein entsprechend breiteres Textfeld,
    // mittig unter dem gesamten Block.
    float maxWidth = GameConfig::ButtonBlockWidth(surface, colSpan) * GameConfig::LabelCellFillRatio;
    float maxHeight = GameConfig::LabelCellHeight(surface) * GameConfig::LabelCellFillRatio;
    float scale = std::min(maxWidth / static_cast<float>(pixelWidth), maxHeight / static_cast<float>(pixelHeight));
    float halfWidth = static_cast<float>(pixelWidth) * scale * 0.5f;
    float halfHeight = static_cast<float>(pixelHeight) * scale * 0.5f;

    Vector3 right = Vector3Normalize(surface.vAxis);
    Vector3 down = Vector3Negate(Vector3Normalize(surface.uAxis));
    Vector3 normal = GameConfig::SurfaceNormal(surface);
    Vector3 center = Vector3Add(GameConfig::GridBlockCenter(surface, row, col, colSpan, 1),
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
    for (auto& screen : screens) {
        UnloadRenderTexture(screen.target);
    }
    if (screenMaterialLoaded) {
        // Die zuletzt eingehaengte Display-Textur gehoert screens und ist oben schon freigegeben -
        // vor dem Aufraeumen zurueck auf raylibs Standardtextur setzen, damit UnloadMaterial sie
        // nicht ein zweites Mal freigibt (es laesst genau diese Textur in Ruhe).
        screenMaterial.maps[MATERIAL_MAP_DIFFUSE].texture.id = rlGetTextureIdDefault();
        UnloadMaterial(screenMaterial);
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

    LoadedModel loaded{};
    loaded.model = model;
    loaded.autoScale = autoScale;
    loaded.bounds = bounds;
    loaded.size = size;
    return loadedModels.emplace(path, loaded).first->second;
}

void PlacementSystem::PrepareScreenMesh(LoadedModel& loaded, Color marker) {
    if (loaded.screenMesh >= 0) {
        return;
    }

    const Model& model = loaded.model;
    for (int i = 0; i < model.meshCount; i++) {
        int materialIndex = model.meshMaterial[i];
        if (materialIndex < 0 || materialIndex >= model.materialCount) {
            continue;
        }
        if (!SameRGB(model.materials[materialIndex].maps[MATERIAL_MAP_DIFFUSE].color, marker)) {
            continue;
        }

        loaded.screenMesh = i;

        Mesh& mesh = model.meshes[i];
        if (mesh.texcoords == nullptr) {
            TraceLog(LOG_WARNING, "PLATZIERUNG: Bildschirmflaeche hat keine Texturkoordinaten - "
                                  "in Blender fehlt die UV-Map");
            return;
        }

        // Die Texturkoordinaten werden hier komplett neu berechnet, statt die aus Blender zu
        // benutzen. Grund: wie herum eine UV-Map liegt, sieht man dem Modell im Spiel nicht an -
        // eine um 90 Grad verdrehte Map faellt erst auf, wenn der fertige Displaytext hochkant
        // steht. Aus der Geometrie ergibt sich die richtige Lage dagegen eindeutig, und zwar
        // genau nach derselben Regel wie bei den Textfeldern (siehe BuildLabel): quer ueber die
        // Flaeche laeuft die Spaltenrichtung, von oben nach unten die Zeilenrichtung.
        //
        // Nach GameConfig::SurfaceOrientation zeigt die lokale Modellachse X entlang uAxis
        // (Zeilen, vom Spieler weg) und Z entlang vAxis (Spalten, quer). Also:
        //   Textur-x (links -> rechts) laeuft entlang +Z
        //   Textur-y (oben -> unten)   laeuft entlang -X
        // Der y-Anteil ist dabei bereits umgedreht, weil ein Renderziel im Grafikspeicher
        // zeilenweise andersherum liegt als eine geladene Bilddatei - ohne das stuende der
        // Inhalt auf dem Kopf.
        float minX = FLT_MAX, maxX = -FLT_MAX, minZ = FLT_MAX, maxZ = -FLT_MAX;
        for (int v = 0; v < mesh.vertexCount; v++) {
            minX = std::min(minX, mesh.vertices[v * 3 + 0]);
            maxX = std::max(maxX, mesh.vertices[v * 3 + 0]);
            minZ = std::min(minZ, mesh.vertices[v * 3 + 2]);
            maxZ = std::max(maxZ, mesh.vertices[v * 3 + 2]);
        }
        float spanX = std::max(maxX - minX, 0.0001f);
        float spanZ = std::max(maxZ - minZ, 0.0001f);

        for (int v = 0; v < mesh.vertexCount; v++) {
            mesh.texcoords[v * 2 + 0] = (mesh.vertices[v * 3 + 2] - minZ) / spanZ;
            mesh.texcoords[v * 2 + 1] = (mesh.vertices[v * 3 + 0] - minX) / spanX;
        }
        UpdateMeshBuffer(mesh, 1, mesh.texcoords, mesh.vertexCount * 2 * static_cast<int>(sizeof(float)), 0);
        return;
    }
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

            TypeModels typeModels;
            typeModels.states = states;
            typeModels.modelsByState = modelsByState;

            if (typeEntry.contains("size")) {
                typeModels.colSpan = ReadPositiveInt(typeEntry["size"], "cols", 1);
                typeModels.rowSpan = ReadPositiveInt(typeEntry["size"], "rows", 1);
            }
            typeModels.stretch = typeEntry.value("fit", std::string("uniform")) == "stretch";

            // Bildschirmflaeche in JEDEM Zustandsmodell suchen: eine Anzeige, die zwischen
            // "an" und "gestoert" umschaltet, hat den Bildschirm in beiden Modellen.
            if (typeEntry.contains("screen")) {
                const auto& screenJson = typeEntry["screen"];
                Color marker = ReadMarkerColor(screenJson);
                for (LoadedModel* loaded : modelsByState) {
                    PrepareScreenMesh(*loaded, marker);
                }
                typeModels.hasScreen = true;
                typeModels.screenWidth = ReadPositiveInt(screenJson, "width", GameConfig::ScreenDefaultWidth);
                typeModels.screenHeight = ReadPositiveInt(screenJson, "height", GameConfig::ScreenDefaultHeight);

                // LoadMaterialDefault liefert raylibs Standardshader - also ohne Beleuchtung,
                // genau richtig fuer eine leuchtende Anzeige. Erst hier angelegt, damit ein
                // Projekt ganz ohne Displays gar kein Material dafuer erzeugt.
                if (!screenMaterialLoaded) {
                    screenMaterial = LoadMaterialDefault();
                    screenMaterialLoaded = true;
                }

                if (modelsByState.front()->screenMesh < 0) {
                    TraceLog(LOG_WARNING, "PLATZIERUNG: Typ '%s' hat \"screen\", aber im Modell "
                             "liegt keine Flaeche mit der Markierungsfarbe (%d,%d,%d)",
                             typeId.c_str(), marker.r, marker.g, marker.b);
                }
            }

            registry.RegisterType(typeId, states);
            modelsByType[typeId] = std::move(typeModels);
        }
    }

    // Merkt sich section/row/col jedes Elements, damit "frames" (Durchgang 3) per id darauf
    // verweisen kann, ohne die Grid-Position ein zweites Mal in der JSON angeben zu muessen.
    // Rein lokal, im Gegensatz zu elementsByCell (Member, fuer Klick-Lookup zur Laufzeit).
    struct ElementRef {
        size_t section;
        int row;
        int col;
        int colSpan;
        int rowSpan;
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

            int row = group["row"].get<int>();
            int col = group["col"].get<int>();
            int colSpan = typeModels.colSpan;
            int rowSpan = typeModels.rowSpan;

            // Ein Element, das auch nur mit einer Zelle auf einem bereits platzierten liegt, wird
            // ganz uebersprungen: sonst wuerde es die Belegungskarte des anderen teilweise
            // ueberschreiben und Klicks landeten je nach Zelle beim falschen Element.
            std::vector<std::pair<int, int>> cells = BlockCells(row, col, colSpan, rowSpan);
            bool cellsFree = true;
            for (const auto& [cellRow, cellCol] : cells) {
                if (elementsByCell.count(CellKey(static_cast<int>(section), cellRow, cellCol)) > 0) {
                    TraceLog(LOG_WARNING, "PLATZIERUNG: '%s' uebersprungen - Zelle (%d, %d, %d) ist "
                             "schon von '%s' belegt", id.c_str(), static_cast<int>(section), cellRow, cellCol,
                             elementsByCell[CellKey(static_cast<int>(section), cellRow, cellCol)].c_str());
                    cellsFree = false;
                    break;
                }
            }
            if (!cellsFree) {
                continue;
            }

            // Skalierung/Boden-Ausgleich werden einmalig anhand des Start-Zustand-Modells
            // berechnet und fuer alle Zustaende dieser Instanz beibehalten - setzt voraus, dass
            // alle Modelle eines Typs (z.B. die Farbvarianten eines Tasters) aehnlich grosse
            // Rohabmessungen haben. Bei sehr unterschiedlich grossen Zustandsmodellen muesste das
            // stattdessen pro Zustand in Draw() neu berechnet werden.
            const LoadedModel& initialModel = *typeModels.modelsByState[initialStateIndex];
            float userScale = group.value("scale", 1.0f);

            // Lokales X des Modells zeigt nach der Ausrichtung entlang uAxis (Zeilenrichtung),
            // lokales Z entlang vAxis (Spaltenrichtung) - siehe GameConfig::SurfaceOrientation.
            Vector3 scale{};
            if (typeModels.stretch) {
                // Beide Kanten des Blocks einzeln treffen. Die Hochachse bekommt den kleineren der
                // beiden Faktoren, damit ein gedehntes Gehaeuse nicht gleichzeitig in die Hoehe
                // schiesst.
                float scaleX = GameConfig::ButtonBlockHeight(surface, rowSpan) /
                               std::max(initialModel.size.x, 0.0001f);
                float scaleZ = GameConfig::ButtonBlockWidth(surface, colSpan) /
                               std::max(initialModel.size.z, 0.0001f);
                scale = { scaleX * userScale, std::min(scaleX, scaleZ) * userScale, scaleZ * userScale };
            } else {
                // autoScale bringt die groesste Rohabmessung auf 1 Einheit, ButtonBlockSize dann
                // auf die tatsaechliche Blockkante. scale=1.0 fuellt damit genau den Block.
                float uniform = userScale * initialModel.autoScale * GameConfig::ButtonBlockSize(surface, colSpan, rowSpan);
                scale = { uniform, uniform, uniform };
            }

            // Skalierung und Ausrichtung in einer Matrix: bei "stretch" ist die Skalierung
            // achsenweise verschieden und laesst sich nicht mehr als einzelner Faktor aus der
            // Boden-Ausgleichsrechnung herausziehen.
            Matrix facing = MatrixMultiply(MatrixRotateY(group.value("rotation", 0.0f) * DEG2RAD), orientation);
            Matrix transform = MatrixMultiply(MatrixScale(scale.x, scale.y, scale.z), facing);

            // Offset entlang der Flaechen-Normalen, damit der tiefste Punkt des ausgerichteten
            // Modells exakt auf der Tischflaeche aufsteht (statt auf world-Y = 0, was bei
            // geneigten/verdrehten Flaechen daneben laege).
            float groundOffset = -RotatedMinAlongAxis(initialModel.bounds, transform, normal);
            float height = group.value("height", 0.0f) + groundOffset;

            Vector3 blockCenter = GameConfig::GridBlockCenter(surface, row, col, colSpan, rowSpan);
            Vector3 position = Vector3Add(blockCenter, Vector3Scale(normal, height));

            // Eigenes Renderziel pro Display-INSTANZ, damit zwei Anzeigen desselben Typs
            // Verschiedenes zeigen koennen.
            int screenIndex = -1;
            if (typeModels.hasScreen && initialModel.screenMesh >= 0) {
                screenIndex = static_cast<int>(screens.size());
                screens.push_back(DisplayScreen{
                    id, LoadRenderTexture(typeModels.screenWidth, typeModels.screenHeight) });
            }

            registry.RegisterInstance(id, typeId, initialStateIndex);
            elementRefs[id] = ElementRef{ section, row, col, colSpan, rowSpan };
            for (const auto& [cellRow, cellCol] : cells) {
                elementsByCell[CellKey(static_cast<int>(section), cellRow, cellCol)] = id;
            }
            placements.push_back(Placement{ id, typeId, position, transform, screenIndex });

            // Textfeld faellt automatisch in die Label-Zeile direkt unter dem Block.
            labelPlacements.push_back(BuildLabel(surface, GameConfig::LabelRowBelow(row, rowSpan),
                                                 col, colSpan, group["text"].get<std::string>()));
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
            int rowMin = INT_MAX, rowEndMax = INT_MIN, colMin = INT_MAX, colEndMax = INT_MIN;
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
                const ElementRef& ref = it->second;
                rowMin = std::min(rowMin, ref.row);
                colMin = std::min(colMin, ref.col);
                // Untere Kante: eine Zeile HINTER der Label-Zeile des Elements, damit diese noch
                // mit im Rahmen liegt. Bei einem mehrzeiligen Element ist das entsprechend weiter
                // unten, deshalb pro Element aus dessen eigener Spannweite berechnet.
                rowEndMax = std::max(rowEndMax, GameConfig::LabelRowBelow(ref.row, ref.rowSpan) + 1);
                colEndMax = std::max(colEndMax, ref.col + ref.colSpan);
            }
            if (!valid || !frameSection.has_value()) {
                continue;
            }

            const GameConfig::TableSurface& surface = GameConfig::TableSurfaces[*frameSection];
            float tuStart = GameConfig::RowBoundaryFraction(surface, rowMin);
            float tuEnd = GameConfig::RowBoundaryFraction(surface, rowEndMax);
            float tvStart = static_cast<float>(colMin) / surface.cols;
            float tvEnd = static_cast<float>(colEndMax) / surface.cols;

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

void PlacementSystem::RenderDisplays(const std::function<void(const std::string&, int, int)>& drawContent) {
    for (const auto& screen : screens) {
        BeginTextureMode(screen.target);
        ClearBackground(GameConfig::ScreenClearColor);
        drawContent(screen.id, screen.target.texture.width, screen.target.texture.height);
        EndTextureMode();
    }
}

void PlacementSystem::Draw() const {
    for (const auto& placement : placements) {
        const TypeModels& typeModels = modelsByType.at(placement.typeId);
        int stateIndex = registry.GetStateIndex(placement.id);
        if (stateIndex < 0 || static_cast<size_t>(stateIndex) >= typeModels.modelsByState.size()) {
            continue;
        }

        const LoadedModel& loaded = *typeModels.modelsByState[stateIndex];
        const Model& model = loaded.model;

        // Statt DrawModel wird Mesh fuer Mesh gezeichnet, weil die Bildschirmflaeche ein anderes
        // Material braucht als der Rest des Modells. Die Matrix ist dieselbe, die DrawModel intern
        // bilden wuerde: erst die fertige Skalierung/Ausrichtung, dann die Verschiebung.
        Matrix transform = MatrixMultiply(placement.transform,
            MatrixTranslate(placement.position.x, placement.position.y, placement.position.z));

        for (int i = 0; i < model.meshCount; i++) {
            if (i == loaded.screenMesh && placement.screenIndex >= 0) {
                // Unbeleuchtet mit der Textur genau dieser Instanz - DrawMesh wertet das Material
                // sofort aus, das Umhaengen der Textur wirkt also nur fuer diesen einen Aufruf.
                screenMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = screens[placement.screenIndex].target.texture;
                DrawMesh(model.meshes[i], screenMaterial, transform);
            } else {
                DrawMesh(model.meshes[i], model.materials[model.meshMaterial[i]], transform);
            }
        }
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
