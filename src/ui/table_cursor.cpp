#include "table_cursor.h"
#include "../core/gameconfig.h"
#include "raymath.h"
#include <cmath>

namespace {

// raylib buendelt Dreiecke intern und schickt sie erst gesammelt an die GPU; ein
// rlDisableBackfaceCulling() rund um einzelne DrawTriangle3D-Aufrufe wirkt dadurch nicht
// zuverlaessig auf die richtigen Dreiecke. Deshalb wird hier statt Culling-Toggle einfach
// jedes Dreieck in beiden Wicklungsrichtungen gezeichnet, damit es unabhaengig vom
// Blickwinkel sichtbar bleibt.
void DrawTriangle3DBothSides(Vector3 v1, Vector3 v2, Vector3 v3, Color color) {
    DrawTriangle3D(v1, v2, v3, color);
    DrawTriangle3D(v1, v3, v2, color);
}

constexpr float kTwoPi = 6.28318530718f;

} // namespace

void TableCursor::Update(const Camera3D& camera) {
    Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);

    visible = false;
    hoverSection = -1;
    hoverRow = -1;
    hoverCol = -1;
    float closestDistance = 0.0f;

    for (size_t i = 0; i < GameConfig::TableSurfaceCount; i++) {
        const GameConfig::TableSurface& surface = GameConfig::TableSurfaces[i];

        Vector3 p1 = surface.origin;
        Vector3 p2 = Vector3Add(p1, surface.uAxis);
        Vector3 p3 = Vector3Add(p2, surface.vAxis);
        Vector3 p4 = Vector3Add(p1, surface.vAxis);

        RayCollision collision = GetRayCollisionQuad(ray, p1, p2, p3, p4);
        if (!collision.hit) {
            continue;
        }
        if (visible && collision.distance >= closestDistance) {
            continue;
        }

        Vector3 n = GameConfig::SurfaceNormal(surface);
        Vector3 local = Vector3Subtract(collision.point, surface.origin);
        float tu = Vector3DotProduct(local, surface.uAxis) / Vector3DotProduct(surface.uAxis, surface.uAxis);
        float tv = Vector3DotProduct(local, surface.vAxis) / Vector3DotProduct(surface.vAxis, surface.vAxis);
        tu = Clamp(tu, 0.0f, 0.9999f);
        tv = Clamp(tv, 0.0f, 0.9999f);

        point = Vector3Add(collision.point, Vector3Scale(n, GameConfig::TableCursorSurfaceOffset));
        normal = n;
        hoverSection = static_cast<int>(i);
        hoverRow = GameConfig::RowFromFraction(surface, tu);
        hoverCol = static_cast<int>(tv * surface.cols);
        closestDistance = collision.distance;
        visible = true;
    }
}

Vector3 TableCursor::CornerPoint(int section, float fu, float fv) const {
    const GameConfig::TableSurface& s = GameConfig::TableSurfaces[section];
    Vector3 onSurface = Vector3Add(s.origin,
        Vector3Add(Vector3Scale(s.uAxis, fu), Vector3Scale(s.vAxis, fv)));
    return Vector3Add(onSurface, Vector3Scale(GameConfig::SurfaceNormal(s), GameConfig::TableCursorSurfaceOffset));
}

void TableCursor::Draw() const {
    for (size_t s = 0; s < GameConfig::TableSurfaceCount; s++) {
        const GameConfig::TableSurface& surface = GameConfig::TableSurfaces[s];
        for (int row = 0; row <= surface.rows; row++) {
            float fu = GameConfig::RowBoundaryFraction(surface, row);
            Vector3 from = CornerPoint(static_cast<int>(s), fu, 0.0f);
            Vector3 to = CornerPoint(static_cast<int>(s), fu, 1.0f);
            DrawLine3D(from, to, DARKGRAY);
        }
        for (int col = 0; col <= surface.cols; col++) {
            Vector3 from = CornerPoint(static_cast<int>(s), 0.0f, static_cast<float>(col) / surface.cols);
            Vector3 to = CornerPoint(static_cast<int>(s), 1.0f, static_cast<float>(col) / surface.cols);
            DrawLine3D(from, to, DARKGRAY);
        }
    }

    if (!visible) {
        return;
    }

    const GameConfig::TableSurface& hoveredSurface = GameConfig::TableSurfaces[hoverSection];
    float rowStart = GameConfig::RowBoundaryFraction(hoveredSurface, hoverRow);
    float rowEnd = GameConfig::RowBoundaryFraction(hoveredSurface, hoverRow + 1);
    float colStart = static_cast<float>(hoverCol) / hoveredSurface.cols;
    float colEnd = static_cast<float>(hoverCol + 1) / hoveredSurface.cols;
    Vector3 a = CornerPoint(hoverSection, rowStart, colStart);
    Vector3 b = CornerPoint(hoverSection, rowEnd, colStart);
    Vector3 c = CornerPoint(hoverSection, rowEnd, colEnd);
    Vector3 d = CornerPoint(hoverSection, rowStart, colEnd);

    DrawTriangle3DBothSides(a, b, c, ORANGE);
    DrawTriangle3DBothSides(a, c, d, ORANGE);

    Vector3 arbitrary = (fabsf(normal.y) < 0.99f) ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
    Vector3 tangent = Vector3Normalize(Vector3CrossProduct(arbitrary, normal));
    Vector3 bitangent = Vector3CrossProduct(normal, tangent);

    for (int i = 0; i < GameConfig::TableCursorSegments; i++) {
        float a0 = kTwoPi * static_cast<float>(i) / GameConfig::TableCursorSegments;
        float a1 = kTwoPi * static_cast<float>(i + 1) / GameConfig::TableCursorSegments;

        Vector3 offset0 = Vector3Add(Vector3Scale(tangent, cosf(a0) * GameConfig::TableCursorRadius),
                                      Vector3Scale(bitangent, sinf(a0) * GameConfig::TableCursorRadius));
        Vector3 offset1 = Vector3Add(Vector3Scale(tangent, cosf(a1) * GameConfig::TableCursorRadius),
                                      Vector3Scale(bitangent, sinf(a1) * GameConfig::TableCursorRadius));

        DrawTriangle3DBothSides(point, Vector3Add(point, offset0), Vector3Add(point, offset1), YELLOW);
    }
}
