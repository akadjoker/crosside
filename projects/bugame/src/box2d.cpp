#include "bindings.hpp"
#include "box2d_joints.hpp"
#include "interpreter.hpp"
#include <raylib.h>
#include <rlgl.h>
#include <box2d/box2d.h>
#include <cmath>
#include <cstdint>
#include <vector>

namespace BindingsBox2D
{
    static constexpr const char *kClassBody = "Body";
    static constexpr const char *kClassBodyDef = "BodyDef";
    static constexpr const char *kClassFixture = "Fixture";
    static constexpr const char *kClassFixtureDef = "FixtureDef";

    static constexpr float kDefaultPixelsPerMeter = 30.0f;
    static b2World *gWorld = nullptr;
    static float gPixelsPerMeter = kDefaultPixelsPerMeter;
    static float gLastStepTime = -1.0f;
    static int32 gVelocityIterations = 8;
    static int32 gPositionIterations = 3;
    static bool gDebugRenderEnabled = false;
    static uint32 gDebugFlags = b2Draw::e_shapeBit | b2Draw::e_jointBit | b2Draw::e_centerOfMassBit;
    struct FixtureDefHandle
    {
        b2FixtureDef fixture;
        b2Shape *ownedShape{nullptr};

        FixtureDefHandle()
        {
            fixture.shape = nullptr;
        }

        ~FixtureDefHandle()
        {
            clearShape();
        }

        void clearShape()
        {
            if (ownedShape)
            {
                delete ownedShape;
                ownedShape = nullptr;
            }
            fixture.shape = nullptr;
        }

        void setCircleShape(float radius, float centerX = 0.0f, float centerY = 0.0f)
        {
            clearShape();
            auto *copy = new b2CircleShape();
            copy->m_radius = radius;
            copy->m_p.Set(centerX, centerY);
            ownedShape = copy;
            fixture.shape = copy;
        }

        void setBoxShape(float halfW, float halfH, float centerX = 0.0f, float centerY = 0.0f, float angleRad = 0.0f)
        {
            clearShape();
            auto *poly = new b2PolygonShape();
            poly->SetAsBox(halfW, halfH, b2Vec2(centerX, centerY), angleRad);
            ownedShape = poly;
            fixture.shape = poly;
        }

        void setEdgeShape(float x1, float y1, float x2, float y2)
        {
            clearShape();
            auto *edge = new b2EdgeShape();
            edge->SetTwoSided(b2Vec2(x1, y1), b2Vec2(x2, y2));
            ownedShape = edge;
            fixture.shape = edge;
        }

        void setChainShape(const std::vector<b2Vec2> &points, bool loop)
        {
            clearShape();
            auto *chain = new b2ChainShape();
            if (loop)
            {
                chain->CreateLoop(points.data(), (int32)points.size());
            }
            else
            {
                chain->CreateChain(points.data(), (int32)points.size(), points[0], points.back());
            }
            ownedShape = chain;
            fixture.shape = chain;
        }
    };

    enum BodyType
    {
        BODY_DYNAMIC = 0,
        BODY_STATIC = 1,
        BODY_KINEMATIC = 2,
    };

    enum ShapeType
    {
        SHAPE_BOX = 0,
        SHAPE_CIRCLE = 1,
    };

    enum SyncMode
    {
        SYNC_AUTO = 0,
        SYNC_PROCESS_TO_BODY = 1,
        SYNC_BODY_TO_PROCESS = 2,
    };

    b2Vec2 pixelToWorld(float x, float y)
    {
        return b2Vec2(x / kDefaultPixelsPerMeter, y / kDefaultPixelsPerMeter);
    }

    b2Vec2 vectorToWorld(const Vector2 &v)
    {
        return b2Vec2(v.x / kDefaultPixelsPerMeter, v.y / kDefaultPixelsPerMeter);
    }
    // Converta uma posição de mundo para uma posição de pixel
    Vector2 worldToPixel(const b2Vec2 &pos)
    {
        Vector2 v;
        v.x = pos.x * kDefaultPixelsPerMeter;
        v.y = pos.y * kDefaultPixelsPerMeter;
        return v;
    }

    float worldToPixel(float value)
    {
        return value * kDefaultPixelsPerMeter;
    }

    float pixelToWorld(float value)
    {
        return value / kDefaultPixelsPerMeter;
    }

    float degreesToRadians(float degrees)
    {
        return degrees * b2_pi / 180.0f;
    }

    Color getColor(const b2Color &color)
    {
        Color c;
        c.r = color.r * 255;
        c.g = color.g * 255;
        c.b = color.b * 255;
        c.a = color.a * 255;
        return c;
    }

    void rDrawCircle(const b2Vec2 &center, float radius, const b2Color &color)
    {
        Vector2 pos = worldToPixel(center);
        float r = worldToPixel(radius);
        DrawCircleLines(pos.x, pos.y, r, getColor(color));
    }
    void rDrawSolidCircle(const b2Vec2 &center, float radius, const b2Color &color)
    {
        Vector2 pos = worldToPixel(center);
        float r = worldToPixel(radius);
        DrawCircle(pos.x, pos.y, r, getColor(color));
    }

    static inline float degToRad(float deg)
    {
        return deg * b2_pi / 180.0f;
    }

    static inline float radToDeg(float rad)
    {
        return rad * 180.0f / b2_pi;
    }

    static bool value_to_bool(const Value &v, bool *out)
    {
        if (!out)
            return false;
        if (v.isBool())
        {
            *out = v.asBool();
            return true;
        }
        if (v.isNumber())
        {
            *out = (v.asNumber() != 0.0);
            return true;
        }
        return false;
    }

    static NativeClassDef *require_native_class(Interpreter *vm, const char *className)
    {
        NativeClassDef *klass = nullptr;
        if (!vm || !vm->tryGetNativeClassDef(className, &klass) || !klass)
        {
            Error("%s class is not registered !", className);
            return nullptr;
        }
        return klass;
    }

    static NativeClassInstance *require_native_instance(Interpreter *vm, const Value &value, const char *className)
    {
        if (!value.isNativeClassInstance())
        {
            Error("Expected %s instance", className);
            return nullptr;
        }
        NativeClassInstance *instance = value.asNativeClassInstance();
        NativeClassDef *klass = require_native_class(vm, className);
        if (!klass)
            return nullptr;
        if (!instance || instance->klass != klass)
        {
            Error("Expected %s instance", className);
            return nullptr;
        }
        if (!instance->userData)
        {
            Error("%s instance has null userData", className);
            return nullptr;
        }
        return instance;
    }

    static bool push_native_instance(Interpreter *vm, const char *className, void *userData)
    {
        NativeClassDef *klass = require_native_class(vm, className);
        if (!klass || !userData)
        {
            vm->pushNil();
            return false;
        }
        Value value = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = value.asNativeClassInstance();
        instance->klass = klass;
        instance->userData = userData;
        vm->push(value);
        return true;
    }

    static FixtureDefHandle *as_fixture_def_handle(void *data, const char *funcName);
    static b2Fixture *as_fixture_handle(void *data, const char *funcName);

    static bool drawFilled = false;

    class bDebugDraw : public b2Draw
    {
    public:
        bDebugDraw() {}
        ~bDebugDraw() {}

        void DrawPolygon(const b2Vec2 *vertices, int32 vertexCount, const b2Color &color)
        {

            for (int i = 0; i < vertexCount; i++)
            {
                int j = (i + 1) % vertexCount;
                Vector2 p1 = worldToPixel(vertices[i]);
                Vector2 p2 = worldToPixel(vertices[j]);
                DrawLine(p1.x, p1.y, p2.x, p2.y, raylibColor(color));
            }
        }

        void DrawSolidPolygon(const b2Vec2 *vertices, int32 vertexCount, const b2Color &color)
        {
            if (vertexCount < 3)
                return;

            // Box2D polygons are convex; triangle fan is the correct fill path.
            Vector2 pivot = worldToPixel(vertices[0]);
            Color fill = raylibColor(color);
            fill.a = (unsigned char)((int)fill.a / 2);

            rlBegin(RL_TRIANGLES);
            rlColor4ub(fill.r, fill.g, fill.b, fill.a);
            for (int i = 1; i < vertexCount - 1; ++i)
            {
                Vector2 v1 = worldToPixel(vertices[i]);
                Vector2 v2 = worldToPixel(vertices[i + 1]);
                rlVertex2f(pivot.x, pivot.y);
                rlVertex2f(v2.x, v2.y);
                rlVertex2f(v1.x, v1.y);
            }
            rlEnd();

            // Color stroke = raylibColor(color);
            // for (int i = 0; i < vertexCount; ++i)
            // {
            //     int j = (i + 1) % vertexCount;
            //     Vector2 p1 = worldToPixel(vertices[i]);
            //     Vector2 p2 = worldToPixel(vertices[j]);
            //     DrawLine((int)p1.x, (int)p1.y, (int)p2.x, (int)p2.y, stroke);
            // }
        }

        void DrawCircle(const b2Vec2 &center, float radius, const b2Color &color)
        {
            rDrawCircle(center, radius, color);
        }

        void DrawSolidCircle(const b2Vec2 &center, float radius, const b2Vec2 &axis, const b2Color &color)
        {
            rDrawSolidCircle(center, radius, color);
            //Vector2 pos = worldToPixel(center);
            //DrawLine(pos.x, pos.y, pos.x + axis.x * radius * kDefaultPixelsPerMeter, pos.y + axis.y * radius * kDefaultPixelsPerMeter, RED);
        }

        void DrawSegment(const b2Vec2 &p1, const b2Vec2 &p2, const b2Color &color)
        {
            Vector2 pos1 = worldToPixel(p1);
            Vector2 pos2 = worldToPixel(p2);
            DrawLine(pos1.x, pos1.y, pos2.x, pos2.y, raylibColor(color));
        }

        void DrawTransform(const b2Transform &xf)
        {
            Vector2 p1 = worldToPixel(xf.p);
            Vector2 p2 = worldToPixel(b2Mul(xf, b2Vec2(0.5f, 0.0f)));
            DrawLine(p1.x, p1.y, p2.x, p2.y, GREEN);
        }
        void DrawPoint(const b2Vec2 &p, float size, const b2Color &color)
        {
            rDrawCircle(p, 0.1f, color);
        }

    private:
        Color raylibColor(const b2Color &color)
        {
            return {(unsigned char)(color.r * 255), (unsigned char)(color.g * 255), (unsigned char)(color.b * 255), (unsigned char)(color.a * 255)};
        }
    };

    bDebugDraw debugDraw;
    static uint64_t make_contact_key(uint32 a, uint32 b)
    {
        if (a > b)
        {
            const uint32 t = a;
            a = b;
            b = t;
        }
        return (uint64_t(a) << 32) | uint64_t(b);
    }

    static bool contact_key_has_id(uint64_t key, uint32 id)
    {
        const uint32 a = (uint32)(key >> 32);
        const uint32 b = (uint32)(key & 0xFFFFFFFFu);
        return a == id || b == id;
    }

    static uint32 contact_key_other_id(uint64_t key, uint32 id)
    {
        const uint32 a = (uint32)(key >> 32);
        const uint32 b = (uint32)(key & 0xFFFFFFFFu);
        if (a == id)
            return b;
        if (b == id)
            return a;
        return 0;
    }

    static uint32 body_process_id(const b2Body *body)
    {
        if (!body)
            return 0;
        const uintptr_t ptr = body->GetUserData().pointer;
        return (uint32)ptr;
    }

    struct ProcessTypeEntry
    {
        uint32 id;
        int blueprint;
    };

    static std::vector<ProcessTypeEntry> gProcessTypes;

    static int find_process_type_index(uint32 id)
    {
        for (size_t i = 0; i < gProcessTypes.size(); ++i)
        {
            if (gProcessTypes[i].id == id)
                return (int)i;
        }
        return -1;
    }

    static void set_process_type(uint32 id, int blueprint)
    {
        if (id == 0)
            return;
        const int index = find_process_type_index(id);
        if (index >= 0)
        {
            gProcessTypes[(size_t)index].blueprint = blueprint;
            return;
        }
        gProcessTypes.push_back({id, blueprint});
    }

    static int get_process_type(uint32 id)
    {
        const int index = find_process_type_index(id);
        if (index < 0)
            return -1;
        return gProcessTypes[(size_t)index].blueprint;
    }

    static void remove_process_type(uint32 id)
    {
        const int index = find_process_type_index(id);
        if (index < 0)
            return;
        const size_t i = (size_t)index;
        gProcessTypes[i] = gProcessTypes.back();
        gProcessTypes.pop_back();
    }

    struct ContactRefEntry
    {
        uint64_t key;
        int refCount;
    };

    static std::vector<ContactRefEntry> gContactRefCounts;
    static std::vector<uint64_t> gCollisionEvents;

    static int find_contact_ref_index(uint64_t key)
    {
        for (size_t i = 0; i < gContactRefCounts.size(); ++i)
        {
            if (gContactRefCounts[i].key == key)
                return (int)i;
        }
        return -1;
    }

    class bContactListener : public b2ContactListener
    {
    public:
        void BeginContact(b2Contact *contact) override
        {
            track(contact, +1);
        }

        void EndContact(b2Contact *contact) override
        {
            track(contact, -1);
        }

    private:
        void track(b2Contact *contact, int delta)
        {
            if (!contact || delta == 0)
                return;

            b2Fixture *fixtureA = contact->GetFixtureA();
            b2Fixture *fixtureB = contact->GetFixtureB();
            if (!fixtureA || !fixtureB)
                return;

            const uint32 idA = body_process_id(fixtureA->GetBody());
            const uint32 idB = body_process_id(fixtureB->GetBody());
            if (idA == 0 || idB == 0)
                return;

            const uint64_t key = make_contact_key(idA, idB);
            const int index = find_contact_ref_index(key);
            if (index < 0)
            {
                if (delta > 0)
                {
                    gContactRefCounts.push_back({key, delta});
                    gCollisionEvents.push_back(key);
                }
                return;
            }

            gContactRefCounts[(size_t)index].refCount += delta;
            if (gContactRefCounts[(size_t)index].refCount <= 0)
            {
                const size_t i = (size_t)index;
                gContactRefCounts[i] = gContactRefCounts.back();
                gContactRefCounts.pop_back();
            }
        }
    };

    static bContactListener gContactListener;
    std::vector<b2Joint *> jointsScheduledForRemoval;
    std::vector<b2Body *> bodysScheduledForRemoval;

    static void schedule_body_for_removal(b2Body *body)
    {
        if (!body)
            return;
        bodysScheduledForRemoval.push_back(body);
    }

    static void destroy_body_now_or_schedule(b2Body *body)
    {
        if (!body || !gWorld)
            return;
        BindingsBox2DJoints::onBodyRemoving(body);
        if (gWorld->IsLocked())
        {
            schedule_body_for_removal(body);
        }
        else
        {
            gWorld->DestroyBody(body);
        }
    }

    static float polygon_signed_area(const std::vector<b2Vec2> &vertices)
    {
        if (vertices.size() < 3)
            return 0.0f;
        float area = 0.0f;
        for (size_t i = 0; i < vertices.size(); ++i)
        {
            const b2Vec2 &a = vertices[i];
            const b2Vec2 &b = vertices[(i + 1) % vertices.size()];
            area += a.x * b.y - b.x * a.y;
        }
        return area * 0.5f;
    }

    static bool is_polygon_convex(const std::vector<b2Vec2> &vertices)
    {
        const size_t n = vertices.size();
        if (n < 3)
            return false;

        bool hasPos = false;
        bool hasNeg = false;
        for (size_t i = 0; i < n; ++i)
        {
            const b2Vec2 &a = vertices[i];
            const b2Vec2 &b = vertices[(i + 1) % n];
            const b2Vec2 &c = vertices[(i + 2) % n];
            const float cross = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
            if (cross > 1e-6f)
                hasPos = true;
            else if (cross < -1e-6f)
                hasNeg = true;
            if (hasPos && hasNeg)
                return false;
        }
        return true;
    }

    static bool point_in_triangle(const b2Vec2 &p, const b2Vec2 &a, const b2Vec2 &b, const b2Vec2 &c)
    {
        const float c1 = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
        const float c2 = (c.x - b.x) * (p.y - b.y) - (c.y - b.y) * (p.x - b.x);
        const float c3 = (a.x - c.x) * (p.y - c.y) - (a.y - c.y) * (p.x - c.x);
        const bool hasNeg = (c1 < 0.0f) || (c2 < 0.0f) || (c3 < 0.0f);
        const bool hasPos = (c1 > 0.0f) || (c2 > 0.0f) || (c3 > 0.0f);
        return !(hasNeg && hasPos);
    }

    static bool is_ear(const std::vector<b2Vec2> &vertices, const std::vector<int> &indices, int earIndex, bool ccw)
    {
        const int n = (int)indices.size();
        const int prevIndex = indices[(earIndex + n - 1) % n];
        const int currIndex = indices[earIndex];
        const int nextIndex = indices[(earIndex + 1) % n];

        const b2Vec2 &a = vertices[prevIndex];
        const b2Vec2 &b = vertices[currIndex];
        const b2Vec2 &c = vertices[nextIndex];

        const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (ccw ? (cross <= 0.0f) : (cross >= 0.0f))
        {
            return false;
        }

        for (int i = 0; i < n; ++i)
        {
            const int idx = indices[i];
            if (idx == prevIndex || idx == currIndex || idx == nextIndex)
                continue;
            if (point_in_triangle(vertices[idx], a, b, c))
            {
                return false;
            }
        }
        return true;
    }

    std::vector<b2Vec2> triangulate(std::vector<b2Vec2> vertices)
    {
        std::vector<b2Vec2> triangles;
        if (vertices.size() < 3)
            return triangles;

        const bool ccw = polygon_signed_area(vertices) > 0.0f;
        std::vector<int> indices;
        indices.reserve(vertices.size());
        for (size_t i = 0; i < vertices.size(); ++i)
        {
            indices.push_back((int)i);
        }

        int guard = 0;
        const int maxGuard = (int)vertices.size() * (int)vertices.size();

        while (indices.size() > 3 && guard++ < maxGuard)
        {
            bool earFound = false;
            for (int i = 0; i < (int)indices.size(); ++i)
            {
                if (!is_ear(vertices, indices, i, ccw))
                    continue;

                const int n = (int)indices.size();
                const int prevIndex = indices[(i + n - 1) % n];
                const int currIndex = indices[i];
                const int nextIndex = indices[(i + 1) % n];

                triangles.push_back(vertices[prevIndex]);
                triangles.push_back(vertices[currIndex]);
                triangles.push_back(vertices[nextIndex]);
                indices.erase(indices.begin() + i);
                earFound = true;
                break;
            }
            if (!earFound)
            {
                triangles.clear();
                return triangles;
            }
        }

        if (indices.size() == 3)
        {
            triangles.push_back(vertices[indices[0]]);
            triangles.push_back(vertices[indices[1]]);
            triangles.push_back(vertices[indices[2]]);
        }
        else
        {
            triangles.clear();
        }
        return triangles;
    }

    static bool parse_shape_points(Interpreter *vm, const Value &value, std::vector<b2Vec2> *outPoints, const char *funcName, size_t minPoints)
    {
        (void)vm;
        if (!outPoints)
            return false;
        outPoints->clear();
        if (!value.isArray())
        {
            Error("%s expects flat array [x0, y0, x1, y1, ...]", funcName);
            return false;
        }

        ArrayInstance *arr = value.asArray();
        const size_t count = arr->values.size();
        if ((count % 2) != 0)
        {
            Error("%s expects even number of values [x0, y0, ...]", funcName);
            return false;
        }

        const size_t pointCount = count / 2;
        if (pointCount < minPoints)
        {
            Error("%s needs at least %d points", funcName, (int)minPoints);
            return false;
        }

        outPoints->reserve(pointCount);
        for (size_t i = 0; i < count; i += 2)
        {
            const Value &vx = arr->values[i];
            const Value &vy = arr->values[i + 1];
            if (!vx.isNumber() || !vy.isNumber())
            {
                Error("%s expects numeric values [x0, y0, ...]", funcName);
                outPoints->clear();
                return false;
            }
            outPoints->push_back(b2Vec2((float)vx.asNumber(), (float)vy.asNumber()));
        }
        return true;
    }

    static bool parse_polygon_points(Interpreter *vm, const Value &value, std::vector<b2Vec2> *outPoints, const char *funcName)
    {
        return parse_shape_points(vm, value, outPoints, funcName, 3);
    }

    int native_create_physics(Interpreter *vm, int argCount, Value *args)
    {
        (void)vm;
        float gx = 0.0f;
        float gy = 9.8f;

        if (argCount != 0 && argCount != 2)
        {
            Error("create_physics expects 0 or 2 arguments ([gx, gy])");
            return 0;
        }
        if (argCount == 2)
        {
            if (!args[0].isNumber() || !args[1].isNumber())
            {
                Error("create_physics expects number arguments (gx, gy)");
                return 0;
            }
            gx = (float)args[0].asNumber();
            gy = (float)args[1].asNumber();
        }

        if (gWorld != nullptr)
        {
            BindingsBox2DJoints::onWorldDestroying();
            delete gWorld;
            gWorld = nullptr;
        }
        gContactRefCounts.clear();
        gCollisionEvents.clear();
        if (gContactRefCounts.capacity() < 256)
        {
            gContactRefCounts.reserve(256);
        }
        if (gCollisionEvents.capacity() < 256)
        {
            gCollisionEvents.reserve(256);
        }
        gWorld = new b2World(b2Vec2(gx, gy));
        gWorld->SetDebugDraw(&debugDraw);
        gWorld->SetContactListener(&gContactListener);
        debugDraw.SetFlags(gDebugFlags);
        BindingsBox2DJoints::setWorld(gWorld);
        return 0;
    }

    int native_set_physics_debug(Interpreter *vm, int argCount, Value *args)
    {
        (void)vm;
        bool enabled = false;
        if (argCount != 1 || !value_to_bool(args[0], &enabled))
        {
            Error("set_physics_debug expects 1 bool argument");
            return 0;
        }
        gDebugRenderEnabled = enabled;
        return 0;
    }

    int native_set_physics_debug_flags(Interpreter *vm, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_physics_debug_flags expects 1 number argument (bitmask)");
            return 0;
        }
        gDebugFlags = (uint32)args[0].asNumber();
        debugDraw.SetFlags(gDebugFlags);
        return 0;
    }

    void renderDebug()
    {
        if (!gDebugRenderEnabled || !gWorld)
            return;
        gWorld->DebugDraw();
    }

    b2Draw *getDebugDraw()
    {
        return &debugDraw;
    }

    void shutdownPhysics()
    {
        BindingsBox2DJoints::onWorldDestroying();
        if (gWorld != nullptr)
        {
            delete gWorld;
            gWorld = nullptr;
        }
        gContactRefCounts.clear();
        gCollisionEvents.clear();
        gProcessTypes.clear();
        jointsScheduledForRemoval.clear();
        bodysScheduledForRemoval.clear();
    }
    void onProcessDestroy(Process *proc)
    {
        if (!proc)
            return;
        const uint32 id = proc->id;
        if (id == 0 || gContactRefCounts.empty())
        {
            remove_process_type(id);
            return;
        }

        for (size_t i = 0; i < gContactRefCounts.size();)
        {
            if (contact_key_has_id(gContactRefCounts[i].key, id))
            {
                gContactRefCounts[i] = gContactRefCounts.back();
                gContactRefCounts.pop_back();
            }
            else
            {
                ++i;
            }
        }

        for (size_t i = 0; i < gCollisionEvents.size();)
        {
            if (contact_key_has_id(gCollisionEvents[i], id))
            {
                gCollisionEvents[i] = gCollisionEvents.back();
                gCollisionEvents.pop_back();
            }
            else
            {
                ++i;
            }
        }

        remove_process_type(id);
    }

    int native_destroy_physics(Interpreter *vm, int argCount, Value *args)
    {
        (void)vm;
        (void)args;
        if (argCount != 0)
        {
            Error("destroy_physics expects no arguments");
            return 0;
        }
        shutdownPhysics();
        return 0;
    }

    int native_get_body_count(Interpreter *vm, int argCount, Value *args)
    {
        (void)args;
        if (argCount != 0)
        {
            Error("get_body_count expects no arguments");
            vm->pushInt(0);
            return 1;
        }
        if (!gWorld)
        {
            vm->pushInt(0);
            return 1;
        }
        vm->pushInt((int)gWorld->GetBodyCount());
        return 1;
    }

    int native_physics_collide(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("physics_collide expects 2 number arguments (idA, idB)");
            vm->pushBool(false);
            return 1;
        }

        const uint32 idA = (uint32)args[0].asNumber();
        const uint32 idB = (uint32)args[1].asNumber();
        if (idA == 0 || idB == 0)
        {
            vm->pushBool(false);
            return 1;
        }

        const uint64_t key = make_contact_key(idA, idB);
        const int index = find_contact_ref_index(key);
        vm->pushBool(index >= 0 && gContactRefCounts[(size_t)index].refCount > 0);
        return 1;
    }

    int native_physics_collide_with(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("physics_collide_with expects 1 argument (process type/blueprint)");
            vm->pushInt(-1);
            return 1;
        }

        Process *self = vm->getCurrentProcess();
        if (!self || self->id == 0)
        {
            Error("physics_collide_with must be called inside a process");
            vm->pushInt(-1);
            return 1;
        }

        const uint32 selfId = self->id;
        int targetBlueprint = -1;

        if (args[0].isProcess())
        {
            targetBlueprint = args[0].asProcessId();
        }
        else if (args[0].isInt())
        {
            targetBlueprint = args[0].asInt();
        }
        else
        {
            Error("physics_collide_with expects process type/blueprint (int)");
            vm->pushInt(-1);
            return 1;
        }

        if (targetBlueprint < 0)
        {
            vm->pushInt(-1);
            return 1;
        }

        for (size_t i = 0; i < gContactRefCounts.size(); ++i)
        {
            const ContactRefEntry &entry = gContactRefCounts[i];
            if (entry.refCount <= 0 || !contact_key_has_id(entry.key, selfId))
                continue;

            const uint32 otherId = contact_key_other_id(entry.key, selfId);
            if (otherId == 0)
                continue;

            if (get_process_type(otherId) == targetBlueprint)
            {
                vm->pushInt((int)otherId);
                return 1;
            }
        }

        vm->pushInt(-1);
        return 1;
    }

    class RayCastClosestCallback : public b2RayCastCallback
    {
    public:
        RayCastClosestCallback(int targetBlueprint, uint32 ignoreProcessId, bool ignoreSensors)
            : targetBlueprint_(targetBlueprint), ignoreProcessId_(ignoreProcessId), ignoreSensors_(ignoreSensors) {}

        float ReportFixture(b2Fixture *fixture, const b2Vec2 &point, const b2Vec2 &normal, float fraction) override
        {
            if (!fixture)
                return -1.0f;
            if (ignoreSensors_ && fixture->IsSensor())
                return -1.0f;

            b2Body *body = fixture->GetBody();
            if (!body)
                return -1.0f;

            const uint32 processId = body_process_id(body);
            if (ignoreProcessId_ != 0 && processId == ignoreProcessId_)
                return -1.0f;

            if (targetBlueprint_ >= 0)
            {
                if (processId == 0)
                    return -1.0f;
                if (get_process_type(processId) != targetBlueprint_)
                    return -1.0f;
            }

            hasHit = true;
            hitProcessId = processId;
            hitPoint = point;
            hitNormal = normal;
            hitFraction = fraction;

            // Keep nearest hit.
            return fraction;
        }

        bool hasHit = false;
        uint32 hitProcessId = 0;
        b2Body *hitBody = nullptr;
        b2Vec2 hitPoint{0.0f, 0.0f};
        b2Vec2 hitNormal{0.0f, 0.0f};
        float hitFraction = 1.0f;

    private:
        int targetBlueprint_;
        uint32 ignoreProcessId_;
        bool ignoreSensors_;
    };

    class OverlapCallback : public b2QueryCallback
    {
    public:
        enum class Mode
        {
            POINT,
            RECT,
            CIRCLE
        };

        OverlapCallback(int targetBlueprint, uint32 ignoreProcessId, bool ignoreSensors, const b2Vec2 &point)
            : mode_(Mode::POINT),
              targetBlueprint_(targetBlueprint),
              ignoreProcessId_(ignoreProcessId),
              ignoreSensors_(ignoreSensors),
              point_(point) {}

        OverlapCallback(int targetBlueprint, uint32 ignoreProcessId, bool ignoreSensors, const b2Vec2 &center, float halfW, float halfH)
            : mode_(Mode::RECT),
              targetBlueprint_(targetBlueprint),
              ignoreProcessId_(ignoreProcessId),
              ignoreSensors_(ignoreSensors),
              queryXf_(center, b2Rot(0.0f))
        {
            rectShape_.SetAsBox(halfW, halfH);
        }

        OverlapCallback(int targetBlueprint, uint32 ignoreProcessId, bool ignoreSensors, const b2Vec2 &center, float radius)
            : mode_(Mode::CIRCLE),
              targetBlueprint_(targetBlueprint),
              ignoreProcessId_(ignoreProcessId),
              ignoreSensors_(ignoreSensors),
              queryXf_(center, b2Rot(0.0f))
        {
            circleShape_.m_radius = radius;
            circleShape_.m_p.Set(0.0f, 0.0f);
        }

        bool ReportFixture(b2Fixture *fixture) override
        {
            if (!fixture)
                return true;
            if (ignoreSensors_ && fixture->IsSensor())
                return true;

            b2Body *body = fixture->GetBody();
            if (!body)
                return true;

            const uint32 processId = body_process_id(body);
            if (ignoreProcessId_ != 0 && processId == ignoreProcessId_)
                return true;

            if (targetBlueprint_ >= 0)
            {
                if (processId == 0 || get_process_type(processId) != targetBlueprint_)
                    return true;
            }

            bool overlaps = false;
            if (mode_ == Mode::POINT)
            {
                overlaps = fixture->TestPoint(point_);
            }
            else
            {
                const b2Shape *shape = fixture->GetShape();
                const int childCount = shape ? shape->GetChildCount() : 0;
                for (int child = 0; child < childCount; ++child)
                {
                    if (mode_ == Mode::RECT)
                    {
                        if (b2TestOverlap(shape, child, &rectShape_, 0, body->GetTransform(), queryXf_))
                        {
                            overlaps = true;
                            break;
                        }
                    }
                    else
                    {
                        if (b2TestOverlap(shape, child, &circleShape_, 0, body->GetTransform(), queryXf_))
                        {
                            overlaps = true;
                            break;
                        }
                    }
                }
            }

            if (!overlaps)
                return true;

            hasHit = true;
            hitProcessId = processId;
            hitBody = body;
            return false; // first hit
        }

        bool hasHit = false;
        uint32 hitProcessId = 0;
        b2Body *hitBody = nullptr;

    private:
        Mode mode_{Mode::POINT};
        int targetBlueprint_{-1};
        uint32 ignoreProcessId_{0};
        bool ignoreSensors_{true};
        b2Vec2 point_{0.0f, 0.0f};
        b2Transform queryXf_{};
        b2PolygonShape rectShape_{};
        b2CircleShape circleShape_{};
    };

    static bool parse_type_and_ignore_self(Interpreter *vm, int argCount, Value *args, int baseArgs,
                                           const char *funcName, int *targetBlueprint, bool *ignoreSelf)
    {
        if (!targetBlueprint || !ignoreSelf)
            return false;
        *targetBlueprint = -1;
        *ignoreSelf = true;

        if (argCount == baseArgs)
            return true;

        if (argCount == baseArgs + 1)
        {
            if (args[baseArgs].isProcess())
            {
                *targetBlueprint = args[baseArgs].asProcessId();
                return true;
            }
            if (args[baseArgs].isInt())
            {
                *targetBlueprint = args[baseArgs].asInt();
                return true;
            }
            if (value_to_bool(args[baseArgs], ignoreSelf))
            {
                return true;
            }
            Error("%s optional argument must be process type/int or bool ignoreSelf", funcName);
            return false;
        }

        if (argCount == baseArgs + 2)
        {
            if (args[baseArgs].isProcess())
            {
                *targetBlueprint = args[baseArgs].asProcessId();
            }
            else if (args[baseArgs].isInt())
            {
                *targetBlueprint = args[baseArgs].asInt();
            }
            else
            {
                Error("%s expects process type/int as 1st optional argument", funcName);
                return false;
            }

            if (!value_to_bool(args[baseArgs + 1], ignoreSelf))
            {
                Error("%s expects bool ignoreSelf as 2nd optional argument", funcName);
                return false;
            }
            return true;
        }

        Error("%s received invalid argument count", funcName);
        return false;
    }

    int native_physics_collision(Interpreter *vm, int argCount, Value *args)
    {
        (void)args;
        if (argCount != 0)
        {
            Error("physics_collision expects no arguments");
            vm->pushInt(-1);
            vm->pushInt(-1);
            return 2;
        }

        if (gCollisionEvents.empty())
        {
            vm->pushInt(-1);
            vm->pushInt(-1);
            return 2;
        }

        const uint64_t key = gCollisionEvents.back();
        gCollisionEvents.pop_back();
        const uint32 idA = (uint32)(key >> 32);
        const uint32 idB = (uint32)(key & 0xFFFFFFFFu);
        vm->pushInt((int)idA);
        vm->pushInt((int)idB);
        return 2;
    }

    int native_physics_raycast(Interpreter *vm, int argCount, Value *args)
    {
        // Returns: hitId, hitX, hitY, normalX, normalY
        if (argCount < 4 || argCount > 6)
        {
            Error("physics_raycast expects 4..6 arguments (x1, y1, x2, y2, [type], [ignoreSelf])");
            vm->pushInt(-1);
            vm->pushDouble(0);
            vm->pushDouble(0);
            vm->pushDouble(0);
            vm->pushDouble(0);
            return 5;
        }

        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber() || !args[3].isNumber())
        {
            Error("physics_raycast first 4 arguments must be numbers (x1, y1, x2, y2)");
            vm->pushInt(-1);
            vm->pushDouble(0);
            vm->pushDouble(0);
            vm->pushDouble(0);
            vm->pushDouble(0);
            return 5;
        }

        int targetBlueprint = -1;
        bool ignoreSelf = true;

        if (argCount >= 5)
        {
            if (args[4].isProcess())
            {
                targetBlueprint = args[4].asProcessId();
            }
            else if (args[4].isInt())
            {
                targetBlueprint = args[4].asInt();
            }
            else if (!value_to_bool(args[4], &ignoreSelf))
            {
                Error("physics_raycast 5th argument must be process type/int or bool ignoreSelf");
                vm->pushInt(-1);
                vm->pushDouble(0);
                vm->pushDouble(0);
                vm->pushDouble(0);
                vm->pushDouble(0);
                return 5;
            }
        }

        if (argCount == 6)
        {
            if (!args[4].isProcess() && !args[4].isInt())
            {
                Error("physics_raycast 6-arg form expects process type/int as 5th argument");
                vm->pushInt(-1);
                vm->pushDouble(0);
                vm->pushDouble(0);
                vm->pushDouble(0);
                vm->pushDouble(0);
                return 5;
            }
            if (!value_to_bool(args[5], &ignoreSelf))
            {
                Error("physics_raycast 6th argument must be bool ignoreSelf");
                vm->pushInt(-1);
                vm->pushDouble(0);
                vm->pushDouble(0);
                vm->pushDouble(0);
                vm->pushDouble(0);
                return 5;
            }
        }

        if (!gWorld)
        {
            vm->pushInt(-1);
            vm->pushDouble(0);
            vm->pushDouble(0);
            vm->pushDouble(0);
            vm->pushDouble(0);
            return 5;
        }

        uint32 ignoreProcessId = 0;
        if (ignoreSelf)
        {
            Process *self = vm->getCurrentProcess();
            if (self)
                ignoreProcessId = self->id;
        }

        b2Vec2 p1(pixelToWorld((float)args[0].asNumber()), pixelToWorld((float)args[1].asNumber()));
        b2Vec2 p2(pixelToWorld((float)args[2].asNumber()), pixelToWorld((float)args[3].asNumber()));

        RayCastClosestCallback callback(targetBlueprint, ignoreProcessId, true);
        gWorld->RayCast(&callback, p1, p2);

        if (!callback.hasHit)
        {
            vm->pushInt(-1);
            vm->pushDouble(0);
            vm->pushDouble(0);
            vm->pushDouble(0);
            vm->pushDouble(0);
            return 5;
        }

        vm->pushInt((int)callback.hitProcessId);
        vm->pushDouble(worldToPixel(callback.hitPoint.x));
        vm->pushDouble(worldToPixel(callback.hitPoint.y));
        vm->pushDouble(callback.hitNormal.x);
        vm->pushDouble(callback.hitNormal.y);
        return 5;
    }

    int native_physics_overlap_point(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount < 2 || argCount > 4 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("physics_overlap_point expects 2..4 arguments (x, y, [type], [ignoreSelf])");
            vm->pushInt(-1);
            vm->pushNil();
            return 2;
        }
        if (!gWorld)
        {
            vm->pushInt(-1);
            vm->pushNil();
            return 2;
        }

        int targetBlueprint = -1;
        bool ignoreSelf = true;
        if (!parse_type_and_ignore_self(vm, argCount, args, 2, "physics_overlap_point", &targetBlueprint, &ignoreSelf))
        {
            vm->pushInt(-1);
            vm->pushNil();
            return 2;
        }

        uint32 ignoreProcessId = 0;
        if (ignoreSelf)
        {
            Process *self = vm->getCurrentProcess();
            if (self)
                ignoreProcessId = self->id;
        }

        const b2Vec2 point(pixelToWorld((float)args[0].asNumber()), pixelToWorld((float)args[1].asNumber()));
        OverlapCallback callback(targetBlueprint, ignoreProcessId, true, point);

        b2AABB aabb;
        const float eps = 0.0001f;
        aabb.lowerBound.Set(point.x - eps, point.y - eps);
        aabb.upperBound.Set(point.x + eps, point.y + eps);
        gWorld->QueryAABB(&callback, aabb);

        vm->pushInt(callback.hasHit ? (int)callback.hitProcessId : -1);
        if (callback.hasHit && callback.hitBody)
        {
            push_native_instance(vm, kClassBody, callback.hitBody);
        }
        else
        {
            vm->pushNil();
        }
        return 2;
    }

    int native_physics_overlap_rect(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount < 4 || argCount > 6 ||
            !args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber() || !args[3].isNumber())
        {
            Error("physics_overlap_rect expects 4..6 arguments (x, y, width, height, [type], [ignoreSelf])");
            vm->pushInt(-1);
            return 1;
        }
        if (!gWorld)
        {
            vm->pushInt(-1);
            return 1;
        }

        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        float w = (float)args[2].asNumber();
        float h = (float)args[3].asNumber();
        if (w < 0.0f)
        {
            x += w;
            w = -w;
        }
        if (h < 0.0f)
        {
            y += h;
            h = -h;
        }
        if (w <= 0.0f || h <= 0.0f)
        {
            vm->pushInt(-1);
            return 1;
        }

        int targetBlueprint = -1;
        bool ignoreSelf = true;
        if (!parse_type_and_ignore_self(vm, argCount, args, 4, "physics_overlap_rect", &targetBlueprint, &ignoreSelf))
        {
            vm->pushInt(-1);
            return 1;
        }

        uint32 ignoreProcessId = 0;
        if (ignoreSelf)
        {
            Process *self = vm->getCurrentProcess();
            if (self)
                ignoreProcessId = self->id;
        }

        const float cx = x + w * 0.5f;
        const float cy = y + h * 0.5f;
        const float halfW = w * 0.5f;
        const float halfH = h * 0.5f;

        const b2Vec2 center(pixelToWorld(cx), pixelToWorld(cy));
        const float halfWWorld = pixelToWorld(halfW);
        const float halfHWorld = pixelToWorld(halfH);
        OverlapCallback callback(targetBlueprint, ignoreProcessId, true, center, halfWWorld, halfHWorld);

        b2AABB aabb;
        aabb.lowerBound.Set(center.x - halfWWorld, center.y - halfHWorld);
        aabb.upperBound.Set(center.x + halfWWorld, center.y + halfHWorld);
        gWorld->QueryAABB(&callback, aabb);

        vm->pushInt(callback.hasHit ? (int)callback.hitProcessId : -1);
        return 1;
    }

    int native_physics_overlap_circle(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount < 3 || argCount > 5 ||
            !args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber())
        {
            Error("physics_overlap_circle expects 3..5 arguments (x, y, radius, [type], [ignoreSelf])");
            vm->pushInt(-1);
            return 1;
        }
        if (!gWorld)
        {
            vm->pushInt(-1);
            return 1;
        }

        const float x = (float)args[0].asNumber();
        const float y = (float)args[1].asNumber();
        const float radius = (float)args[2].asNumber();
        if (radius <= 0.0f)
        {
            vm->pushInt(-1);
            return 1;
        }

        int targetBlueprint = -1;
        bool ignoreSelf = true;
        if (!parse_type_and_ignore_self(vm, argCount, args, 3, "physics_overlap_circle", &targetBlueprint, &ignoreSelf))
        {
            vm->pushInt(-1);
            return 1;
        }

        uint32 ignoreProcessId = 0;
        if (ignoreSelf)
        {
            Process *self = vm->getCurrentProcess();
            if (self)
                ignoreProcessId = self->id;
        }

        const b2Vec2 center(pixelToWorld(x), pixelToWorld(y));
        const float radiusWorld = pixelToWorld(radius);
        OverlapCallback callback(targetBlueprint, ignoreProcessId, true, center, radiusWorld);

        b2AABB aabb;
        aabb.lowerBound.Set(center.x - radiusWorld, center.y - radiusWorld);
        aabb.upperBound.Set(center.x + radiusWorld, center.y + radiusWorld);
        gWorld->QueryAABB(&callback, aabb);

        vm->pushInt(callback.hasHit ? (int)callback.hitProcessId : -1);
        return 1;
    }

    int native_update_physics(Interpreter *vm, int argCount, Value *args)
    {
        (void)vm;
        if (argCount < 1 || argCount > 3)
        {
            Error("update_physics expects 1..3 arguments (dt, [velocityIterations], [positionIterations])");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("update_physics first argument must be number (dt)");
            return 0;
        }
        if (!gWorld)
            return 0;

        float timeStep = (float)args[0].asNumber();
        int32 velocityIterations = gVelocityIterations;
        int32 positionIterations = gPositionIterations;
        if (argCount >= 2)
        {
            if (!args[1].isNumber())
            {
                Error("update_physics second argument must be number (velocityIterations)");
                return 0;
            }
            velocityIterations = (int32)args[1].asNumber();
        }
        if (argCount >= 3)
        {
            if (!args[2].isNumber())
            {
                Error("update_physics third argument must be number (positionIterations)");
                return 0;
            }
            positionIterations = (int32)args[2].asNumber();
        }

        gWorld->Step(timeStep, velocityIterations, positionIterations);

        if (jointsScheduledForRemoval.size() > 0)
        {
            for (auto joint : jointsScheduledForRemoval)
            {

                gWorld->DestroyJoint(joint);
            }
            jointsScheduledForRemoval.clear();
        }

        BindingsBox2DJoints::flushPending();

        if (bodysScheduledForRemoval.size() > 0)
        {
            for (auto body : bodysScheduledForRemoval)
            {
                destroy_body_now_or_schedule(body);
            }
            bodysScheduledForRemoval.clear();
        }

        BindingsBox2DJoints::flushPending();

        return 0;
    }

    class QueryCallback : public b2QueryCallback
    {
    public:
        QueryCallback(const b2Vec2 &point)
        {
            m_point = point;
            m_fixture = NULL;
        }

        bool ReportFixture(b2Fixture *fixture) override
        {
            b2Body *body = fixture->GetBody();
            if (body->GetType() == b2_dynamicBody)
            {
                bool inside = fixture->TestPoint(m_point);
                if (inside)
                {
                    m_fixture = fixture;

                    // We are done, terminate the query.
                    return false;
                }
            }

            // Continue the query.
            return true;
        }

        b2Vec2 m_point;
        b2Fixture *m_fixture;
    };

    void *ctor_native_create_bodydef(Interpreter *vm, int argCount, Value *args)
    {
        b2BodyDef *bodyDef = new b2BodyDef();
        if (argCount == 1)
        {
            if (!args[0].isNumber())
            {
                Error("BodyDef expects type as number");
                delete bodyDef;
                return nullptr;
            }
            int type = (int)args[0].asNumber();
            if (type == 0)
                bodyDef->type = b2_dynamicBody;
            else if (type == 1)
                bodyDef->type = b2_staticBody;
            else if (type == 2)
                bodyDef->type = b2_kinematicBody;
        }
        return bodyDef;
    }

    void dtor_native_destroy_bodydef(Interpreter *vm, void *data)
    {
        b2BodyDef *bodyDefPtr = (b2BodyDef *)data;
        delete bodyDefPtr;
    }

    int native_set_bodydef_position(Interpreter *vm, void *data, int argCount, Value *args)
    {
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("set_position expects 2 number arguments (x, y)");
            return 0;
        }
        b2BodyDef *bodyDefPtr = static_cast<b2BodyDef *>(data);
        bodyDefPtr->position.Set(
            pixelToWorld((float)args[0].asNumber()),
            pixelToWorld((float)args[1].asNumber()));
        return 0;
    }

    int native_set_bodydef_linearVelocity(Interpreter *vm, void *data, int argCount, Value *args)
    {
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("set_linear_velocity expects 2 number arguments (x, y)");
            return 0;
        }
        b2BodyDef *bodyDefPtr = static_cast<b2BodyDef *>(data);
        bodyDefPtr->linearVelocity.Set(args[0].asDouble(), args[1].asDouble());
        return 0;
    }

    int native_set_bodydef_type(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_type expects 1 number argument");
            return 0;
        }
        int type = (int)args[0].asNumber();
        b2BodyDef *bodyDefPtr = static_cast<b2BodyDef *>(data);
        if (!bodyDefPtr)
            return 0;
        if (type == BODY_DYNAMIC)
            bodyDefPtr->type = b2_dynamicBody;
        else if (type == BODY_STATIC)
            bodyDefPtr->type = b2_staticBody;
        else if (type == BODY_KINEMATIC)
            bodyDefPtr->type = b2_kinematicBody;
        else
        {
            Error("set_type invalid body type");
            return 0;
        }
        return 0;
    }

    int native_set_bodydef_angle(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_angle expects 1 number argument (degrees)");
            return 0;
        }
        b2BodyDef *bodyDefPtr = static_cast<b2BodyDef *>(data);
        if (!bodyDefPtr)
            return 0;
        bodyDefPtr->angle = degToRad((float)args[0].asNumber());
        return 0;
    }

    int native_set_bodydef_angularVelocity(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_angular_velocity expects 1 number argument");
            return 0;
        }
        b2BodyDef *bodyDefPtr = static_cast<b2BodyDef *>(data);
        if (!bodyDefPtr)
            return 0;
        bodyDefPtr->angularVelocity = (float)args[0].asNumber();
        return 0;
    }

    int native_set_bodydef_linearDamping(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_linear_damping expects 1 number argument");
            return 0;
        }
        b2BodyDef *bodyDefPtr = static_cast<b2BodyDef *>(data);
        if (!bodyDefPtr)
            return 0;
        bodyDefPtr->linearDamping = (float)args[0].asNumber();
        return 0;
    }

    int native_set_bodydef_angularDamping(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_angular_damping expects 1 number argument");
            return 0;
        }
        b2BodyDef *bodyDefPtr = static_cast<b2BodyDef *>(data);
        if (!bodyDefPtr)
            return 0;
        bodyDefPtr->angularDamping = (float)args[0].asNumber();
        return 0;
    }

    int native_set_bodydef_gravityScale(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_gravity_scale expects 1 number argument");
            return 0;
        }
        b2BodyDef *bodyDefPtr = static_cast<b2BodyDef *>(data);
        if (!bodyDefPtr)
            return 0;
        bodyDefPtr->gravityScale = (float)args[0].asNumber();
        return 0;
    }

    int native_set_bodydef_allowSleep(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        bool enabled = false;
        if (argCount != 1 || !value_to_bool(args[0], &enabled))
        {
            Error("set_allow_sleep expects 1 bool argument");
            return 0;
        }
        b2BodyDef *bodyDefPtr = static_cast<b2BodyDef *>(data);
        if (!bodyDefPtr)
            return 0;
        bodyDefPtr->allowSleep = enabled;
        return 0;
    }

    int native_set_bodydef_awake(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        bool enabled = false;
        if (argCount != 1 || !value_to_bool(args[0], &enabled))
        {
            Error("set_awake expects 1 bool argument");
            return 0;
        }
        b2BodyDef *bodyDefPtr = static_cast<b2BodyDef *>(data);
        if (!bodyDefPtr)
            return 0;
        bodyDefPtr->awake = enabled;
        return 0;
    }

    int native_set_bodydef_fixedRotation(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        bool enabled = false;
        if (argCount != 1 || !value_to_bool(args[0], &enabled))
        {
            Error("set_fixed_rotation expects 1 bool argument");
            return 0;
        }
        b2BodyDef *bodyDefPtr = static_cast<b2BodyDef *>(data);
        if (!bodyDefPtr)
            return 0;
        bodyDefPtr->fixedRotation = enabled;
        return 0;
    }

    int native_set_bodydef_bullet(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        bool enabled = false;
        if (argCount != 1 || !value_to_bool(args[0], &enabled))
        {
            Error("set_bullet expects 1 bool argument");
            return 0;
        }
        b2BodyDef *bodyDefPtr = static_cast<b2BodyDef *>(data);
        if (!bodyDefPtr)
            return 0;
        bodyDefPtr->bullet = enabled;
        return 0;
    }

    int native_set_bodydef_enabled(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        bool enabled = false;
        if (argCount != 1 || !value_to_bool(args[0], &enabled))
        {
            Error("set_enabled expects 1 bool argument");
            return 0;
        }
        b2BodyDef *bodyDefPtr = static_cast<b2BodyDef *>(data);
        if (!bodyDefPtr)
            return 0;
        bodyDefPtr->enabled = enabled;
        return 0;
    }

    void *ctor_native_create_fixture_def(Interpreter *vm, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 0)
        {
            Error("FixtureDef expects no arguments");
            return nullptr;
        }
        FixtureDefHandle *fixture = new FixtureDefHandle();
        return fixture;
    }

    void dtor_native_destroy_fixture_def(Interpreter *vm, void *data)
    {
        (void)vm;
        FixtureDefHandle *fixture = static_cast<FixtureDefHandle *>(data);
        delete fixture;
    }

    int native_fixture_def_set_density(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_density expects 1 number argument");
            return 0;
        }
        FixtureDefHandle *fixture = as_fixture_def_handle(data, "set_density");
        if (!fixture)
            return 0;
        fixture->fixture.density = (float)args[0].asNumber();
        return 0;
    }

    int native_fixture_def_set_friction(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_friction expects 1 number argument");
            return 0;
        }
        FixtureDefHandle *fixture = as_fixture_def_handle(data, "set_friction");
        if (!fixture)
            return 0;
        fixture->fixture.friction = (float)args[0].asNumber();
        return 0;
    }

    int native_fixture_def_set_restitution(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_restitution expects 1 number argument");
            return 0;
        }
        FixtureDefHandle *fixture = as_fixture_def_handle(data, "set_restitution");
        if (!fixture)
            return 0;
        fixture->fixture.restitution = (float)args[0].asNumber();
        return 0;
    }

    int native_fixture_def_set_sensor(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        bool sensor = false;
        if (argCount != 1 || !value_to_bool(args[0], &sensor))
        {
            Error("set_sensor expects 1 bool argument");
            return 0;
        }
        FixtureDefHandle *fixture = as_fixture_def_handle(data, "set_sensor");
        if (!fixture)
            return 0;
        fixture->fixture.isSensor = sensor;
        return 0;
    }

    int native_fixture_def_set_filter(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 3 || !args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber())
        {
            Error("set_filter expects 3 number arguments (categoryBits, maskBits, groupIndex)");
            return 0;
        }
        FixtureDefHandle *fixture = as_fixture_def_handle(data, "set_filter");
        if (!fixture)
            return 0;
        fixture->fixture.filter.categoryBits = (uint16)args[0].asNumber();
        fixture->fixture.filter.maskBits = (uint16)args[1].asNumber();
        fixture->fixture.filter.groupIndex = (int16)args[2].asNumber();
        return 0;
    }

    int native_fixture_def_set_category_bits(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_category_bits expects 1 number argument (bits)");
            return 0;
        }
        FixtureDefHandle *fixture = as_fixture_def_handle(data, "set_category_bits");
        if (!fixture)
            return 0;
        fixture->fixture.filter.categoryBits = (uint16)args[0].asNumber();
        return 0;
    }

    int native_fixture_def_set_mask_bits(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_mask_bits expects 1 number argument (bits)");
            return 0;
        }
        FixtureDefHandle *fixture = as_fixture_def_handle(data, "set_mask_bits");
        if (!fixture)
            return 0;
        fixture->fixture.filter.maskBits = (uint16)args[0].asNumber();
        return 0;
    }

    int native_fixture_def_add_mask_bits(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("add_mask_bits expects 1 number argument (bits)");
            return 0;
        }
        FixtureDefHandle *fixture = as_fixture_def_handle(data, "add_mask_bits");
        if (!fixture)
            return 0;
        fixture->fixture.filter.maskBits = (uint16)(fixture->fixture.filter.maskBits | (uint16)args[0].asNumber());
        return 0;
    }

    int native_fixture_def_remove_mask_bits(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("remove_mask_bits expects 1 number argument (bits)");
            return 0;
        }
        FixtureDefHandle *fixture = as_fixture_def_handle(data, "remove_mask_bits");
        if (!fixture)
            return 0;
        fixture->fixture.filter.maskBits = (uint16)(fixture->fixture.filter.maskBits & ~(uint16)args[0].asNumber());
        return 0;
    }

    int native_fixture_def_set_group_index(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_group_index expects 1 number argument (group)");
            return 0;
        }
        FixtureDefHandle *fixture = as_fixture_def_handle(data, "set_group_index");
        if (!fixture)
            return 0;
        fixture->fixture.filter.groupIndex = (int16)args[0].asNumber();
        return 0;
    }

    int native_fixture_def_set_circle_shape(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 && argCount != 3)
        {
            Error("set_circle_shape expects 1 or 3 number arguments (radius[, centerX, centerY])");
            return 0;
        }
        if (!args[0].isNumber() || (argCount == 3 && (!args[1].isNumber() || !args[2].isNumber())))
        {
            Error("set_circle_shape expects numeric arguments");
            return 0;
        }

        const float radiusPx = (float)args[0].asNumber();
        if (radiusPx <= 0.0f)
        {
            Error("set_circle_shape radius must be > 0");
            return 0;
        }
        float centerX = 0.0f;
        float centerY = 0.0f;
        if (argCount == 3)
        {
            centerX = (float)args[1].asNumber();
            centerY = (float)args[2].asNumber();
        }

        FixtureDefHandle *fixture = as_fixture_def_handle(data, "set_circle_shape");
        if (!fixture)
            return 0;
        fixture->setCircleShape(
            pixelToWorld(radiusPx),
            pixelToWorld(centerX),
            pixelToWorld(centerY));
        return 0;
    }

    int native_fixture_def_set_box_shape(Interpreter *vm, void *data, int argCount, Value *args)
    {
        if (argCount != 2 && argCount != 5)
        {
            Error("set_box_shape expects 2 or 5 number arguments");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("set_box_shape expects number arguments");
            return 0;
        }

        float halfWPx = (float)args[0].asNumber();
        float halfHPx = (float)args[1].asNumber();
        if (halfWPx <= 0.0f || halfHPx <= 0.0f)
        {
            Error("set_box_shape halfW/halfH must be > 0");
            return 0;
        }

        float centerX = 0.0f;
        float centerY = 0.0f;
        float angleRad = 0.0f;

        if (argCount == 5)
        {
            if (!args[2].isNumber() || !args[3].isNumber() || !args[4].isNumber())
            {
                Error("set_box_shape expects number arguments for centerX, centerY and angleDegrees");
                return 0;
            }
            centerX = (float)args[2].asNumber();
            centerY = (float)args[3].asNumber();
            angleRad = degToRad((float)args[4].asNumber());
        }

        FixtureDefHandle *fixture = as_fixture_def_handle(data, "set_box_shape");
        if (!fixture)
            return 0;
        fixture->setBoxShape(
            pixelToWorld(halfWPx),
            pixelToWorld(halfHPx),
            pixelToWorld(centerX),
            pixelToWorld(centerY),
            angleRad);
        return 0;
    }

    int native_fixture_def_set_edge_shape(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 4 || !args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber() || !args[3].isNumber())
        {
            Error("set_edge_shape expects 4 number arguments (x1, y1, x2, y2)");
            return 0;
        }
        FixtureDefHandle *fixture = as_fixture_def_handle(data, "set_edge_shape");
        if (!fixture)
            return 0;
        fixture->setEdgeShape(
            pixelToWorld((float)args[0].asNumber()),
            pixelToWorld((float)args[1].asNumber()),
            pixelToWorld((float)args[2].asNumber()),
            pixelToWorld((float)args[3].asNumber()));
        return 0;
    }

    int native_fixture_def_set_chain_shape(Interpreter *vm, void *data, int argCount, Value *args)
    {
        bool loop = false;
        if (argCount < 1 || argCount > 2)
        {
            Error("set_chain_shape expects 1 or 2 arguments (points, [loop])");
            return 0;
        }
        if (argCount == 2 && !value_to_bool(args[1], &loop))
        {
            Error("set_chain_shape second argument must be bool");
            return 0;
        }

        std::vector<b2Vec2> points;
        if (!parse_shape_points(vm, args[0], &points, "set_chain_shape", loop ? 3 : 2))
            return 0;
        for (b2Vec2 &p : points)
        {
            p.x = pixelToWorld(p.x);
            p.y = pixelToWorld(p.y);
        }

        FixtureDefHandle *fixture = as_fixture_def_handle(data, "set_chain_shape");
        if (!fixture)
            return 0;
        fixture->setChainShape(points, loop);
        return 0;
    }

    int native_fixture_def_set_polygon_shape(Interpreter *vm, void *data, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("set_polygon_shape expects 1 argument (points array)");
            return 0;
        }
        FixtureDefHandle *fixture = as_fixture_def_handle(data, "set_polygon_shape");
        if (!fixture)
            return 0;

        std::vector<b2Vec2> points;
        if (!parse_polygon_points(vm, args[0], &points, "set_polygon_shape"))
            return 0;
        for (b2Vec2 &p : points)
        {
            p.x = pixelToWorld(p.x);
            p.y = pixelToWorld(p.y);
        }
        if (points.size() > (size_t)b2_maxPolygonVertices)
        {
            Error("set_polygon_shape max vertices is %d", b2_maxPolygonVertices);
            return 0;
        }
        if (!is_polygon_convex(points))
        {
            Error("set_polygon_shape expects convex points (for concave use Body.add_polygon)");
            return 0;
        }

        auto *poly = new b2PolygonShape();
        if (!poly->Set(points.data(), (int32)points.size()))
        {
            delete poly;
            Error("set_polygon_shape expects a valid convex polygon");
            return 0;
        }

        fixture->clearShape();
        fixture->ownedShape = poly;
        fixture->fixture.shape = poly;
        return 0;
    }

    int native_fixture_def_clear_shape(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        (void)args;
        if (argCount != 0)
        {
            Error("clear_shape expects no arguments");
            return 0;
        }
        FixtureDefHandle *fixture = as_fixture_def_handle(data, "clear_shape");
        if (!fixture)
            return 0;
        fixture->clearShape();
        return 0;
    }

    void *ctor_native_create_fixture(Interpreter *vm, int argCount, Value *args)
    {
        (void)vm;
        (void)argCount;
        (void)args;
        Error("Fixture cannot be created directly. Use Body.add_fixture().");
        return nullptr;
    }

    void dtor_native_destroy_fixture(Interpreter *vm, void *data)
    {
        (void)vm;
        (void)data;
    }

    static bool push_fixture_instance(Interpreter *vm, b2Fixture *fixture, b2Body *body)
    {
        if (!fixture || !body)
        {
            vm->pushNil();
            return false;
        }
        if (!push_native_instance(vm, kClassFixture, fixture))
        {
            return false;
        }
        return true;
    }

    int native_fixture_set_sensor(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        bool sensor = false;
        if (argCount != 1 || !value_to_bool(args[0], &sensor))
        {
            Error("set_sensor expects 1 bool argument");
            return 0;
        }
        b2Fixture *fixture = as_fixture_handle(data, "set_sensor");
        if (!fixture)
            return 0;
        fixture->SetSensor(sensor);
        return 0;
    }

    int native_fixture_set_filter(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 3 || !args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber())
        {
            Error("set_filter expects 3 number arguments (categoryBits, maskBits, groupIndex)");
            return 0;
        }
        b2Fixture *fixture = as_fixture_handle(data, "set_filter");
        if (!fixture)
            return 0;
        b2Filter filter = fixture->GetFilterData();
        filter.categoryBits = (uint16)args[0].asNumber();
        filter.maskBits = (uint16)args[1].asNumber();
        filter.groupIndex = (int16)args[2].asNumber();
        fixture->SetFilterData(filter);
        return 0;
    }

    int native_fixture_set_category_bits(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_category_bits expects 1 number argument (bits)");
            return 0;
        }
        b2Fixture *fixture = as_fixture_handle(data, "set_category_bits");
        if (!fixture)
            return 0;
        b2Filter filter = fixture->GetFilterData();
        filter.categoryBits = (uint16)args[0].asNumber();
        fixture->SetFilterData(filter);
        return 0;
    }

    int native_fixture_set_mask_bits(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_mask_bits expects 1 number argument (bits)");
            return 0;
        }
        b2Fixture *fixture = as_fixture_handle(data, "set_mask_bits");
        if (!fixture)
            return 0;
        b2Filter filter = fixture->GetFilterData();
        filter.maskBits = (uint16)args[0].asNumber();
        fixture->SetFilterData(filter);
        return 0;
    }

    int native_fixture_add_mask_bits(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("add_mask_bits expects 1 number argument (bits)");
            return 0;
        }
        b2Fixture *fixture = as_fixture_handle(data, "add_mask_bits");
        if (!fixture)
            return 0;
        b2Filter filter = fixture->GetFilterData();
        filter.maskBits = (uint16)(filter.maskBits | (uint16)args[0].asNumber());
        fixture->SetFilterData(filter);
        return 0;
    }

    int native_fixture_remove_mask_bits(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("remove_mask_bits expects 1 number argument (bits)");
            return 0;
        }
        b2Fixture *fixture = as_fixture_handle(data, "remove_mask_bits");
        if (!fixture)
            return 0;
        b2Filter filter = fixture->GetFilterData();
        filter.maskBits = (uint16)(filter.maskBits & ~(uint16)args[0].asNumber());
        fixture->SetFilterData(filter);
        return 0;
    }

    int native_fixture_set_group_index(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_group_index expects 1 number argument (group)");
            return 0;
        }
        b2Fixture *fixture = as_fixture_handle(data, "set_group_index");
        if (!fixture)
            return 0;
        b2Filter filter = fixture->GetFilterData();
        filter.groupIndex = (int16)args[0].asNumber();
        fixture->SetFilterData(filter);
        return 0;
    }

    static void body_apply_filter(b2Body *body, const b2Filter &filter, bool setCategory, bool setMask, bool setGroup)
    {
        if (!body)
            return;
        for (b2Fixture *fx = body->GetFixtureList(); fx; fx = fx->GetNext())
        {
            b2Filter current = fx->GetFilterData();
            if (setCategory)
                current.categoryBits = filter.categoryBits;
            if (setMask)
                current.maskBits = filter.maskBits;
            if (setGroup)
                current.groupIndex = filter.groupIndex;
            fx->SetFilterData(current);
        }
    }

    static b2Body *as_body_handle(void *data, const char *funcName)
    {
        b2Body *body = static_cast<b2Body *>(data);
        if (!body)
        {
            Error("%s invalid body", funcName);
            return nullptr;
        }
        return body;
    }

    static FixtureDefHandle *as_fixture_def_handle(void *data, const char *funcName)
    {
        FixtureDefHandle *handle = static_cast<FixtureDefHandle *>(data);
        if (!handle)
        {
            Error("%s invalid fixture def", funcName);
            return nullptr;
        }
        return handle;
    }

    static b2Fixture *as_fixture_handle(void *data, const char *funcName)
    {
        b2Fixture *fixture = static_cast<b2Fixture *>(data);
        if (!fixture)
        {
            Error("%s invalid fixture", funcName);
            return nullptr;
        }
        return fixture;
    }

    static void *create_body_handle(Interpreter *vm, Process *ownerProc, int argCount, Value *args)
    {
        if (!gWorld)
        {
            Error("create_body requires a physics world. Call create_physics/create_world first.");
            return nullptr;
        }
        if (argCount != 1)
        {
            Error("Body expects 1 BodyDef argument");
            return nullptr;
        }
        NativeClassInstance *instance = require_native_instance(vm, args[0], kClassBodyDef);
        if (!instance)
            return nullptr;
        b2BodyDef *bodyDef = static_cast<b2BodyDef *>(instance->userData);
        b2Body *body = gWorld->CreateBody(bodyDef);
        if (!body)
            return nullptr;
        body->GetUserData().pointer = ownerProc ? (uintptr_t)ownerProc->id : 0u;
        if (ownerProc)
            set_process_type(ownerProc->id, ownerProc->blueprint);
        return body;
    }

    void *ctor_native_create_body(Interpreter *vm, int argCount, Value *args)
    {
        // NativeClass constructor path has no process context.
        return create_body_handle(vm, nullptr, argCount, args);
    }

    void dtor_native_destroy_body(Interpreter *vm, void *data)
    {
        (void)vm;
        (void)data;
    }

    int native_body_remove(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        (void)args;
        if (argCount != 0)
        {
            Error("remove expects no arguments");
            return 0;
        }
        b2Body *body = as_body_handle(data, "remove");
        if (!body)
            return 0;
        if (gWorld)
        {
            destroy_body_now_or_schedule(body);
        }
        return 0;
    }

    int native_body_set_transform(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 3 || !args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber())
        {
            Error("set_transform expects 3 number arguments (x, y, angle_degrees)");
            return 0;
        }
        b2Body *body = as_body_handle(data, "set_transform");
        if (!body)
            return 0;
        body->SetTransform(
            b2Vec2(
                pixelToWorld((float)args[0].asNumber()),
                pixelToWorld((float)args[1].asNumber())),
            degToRad((float)args[2].asNumber()));
        return 0;
    }

    int native_body_get_position(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        if (argCount != 0)
        {
            Error("get_position expects no arguments");
            vm->pushDouble(0);
            vm->pushDouble(0);
            return 2;
        }
        b2Body *body = as_body_handle(data, "get_position");
        if (!body)
        {
            vm->pushDouble(0);
            vm->pushDouble(0);
            return 2;
        }
        b2Vec2 p = body->GetPosition();
        vm->pushDouble(worldToPixel(p.x));
        vm->pushDouble(worldToPixel(p.y));
        return 2;
    }

    int native_body_set_linear_velocity(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("set_linear_velocity expects 2 number arguments (x, y)");
            return 0;
        }
        b2Body *body = as_body_handle(data, "set_linear_velocity");
        if (!body)
            return 0;
        body->SetLinearVelocity(b2Vec2((float)args[0].asNumber(), (float)args[1].asNumber()));
        return 0;
    }

    int native_body_get_linear_velocity(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        if (argCount != 0)
        {
            Error("get_linear_velocity expects no arguments");
            vm->pushDouble(0);
            vm->pushDouble(0);
            return 2;
        }
        b2Body *body = as_body_handle(data, "get_linear_velocity");
        if (!body)
        {
            vm->pushDouble(0);
            vm->pushDouble(0);
            return 2;
        }
        b2Vec2 v = body->GetLinearVelocity();
        vm->pushDouble(v.x);
        vm->pushDouble(v.y);
        return 2;
    }

    int native_body_set_angular_velocity(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_angular_velocity expects 1 number argument");
            return 0;
        }
        b2Body *body = as_body_handle(data, "set_angular_velocity");
        if (!body)
            return 0;
        body->SetAngularVelocity((float)args[0].asNumber());
        return 0;
    }

    int native_body_get_angular_velocity(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        if (argCount != 0)
        {
            Error("get_angular_velocity expects no arguments");
            vm->pushDouble(0);
            return 1;
        }
        b2Body *body = as_body_handle(data, "get_angular_velocity");
        if (!body)
        {
            vm->pushDouble(0);
            return 1;
        }
        vm->pushDouble(body->GetAngularVelocity());
        return 1;
    }

    int native_body_apply_force(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("apply_force expects 2 number arguments (x, y)");
            return 0;
        }
        b2Body *body = as_body_handle(data, "apply_force");
        if (!body)
            return 0;
        body->ApplyForceToCenter(b2Vec2((float)args[0].asNumber(), (float)args[1].asNumber()), true);
        return 0;
    }

    int native_body_apply_impulse(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("apply_impulse expects 2 number arguments (x, y)");
            return 0;
        }
        b2Body *body = as_body_handle(data, "apply_impulse");
        if (!body)
            return 0;
        body->ApplyLinearImpulseToCenter(b2Vec2((float)args[0].asNumber(), (float)args[1].asNumber()), true);
        return 0;
    }

    int native_body_set_gravity_scale(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_gravity_scale expects 1 number argument");
            return 0;
        }
        b2Body *body = as_body_handle(data, "set_gravity_scale");
        if (!body)
            return 0;
        body->SetGravityScale((float)args[0].asNumber());
        return 0;
    }

    int native_body_get_gravity_scale(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        if (argCount != 0)
        {
            Error("get_gravity_scale expects no arguments");
            vm->pushDouble(1.0);
            return 1;
        }
        b2Body *body = as_body_handle(data, "get_gravity_scale");
        if (!body)
        {
            vm->pushDouble(1.0);
            return 1;
        }
        vm->pushDouble(body->GetGravityScale());
        return 1;
    }

    int native_body_set_awake(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        bool enabled = false;
        if (argCount != 1 || !value_to_bool(args[0], &enabled))
        {
            Error("set_awake expects 1 bool argument");
            return 0;
        }
        b2Body *body = as_body_handle(data, "set_awake");
        if (!body)
            return 0;
        body->SetAwake(enabled);
        return 0;
    }

    int native_body_is_awake(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        if (argCount != 0)
        {
            Error("is_awake expects no arguments");
            vm->pushBool(false);
            return 1;
        }
        b2Body *body = as_body_handle(data, "is_awake");
        if (!body)
        {
            vm->pushBool(false);
            return 1;
        }
        vm->pushBool(body->IsAwake());
        return 1;
    }

    int native_body_set_fixed_rotation(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        bool enabled = false;
        if (argCount != 1 || !value_to_bool(args[0], &enabled))
        {
            Error("set_fixed_rotation expects 1 bool argument");
            return 0;
        }
        b2Body *body = as_body_handle(data, "set_fixed_rotation");
        if (!body)
            return 0;
        body->SetFixedRotation(enabled);
        return 0;
    }

    int native_body_is_fixed_rotation(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        if (argCount != 0)
        {
            Error("is_fixed_rotation expects no arguments");
            vm->pushBool(false);
            return 1;
        }
        b2Body *body = as_body_handle(data, "is_fixed_rotation");
        if (!body)
        {
            vm->pushBool(false);
            return 1;
        }
        vm->pushBool(body->IsFixedRotation());
        return 1;
    }

    int native_body_set_bullet(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        bool enabled = false;
        if (argCount != 1 || !value_to_bool(args[0], &enabled))
        {
            Error("set_bullet expects 1 bool argument");
            return 0;
        }
        b2Body *body = as_body_handle(data, "set_bullet");
        if (!body)
            return 0;
        body->SetBullet(enabled);
        return 0;
    }

    int native_body_is_bullet(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        if (argCount != 0)
        {
            Error("is_bullet expects no arguments");
            vm->pushBool(false);
            return 1;
        }
        b2Body *body = as_body_handle(data, "is_bullet");
        if (!body)
        {
            vm->pushBool(false);
            return 1;
        }
        vm->pushBool(body->IsBullet());
        return 1;
    }

    int native_body_get_mass(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        if (argCount != 0)
        {
            Error("get_mass expects no arguments");
            vm->pushDouble(0);
            return 1;
        }
        b2Body *body = as_body_handle(data, "get_mass");
        if (!body)
        {
            vm->pushDouble(0);
            return 1;
        }
        vm->pushDouble(body->GetMass());
        return 1;
    }

    int native_body_get_inertia(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        if (argCount != 0)
        {
            Error("get_inertia expects no arguments");
            vm->pushDouble(0);
            return 1;
        }
        b2Body *body = as_body_handle(data, "get_inertia");
        if (!body)
        {
            vm->pushDouble(0);
            return 1;
        }
        vm->pushDouble(body->GetInertia());
        return 1;
    }

    int native_body_get_angle(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        if (argCount != 0)
        {
            Error("get_angle expects no arguments");
            vm->pushDouble(0);
            return 1;
        }
        b2Body *body = as_body_handle(data, "get_angle");
        if (!body)
        {
            vm->pushDouble(0);
            return 1;
        }
        vm->pushDouble(radToDeg(body->GetAngle()));
        return 1;
    }

    int native_body_set_angle(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_angle expects 1 number argument (degrees)");
            return 0;
        }
        b2Body *body = as_body_handle(data, "set_angle");
        if (!body)
            return 0;
        b2Vec2 p = body->GetPosition();
        body->SetTransform(p, degToRad((float)args[0].asNumber()));
        return 0;
    }

    int native_body_get_type(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        if (argCount != 0)
        {
            Error("get_type expects no arguments");
            vm->pushInt(BODY_STATIC);
            return 1;
        }
        b2Body *body = as_body_handle(data, "get_type");
        if (!body)
        {
            vm->pushInt(BODY_STATIC);
            return 1;
        }
        int type = BODY_STATIC;
        switch (body->GetType())
        {
        case b2_dynamicBody:
            type = BODY_DYNAMIC;
            break;
        case b2_staticBody:
            type = BODY_STATIC;
            break;
        case b2_kinematicBody:
            type = BODY_KINEMATIC;
            break;
        default:
            break;
        }
        vm->pushInt(type);
        return 1;
    }

    int native_body_set_filter(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 3 || !args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber())
        {
            Error("set_filter expects 3 number arguments (categoryBits, maskBits, groupIndex)");
            return 0;
        }
        b2Body *body = as_body_handle(data, "set_filter");
        if (!body)
            return 0;
        b2Filter filter{};
        filter.categoryBits = (uint16)args[0].asNumber();
        filter.maskBits = (uint16)args[1].asNumber();
        filter.groupIndex = (int16)args[2].asNumber();
        body_apply_filter(body, filter, true, true, true);
        return 0;
    }

    int native_body_set_category_bits(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_category_bits expects 1 number argument (bits)");
            return 0;
        }
        b2Body *body = as_body_handle(data, "set_category_bits");
        if (!body)
            return 0;
        b2Filter filter{};
        filter.categoryBits = (uint16)args[0].asNumber();
        body_apply_filter(body, filter, true, false, false);
        return 0;
    }

    int native_body_set_mask_bits(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_mask_bits expects 1 number argument (bits)");
            return 0;
        }
        b2Body *body = as_body_handle(data, "set_mask_bits");
        if (!body)
            return 0;
        b2Filter filter{};
        filter.maskBits = (uint16)args[0].asNumber();
        body_apply_filter(body, filter, false, true, false);
        return 0;
    }

    int native_body_add_mask_bits(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("add_mask_bits expects 1 number argument (bits)");
            return 0;
        }
        b2Body *body = as_body_handle(data, "add_mask_bits");
        if (!body)
            return 0;
        const uint16 bits = (uint16)args[0].asNumber();
        for (b2Fixture *fx = body->GetFixtureList(); fx; fx = fx->GetNext())
        {
            b2Filter filter = fx->GetFilterData();
            filter.maskBits = (uint16)(filter.maskBits | bits);
            fx->SetFilterData(filter);
        }
        return 0;
    }

    int native_body_remove_mask_bits(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("remove_mask_bits expects 1 number argument (bits)");
            return 0;
        }
        b2Body *body = as_body_handle(data, "remove_mask_bits");
        if (!body)
            return 0;
        const uint16 bits = (uint16)args[0].asNumber();
        for (b2Fixture *fx = body->GetFixtureList(); fx; fx = fx->GetNext())
        {
            b2Filter filter = fx->GetFilterData();
            filter.maskBits = (uint16)(filter.maskBits & ~bits);
            fx->SetFilterData(filter);
        }
        return 0;
    }

    int native_body_set_group_index(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("set_group_index expects 1 number argument (group)");
            return 0;
        }
        b2Body *body = as_body_handle(data, "set_group_index");
        if (!body)
            return 0;
        b2Filter filter{};
        filter.groupIndex = (int16)args[0].asNumber();
        body_apply_filter(body, filter, false, false, true);
        return 0;
    }

    int native_body_add_box(Interpreter *vm, void *data, int argCount, Value *args)
    {
        if ((argCount != 2 && argCount != 3) || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("add_box expects 2 or 3 args (halfW, halfH, [FixtureDef])");
            return 0;
        }
        b2Body *body = as_body_handle(data, "add_box");
        if (!body)
            return 0;
        float halfWPx = (float)args[0].asNumber();
        float halfHPx = (float)args[1].asNumber();
        if (halfWPx <= 0.0f || halfHPx <= 0.0f)
        {
            Error("add_box halfW/halfH must be > 0");
            return 0;
        }

        b2FixtureDef fixture;
        if (argCount == 3)
        {
            NativeClassInstance *fixtureInstance = require_native_instance(vm, args[2], kClassFixtureDef);
            if (!fixtureInstance)
                return 0;
            FixtureDefHandle *fixtureHandle = static_cast<FixtureDefHandle *>(fixtureInstance->userData);
            fixture = fixtureHandle->fixture;
        }
        else
        {
            fixture.density = 1.0f;
            fixture.friction = 0.3f;
            fixture.restitution = 0.0f;
            fixture.isSensor = false;
        }

        b2PolygonShape shape;
        shape.SetAsBox(pixelToWorld(halfWPx), pixelToWorld(halfHPx));

        fixture.shape = &shape;
        body->CreateFixture(&fixture);
        return 0;
    }

    int native_body_add_circle(Interpreter *vm, void *data, int argCount, Value *args)
    {
        if ((argCount != 1 && argCount != 2) || !args[0].isNumber())
        {
            Error("add_circle expects 1 or 2 args (radius, [FixtureDef])");
            return 0;
        }
        b2Body *body = as_body_handle(data, "add_circle");
        if (!body)
            return 0;
        float radiusPx = (float)args[0].asNumber();
        if (radiusPx <= 0.0f)
        {
            Error("add_circle radius must be > 0");
            return 0;
        }

        b2FixtureDef fixture;
        if (argCount == 2)
        {
            NativeClassInstance *fixtureInstance = require_native_instance(vm, args[1], kClassFixtureDef);
            if (!fixtureInstance)
                return 0;
            FixtureDefHandle *fixtureHandle = static_cast<FixtureDefHandle *>(fixtureInstance->userData);
            fixture = fixtureHandle->fixture;
        }
        else
        {
            fixture.density = 1.0f;
            fixture.friction = 0.3f;
            fixture.restitution = 0.0f;
            fixture.isSensor = false;
        }

        b2CircleShape shape;
        shape.m_radius = pixelToWorld(radiusPx);

        fixture.shape = &shape;
        body->CreateFixture(&fixture);
        return 0;
    }

    int native_body_add_edge(Interpreter *vm, void *data, int argCount, Value *args)
    {
        if ((argCount != 4 && argCount != 5) || !args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber() || !args[3].isNumber())
        {
            Error("add_edge expects 4 or 5 args (x1, y1, x2, y2, [FixtureDef])");
            return 0;
        }

        b2Body *body = as_body_handle(data, "add_edge");
        if (!body)
            return 0;

        b2FixtureDef fixture;
        if (argCount == 5)
        {
            NativeClassInstance *fixtureInstance = require_native_instance(vm, args[4], kClassFixtureDef);
            if (!fixtureInstance)
                return 0;
            FixtureDefHandle *fixtureHandle = static_cast<FixtureDefHandle *>(fixtureInstance->userData);
            fixture = fixtureHandle->fixture;
        }
        else
        {
            fixture.density = 1.0f;
            fixture.friction = 0.3f;
            fixture.restitution = 0.0f;
            fixture.isSensor = false;
        }

        b2EdgeShape shape;
        shape.SetTwoSided(
            b2Vec2(pixelToWorld((float)args[0].asNumber()), pixelToWorld((float)args[1].asNumber())),
            b2Vec2(pixelToWorld((float)args[2].asNumber()), pixelToWorld((float)args[3].asNumber())));
        fixture.shape = &shape;
        body->CreateFixture(&fixture);
        return 0;
    }

    int native_body_add_chain(Interpreter *vm, void *data, int argCount, Value *args)
    {
        if (argCount < 1 || argCount > 3)
        {
            Error("add_chain expects (points), (points, loop), (points, FixtureDef), or (points, loop, FixtureDef)");
            return 0;
        }

        b2Body *body = as_body_handle(data, "add_chain");
        if (!body)
            return 0;

        bool loop = false;
        FixtureDefHandle *fixtureHandle = nullptr;

        if (argCount >= 2)
        {
            if (args[1].isNativeClassInstance())
            {
                if (argCount == 3)
                {
                    Error("add_chain 3-arg form must be (points, loop, FixtureDef)");
                    return 0;
                }
                NativeClassInstance *fixtureInstance = require_native_instance(vm, args[1], kClassFixtureDef);
                if (!fixtureInstance)
                    return 0;
                fixtureHandle = static_cast<FixtureDefHandle *>(fixtureInstance->userData);
            }
            else if (!value_to_bool(args[1], &loop))
            {
                Error("add_chain second argument must be bool or FixtureDef");
                return 0;
            }
        }

        if (argCount == 3)
        {
            if (!value_to_bool(args[1], &loop))
            {
                Error("add_chain 3-arg form expects bool loop as second argument");
                return 0;
            }
            NativeClassInstance *fixtureInstance = require_native_instance(vm, args[2], kClassFixtureDef);
            if (!fixtureInstance)
            {
                Error("add_chain 3-arg form expects FixtureDef as third argument");
                return 0;
            }
            fixtureHandle = static_cast<FixtureDefHandle *>(fixtureInstance->userData);
        }

        std::vector<b2Vec2> points;
        if (!parse_shape_points(vm, args[0], &points, "add_chain", loop ? 3 : 2))
            return 0;
        for (b2Vec2 &p : points)
        {
            p.x = pixelToWorld(p.x);
            p.y = pixelToWorld(p.y);
        }

        b2FixtureDef fixture;
        if (fixtureHandle)
        {
            fixture = fixtureHandle->fixture;
        }
        else
        {
            fixture.density = 1.0f;
            fixture.friction = 0.3f;
            fixture.restitution = 0.0f;
            fixture.isSensor = false;
        }

        b2ChainShape shape;
        if (loop)
            shape.CreateLoop(points.data(), (int32)points.size());
        else
            shape.CreateChain(points.data(), (int32)points.size(), points[0], points.back());

        fixture.shape = &shape;
        body->CreateFixture(&fixture);
        return 0;
    }

    int native_body_add_fixture(Interpreter *vm, void *data, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("add_fixture expects 1 FixtureDef argument");
            return 0;
        }

        b2Body *body = as_body_handle(data, "add_fixture");
        if (!body)
            return 0;

        NativeClassInstance *fixtureInstance = require_native_instance(vm, args[0], kClassFixtureDef);
        if (!fixtureInstance)
            return 0;
        FixtureDefHandle *fixture = static_cast<FixtureDefHandle *>(fixtureInstance->userData);
        if (!fixture->fixture.shape)
        {
            Error("add_fixture fixture has no shape. Set shape first.");
            return 0;
        }
        b2Fixture *created = body->CreateFixture(&fixture->fixture);
        if (!push_fixture_instance(vm, created, body))
            return 1;
        return 1;
    }

    int native_body_add_polygon(Interpreter *vm, void *data, int argCount, Value *args)
    {
        if (argCount != 1 && argCount != 2)
        {
            Error("add_polygon expects 1 or 2 args (points, [FixtureDef])");
            vm->pushInt(0);
            return 1;
        }

        b2Body *body = as_body_handle(data, "add_polygon");
        if (!body)
        {
            vm->pushInt(0);
            return 1;
        }

        std::vector<b2Vec2> points;
        if (!parse_polygon_points(vm, args[0], &points, "add_polygon"))
        {
            vm->pushInt(0);
            return 1;
        }
        for (b2Vec2 &p : points)
        {
            p.x = pixelToWorld(p.x);
            p.y = pixelToWorld(p.y);
        }

        b2FixtureDef fixtureTemplate;
        if (argCount == 2)
        {
            NativeClassInstance *fixtureInstance = require_native_instance(vm, args[1], kClassFixtureDef);
            if (!fixtureInstance)
            {
                vm->pushInt(0);
                return 1;
            }
            FixtureDefHandle *fixtureHandle = static_cast<FixtureDefHandle *>(fixtureInstance->userData);
            fixtureTemplate = fixtureHandle->fixture;
        }
        else
        {
            fixtureTemplate.density = 1.0f;
            fixtureTemplate.friction = 0.3f;
            fixtureTemplate.restitution = 0.0f;
            fixtureTemplate.isSensor = false;
        }

        int createdCount = 0;
        auto create_polygon_fixture = [&](const b2Vec2 *verts, int32 count) -> bool
        {
            b2PolygonShape shape;
            if (!shape.Set(verts, count))
                return false;
            b2FixtureDef fixture = fixtureTemplate;
            fixture.shape = &shape;
            b2Fixture *created = body->CreateFixture(&fixture);
            return created != nullptr;
        };

        if (is_polygon_convex(points) && points.size() <= (size_t)b2_maxPolygonVertices)
        {
            if (create_polygon_fixture(points.data(), (int32)points.size()))
            {
                vm->pushInt(1);
                return 1;
            }
        }

        std::vector<b2Vec2> triangles = triangulate(points);
        if (triangles.empty())
        {
            Error("add_polygon failed to triangulate polygon");
            vm->pushInt(0);
            return 1;
        }

        for (size_t i = 0; i + 2 < triangles.size(); i += 3)
        {
            b2Vec2 tri[3] = {triangles[i], triangles[i + 1], triangles[i + 2]};
            if (create_polygon_fixture(tri, 3))
            {
                createdCount++;
            }
        }

        if (createdCount == 0)
        {
            Error("add_polygon failed to create fixtures");
        }
        vm->pushInt(createdCount);
        return 1;
    }


    int native_create_fixture_def(Interpreter *vm, int argCount, Value *args)
    {
        void *fixtureDef = ctor_native_create_fixture_def(vm, argCount, args);
        if (!push_native_instance(vm, kClassFixtureDef, fixtureDef))
            return 1;
        return 1;
    }

    int native_create_bodydef(Interpreter *vm, int argCount, Value *args)
    {
        void *bodyDef = ctor_native_create_bodydef(vm, argCount, args);
        if (!push_native_instance(vm, kClassBodyDef, bodyDef))
            return 1;
        return 1;
    }

    int native_create_body_process(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        void *body = create_body_handle(vm, proc, argCount, args);
        if (!push_native_instance(vm, kClassBody, body))
            return 1;
        return 1;
    }

    void registerAll(Interpreter &vm)
    {

        NativeClassDef *Body = vm.registerNativeClass(
            kClassBody,
            ctor_native_create_body,
            dtor_native_destroy_body,
            1,
            false);

        NativeClassDef *BodyDef = vm.registerNativeClass(
            kClassBodyDef,
            ctor_native_create_bodydef,
            dtor_native_destroy_bodydef,
            -1,
            false);
        NativeClassDef *Fixture = vm.registerNativeClass(
            kClassFixture,
            ctor_native_create_fixture,
            dtor_native_destroy_fixture,
            0,
            false);
        NativeClassDef *FixtureDef = vm.registerNativeClass(
            kClassFixtureDef,
            ctor_native_create_fixture_def,
            dtor_native_destroy_fixture_def,
            0,
            false);

        vm.addNativeMethod(Fixture, "set_sensor", native_fixture_set_sensor);
        vm.addNativeMethod(Fixture, "set_filter", native_fixture_set_filter);
        vm.addNativeMethod(Fixture, "set_category_bits", native_fixture_set_category_bits);
        vm.addNativeMethod(Fixture, "set_mask_bits", native_fixture_set_mask_bits);
        vm.addNativeMethod(Fixture, "add_mask_bits", native_fixture_add_mask_bits);
        vm.addNativeMethod(Fixture, "remove_mask_bits", native_fixture_remove_mask_bits);
        vm.addNativeMethod(Fixture, "set_group_index", native_fixture_set_group_index);
        vm.addNativeMethod(Fixture, "set_category", native_fixture_set_category_bits);
        vm.addNativeMethod(Fixture, "set_mask", native_fixture_set_mask_bits);
        vm.addNativeMethod(Fixture, "set_group", native_fixture_set_group_index);

        vm.addNativeMethod(FixtureDef, "set_density", native_fixture_def_set_density);
        vm.addNativeMethod(FixtureDef, "set_friction", native_fixture_def_set_friction);
        vm.addNativeMethod(FixtureDef, "set_restitution", native_fixture_def_set_restitution);
        vm.addNativeMethod(FixtureDef, "set_sensor", native_fixture_def_set_sensor);
        vm.addNativeMethod(FixtureDef, "set_filter", native_fixture_def_set_filter);
        vm.addNativeMethod(FixtureDef, "set_category_bits", native_fixture_def_set_category_bits);
        vm.addNativeMethod(FixtureDef, "set_mask_bits", native_fixture_def_set_mask_bits);
        vm.addNativeMethod(FixtureDef, "add_mask_bits", native_fixture_def_add_mask_bits);
        vm.addNativeMethod(FixtureDef, "remove_mask_bits", native_fixture_def_remove_mask_bits);
        vm.addNativeMethod(FixtureDef, "set_group_index", native_fixture_def_set_group_index);
        vm.addNativeMethod(FixtureDef, "set_category", native_fixture_def_set_category_bits);
        vm.addNativeMethod(FixtureDef, "set_mask", native_fixture_def_set_mask_bits);
        vm.addNativeMethod(FixtureDef, "set_group", native_fixture_def_set_group_index);
        vm.addNativeMethod(FixtureDef, "set_circle_shape", native_fixture_def_set_circle_shape);
        vm.addNativeMethod(FixtureDef, "set_box_shape", native_fixture_def_set_box_shape);
        vm.addNativeMethod(FixtureDef, "set_edge_shape", native_fixture_def_set_edge_shape);
        vm.addNativeMethod(FixtureDef, "set_chain_shape", native_fixture_def_set_chain_shape);
        vm.addNativeMethod(FixtureDef, "set_polygon_shape", native_fixture_def_set_polygon_shape);
        vm.addNativeMethod(FixtureDef, "clear_shape", native_fixture_def_clear_shape);

        vm.addNativeMethod(BodyDef, "set_type", native_set_bodydef_type);
        vm.addNativeMethod(BodyDef, "set_position", native_set_bodydef_position);
        vm.addNativeMethod(BodyDef, "set_linear_velocity", native_set_bodydef_linearVelocity);
        vm.addNativeMethod(BodyDef, "set_angle", native_set_bodydef_angle);
        vm.addNativeMethod(BodyDef, "set_angular_velocity", native_set_bodydef_angularVelocity);
        vm.addNativeMethod(BodyDef, "set_linear_damping", native_set_bodydef_linearDamping);
        vm.addNativeMethod(BodyDef, "set_angular_damping", native_set_bodydef_angularDamping);
        vm.addNativeMethod(BodyDef, "set_gravity_scale", native_set_bodydef_gravityScale);
        vm.addNativeMethod(BodyDef, "set_allow_sleep", native_set_bodydef_allowSleep);
        vm.addNativeMethod(BodyDef, "set_awake", native_set_bodydef_awake);
        vm.addNativeMethod(BodyDef, "set_fixed_rotation", native_set_bodydef_fixedRotation);
        vm.addNativeMethod(BodyDef, "set_bullet", native_set_bodydef_bullet);
        vm.addNativeMethod(BodyDef, "set_enabled", native_set_bodydef_enabled);

        vm.addNativeMethod(Body, "remove", native_body_remove);
        vm.addNativeMethod(Body, "set_transform", native_body_set_transform);
        vm.addNativeMethod(Body, "get_position", native_body_get_position);
        vm.addNativeMethod(Body, "set_linear_velocity", native_body_set_linear_velocity);
        vm.addNativeMethod(Body, "get_linear_velocity", native_body_get_linear_velocity);
        vm.addNativeMethod(Body, "set_angular_velocity", native_body_set_angular_velocity);
        vm.addNativeMethod(Body, "get_angular_velocity", native_body_get_angular_velocity);
        vm.addNativeMethod(Body, "apply_force", native_body_apply_force);
        vm.addNativeMethod(Body, "apply_impulse", native_body_apply_impulse);
        vm.addNativeMethod(Body, "set_gravity_scale", native_body_set_gravity_scale);
        vm.addNativeMethod(Body, "get_gravity_scale", native_body_get_gravity_scale);
        vm.addNativeMethod(Body, "set_awake", native_body_set_awake);
        vm.addNativeMethod(Body, "is_awake", native_body_is_awake);
        vm.addNativeMethod(Body, "set_fixed_rotation", native_body_set_fixed_rotation);
        vm.addNativeMethod(Body, "is_fixed_rotation", native_body_is_fixed_rotation);
        vm.addNativeMethod(Body, "set_bullet", native_body_set_bullet);
        vm.addNativeMethod(Body, "is_bullet", native_body_is_bullet);
        vm.addNativeMethod(Body, "get_mass", native_body_get_mass);
        vm.addNativeMethod(Body, "get_inertia", native_body_get_inertia);
        vm.addNativeMethod(Body, "get_angle", native_body_get_angle);
        vm.addNativeMethod(Body, "set_angle", native_body_set_angle);
        vm.addNativeMethod(Body, "get_type", native_body_get_type);
        vm.addNativeMethod(Body, "set_filter", native_body_set_filter);
        vm.addNativeMethod(Body, "set_category_bits", native_body_set_category_bits);
        vm.addNativeMethod(Body, "set_mask_bits", native_body_set_mask_bits);
        vm.addNativeMethod(Body, "add_mask_bits", native_body_add_mask_bits);
        vm.addNativeMethod(Body, "remove_mask_bits", native_body_remove_mask_bits);
        vm.addNativeMethod(Body, "set_group_index", native_body_set_group_index);
        vm.addNativeMethod(Body, "set_category", native_body_set_category_bits);
        vm.addNativeMethod(Body, "set_mask", native_body_set_mask_bits);
        vm.addNativeMethod(Body, "set_group", native_body_set_group_index);
        vm.addNativeMethod(Body, "add_box", native_body_add_box);
        vm.addNativeMethod(Body, "add_circle", native_body_add_circle);
        vm.addNativeMethod(Body, "add_edge", native_body_add_edge);
        vm.addNativeMethod(Body, "add_chain", native_body_add_chain);
        vm.addNativeMethod(Body, "add_polygon", native_body_add_polygon);
        vm.addNativeMethod(Body, "add_fixture", native_body_add_fixture);

        vm.registerNative("create_physics", native_create_physics, -1);
        vm.registerNative("update_physics", native_update_physics, -1);
        vm.registerNative("destroy_physics", native_destroy_physics, 0);
        vm.registerNative("set_physics_debug", native_set_physics_debug, 1);
        vm.registerNative("set_physics_debug_flags", native_set_physics_debug_flags, 1);
        vm.registerNative("debug_physics", native_set_physics_debug, 1);
        vm.registerNative("create_world", native_create_physics, -1);
        vm.registerNative("update_world", native_update_physics, -1);
        vm.registerNative("clean_world", native_destroy_physics, 0);
        vm.registerNative("get_body_count", native_get_body_count, 0);
        vm.registerNative("body_count", native_get_body_count, 0);
        vm.registerNative("physics_collide", native_physics_collide, 2);
        vm.registerNative("body_collide", native_physics_collide, 2);
        vm.registerNative("physics_collide_with", native_physics_collide_with, 1);
        vm.registerNative("body_collide_with", native_physics_collide_with, 1);
        vm.registerNative("physics_collision", native_physics_collision, 0);
        vm.registerNative("physics_raycast", native_physics_raycast, -1);
        vm.registerNative("body_raycast", native_physics_raycast, -1);
        vm.registerNative("physics_overlap_point", native_physics_overlap_point, -1);
        vm.registerNative("body_overlap_point", native_physics_overlap_point, -1);
        vm.registerNative("physics_overlap_rect", native_physics_overlap_rect, -1);
        vm.registerNative("body_overlap_rect", native_physics_overlap_rect, -1);
        vm.registerNative("physics_overlap_circle", native_physics_overlap_circle, -1);
        vm.registerNative("body_overlap_circle", native_physics_overlap_circle, -1);
 

        vm.registerNative("create_fixture_def", native_create_fixture_def, 0);
        vm.registerNative("create_bodydef", native_create_bodydef, -1);
        vm.registerNativeProcess("create_body", native_create_body_process, 1);

        // Joint APIs live in a dedicated module to keep this file manageable.
        BindingsBox2DJoints::registerAll(vm);

        vm.addGlobal("BODY_DYNAMIC", vm.makeInt(BODY_DYNAMIC));
        vm.addGlobal("BODY_STATIC", vm.makeInt(BODY_STATIC));
        vm.addGlobal("BODY_KINEMATIC", vm.makeInt(BODY_KINEMATIC));
        vm.addGlobal("SHAPE_BOX", vm.makeInt(SHAPE_BOX));
        vm.addGlobal("SHAPE_CIRCLE", vm.makeInt(SHAPE_CIRCLE));
        vm.addGlobal("BODY_SYNC_AUTO", vm.makeInt(SYNC_AUTO));
        vm.addGlobal("BODY_SYNC_PROCESS_TO_BODY", vm.makeInt(SYNC_PROCESS_TO_BODY));
        vm.addGlobal("BODY_SYNC_BODY_TO_PROCESS", vm.makeInt(SYNC_BODY_TO_PROCESS));
    }
}
