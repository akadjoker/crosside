#include "bindings.hpp"
#include "engine.hpp"
#include "math.hpp"
#include <cmath>
extern GraphLib gGraphLib;
extern SoundLib gSoundLib;
extern Scene gScene;

namespace Bindings
{

    void CollisionCallback(Entity *a, Entity *b, void *userdata)
    {
    }

    static void *native_mask_ctor(Interpreter *vm, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 3)
        {
            Error("Mask expects 3 arguments (width, height, resolution)");
            return nullptr;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber())
        {
            Error("Mask expects 3 number arguments (width, height, resolution)");
            return nullptr;
        }

        int width = (int)args[0].asNumber();
        int height = (int)args[1].asNumber();
        int resolution = (int)args[2].asNumber();

        if (width <= 0 || height <= 0 || resolution <= 0)
        {
            Error("Mask expects positive arguments (width, height, resolution)");
            return nullptr;
        }

        return new Mask(width, height, resolution);
    }

    static void native_mask_dtor(Interpreter *vm, void *instance)
    {
        (void)vm;
        delete static_cast<Mask *>(instance);
    }

    static int native_mask_set_occupied(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        Mask *mask = static_cast<Mask *>(data);
        if (!mask)
        {
            Error("Mask instance is null");
            return 0;
        }
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("set_occupied expects 2 number arguments (x, y)");
            return 0;
        }

        mask->setOccupied((int)args[0].asNumber(), (int)args[1].asNumber());
        return 0;
    }

    static int native_mask_set_free(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        Mask *mask = static_cast<Mask *>(data);
        if (!mask)
        {
            Error("Mask instance is null");
            return 0;
        }
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("set_free expects 2 number arguments (x, y)");
            return 0;
        }

        mask->setFree((int)args[0].asNumber(), (int)args[1].asNumber());
        return 0;
    }

    static int native_mask_clear_all(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        (void)args;
        Mask *mask = static_cast<Mask *>(data);
        if (!mask)
        {
            Error("Mask instance is null");
            return 0;
        }
        if (argCount != 0)
        {
            Error("clear_all expects no arguments");
            return 0;
        }

        mask->clearAll();
        return 0;
    }

    static int native_mask_is_occupied(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Mask *mask = static_cast<Mask *>(data);
        if (!mask)
        {
            Error("Mask instance is null");
            vm->pushBool(false);
            return 1;
        }
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("is_occupied expects 2 number arguments (x, y)");
            vm->pushBool(false);
            return 1;
        }

        bool occupied = mask->isOccupied((int)args[0].asNumber(), (int)args[1].asNumber());
        vm->pushBool(occupied);
        return 1;
    }

    static int native_mask_is_walkable(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Mask *mask = static_cast<Mask *>(data);
        if (!mask)
        {
            Error("Mask instance is null");
            vm->pushBool(false);
            return 1;
        }
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("is_walkable expects 2 number arguments (x, y)");
            vm->pushBool(false);
            return 1;
        }

        bool walkable = mask->isWalkable((int)args[0].asNumber(), (int)args[1].asNumber());
        vm->pushBool(walkable);
        return 1;
    }

    static int native_mask_load_from_image(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)vm;
        Mask *mask = static_cast<Mask *>(data);
        if (!mask)
        {
            Error("Mask instance is null");
            return 0;
        }
        if (argCount < 1 || argCount > 2)
        {
            Error("load_from_image expects 1 or 2 arguments (path, [threshold])");
            return 0;
        }
        if (!args[0].isString())
        {
            Error("load_from_image expects a string path");
            return 0;
        }
        if (argCount == 2 && !args[1].isNumber())
        {
            Error("load_from_image optional threshold must be number");
            return 0;
        }

        int threshold = (argCount == 2) ? (int)args[1].asNumber() : 128;
        mask->loadFromImage(args[0].asStringChars(), threshold);
        return 0;
    }

    static int native_mask_get_width(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        Mask *mask = static_cast<Mask *>(data);
        if (!mask)
        {
            Error("Mask instance is null");
            vm->pushInt(0);
            return 1;
        }
        if (argCount != 0)
        {
            Error("get_width expects no arguments");
            vm->pushInt(0);
            return 1;
        }
        vm->pushInt(mask->getWidth());
        return 1;
    }

    static int native_mask_get_height(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        Mask *mask = static_cast<Mask *>(data);
        if (!mask)
        {
            Error("Mask instance is null");
            vm->pushInt(0);
            return 1;
        }
        if (argCount != 0)
        {
            Error("get_height expects no arguments");
            vm->pushInt(0);
            return 1;
        }
        vm->pushInt(mask->getHeight());
        return 1;
    }

    static int native_mask_get_resolution(Interpreter *vm, void *data, int argCount, Value *args)
    {
        (void)args;
        Mask *mask = static_cast<Mask *>(data);
        if (!mask)
        {
            Error("Mask instance is null");
            vm->pushInt(0);
            return 1;
        }
        if (argCount != 0)
        {
            Error("get_resolution expects no arguments");
            vm->pushInt(0);
            return 1;
        }
        vm->pushInt(mask->getResolution());
        return 1;
    }

    static int native_mask_world_to_grid(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Mask *mask = static_cast<Mask *>(data);
        if (!mask)
        {
            Error("Mask instance is null");
            vm->pushNil();
            return 1;
        }
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("world_to_grid expects 2 number arguments (x, y)");
            vm->pushNil();
            return 1;
        }

        Vector2 grid = mask->worldToGrid({(float)args[0].asNumber(), (float)args[1].asNumber()});
        vm->pushDouble(grid.x);
        vm->pushDouble(grid.y);

        return 2;
    }

    static int native_mask_grid_to_world(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Mask *mask = static_cast<Mask *>(data);
        if (!mask)
        {
            Error("Mask instance is null");
            vm->pushNil();
            return 1;
        }
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("grid_to_world expects 2 number arguments (x, y)");
            vm->pushNil();
            return 1;
        }

        Vector2 world = mask->gridToWorld({(float)args[0].asNumber(), (float)args[1].asNumber()});
        vm->pushDouble(world.x);
        vm->pushDouble(world.y);
        return 2;
    }

    static int native_mask_find_path(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Mask *mask = static_cast<Mask *>(data);
        if (!mask)
        {
            Error("Mask instance is null");
            vm->pushNil();
            return 1;
        }
        if (argCount != 7)
        {
            Error("find_path expects  7 arguments (sx, sy, ex, ey, diag, algo, heur)");
            vm->pushNil();
            return 1;
        }

        int diag = 1;
        PathAlgorithm algo = PATH_ASTAR;
        PathHeuristic heur = PF_MANHATTAN;

        diag = args[4].asNumber() != 0 ? 1 : 0;
        int algoInt = (int)args[5].asNumber();
        if (algoInt < (int)PATH_ASTAR || algoInt > (int)PATH_DIJKSTRA)
        {
            algoInt = (int)PATH_ASTAR;
        }
        algo = (PathAlgorithm)algoInt;
        int heurInt = (int)args[6].asNumber();
        if (heurInt < (int)PF_MANHATTAN || heurInt > (int)PF_CHEBYSHEV)
        {
            heurInt = (int)PF_MANHATTAN;
        }
        heur = (PathHeuristic)heurInt;

        std::vector<Vector2> path = mask->findPath(
            (int)args[0].asNumber(),
            (int)args[1].asNumber(),
            (int)args[2].asNumber(),
            (int)args[3].asNumber(),
            diag,
            algo,
            heur);

        Value value = vm->makeArray();
        ArrayInstance *arr = value.as.array;
        for (size_t i = 0; i < path.size(); i++)
        {
            Vector2 point = path[i];
            arr->values.push(vm->makeDouble(point.x));
            arr->values.push(vm->makeDouble(point.y));
        }
        vm->push(value);
        return 1;
    }


    static int native_mask_find_path_ex(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Mask *mask = static_cast<Mask *>(data);
        if (!mask)
        {
            Error("Mask instance is null");
            vm->pushNil();
            return 1;
        }
        if (argCount != 7)
        {
            Error("find_path_ex expects  7 arguments (sx, sy, ex, ey, diag, algo, heur)");
            vm->pushNil();
            return 1;
        }

        int diag = 1;
        PathAlgorithm algo = PATH_ASTAR;
        PathHeuristic heur = PF_MANHATTAN;

        diag = args[4].asNumber() != 0 ? 1 : 0;
        int algoInt = (int)args[5].asNumber();
        if (algoInt < (int)PATH_ASTAR || algoInt > (int)PATH_DIJKSTRA)
        {
            algoInt = (int)PATH_ASTAR;
        }
        algo = (PathAlgorithm)algoInt;
        int heurInt = (int)args[6].asNumber();
        if (heurInt < (int)PF_MANHATTAN || heurInt > (int)PF_CHEBYSHEV)
        {
            heurInt = (int)PF_MANHATTAN;
        }
        heur = (PathHeuristic)heurInt;

       bool result= mask->findPathEx(
            (int)args[0].asNumber(),
            (int)args[1].asNumber(),
            (int)args[2].asNumber(),
            (int)args[3].asNumber(),
            diag,
            algo,
            heur);

        vm->pushBool(result);
        return 1;
    }

    static int native_mask_get_result_count(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Mask *mask = static_cast<Mask *>(data);
        if (!mask)
        {
            Error("Mask instance is null");
            vm->pushNil();
            return 1;
        }
        if (argCount != 0)
        {
            Error("get_result_count expects no arguments");
            vm->pushNil();
            return 1;
        }
        vm->pushInt(mask->getResultCount());
        return 1;
    }

    static int native_mask_get_result(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Mask *mask = static_cast<Mask *>(data);
        if (!mask)
        {
            Error("Mask instance is null");
            vm->pushInt(0);
            vm->pushInt(0);
            return 2;
        }
        
        int index = (int)args[0].asNumber();
        if (index < 0 || index >= mask->getResultCount())
        {
            Error("get_result index out of bounds");
            vm->pushInt(0);
            vm->pushInt(0);
            return 2;
        }
        Vector2 point = mask->getResultPoint(index);
        vm->pushDouble(point.x);
        vm->pushDouble(point.y);
        return 2;
    }

    static int native_mask_fill_from_layer(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Mask *mask = static_cast<Mask *>(data);
        if (!mask)
        {
            Error("Mask instance is null");
            vm->pushInt(0);
            return 1;
        }
        if (argCount < 1 || argCount > 3)
        {
            Error("fill_from_layer expects 1 to 3 arguments (layer, [use_solid], [clear_first])");
            vm->pushInt(0);
            return 1;
        }
        if (!args[0].isNumber())
        {
            Error("fill_from_layer expects layer as number");
            vm->pushInt(0);
            return 1;
        }

        int layer = (int)args[0].asNumber();
        bool useSolid = true;
        bool clearFirst = true;

        if (argCount >= 2)
        {
            if (!args[1].isNumber() && !args[1].isBool())
            {
                Error("fill_from_layer optional use_solid must be number/bool");
                vm->pushInt(0);
                return 1;
            }
            useSolid = args[1].isBool() ? args[1].asBool() : (args[1].asNumber() != 0.0);
        }
        if (argCount >= 3)
        {
            if (!args[2].isNumber() && !args[2].isBool())
            {
                Error("fill_from_layer optional clear_first must be number/bool");
                vm->pushInt(0);
                return 1;
            }
            clearFirst = args[2].isBool() ? args[2].asBool() : (args[2].asNumber() != 0.0);
        }

        if (layer < 0 || layer >= MAX_LAYERS)
        {
            Error("fill_from_layer invalid layer index: %d", layer);
            vm->pushInt(0);
            return 1;
        }

        Tilemap *tilemap = gScene.layers[layer].tilemap;
        if (!tilemap)
        {
            vm->pushInt(0);
            return 1;
        }

        if (clearFirst)
        {
            mask->clearAll();
        }

        const int maskWidth = mask->getWidth();
        const int maskHeight = mask->getHeight();
        const int resolution = mask->getResolution();
        if (resolution <= 0)
        {
            vm->pushInt(0);
            return 1;
        }

        int filledCells = 0;
        for (int y = 0; y < tilemap->height; y++)
        {
            for (int x = 0; x < tilemap->width; x++)
            {
                Tile *t = tilemap->getTile(x, y);
                if (!t)
                    continue;

                bool occupied = useSolid ? (t->solid != 0) : (t->id != 0);
                if (!occupied)
                    continue;

                Vector2 world = tilemap->gridToWorld(x, y);
                float worldX = world.x + tilemap->offset_x;
                float worldY = world.y + tilemap->offset_y;
                float worldW = (float)tilemap->tilewidth;
                float worldH = (float)tilemap->tileheight;

                int gx0 = (int)floor(worldX / (float)resolution);
                int gy0 = (int)floor(worldY / (float)resolution);
                int gx1 = (int)floor((worldX + worldW - 1.0f) / (float)resolution);
                int gy1 = (int)floor((worldY + worldH - 1.0f) / (float)resolution);

                if (gx0 < 0)
                    gx0 = 0;
                if (gy0 < 0)
                    gy0 = 0;
                if (gx1 >= maskWidth)
                    gx1 = maskWidth - 1;
                if (gy1 >= maskHeight)
                    gy1 = maskHeight - 1;

                if (gx0 > gx1 || gy0 > gy1)
                    continue;

                for (int my = gy0; my <= gy1; my++)
                {
                    for (int mx = gx0; mx <= gx1; mx++)
                    {
                        if (mask->isWalkable(mx, my))
                        {
                            filledCells++;
                        }
                        mask->setOccupied(mx, my);
                    }
                }
            }
        }

        vm->pushInt(filledCells);
        return 1;
    }

    static int native_load_graph(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("load_graph expects 1 string argument (path)");
            return 0;
        }
        if (!args[0].isString())
        {
            Error("load_graph expects 1 string argument (path)");
            return 0;
        }

        const char *path = args[0].asStringChars();
        const char *name = GetFileNameWithoutExt(path);
        int graphId = gGraphLib.load(name, path);
        if (graphId < 0)
        {
            Error("Failed to load graph: %s from path: %s", name, path);
            return 0;
        }

        vm->pushInt(graphId);

        return 1;
    }

    static int native_load_atlas(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 3)
        {
            Error("load_atlas expects 3 arguments (texturePath, countX, countY)");
            return 0;
        }
        const char *path = args[0].asStringChars();
        const char *name = GetFileNameWithoutExt(path);
        int count_x = (int)args[1].asNumber();
        int count_y = (int)args[2].asNumber();

        int graphId = gGraphLib.loadAtlas(name, path, count_x, count_y);
        if (graphId < 0)
        {
            Error("Failed to load atlas: %s from path: %s", name, path);
            return 0;
        }

        vm->pushInt(graphId);

        return 1;
    }

    static int native_load_subgraph(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 6)
        {
            Error("load_subgraph expects 6 arguments (parentId, name, x, y, width, height)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isString() || !args[2].isNumber() || !args[3].isNumber() || !args[4].isNumber() || !args[5].isNumber())
        {
            Error("load_subgraph expects 6 arguments (parentId, name, x, y, width, height)");
            return 0;
        }

        int parentId = (int)args[0].asNumber();
        const char *name = args[1].asStringChars();
        int x = (int)args[2].asNumber();
        int y = (int)args[3].asNumber();
        int width = (int)args[4].asNumber();
        int height = (int)args[5].asNumber();

        int graphId = gGraphLib.addSubGraph(parentId, name, x, y, width, height);
        if (graphId < 0)
        {
            Error("Failed to load subgraph: %s from parent ID: %d", name, parentId);
            return 0;
        }

        vm->pushInt(graphId);

        return 1;
    }

    static int native_save_graphics(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("save_graphics expects 1 string argument (filename)");
            return 0;
        }
        if (!args[0].isString())
        {
            Error("save_graphics expects 1 string argument (filename)");
            return 0;
        }

        const char *filename = args[0].asStringChars();

        bool success = gGraphLib.savePak(filename);
        if (!success)
        {
            Error("Failed to save graphics to file: %s", filename);
            return 0;
        }

        return 0;
    }

    static int native_load_graphics(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("load_graphics expects 1 string argument (filename)");
            return 0;
        }
        if (!args[0].isString())
        {
            Error("load_graphics expects 1 string argument (filename)");
            return 0;
        }

        const char *filename = args[0].asStringChars();

        bool success = gGraphLib.loadPak(filename);
        if (!success)
        {
            Error("Failed to load graphics from file: %s", filename);
            return 0;
        }

        return 0;
    }

    static int native_init_collision(Interpreter *vm, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 4)
        {
            Error("init_collision expects 4 arguments (x, y, width, height)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber() ||
            !args[2].isNumber() || !args[3].isNumber())
        {
            Error("init_collision expects 4 number arguments (x, y, width, height)");
            return 0;
        }

        int x = (int)args[0].asNumber();
        int y = (int)args[1].asNumber();
        int width = (int)args[2].asNumber();
        int height = (int)args[3].asNumber();
        InitCollision(x, y, width, height, nullptr);
        return 0;
    }

    int native_set_graphics_pointer(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 3)
        {
            Error("set_graphics_pointer expects 3 arguments (graphics, x, y)");
            return 0;
        }
        if (!args[0].isInt() || !args[1].isNumber() || !args[2].isNumber())
        {
            Error("set_graphics_pointer expects 3 arguments (graphics, x, y)");
            return 0;
        }
        int graphId = (int)args[0].asInt();
        float x = (float)args[1].asNumber();
        float y = (float)args[2].asNumber();

        Graph *g = gGraphLib.getGraph(graphId);
        if (!g)
        {
            Error("Invalid graph ID: %d", graphId);
            return 0;
        }

        g->points.push_back({x, y});

        return 0;
    }

    static int native_has_tile_map(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("has_tile_map expects 1 number argument (layer)");
            vm->pushBool(false);
            return 1;
        }

        int layer = (int)args[0].asNumber();
        bool hasTileMap = false;
        if (layer >= 0 && layer < MAX_LAYERS)
        {
            hasTileMap = (gScene.layers[layer].tilemap != nullptr);
        }
        vm->pushBool(hasTileMap);
        return 1;
    }

    int native_proc(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("proc expects 1 argument (process id)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber())
        {
            Error("proc expects 1 number argument (process id)");
            vm->pushNil();
            return 1;
        }

        uint32 id = (uint32)args[0].asNumber();

        Process *target = vm->findProcessById(id);
        if (!target)
        {
            vm->pushNil();
            return 1;
        }

        vm->push(vm->makeProcessInstance(target));

        return 1;
    }

    int native_type(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("type expects 1 argument (process or id)");
            vm->pushString("nil");
            return 1;
        }

        // type(process_instance) - direct pointer access
        if (args[0].isProcessInstance())
        {
            Process *target = args[0].asProcess();
            if (!target || !target->name)
            {
                vm->pushString("nil");
                return 1;
            }
            vm->pushString(target->name->chars());
            return 1;
        }

        // type(id) - lookup by integer id
        if (args[0].isNumber())
        {
            uint32 id = (uint32)args[0].asNumber();
            Process *target = vm->findProcessById(id);
            if (!target || !target->name)
            {
                vm->pushString("nil");
                return 1;
            }
            vm->pushString(target->name->chars());
            return 1;
        }

        vm->pushString("nil");
        return 1;
    }

    static Process *resolve_debug_process(Interpreter *vm, int argCount, Value *args, bool *badArgs)
    {
        if (badArgs)
            *badArgs = false;

        if (argCount == 0)
        {
            return vm->getCurrentProcess();
        }

        if (argCount != 1)
        {
            if (badArgs)
                *badArgs = true;
            return nullptr;
        }

        if (args[0].isProcessInstance())
        {
            return args[0].asProcess();
        }

        if (args[0].isNumber())
        {
            uint32 id = (uint32)args[0].asNumber();
            return vm->findProcessById(id);
        }

        if (badArgs)
            *badArgs = true;
        return nullptr;
    }

    int native_debug_stack(Interpreter *vm, int argCount, Value *args)
    {
        bool badArgs = false;
        Process *target = resolve_debug_process(vm, argCount, args, &badArgs);
        if (badArgs)
        {
            Error("debug_stack expects 0 or 1 argument (process|id)");
            vm->pushNil();
            return 1;
        }
        if (!target || target->state == ProcessState::DEAD)
        {
            vm->pushNil();
            return 1;
        }

        ProcessExec *exec = target;
        Value arrValue = vm->makeArray();
        ArrayInstance *arr = arrValue.as.array;

        for (Value *slot = exec->stackTop; slot > exec->stack;)
        {
            --slot; // top to bottom, same order as debugger output
            arr->values.push(*slot);
        }

        vm->push(arrValue);
        return 1;
    }

    int native_debug_locals(Interpreter *vm, int argCount, Value *args)
    {
        bool badArgs = false;
        Process *target = resolve_debug_process(vm, argCount, args, &badArgs);
        if (badArgs)
        {
            Error("debug_locals expects 0 or 1 argument (process|id)");
            vm->pushNil();
            return 1;
        }
        if (!target || target->state == ProcessState::DEAD)
        {
            vm->pushNil();
            return 1;
        }

        ProcessExec *exec = target;
        if (exec->frameCount <= 0)
        {
            vm->push(vm->makeArray());
            return 1;
        }

        CallFrame *frame = &exec->frames[exec->frameCount - 1];
        Value *start = frame->slots;
        if (!start || start < exec->stack || start > exec->stackTop)
        {
            start = exec->stack;
        }

        Value arrValue = vm->makeArray();
        ArrayInstance *arr = arrValue.as.array;
        for (Value *slot = start; slot < exec->stackTop; ++slot)
        {
            arr->values.push(*slot);
        }

        vm->push(arrValue);
        return 1;
    }

    int native_debug_frames(Interpreter *vm, int argCount, Value *args)
    {
        bool badArgs = false;
        Process *target = resolve_debug_process(vm, argCount, args, &badArgs);
        if (badArgs)
        {
            Error("debug_frames expects 0 or 1 argument (process|id)");
            vm->pushNil();
            return 1;
        }
        if (!target || target->state == ProcessState::DEAD)
        {
            vm->pushNil();
            return 1;
        }

        ProcessExec *exec = target;
        Value outValue = vm->makeArray();
        ArrayInstance *out = outValue.as.array;

        for (int i = exec->frameCount - 1; i >= 0; --i)
        {
            CallFrame *frame = &exec->frames[i];
            Function *func = frame->func;

            Value frameMapValue = vm->makeMap();
            MapInstance *frameMap = frameMapValue.as.map;

            frameMap->table.set(vm->makeString("index").asString(), vm->makeInt(i));

            const char *funcName = "<script>";
            if (func && func->name)
            {
                funcName = func->name->chars();
            }
            frameMap->table.set(vm->makeString("func").asString(), vm->makeString(funcName));

            int ipOffset = 0;
            int line = -1;
            if (func && func->chunk && frame->ip && func->chunk->count > 0)
            {
                ptrdiff_t offset = frame->ip - func->chunk->code;
                if (offset > 0)
                {
                    offset -= 1;
                }
                if (offset < 0)
                {
                    offset = 0;
                }
                if ((size_t)offset >= func->chunk->count)
                {
                    offset = (ptrdiff_t)(func->chunk->count - 1);
                }

                ipOffset = (int)offset;
                line = func->chunk->lines[offset];
            }

            frameMap->table.set(vm->makeString("ip").asString(), vm->makeInt(ipOffset));
            frameMap->table.set(vm->makeString("line").asString(), vm->makeInt(line));

            int slotStart = 0;
            if (frame->slots && frame->slots >= exec->stack && frame->slots <= exec->stackTop)
            {
                slotStart = (int)(frame->slots - exec->stack);
            }
            frameMap->table.set(vm->makeString("slot").asString(), vm->makeInt(slotStart));

            out->values.push(frameMapValue);
        }

        vm->push(outValue);
        return 1;
    }

    int native_debug_processes(Interpreter *vm, int argCount, Value *args)
    {
        (void)args;
        if (argCount != 0)
        {
            Error("debug_processes expects no arguments");
            vm->pushNil();
            return 1;
        }

        Value outValue = vm->makeArray();
        ArrayInstance *out = outValue.as.array;

        const auto &alive = vm->getAliveProcesses();
        for (size_t i = 0; i < alive.size(); i++)
        {
            Process *proc = alive[i];
            if (!proc)
                continue;

            Value procMapValue = vm->makeMap();
            MapInstance *procMap = procMapValue.as.map;

            procMap->table.set(vm->makeString("id").asString(), vm->makeInt((int)proc->id));
            procMap->table.set(vm->makeString("blueprint").asString(), vm->makeInt(proc->blueprint));
            procMap->table.set(vm->makeString("state").asString(), vm->makeInt((int)proc->state));
            procMap->table.set(vm->makeString("frames").asString(), vm->makeInt(proc->frameCount));
            procMap->table.set(vm->makeString("stack").asString(), vm->makeInt((int)(proc->stackTop - proc->stack)));

            const char *name = "<unnamed>";
            if (proc->name)
                name = proc->name->chars();
            procMap->table.set(vm->makeString("name").asString(), vm->makeString(name));
            procMap->table.set(vm->makeString("process").asString(), vm->makeProcessInstance(proc));

            out->values.push(procMapValue);
        }

        vm->push(outValue);
        return 1;
    }

    static void applySignal(Process *proc, int signalType)
    {
        switch (signalType)
        {
        case 0: // S_KILL
            proc->state = ProcessState::DEAD;
            break;
        case 1: // S_FREEZE
            if (proc->state == ProcessState::RUNNING || proc->state == ProcessState::SUSPENDED)
                proc->state = ProcessState::FROZEN;
            break;
        case 2: // S_HIDE - freeze + hide (same as freeze for now)
            if (proc->state == ProcessState::RUNNING || proc->state == ProcessState::SUSPENDED)
                proc->state = ProcessState::FROZEN;
            break;
        case 3: // S_SHOW - wakeup from frozen
            if (proc->state == ProcessState::FROZEN)
                proc->state = ProcessState::RUNNING;
            break;
        }
    }

    int native_signal(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("signal expects 2 arguments (target, signal_type)");
            return 0;
        }


        //Info(" type %s",valueTypeToString(args[0].type));

        int signalType = (int)args[1].asInt();

        // signal(process_instance, SKILL) - by specific process
        if (args[0].isProcessInstance())
        {
            Process *proc = args[0].asProcess();
            if (proc && proc->state != ProcessState::DEAD)
                applySignal(proc, signalType);
            return 0;
        }

        int target = (int)args[0].asInt();

        // signal(-1, SKILL) - all processes
        if (target == -1)
        {
            const auto &alive = vm->getAliveProcesses();
            for (size_t i = 0; i < alive.size(); i++)
            {
                Process *proc = alive[i];
                if (proc)
                    applySignal(proc, signalType);
            }
            return 0;
        }

        // signal(type enemy, SKILL) - by blueprint index
        const auto &alive = vm->getAliveProcesses();
        for (size_t i = 0; i < alive.size(); i++)
        {
            Process *proc = alive[i];
            if (proc && proc->blueprint == target)
                applySignal(proc, signalType);
        }

        return 0;
    }

 

    int native_exists(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            vm->pushBool(false);
            return 1;
        }

        // exists(process_instance) - direct pointer check
        if (args[0].isProcessInstance())
        {
            Process *proc = args[0].asProcess();
            vm->pushBool(proc && proc->state != ProcessState::DEAD);
            return 1;
        }

        // exists(type enemy) - check if any process of this type is alive
        if (args[0].isInt())
        {
            int targetBlueprint = args[0].asInt();
            const auto &alive = vm->getAliveProcesses();
            for (size_t i = 0; i < alive.size(); i++)
            {
                Process *proc = alive[i];
                if (proc && proc->blueprint == targetBlueprint && proc->state != ProcessState::DEAD)
                {
                    vm->pushBool(true);
                    return 1;
                }
            }
            vm->pushBool(false);
            return 1;
        }

        // exists(process_id) - check if specific process exists by int id
        if (args[0].isNumber())
        {
            uint32 id = (uint32)args[0].asNumber();
            Process *target = vm->findProcessById(id);
            vm->pushBool(target != nullptr);
            return 1;
        }

        vm->pushBool(false);
        return 1;
    }

    int native_get_count(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isInt())
        {
            vm->pushInt(0);
            return 1;
        }

        int targetBlueprint = args[0].asInt();
        int count = 0;

        const auto &alive = vm->getAliveProcesses();
        for (size_t i = 0; i < alive.size(); i++)
        {
            Process *proc = alive[i];
            if (proc && proc->blueprint == targetBlueprint && proc->state != ProcessState::DEAD)
                count++;
        }

        vm->pushInt(count);
        return 1;
    }

    int native_get_ids(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isInt())
        {
            vm->push(vm->makeArray());
            return 1;
        }

        int targetBlueprint = args[0].asInt();
        Value arr = vm->makeArray();
        ArrayInstance *array = arr.as.array;

        const auto &alive = vm->getAliveProcesses();
        for (size_t i = 0; i < alive.size(); i++)
        {
            Process *proc = alive[i];
            if (proc && proc->blueprint == targetBlueprint && proc->state != ProcessState::DEAD)
                array->values.push(vm->makeProcessInstance(proc));
        }

        vm->push(arr);
        return 1;
    }

    int native_get_id(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isInt())
        {
            vm->pushNil();
            return 1;
        }

        int targetBlueprint = args[0].asInt();

        const auto &alive = vm->getAliveProcesses();
        for (size_t i = 0; i < alive.size(); i++)
        {
            Process *proc = alive[i];
            if (proc && proc->blueprint == targetBlueprint && proc->state != ProcessState::DEAD)
            {
                vm->push(vm->makeProcessInstance(proc));
                return 1;
            }
        }

        vm->pushNil();
        return 1;
    }

    int native_load_sound(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("load_sound expects 1 string argument (path)");
            return 0;
        }
        if (!args[0].isString())
        {
            Error("load_sound expects 1 string argument (path)");
            return 0;
        }

        const char *path = args[0].asStringChars();
        int soundId = gSoundLib.load(GetFileNameWithoutExt(path), path);
        if (soundId < 0)
        {
            Error("Failed to load sound from path: %s", path);
            return 0;
        }

        vm->pushInt(soundId);

        return 1;
    }

    int native_play_sound(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 3)
        {
            Error("play_sound expects 3 arguments (soundId, volume, pitch)");
            return 0;
        }
        if (!args[0].isInt() || !args[1].isNumber() || !args[2].isNumber())
        {
            Error("play_sound expects 3 arguments (soundId, volume, pitch)");
            return 0;
        }

        int soundId = (int)args[0].asInt();
        float volume = (float)args[1].asNumber();
        float pitch = (float)args[2].asNumber();

        gSoundLib.play(soundId, volume, pitch);

        return 0;
    }

    int native_stop_sound(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("stop_sound expects 1 argument (soundId)");
            return 0;
        }
        if (!args[0].isInt())
        {
            Error("stop_sound expects 1 int argument (soundId)");
            return 0;
        }

        int soundId = (int)args[0].asInt();
        gSoundLib.stop(soundId);
        return 0;
    }

    int native_is_sound_playing(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("is_sound_playing expects 1 argument (soundId)");
            vm->pushBool(false);
            return 1;
        }
        if (!args[0].isInt())
        {
            Error("is_sound_playing expects 1 int argument (soundId)");
            vm->pushBool(false);
            return 1;
        }

        int soundId = (int)args[0].asInt();
        bool playing = gSoundLib.isSoundPlaying(soundId);
        vm->pushBool(playing);
        return 1;
    }

    int native_pause_sound(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("pause_sound expects 1 argument (soundId)");
            return 0;
        }
        if (!args[0].isInt())
        {
            Error("pause_sound expects 1 int argument (soundId)");
            return 0;
        }

        int soundId = (int)args[0].asInt();
        gSoundLib.pause(soundId);
        return 0;
    }

    int native_resume_sound(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("resume_sound expects 1 argument (soundId)");
            return 0;
        }
        if (!args[0].isInt())
        {
            Error("resume_sound expects 1 int argument (soundId)");
            return 0;
        }

        int soundId = (int)args[0].asInt();
        gSoundLib.resume(soundId);
        return 0;
    }

    int native_set_layer_mode(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("set_layer_mode expects 2 arguments (layer, mode)");
            return 0;
        }
        if (!args[0].isInt() || !args[1].isInt())
        {
            Error("set_layer_mode expects 2 int arguments (layer, mode)");
            return 0;
        }

        int layer = (int)args[0].asInt();
        int mode = (int)args[1].asInt();

        SetLayerMode(layer, mode);
        return 0;
    }

    int native_set_layer_clip(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("set_layer_clip expects 1 argument (clip)");
            return 0;
        }
        if (!args[0].isBool())
        {
            Error("set_layer_clip expects 1 bool argument (clip)");
            return 0;
        }
        gScene.clip = args[0].asBool();
        return 0;
    }

    int native_set_layer_scroll_factor(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 3)
        {
            Error("set_layer_scroll_factor expects 3 arguments (layer, x, y)");
            return 0;
        }
        if (!args[0].isInt() || !args[1].isNumber() || !args[2].isNumber())
        {
            Error("set_layer_scroll_factor expects 3 arguments (layer, x, y)");
            return 0;
        }

        int layer = (int)args[0].asInt();
        double x = args[1].asNumber();
        double y = args[2].asNumber();

        SetLayerScrollFactor(layer, x, y);
        return 0;
    }

    int native_set_layer_visible(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("set_layer_visible expects 2 arguments (layer, visible)");
            return 0;
        }
        

        int layer = args[0].asInt();
        bool visible = args[1].asBool();

        SetLayerVisible(layer, visible);
        return 0;
    }

    int native_set_layer_size(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 5)
        {
            Error("set_layer_size expects 5 arguments (layer, x, y, width, height)");
            return 0;
        }
        if (!args[0].isInt() || !args[1].isInt() || !args[2].isInt() || !args[3].isInt() || !args[4].isInt())
        {
            Error("set_layer_size expects 5 int arguments (layer, x, y, width, height)");
            return 0;
        }

        int layer = (int)args[0].asInt();
        int x = (int)args[1].asInt();
        int y = (int)args[2].asInt();
        int width = (int)args[3].asInt();
        int height = (int)args[4].asInt();

        SetLayerSize(layer, x, y, width, height);
        return 0;
    }

    int native_set_layer_back_graph(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("set_layer_back_graph expects 2 arguments (layer, graph)");
            return 0;
        }
        if (!args[0].isInt() || !args[1].isInt())
        {
            Error("set_layer_back_graph expects 2 int arguments (layer, graph)");
            return 0;
        }

        int layer = (int)args[0].asInt();
        int graph = (int)args[1].asInt();

        SetLayerBackGraph(layer, graph);
        return 0;
    }

    int native_set_layer_front_graph(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("set_layer_front_graph expects 2 arguments (layer, graph)");
            return 0;
        }
        if (!args[0].isInt() || !args[1].isInt())
        {
            Error("set_layer_front_graph expects 2 int arguments (layer, graph)");
            return 0;
        }

        int layer = (int)args[0].asInt();
        int graph = (int)args[1].asInt();

        SetLayerFrontGraph(layer, graph);
        return 0;
    }

    int native_set_scroll(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("set_scroll expects 2 arguments (x, y)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("set_scroll expects 2 number arguments (x, y)");
            return 0;
        }

        double x = args[0].asNumber();
        double y = args[1].asNumber();

        SetScroll(x, y);
        return 0;
    }

    int native_set_tile_map(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 7)
        {
            Error("set_tile_map expects 7 arguments (layer, map_width, map_height, tile_width, tile_height, columns, graph)");
            return 0;
        }
        if (!args[0].isInt() || !args[1].isInt() || !args[2].isInt() || !args[3].isInt() || !args[4].isInt() || !args[5].isInt() || !args[6].isInt())
        {
            Error("set_tile_map expects 7 int arguments (layer, map_width, map_height, tile_width, tile_height, columns, graph)");
            return 0;
        }

        int layer = (int)args[0].asInt();
        int map_width = (int)args[1].asInt();
        int map_height = (int)args[2].asInt();
        int tile_width = (int)args[3].asInt();
        int tile_height = (int)args[4].asInt();
        int columns = (int)args[5].asInt();
        int graph = (int)args[6].asInt();

        SetTileMap(layer, map_width, map_height, tile_width, tile_height, columns, graph);
        return 0;
    }

    int native_clear_tile_map(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("clear_tile_map expects 1 argument (layer)");
            return 0;
        }
        if (!args[0].isInt())
        {
            Error("clear_tile_map expects 1 int argument (layer)");
            return 0;
        }

        int layer = (int)args[0].asInt();

        return 0;
    }

    int native_set_tile_map_free(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("set_tile_map_free expects 2 arguments (layer, free)");
            return 0;
        }
        if (!args[0].isInt() || !args[1].isInt())
        {
            Error("set_tile_map_free expects 2 int arguments (layer, free)");
            return 0;
        }

        int layer = (int)args[0].asInt();
        int free = (int)args[1].asInt();

        SetTileMapFree(layer, free);
        return 0;
    }

     int native_set_tile_map_visible(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("set_tile_map_visible expects 2 arguments (layer, visible)");
            return 0;
        }
         

        int layer = (int)args[0].asInt();
        bool visible = args[1].asBool();
        SetTileMapVisible(layer, visible);
        return 0;
    }


    int native_set_tile_map_solid(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("set_tile_map_solid expects 2 arguments (layer, solid)");
            return 0;
        }
        
        

        int layer = (int)args[0].asInt();
        int solid = (int)args[1].asInt();

        SetTileMapSolid(layer, solid);
        return 0;
    }

    int native_set_tile_map_spacing(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("set_tile_map_spacing expects 2 arguments (layer, spacing)");
            return 0;
        }
        if (!args[0].isInt() || !args[1].isNumber())
        {
            Error("set_tile_map_spacing expects 1 int and 1 number argument (layer, spacing)");
            return 0;
        }

        int layer = (int)args[0].asInt();
        double spacing = args[1].asNumber();

        SetTileMapSpacing(layer, spacing);
        return 0;
    }

    int native_set_tile_map_margin(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("set_tile_map_margin expects 2 arguments (layer, margin)");
            return 0;
        }
        if (!args[0].isInt() || !args[1].isNumber())
        {
            Error("set_tile_map_margin expects 1 int and 1 number argument (layer, margin)");
            return 0;
        }

        int layer = (int)args[0].asInt();
        double margin = args[1].asNumber();

        SetTileMapMargin(layer, margin);
        return 0;
    }

    int native_set_tile_debug(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 3)
        {
            Error("set_tile_debug expects 3 arguments (layer, grid, ids)");
            return 0;
        }
        if (!args[0].isInt() || !args[1].isBool() || !args[2].isBool())
        {
            Error("set_tile_debug expects 1 int and 2 bool arguments (layer, grid, ids)");
            return 0;
        }

        int layer = (int)args[0].asInt();
        bool grid = args[1].asBool();
        bool ids = args[2].asBool();

        SetTileMapDebug(layer, grid != 0, ids != 0);
        return 0;
    }
    int native_set_tile_map_color(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("set_tile_map_color expects 2 arguments (layer, color)");
            return 0;
        }
        if (!args[0].isInt() || !args[1].isNativeStructInstance())
        {
            Error("set_tile_map_color expects 2 int arguments (layer, color)");
            return 0;
        }

        auto *inst = args[1].asNativeStructInstance();
        Color *color = (Color *)inst->data;
        int layer = (int)args[0].asInt();
        SetTileMapColor(layer, *color);
        return 0;
    }

    int native_set_tile_map_mode(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("set_tile_map_mode expects 2 arguments (layer, mode)");
            return 0;
        }
        if (!args[0].isInt() || !args[1].isInt())
        {
            Error("set_tile_map_mode expects 2 int arguments (layer, mode)");
            return 0;
        }

        int layer = (int)args[0].asInt();
        int mode = (int)args[1].asInt();

        SetTileMapMode(layer, mode);
        return 0;
    }

    int native_set_tile_map_iso_compression(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("set_tile_map_iso_compression expects 2 arguments (layer, compression)");
            return 0;
        }

        int layer = (int)args[0].asInt();
        double compression = args[1].asNumber();

        SetTileMapIsoCompression(layer, compression);
        return 0;
    }

    int native_set_tile_map_tile(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 5)
        {
            Error("set_tile_map_tile expects 5 arguments (layer, x, y, tile, solid)");
            return 0;
        }

        int layer = (int)args[0].asInt();
        int x = (int)args[1].asInt();
        int y = (int)args[2].asInt();
        int tile = (int)args[3].asInt();
        int solid = (int)args[4].asInt();

        SetTileMapTile(layer, x, y, tile, solid);
        return 0;
    }

    int native_get_tile_map_tile(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 3)
        {
            Error("get_tile_map_tile expects 3 arguments (layer, x, y)");
            return 0;
        }
        if (!args[0].isInt() || !args[1].isInt() || !args[2].isInt())
        {
            Error("get_tile_map_tile expects 3 int arguments (layer, x, y)");
            return 0;
        }

        int layer = (int)args[0].asInt();
        int x = (int)args[1].asInt();
        int y = (int)args[2].asInt();

        int tile = GetTileMapTile(layer, x, y);
        vm->pushInt(tile);
        return 1;
    }

    int native_import_tmx(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("import_tilemap expects 1 string argument (filename)");
            vm->pushBool(false);
            return 1;
        }
        if (!args[0].isString())
        {
            Error("import_tilemap expects 1 string argument (filename)");
            vm->pushBool(false);
            return 1;
        }

        bool success = gScene.ImportTileMap(args[0].asStringChars());
        vm->pushBool(success);

        return 1;
    }

    int native_delta_time(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("delta_time expects no arguments");
            return 0;
        }

        vm->pushDouble(GetFrameTime());
        return 1;
    }

    int native_time(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("time expects no arguments");
            return 0;
        }

        vm->pushDouble(GetTime());
        return 1;
    }

    int native_get_fps(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("get_fps expects no arguments");
            return 0;
        }

        vm->pushInt(GetFPS());
        return 1;
    }

    // ==========================================
    // Game Math Functions (DIV-style)
    // Convention: 0=right, 90=up, 180=left, 270=down
    // ==========================================

    // get_distx(angle, distance) -> X component of movement
    static int native_get_distx(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("get_distx expects 2 number arguments (angle, distance)");
            vm->pushDouble(0);
            return 1;
        }
        double angle = args[0].asNumber();
        double dist = args[1].asNumber();
        double rad = angle * PI / 180.0;
        vm->pushDouble(cos(rad) * dist);
        return 1;
    }

    // get_disty(angle, distance) -> Y component of movement (screen coords, Y-flip)
    static int native_get_disty(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("get_disty expects 2 number arguments (angle, distance)");
            vm->pushDouble(0);
            return 1;
        }
        double angle = args[0].asNumber();
        double dist = args[1].asNumber();
        double rad = angle * PI / 180.0;
        vm->pushDouble(-sin(rad) * dist);
        return 1;
    }

    // get_angle(x1, y1, x2, y2) -> angle from point to point (degrees)
    static int native_get_angle(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 4 || !args[0].isNumber() || !args[1].isNumber() ||
            !args[2].isNumber() || !args[3].isNumber())
        {
            Error("get_angle expects 4 number arguments (x1, y1, x2, y2)");
            vm->pushDouble(0);
            return 1;
        }
        double x1 = args[0].asNumber();
        double y1 = args[1].asNumber();
        double x2 = args[2].asNumber();
        double y2 = args[3].asNumber();
        double dx = x2 - x1;
        double dy = -(y2 - y1); // flip Y for screen coords
        double angle = atan2(dy, dx) * 180.0 / PI;
        vm->pushDouble(angle);
        return 1;
    }

    // get_dist(x1, y1, x2, y2) -> distance between two points
    static int native_get_dist(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 4 || !args[0].isNumber() || !args[1].isNumber() ||
            !args[2].isNumber() || !args[3].isNumber())
        {
            Error("get_dist expects 4 number arguments (x1, y1, x2, y2)");
            vm->pushDouble(0);
            return 1;
        }
        double x1 = args[0].asNumber();
        double y1 = args[1].asNumber();
        double x2 = args[2].asNumber();
        double y2 = args[3].asNumber();
        double dx = x2 - x1;
        double dy = y2 - y1;
        vm->pushDouble(sqrt(dx * dx + dy * dy));
        return 1;
    }

    // angle_delta(from, to) -> shortest signed delta in [-180, 180]
    static int native_angle_delta(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2 || !args[0].isNumber() || !args[1].isNumber())
        {
            Error("angle_delta expects 2 number arguments (from, to)");
            vm->pushDouble(0);
            return 1;
        }

        double from = args[0].asNumber();
        double to = args[1].asNumber();
        double diff = fmod(to - from, 360.0);
        if (diff > 180.0)
            diff -= 360.0;
        if (diff < -180.0)
            diff += 360.0;

        vm->pushDouble(diff);
        return 1;
    }

    // near_angle(current, target, step) -> rotate current toward target by at most step degrees
    static int native_near_angle(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 3 || !args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber())
        {
            Error("near_angle expects 3 number arguments (current, target, step)");
            vm->pushDouble(0);
            return 1;
        }
        double current = args[0].asNumber();
        double target = args[1].asNumber();
        double step = fabs(args[2].asNumber());

        double diff = fmod(target - current, 360.0);
        if (diff > 180.0)
            diff -= 360.0;
        if (diff < -180.0)
            diff += 360.0;

        if (fabs(diff) <= step)
        {
            vm->pushDouble(target);
        }
        else
        {
            vm->pushDouble(current + (diff > 0 ? step : -step));
        }
        return 1;
    }

    // normalize_angle(angle) -> normalize to [0, 360) range
    static int native_normalize_angle(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isNumber())
        {
            Error("normalize_angle expects 1 number argument (angle)");
            vm->pushDouble(0);
            return 1;
        }
        double angle = fmod(args[0].asNumber(), 360.0);
        if (angle < 0)
            angle += 360.0;
        vm->pushDouble(angle);
        return 1;
    }

    void registerAll(Interpreter &vm)
    {
        NativeClassDef *mask = vm.registerNativeClass(
            "Path",
            native_mask_ctor,
            native_mask_dtor,
            3,
            false);

        vm.addNativeMethod(mask, "set_occupied", native_mask_set_occupied);
        vm.addNativeMethod(mask, "set_free", native_mask_set_free);
        vm.addNativeMethod(mask, "clear_all", native_mask_clear_all);
        vm.addNativeMethod(mask, "is_occupied", native_mask_is_occupied);
        vm.addNativeMethod(mask, "is_walkable", native_mask_is_walkable);
        vm.addNativeMethod(mask, "load_from_image", native_mask_load_from_image);
        vm.addNativeMethod(mask, "get_width", native_mask_get_width);
        vm.addNativeMethod(mask, "get_height", native_mask_get_height);
        vm.addNativeMethod(mask, "get_resolution", native_mask_get_resolution);
        vm.addNativeMethod(mask, "world_to_grid", native_mask_world_to_grid);
        vm.addNativeMethod(mask, "grid_to_world", native_mask_grid_to_world);
        vm.addNativeMethod(mask, "find", native_mask_find_path);
        vm.addNativeMethod(mask, "fill_from_layer", native_mask_fill_from_layer);
        vm.addNativeMethod(mask, "find_ex", native_mask_find_path_ex);
        vm.addNativeMethod(mask, "get_result_count", native_mask_get_result_count);
        vm.addNativeMethod(mask, "get_result", native_mask_get_result);

        vm.registerNative("load_graph", native_load_graph, 1);
        vm.registerNative("load_atlas", native_load_atlas, 3);
        vm.registerNative("load_subgraph", native_load_subgraph, 6);
        vm.registerNative("save_graphics", native_save_graphics, 1);
        vm.registerNative("load_graphics", native_load_graphics, 1);
        vm.registerNative("set_graphics_point", native_set_graphics_pointer, 3);
        vm.registerNative("init_collision", native_init_collision, 4);
        vm.registerNative("signal", native_signal, 2);
        vm.registerNative("exists", native_exists, 1);
        vm.registerNative("count_processes", native_get_count, 1);
        vm.registerNative("get_ids", native_get_ids, 1);
        vm.registerNative("play_sound", native_play_sound, 3);
        vm.registerNative("stop_sound", native_stop_sound, 1);
        vm.registerNative("load_sound", native_load_sound, 1);
        vm.registerNative("is_sound_playing", native_is_sound_playing, 1);
        vm.registerNative("pause_sound", native_pause_sound, 1);
        vm.registerNative("resume_sound", native_resume_sound, 1);
        vm.registerNative("set_layer_mode", native_set_layer_mode, 2);
        vm.registerNative("set_layer_clip", native_set_layer_clip, 1);

        vm.registerNative("set_layer_scroll_factor", native_set_layer_scroll_factor, 3);
        vm.registerNative("set_layer_size", native_set_layer_size, 5);
        vm.registerNative("set_layer_back_graph", native_set_layer_back_graph, 2);
        vm.registerNative("set_layer_front_graph", native_set_layer_front_graph, 2);
        vm.registerNative("set_layer_visible", native_set_layer_visible, 2);
        vm.registerNative("set_scroll", native_set_scroll, 2);
        vm.registerNative("set_tile_map", native_set_tile_map, 7);
        vm.registerNative("set_tile_map_spacing", native_set_tile_map_spacing, 2);
        vm.registerNative("set_tile_map_free", native_set_tile_map_free, 2);
        vm.registerNative("set_tile_map_solid", native_set_tile_map_solid, 2);
        vm.registerNative("set_tile_map_visible", native_set_tile_map_visible, 2);
        vm.registerNative("set_tile_map_margin", native_set_tile_map_margin, 2);
        vm.registerNative("set_tile_map_mode", native_set_tile_map_mode, 2);
        vm.registerNative("set_tile_map_color", native_set_tile_map_color, 2);
        vm.registerNative("set_tile_debug", native_set_tile_debug, 3);
        vm.registerNative("set_tile_map_iso_compression", native_set_tile_map_iso_compression, 2);
        vm.registerNative("set_tile_map_tile", native_set_tile_map_tile, 5);
        vm.registerNative("get_tile_map_tile", native_get_tile_map_tile, 3);
        vm.registerNative("has_tile_map", native_has_tile_map, 1);
        vm.registerNative("import_tilemap", native_import_tmx, 1);

        vm.registerNative("delta", native_delta_time, 0);
        vm.registerNative("time", native_time, 0);
        vm.registerNative("get_fps", native_get_fps, 0);

        // Game math functions (DIV-style)
        vm.registerNative("get_distx", native_get_distx, 2);
        vm.registerNative("get_disty", native_get_disty, 2);
        vm.registerNative("get_angle", native_get_angle, 4);
        vm.registerNative("get_dist", native_get_dist, 4);
        vm.registerNative("angle_delta", native_angle_delta, 2);
        vm.registerNative("near_angle", native_near_angle, 3);
        vm.registerNative("normalize_angle", native_normalize_angle, 1);
        vm.registerNative("debug_stack", native_debug_stack, -1);
        vm.registerNative("debug_locals", native_debug_locals, -1);
        vm.registerNative("debug_frames", native_debug_frames, -1);
        vm.registerNative("debug_processes", native_debug_processes, 0);

        vm.addGlobal("SKILL", vm.makeInt(0));
        vm.addGlobal("SFREEZE", vm.makeInt(1));
        vm.addGlobal("SHIDE", vm.makeInt(2));
        vm.addGlobal("SSHOW", vm.makeInt(3));
        vm.addGlobal("PATH_ASTAR", vm.makeInt((int)PATH_ASTAR));
        vm.addGlobal("PATH_DIJKSTRA", vm.makeInt((int)PATH_DIJKSTRA));
        vm.addGlobal("PF_MANHATTAN", vm.makeInt((int)PF_MANHATTAN));
        vm.addGlobal("PF_EUCLIDEAN", vm.makeInt((int)PF_EUCLIDEAN));
        vm.addGlobal("PF_OCTILE", vm.makeInt((int)PF_OCTILE));
        vm.addGlobal("PF_CHEBYSHEV", vm.makeInt((int)PF_CHEBYSHEV));

        BindingsInput::registerAll(vm);
        BindingsImage::registerAll(vm);
        BindingsProcess::registerAll(vm);
        BindingsBox2D::registerAll(vm);
        BindingsPoly2Tri::registerAll(vm);
        BindingsDraw::registerAll(vm);
        BindingsParticles::registerAll(vm);
        BindingsEase::registerAll(vm);
        BindingsMessage::registerAll(vm);
    }

} // namespace OgreBindings
