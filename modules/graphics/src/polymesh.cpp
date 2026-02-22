#include "engine.hpp"
#include <raymath.h>
#include <poly2tri.h>

MeshLib gMeshLib;

static inline Vector2 normalizeSafe(Vector2 v)
{
    float len = sqrtf(v.x * v.x + v.y * v.y);
    if (len <= 0.0001f) return {0.0f, -1.0f};
    return {v.x / len, v.y / len};
}

static bool buildStripBuffers(const std::vector<Vector2> &top,
                              const std::vector<Vector2> &bottom,
                              const std::vector<float> &uCoords,
                              std::vector<float> *outVertices,
                              std::vector<float> *outUVs,
                              std::vector<unsigned short> *outIndices)
{
    if (!outVertices || !outUVs || !outIndices) return false;
    if (top.size() < 2 || bottom.size() != top.size() || uCoords.size() != top.size()) return false;
    const int n = (int)top.size();
    const int triVerts = (n - 1) * 6; // 2 triangles per segment, 3 vertices each

    outVertices->clear();
    outUVs->clear();
    outIndices->clear(); // non-indexed path (more robust for immediate mode)
    outVertices->reserve(triVerts * 3);
    outUVs->reserve(triVerts * 2);

    auto pushVertex = [&](float x, float y, float u, float v)
    {
        outVertices->push_back(x);
        outVertices->push_back(y);
        outVertices->push_back(0.0f);
        outUVs->push_back(u);
        outUVs->push_back(v);
    };

    for (int i = 0; i < n - 1; i++)
    {
        const float u1 = uCoords[i];
        const float u2 = uCoords[i + 1];

        const Vector2 t1 = top[i];
        const Vector2 b1 = bottom[i];
        const Vector2 t2 = top[i + 1];
        const Vector2 b2 = bottom[i + 1];

        // Triangle 1
        pushVertex(t1.x, t1.y, u1, 0.0f);
        pushVertex(b1.x, b1.y, u1, 1.0f);
        pushVertex(t2.x, t2.y, u2, 0.0f);

        // Triangle 2
        pushVertex(t2.x, t2.y, u2, 0.0f);
        pushVertex(b1.x, b1.y, u1, 1.0f);
        pushVertex(b2.x, b2.y, u2, 1.0f);
    }

    return true;
}

static std::vector<float> buildUCoords(const std::vector<Vector2> &points, float uvScale)
{
    std::vector<float> u;
    u.resize(points.size(), 0.0f);
    if (points.empty()) return u;

    float acc = 0.0f;
    u[0] = 0.0f;
    for (size_t i = 1; i < points.size(); i++)
    {
        float dx = points[i].x - points[i - 1].x;
        float dy = points[i].y - points[i - 1].y;
        acc += sqrtf(dx * dx + dy * dy);
        u[i] = acc * uvScale;
    }
    return u;
}

static inline float cross2D(const Vector2 &a, const Vector2 &b, const Vector2 &c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static bool samePoint(const Vector2 &a, const Vector2 &b, float eps = 1e-4f)
{
    return (fabsf(a.x - b.x) <= eps) && (fabsf(a.y - b.y) <= eps);
}

static bool cleanRingPoints(const std::vector<Vector2> &in, std::vector<Vector2> *out)
{
    if (!out) return false;
    out->clear();
    if (in.empty()) return false;

    out->reserve(in.size());
    for (const Vector2 &p : in)
    {
        if (out->empty() || !samePoint(out->back(), p))
        {
            out->push_back(p);
        }
    }

    if (out->size() >= 2 && samePoint(out->front(), out->back()))
    {
        out->pop_back();
    }
    if (out->size() < 3) return false;

    bool changed = true;
    int pass = 0;
    while (changed && out->size() >= 3 && pass < 64)
    {
        changed = false;
        std::vector<Vector2> next;
        const int n = (int)out->size();
        next.reserve(n);

        for (int i = 0; i < n; i++)
        {
            const Vector2 &prev = (*out)[(i + n - 1) % n];
            const Vector2 &curr = (*out)[i];
            const Vector2 &nxt = (*out)[(i + 1) % n];

            if (samePoint(prev, curr) || samePoint(curr, nxt) || fabsf(cross2D(prev, curr, nxt)) <= 1e-5f)
            {
                changed = true;
                continue;
            }
            next.push_back(curr);
        }

        if (next.size() < 3)
        {
            out->clear();
            return false;
        }

        if (next.size() != out->size())
        {
            *out = next;
        }
        pass++;
    }

    return out->size() >= 3;
}

static float polygonSignedArea(const std::vector<Vector2> &poly)
{
    if (poly.size() < 3) return 0.0f;
    double area = 0.0;
    const int n = (int)poly.size();
    for (int i = 0; i < n; i++)
    {
        const Vector2 &a = poly[i];
        const Vector2 &b = poly[(i + 1) % n];
        area += (double)a.x * (double)b.y - (double)b.x * (double)a.y;
    }
    return (float)(area * 0.5);
}

static bool pointInTriangleCCW(const Vector2 &p, const Vector2 &a, const Vector2 &b, const Vector2 &c)
{
    const float eps = 1e-6f;
    const float c1 = cross2D(a, b, p);
    const float c2 = cross2D(b, c, p);
    const float c3 = cross2D(c, a, p);
    return (c1 >= -eps) && (c2 >= -eps) && (c3 >= -eps);
}

static bool triangulatePolygonEarClipping(const std::vector<Vector2> &poly, std::vector<unsigned short> *outIndices)
{
    if (!outIndices) return false;
    outIndices->clear();
    if (poly.size() < 3 || poly.size() > 65535) return false;

    const int n = (int)poly.size();
    std::vector<int> V(n);

    if (polygonSignedArea(poly) > 0.0f)
    {
        for (int i = 0; i < n; i++) V[i] = i;
    }
    else
    {
        for (int i = 0; i < n; i++) V[i] = (n - 1 - i);
    }

    int nv = n;
    int count = 2 * nv;
    int v = nv - 1;

    while (nv > 2)
    {
        if ((count--) <= 0)
        {
            outIndices->clear();
            return false;
        }

        int u = v;
        if (u >= nv) u = 0;
        v = u + 1;
        if (v >= nv) v = 0;
        int w = v + 1;
        if (w >= nv) w = 0;

        const int ia = V[u];
        const int ib = V[v];
        const int ic = V[w];

        const Vector2 &a = poly[ia];
        const Vector2 &b = poly[ib];
        const Vector2 &c = poly[ic];

        // Ear needs convex vertex in CCW ordering.
        if (cross2D(a, b, c) <= 1e-6f)
        {
            continue;
        }

        bool ear = true;
        for (int p = 0; p < nv; p++)
        {
            if (p == u || p == v || p == w) continue;
            const Vector2 &pt = poly[V[p]];
            if (pointInTriangleCCW(pt, a, b, c))
            {
                ear = false;
                break;
            }
        }

        if (!ear) continue;

        outIndices->push_back((unsigned short)ia);
        outIndices->push_back((unsigned short)ib);
        outIndices->push_back((unsigned short)ic);

        for (int s = v, t = v + 1; t < nv; s++, t++)
        {
            V[s] = V[t];
        }
        nv--;
        count = 2 * nv;
    }

    return !outIndices->empty();
}

static int findOrAddVertex(std::vector<Vector2> *vertices, const Vector2 &p, float eps = 1e-4f)
{
    if (!vertices) return -1;
    for (int i = 0; i < (int)vertices->size(); i++)
    {
        if (fabsf((*vertices)[i].x - p.x) <= eps && fabsf((*vertices)[i].y - p.y) <= eps)
        {
            return i;
        }
    }
    if (vertices->size() >= 65535) return -1;
    vertices->push_back(p);
    return (int)vertices->size() - 1;
}

static bool triangulatePolygonPoly2Tri(const std::vector<Vector2> &poly,
                                       std::vector<Vector2> *outVertices,
                                       std::vector<unsigned short> *outIndices)
{
    if (!outVertices || !outIndices) return false;
    outVertices->clear();
    outIndices->clear();
    if (poly.size() < 3 || poly.size() > 65535) return false;

    // Start from original ring vertices; add Steiner/extra points on demand.
    *outVertices = poly;

    std::vector<p2t::Point *> p2points;
    p2points.reserve(poly.size());

    for (size_t i = 0; i < poly.size(); i++)
    {
        p2t::Point *pt = new p2t::Point((double)poly[i].x, (double)poly[i].y);
        p2points.push_back(pt);
    }

    bool ok = false;
    {
        p2t::CDT cdt(p2points);
        cdt.Triangulate();
        std::vector<p2t::Triangle *> tris = cdt.GetTriangles();
        outIndices->reserve(tris.size() * 3);

        for (p2t::Triangle *tri : tris)
        {
            if (!tri) continue;
            p2t::Point *a = tri->GetPoint(0);
            p2t::Point *b = tri->GetPoint(1);
            p2t::Point *c = tri->GetPoint(2);
            if (!a || !b || !c) continue;

            int ia = findOrAddVertex(outVertices, {(float)a->x, (float)a->y});
            int ib = findOrAddVertex(outVertices, {(float)b->x, (float)b->y});
            int ic = findOrAddVertex(outVertices, {(float)c->x, (float)c->y});
            if (ia < 0 || ib < 0 || ic < 0) continue;

            outIndices->push_back((unsigned short)ia);
            outIndices->push_back((unsigned short)ib);
            outIndices->push_back((unsigned short)ic);
        }
        ok = !outIndices->empty();
    }

    for (p2t::Point *pt : p2points) delete pt;
    return ok;
}

static bool buildPolygonBuffers(const std::vector<Vector2> &vertices,
                                const std::vector<unsigned short> &indices,
                                float uvScale,
                                std::vector<float> *outVertices,
                                std::vector<float> *outUVs,
                                std::vector<unsigned short> *outIndices)
{
    if (!outVertices || !outUVs || !outIndices) return false;
    if (vertices.size() < 3 || indices.size() < 3) return false;

    outVertices->assign(vertices.size() * 3, 0.0f);
    outUVs->assign(vertices.size() * 2, 0.0f);
    outIndices->assign(indices.begin(), indices.end());

    const float baseX = vertices[0].x;
    const float baseY = vertices[0].y;

    for (size_t i = 0; i < vertices.size(); i++)
    {
        (*outVertices)[i * 3 + 0] = vertices[i].x;
        (*outVertices)[i * 3 + 1] = vertices[i].y;
        (*outVertices)[i * 3 + 2] = 0.5f;

        (*outUVs)[i * 2 + 0] = (vertices[i].x - baseX) * uvScale;
        (*outUVs)[i * 2 + 1] = (vertices[i].y - baseY) * uvScale;
    }

    return true;
}
 

static Vector2 computeTrackNormal(const std::vector<Vector2> &points, int i)
{
    const int n = (int)points.size();
    Vector2 dir = {1.0f, 0.0f};

    if (n >= 2)
    {
        if (i <= 0)
        {
            dir = {points[1].x - points[0].x, points[1].y - points[0].y};
        }
        else if (i >= n - 1)
        {
            dir = {points[n - 1].x - points[n - 2].x, points[n - 1].y - points[n - 2].y};
        }
        else
        {
            dir = {points[i + 1].x - points[i - 1].x, points[i + 1].y - points[i - 1].y};
        }
    }

    dir = normalizeSafe(dir);
    Vector2 normal = {dir.y, -dir.x}; // left-handed screen-space "up" normal
    if (normal.y > 0.0f)
    {
        normal.x = -normal.x;
        normal.y = -normal.y;
    }
    return normalizeSafe(normal);
}

PolyMesh::PolyMesh()
{
    bodyTex = {0};
    edgeTex = {0};
}

PolyMesh::~PolyMesh()
{
    clear();
}

void PolyMesh::addPoint(float x, float y)
{
    points.push_back({x, y});
}

void PolyMesh::clear()
{
    points.clear();
    bodyVertices.clear();
    bodyUVs.clear();
    bodyIndices.clear();
    edgeVertices.clear();
    edgeUVs.clear();
    edgeIndices.clear();
    bodyTopStrip.clear();
    bodyBottomStrip.clear();
    bodyUStrip.clear();
    edgeTopStrip.clear();
    edgeBottomStrip.clear();
    edgeUStrip.clear();
    bodyUScaleStrip = 1.0f;
    bodyVScaleStrip = 1.0f;
    edgeUScaleStrip = 1.0f;
    edgeVScaleStrip = 1.0f;
    bodyReady = false;
    edgeReady = false;
}

void PolyMesh::buildTrack(float depth, float uvScale)
{
    buildTrackLayered(depth, 0.0f, uvScale, uvScale, 1.0f, 1.0f);
}

void PolyMesh::buildTrackLayered(float depth, float edgeWidth, float bodyUvScale, float edgeUvScale, float bodyVScale, float edgeVScale)
{
    if (points.size() < 2) return;
    bodyVertices.clear();
    bodyUVs.clear();
    bodyIndices.clear();
    edgeVertices.clear();
    edgeUVs.clear();
    edgeIndices.clear();
    bodyTopStrip.clear();
    bodyBottomStrip.clear();
    bodyUStrip.clear();
    edgeTopStrip.clear();
    edgeBottomStrip.clear();
    edgeUStrip.clear();
    bodyUScaleStrip = 1.0f;
    bodyVScaleStrip = 1.0f;
    edgeUScaleStrip = 1.0f;
    edgeVScaleStrip = 1.0f;
    bodyReady = false;
    edgeReady = false;

    if (bodyVScale <= 0.0f) bodyVScale = 1.0f;
    if (edgeVScale <= 0.0f) edgeVScale = 1.0f;

 
    // --- Body ---
    if (depth > 0.0f)
    {
        std::vector<Vector2> bodyTop = points;
        std::vector<Vector2> bodyBottom = points;
        float maxTopY = bodyTop[0].y;
        for (size_t i = 1; i < bodyTop.size(); i++)
        {
            if (bodyTop[i].y > maxTopY) maxTopY = bodyTop[i].y;
        }
        const float flatBottomY = maxTopY + depth;
        for (size_t i = 0; i < bodyBottom.size(); i++)
        {
            bodyBottom[i].y = flatBottomY;
        }

        std::vector<float> bodyU = buildUCoords(points, 1.0f);
        bodyTopStrip = bodyTop;
        bodyBottomStrip = bodyBottom;
        bodyUStrip = bodyU;
        bodyUScaleStrip = bodyUvScale;
        bodyVScaleStrip = bodyVScale;
        if (buildStripBuffers(bodyTop, bodyBottom, bodyU, &bodyVertices, &bodyUVs, &bodyIndices))
        {
            bodyReady = true;
        }
    }

    // --- Edge strip (top surface) ---
    if (edgeWidth > 0.0f)
    {
        const float half = edgeWidth * 0.5f;
        std::vector<Vector2> edgeTop;
        std::vector<Vector2> edgeBottom;
        edgeTop.resize(points.size());
        edgeBottom.resize(points.size());

        for (int i = 0; i < (int)points.size(); i++)
        {
            Vector2 n = computeTrackNormal(points, i);
            edgeTop[i] = {points[i].x + n.x * half, points[i].y + n.y * half};
            edgeBottom[i] = {points[i].x - n.x * half, points[i].y - n.y * half};
        }

        std::vector<float> edgeU = buildUCoords(points, 1.0f);
        edgeTopStrip = edgeTop;
        edgeBottomStrip = edgeBottom;
        edgeUStrip = edgeU;
        edgeUScaleStrip = edgeUvScale;
        edgeVScaleStrip = edgeVScale;
        if (buildStripBuffers(edgeTop, edgeBottom, edgeU, &edgeVertices, &edgeUVs, &edgeIndices))
        {
            edgeReady = true;
        }
    }

}

void PolyMesh::buildPolygon(float uvScale)
{
    if (points.size() < 3) return;
    bodyVertices.clear();
    bodyUVs.clear();
    bodyIndices.clear();
    edgeVertices.clear();
    edgeUVs.clear();
    edgeIndices.clear();
    bodyTopStrip.clear();
    bodyBottomStrip.clear();
    bodyUStrip.clear();
    edgeTopStrip.clear();
    edgeBottomStrip.clear();
    edgeUStrip.clear();
    bodyReady = false;
    edgeReady = false;
 
    std::vector<Vector2> clean;
    if (!cleanRingPoints(points, &clean))
    {
        Error("buildPolygon invalid ring: need at least 3 non-collinear points");
        return;
    }

    std::vector<Vector2> meshVertices;
    std::vector<unsigned short> triIndices;
    if (!triangulatePolygonPoly2Tri(clean, &meshVertices, &triIndices))
    {
        // Fallback para casos extremos em que o CDT nao consegue triangulacao.
        if (!triangulatePolygonEarClipping(clean, &triIndices))
        {
            Error("buildPolygon triangulation failed (poly2tri + fallback)");
            return;
        }
        meshVertices = clean;
    }

    if (buildPolygonBuffers(meshVertices, triIndices, uvScale, &bodyVertices, &bodyUVs, &bodyIndices))
    {
        bodyReady = true;
    }
}

void PolyMesh::setTexture(Texture2D tex)
{
    setBodyTexture(tex);
    setEdgeTexture(tex);
}

void PolyMesh::setBodyTexture(Texture2D tex)
{
    bodyTex = tex;
    if (tex.id > 0)
    {
        SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
        // Avoid seam bleeding when UV repeats over non-padded tile borders.
        SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    }
}

void PolyMesh::setEdgeTexture(Texture2D tex)
{
    edgeTex = tex;
    if (tex.id > 0)
    {
        SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
        SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    }
}

void PolyMesh::setTopScale(float sx, float sy)
{
    if (sx <= 0.0f) sx = 1.0f;
    if (sy <= 0.0f) sy = 1.0f;
    edgeScaleX = sx;
    edgeScaleY = sy;
}

void PolyMesh::setBottomScale(float sx, float sy)
{
    if (sx <= 0.0f) sx = 1.0f;
    if (sy <= 0.0f) sy = 1.0f;
    bodyScaleX = sx;
    bodyScaleY = sy;
}
 

static void drawStripImmediate2D(const std::vector<Vector2> &top,
                                 const std::vector<Vector2> &bottom,
                                 const std::vector<float> &uCoords,
                                 float uScale,
                                 float vScale,
                                 const Texture2D *tex,
                                 Color tint)
{
    if (top.size() < 2 || bottom.size() != top.size() || uCoords.size() != top.size()) return;
    if (!tex || tex->id <= 0) return;
    if (uScale <= 0.0f) uScale = 1.0f;
    if (vScale <= 0.0f) vScale = 1.0f;

    const int n = (int)top.size();
    rQuad quad = {};
    quad.tex = *tex;
    quad.blend = 0;

    for (int i = 0; i < n - 1; i++)
    {
        float u1 = uCoords[i] * uScale;
        float u2 = uCoords[i + 1] * uScale;

        // RenderQuad expects:
        // v1=top-left, v0=bottom-left, v3=bottom-right, v2=top-right
        quad.v[1].x = top[i].x;        quad.v[1].y = top[i].y;        quad.v[1].z = 0.0f;
        quad.v[1].tx = u1;             quad.v[1].ty = 0.0f;           quad.v[1].col = tint;

        quad.v[0].x = bottom[i].x;     quad.v[0].y = bottom[i].y;     quad.v[0].z = 0.0f;
        quad.v[0].tx = u1;             quad.v[0].ty = vScale;         quad.v[0].col = tint;

        quad.v[3].x = bottom[i + 1].x; quad.v[3].y = bottom[i + 1].y; quad.v[3].z = 0.0f;
        quad.v[3].tx = u2;             quad.v[3].ty = vScale;         quad.v[3].col = tint;

        quad.v[2].x = top[i + 1].x;    quad.v[2].y = top[i + 1].y;    quad.v[2].z = 0.0f;
        quad.v[2].tx = u2;             quad.v[2].ty = 0.0f;           quad.v[2].col = tint;

        RenderQuad(&quad);
    }
}

 
static void drawTrisImmediate2D(const std::vector<Vector2> &top,
                                         const std::vector<Vector2> &bottom,
                                         const std::vector<float> &uCoords,
                                         float uScale,
                                         float vScale,
                                         const Texture2D *tex,
                                         Color tint)
{
    if (top.size() < 2 || bottom.size() != top.size() || uCoords.size() != top.size()) return;
    if (!tex || tex->id <= 0) return;
    if (uScale <= 0.0f) uScale = 1.0f;
    if (vScale <= 0.0f) vScale = 1.0f;

    const int n = (int)top.size();
    const int quadVertexCount = (n - 1) * 4;

    rlCheckRenderBatchLimit(quadVertexCount);
    rlSetTexture(tex->id);
    rlBegin(RL_QUADS);
    rlNormal3f(0.0f, 0.0f, 1.0f);

    for (int i = 0; i < n - 1; i++)
    {
        float u1 = uCoords[i] * uScale;
        float u2 = uCoords[i + 1] * uScale;

        const Vector2 v1 = top[i];
        const Vector2 v0 = bottom[i];
        const Vector2 v3 = bottom[i + 1];
        const Vector2 v2 = top[i + 1];

        // Match RenderQuad winding/order exactly: v1, v0, v3, v2
        rlColor4ub(tint.r, tint.g, tint.b, tint.a);
        rlColor4ub(tint.r, tint.g, tint.b, tint.a);
        rlTexCoord2f(u1, 0.0f);
        rlVertex3f(v1.x, v1.y, 0.0f);

        rlColor4ub(tint.r, tint.g, tint.b, tint.a);
        rlTexCoord2f(u1, vScale);
        rlVertex3f(v0.x, v0.y, 0.0f);

        rlColor4ub(tint.r, tint.g, tint.b, tint.a);
        rlTexCoord2f(u2, vScale);
        rlVertex3f(v3.x, v3.y, 0.0f);

        rlColor4ub(tint.r, tint.g, tint.b, tint.a);
        rlTexCoord2f(u2, 0.0f);
        rlVertex3f(v2.x, v2.y, 0.0f);
    }

    rlEnd();
    rlSetTexture(0);
}

void PolyMesh::draw(float x, float y, float rotation, float scale, Color tint)
{
    if (!bodyReady && !edgeReady) return;

    // rlDisableBackfaceCulling();
    // rlDisableDepthTest();
    // rlDisableDepthMask();
 

    rlPushMatrix();
    rlTranslatef(x, y, 0);
    rlRotatef(rotation, 0, 0, 1);
    rlScalef(scale, scale, 1);

    if (bodyReady)
    {
             drawTrisImmediate2D(bodyTopStrip, bodyBottomStrip, bodyUStrip, bodyUScaleStrip * bodyScaleX, bodyVScaleStrip * bodyScaleY, &bodyTex, tint);
     
    }
    if (edgeReady)
    {
              drawTrisImmediate2D(edgeTopStrip, edgeBottomStrip, edgeUStrip, edgeUScaleStrip * edgeScaleX, edgeVScaleStrip * edgeScaleY, &edgeTex, tint);
       
    }

    rlPopMatrix();

    // rlEnableDepthMask();
    // rlEnableDepthTest();
    // rlEnableBackfaceCulling();
}

// --- MeshLib Implementation ---
int MeshLib::create() { meshes.push_back(new PolyMesh()); return meshes.size() - 1; }
PolyMesh* MeshLib::get(int id) { if (id < 0 || id >= (int)meshes.size()) return nullptr; return meshes[id]; }
void MeshLib::destroy() { for (auto m : meshes) delete m; meshes.clear(); }
