#include "bindings.hpp"
#include "engine.hpp"
#include "math.hpp"
#include "interpreter.hpp"
#include <raylib.h>
#include <cfloat>
extern GraphLib gGraphLib;
extern Scene gScene;
 

namespace BindingsProcess
{

    double get_distx(double a, double d)
    {
        double angulo = (double)a * RAD;
        return ((double)(cos(angulo) * d));
    }

    double get_disty(double a, double d)
    {
        double angulo = (double)a * RAD;
        return (-(double)(sin(angulo) * d));
    }

    int native_advance(Interpreter *vm, Process *proc, int argCount, Value *args)
    {

        if (argCount != 1)
        {
            Error("advance expects 1 argument speed");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("advance expects a number argument speed ");
            return 0;
        }

        double distance = args[0].asNumber();

        double x = proc->privates[0].asNumber();
        double y = proc->privates[1].asNumber();
        double angle = -proc->privates[4].asNumber();

        x += get_distx(angle, distance);
        y += get_disty(angle, distance);

        proc->privates[0] = vm->makeDouble(x);
        proc->privates[1] = vm->makeDouble(y);
        //  proc->privates[4] = vm->makeDouble(angle);

        return 0;
    }

    int native_xadvance(Interpreter *vm, Process *proc, int argCount, Value *args)
    {

        if (argCount != 2)
        {
            Error("xadvance expects 2 arguments speed and angle");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("xadvance expects 2 number arguments speed and angle");
            return 0;
        }
        double distance = args[0].asNumber();
        double angle = -args[1].asNumber();

        double x = proc->privates[0].asNumber();
        double y = proc->privates[1].asNumber();

        x += get_distx(angle, distance);
        y += get_disty(angle, distance);

        proc->privates[0] = vm->makeDouble(x);
        proc->privates[1] = vm->makeDouble(y);
        //        proc->privates[4] = vm->makeDouble(angle);

        return 0;
    }

    int native_get_point(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("get_point expects 1 argument pointIndex");
            vm->pushDouble(0);
            vm->pushDouble(0);
            return 2;
        }
        if (!args[0].isNumber())
        {
            Error("get_point expects a number argument pointIndex");
            vm->pushDouble(0);
            vm->pushDouble(0);
            return 2;
        }

        int pointIndex = (int)args[0].asNumber();

        if (proc->userData == nullptr)
        {
            vm->pushDouble(0);
            vm->pushDouble(0);
            return 2;
        }

        Entity *entity = (Entity *)proc->userData;
        int graphId = entity->graph;

        Graph *g = gGraphLib.getGraph(graphId);
        if (!g || pointIndex < 0 || pointIndex >= (int)g->points.size())
        {
            vm->pushDouble(0);
            vm->pushDouble(0);
            return 2;
        }
        Vector2 point = g->points[pointIndex];

        vm->pushDouble(point.x);
        vm->pushDouble(point.y);
        return 2;
    }

    int native_get_real_point(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("get_real_point expects 1 argument pointIndex");
            vm->pushDouble(0);
            vm->pushDouble(0);
            return 2;
        }
        if (!args[0].isNumber())
        {
            Error("get_real_point expects a number argument pointIndex");
            vm->pushDouble(0);
            vm->pushDouble(0);
            return 2;
        }

        int pointIndex = (int)args[0].asNumber();

        if (proc->userData == nullptr)
        {
            vm->pushDouble(0);
            vm->pushDouble(0);

            return 2;
        }

        Entity *entity = (Entity *)proc->userData;
        Vector2 realPoint = entity->getRealPoint(pointIndex);

        //        Info("get_real_point: pointIndex=%d realX=%f realY=%f", pointIndex, realPoint.x, realPoint.y);

        //DrawCircleLines((int)realPoint.x, (int)realPoint.y, 5, RED);

        vm->pushDouble(realPoint.x);
        vm->pushDouble(realPoint.y);
        return 2;
    }

    static Entity *requireEntity(Process *proc, const char *funcName)
    {
        if (!proc || !proc->userData)
        {
            Error("%s process has no associated entity!", funcName);
            return nullptr;
        }
        return (Entity *)proc->userData;
    }

    int native_set_rect_shape(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 4)
        {
            Error("set_rect_shape expects 4 arguments (x, y, w, h)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber() || !args[3].isNumber())
        {
            Error("set_rect_shape expects 4 number arguments (x, y, w, h)");
            return 0;
        }

        Entity *entity = requireEntity(proc, "set_rect_shape");
        if (!entity)
            return 0;

        int x = (int)args[0].asNumber();
        int y = (int)args[1].asNumber();
        int w = (int)args[2].asNumber();
        int h = (int)args[3].asNumber();
        entity->setRectangleShape(x, y, w, h);
        return 0;
    }

    int native_set_circle_shape(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1)
        {
            Error("set_circle_shape expects 1 argument (radius)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("set_circle_shape expects 1 number argument (radius)");
            return 0;
        }

        Entity *entity = requireEntity(proc, "set_circle_shape");
        if (!entity)
            return 0;

        float radius = (float)args[0].asNumber();
        entity->setCircleShape(radius);
        return 0;
    }

    int native_set_collision_layer(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1)
        {
            Error("set_collision_layer expects 1 argument (layer)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("set_collision_layer expects 1 number argument (layer)");
            return 0;
        }

        Entity *entity = requireEntity(proc, "set_collision_layer");
        if (!entity)
            return 0;

        int layer = (int)args[0].asNumber();
        entity->setCollisionLayer(layer);
        return 0;
    }

    int native_set_collision_mask(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1)
        {
            Error("set_collision_mask expects 1 argument (mask)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("set_collision_mask expects 1 number argument (mask)");
            return 0;
        }

        Entity *entity = requireEntity(proc, "set_collision_mask");
        if (!entity)
            return 0;

        uint32 mask = (uint32)args[0].asNumber();
        entity->setCollisionMask(mask);
        return 0;
    }

    int native_add_collision_mask(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1)
        {
            Error("add_collision_mask expects 1 argument (layer)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("add_collision_mask expects 1 number argument (layer)");
            return 0;
        }

        Entity *entity = requireEntity(proc, "add_collision_mask");
        if (!entity)
            return 0;

        int layer = (int)args[0].asNumber();
        entity->addCollisionMask(layer);
        return 0;
    }

    int native_remove_collision_mask(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        (void)vm;
        if (argCount != 1)
        {
            Error("remove_collision_mask expects 1 argument (layer)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("remove_collision_mask expects 1 number argument (layer)");
            return 0;
        }

        Entity *entity = requireEntity(proc, "remove_collision_mask");
        if (!entity)
            return 0;

        int layer = (int)args[0].asNumber();
        entity->removeCollisionMask(layer);
        return 0;
    }

    int native_set_static(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        (void)vm;
        (void)args;
        if (argCount != 0)
        {
            Error("set_static expects 0 arguments");
            return 0;
        }

        Entity *entity = requireEntity(proc, "set_static");
        if (!entity)
            return 0;

        entity->setStatic();
        return 0;
    }

    int native_enable_collision(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        (void)vm;
        (void)args;
        if (argCount != 0)
        {
            Error("enable_collision expects 0 arguments");
            return 0;
        }

        Entity *entity = requireEntity(proc, "enable_collision");
        if (!entity)
            return 0;

        entity->enableCollision();
        return 0;
    }

    int native_disable_collision(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        (void)vm;
        (void)args;
        if (argCount != 0)
        {
            Error("disable_collision expects 0 arguments");
            return 0;
        }

        Entity *entity = requireEntity(proc, "disable_collision");
        if (!entity)
            return 0;

        entity->disableCollision();
        return 0;
    }

    int native_place_free(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("place_free expects 2 arguments (x, y)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("place_free expects 2 number arguments (x, y)");
            return 0;
        }

        Entity *entity = requireEntity(proc, "place_free");
        if (!entity)
            return 0;

        double x = args[0].asNumber();
        double y = args[1].asNumber();
        bool free = entity->place_free(x, y);
        vm->pushBool(free);
        return 1;
    }

    int native_place_meeting(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("place_meeting expects 2 arguments (x, y)");
            vm->pushBool(false);
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("place_meeting expects 2 number arguments (x, y)");
            vm->pushBool(false);
            return 1;
        }

        Entity *entity = requireEntity(proc, "place_meeting");
        if (!entity)
        {
            vm->pushBool(false);
            return 1;
        }

        double x = args[0].asNumber();
        double y = args[1].asNumber();
        Entity *hit = entity->place_meeting(x, y);
        if (!hit)
        {
            vm->pushBool(false);
            return 1;
        }

        Process *hitProc = (Process*)hit->userData;
        if (hitProc && hitProc->state != ProcessState::DEAD)
            vm->push(vm->makeProcessInstance(hitProc));
        else
            vm->pushBool(false);
        return 1;
    }

    int native_atach(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("atach expects 2 arguments (childProcID,front)");
            return 0;
        }
        if (!args[0].isProcessInstance())
        {
            Error("atach expects 1 integer argument (childProcID)  get %s", valueTypeToString(args[0].type));
            return 0;
        }

        bool front = args[1].asBool();
        

        
        
        Entity *entity = requireEntity(proc, "atach");
        if (!entity) return 0;
        
        Process *childProc = args[0].asProcess();
         if (!childProc || childProc->state == ProcessState::DEAD)
         {
             Error("atach: child process is dead or invalid");
             return 0;
         }
         if (!childProc->userData)
         {
             Error("atach: child process has no associated entity!");
             return 0;
         }

         Entity *childEntity = (Entity *)childProc->userData;
         gScene.moveEntityToParent(childEntity,entity,front);

        return 0;
    }

     int native_get_world_point(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("get_world_point expects 2 arguments (x, y)");
            vm->pushDouble(0);
            vm->pushDouble(0);
            return 2;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("get_world_point expects 2 number arguments (x, y)");
            vm->pushDouble(0);
            vm->pushDouble(0);
            return 2;   
        }

        if (proc->userData == nullptr)
        {
            vm->pushDouble(0);
            vm->pushDouble(0);

            return 2;
        }

        Entity *entity = (Entity *)proc->userData;
        Vector2 worldPoint = entity->getWorldPoint(args[0].asNumber(), args[1].asNumber());

        vm->pushDouble(worldPoint.x);
        vm->pushDouble(worldPoint.y);
        return 2;
    }
     int native_get_local_point(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("get_local_point expects 2 arguments (x, y)");
            vm->pushDouble(0);
            vm->pushDouble(0);
            return 2;
        }
        

        if (proc->userData == nullptr)
        {
            vm->pushDouble(0);
            vm->pushDouble(0);

            return 2;
        }

        Entity *entity = (Entity *)proc->userData;
        
        Vector2 localPoint = entity->getLocalPoint(args[0].asNumber(), args[1].asNumber());

        //        Info("get_real_point: pointIndex=%d realX=%f realY=%f", pointIndex, realPoint.x, realPoint.y);

        //DrawCircleLines((int)localPoint.x, (int)localPoint.y, 5, RED);

        vm->pushDouble(localPoint.x);
        vm->pushDouble(localPoint.y);
        return 2;
    }

    int native_out_screen(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("out_screen expects 0 arguments");
            vm->pushBool(false);
            return 1;
        }

        Entity *entity = requireEntity(proc, "out_screen");
        if (!entity)
        {
            vm->pushBool(false);
            return 1;
        }

        vm->pushBool(gScene.IsOutOfScreen(entity));
        return 1;
    }

    int native_set_layer(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("set_layer expects 1 argument (layer)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("set_layer expects 1 number argument (layer)");
            return 0;
        }

        Entity *entity = requireEntity(proc, "set_layer");
        if (!entity)
            return 0;

        int layer = (int)args[0].asNumber();
        entity->layer = layer;
        return 0;
    }

    int native_get_layer(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 0)
        {
            Error("get_layer expects 0 arguments");
            vm->pushInt(0);
            return 1;
        }

        Entity *entity = requireEntity(proc, "get_layer");
        if (!entity)
        {
            vm->pushInt(0);
            return 1;
        }

        vm->pushInt(entity->layer);
        return 1;
    }
 
    int native_mirror_vertical(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("flip_vertical expects 1 arguments");
            return 0;
        }

        Entity *entity = requireEntity(proc, "flip_vertical");
        if (!entity)
            return 0;

        bool flip = args[0].asBool();

        entity->flip_y= flip;
        return 0;
    }

    int native_mirror_horizontal(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("flip_horizontal expects 1 arguments");
            return 0;
        }

        Entity *entity = requireEntity(proc, "flip_horizontal");
        if (!entity)
            return 0;

        bool flip = args[0].asBool();

        entity->flip_x = flip;
        return 0;
    }

    int native_set_visible(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("set_visible expects 1 arguments");
            return 0;
        }

        Entity *entity = requireEntity(proc, "set_visible");
        if (!entity)
            return 0;

        bool visible = args[0].asBool();

        if (visible)
            entity->flags |= B_VISIBLE;
        else
            entity->flags &= ~B_VISIBLE;

        return 0;
    }

    int native_flip(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("flip expects 2 arguments (flipX, flipY)");
            return 0;
        }

        Entity *entity = requireEntity(proc, "flip");
        if (!entity)
            return 0;

        bool flipX = args[0].asBool();
        bool flipY = args[1].asBool();

        entity->flip_x = flipX;
        entity->flip_y = flipY;

        return 0;
    }
 
    


 


    // Helper: resolve target - accepts Process* (ValueType::PROCESS_INSTANCE) or type (ValueType::INT)
    // For type, finds the nearest process of that type
    static Process *resolveTarget(Interpreter *vm, Process *proc, Value &arg)
    {
        // Direct process pointer
        if (arg.isProcessInstance())
            return arg.asProcess();

        // Type (blueprint index) -> find nearest
        if (arg.isInt())
        {
            int blueprint = arg.asInt();
            double mx = proc->privates[0].asNumber();
            double my = proc->privates[1].asNumber();
            double bestDist = DBL_MAX;
            Process *best = nullptr;

            const auto &alive = vm->getAliveProcesses();
            for (size_t i = 0; i < alive.size(); i++)
            {
                Process *other = alive[i];
                if (!other || other == proc) continue;
                if (other->blueprint != blueprint) continue;
                if (other->state == ProcessState::DEAD || other->state == ProcessState::FROZEN) continue;

                double dx = other->privates[0].asNumber() - mx;
                double dy = other->privates[1].asNumber() - my;
                double d = dx * dx + dy * dy;
                if (d < bestDist)
                {
                    bestDist = d;
                    best = other;
                }
            }
            return best;
        }

        return nullptr;
    }

    // get_nearest(type X) -> nearest process of type X
    int native_get_nearest(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isInt())
        {
            Error("get_nearest expects 1 argument (type)");
            vm->pushBool(false);
            return 1;
        }

        Process *target = resolveTarget(vm, proc, args[0]);
        if (!target)
        {
            vm->pushBool(false);
            return 1;
        }

        vm->push(vm->makeProcessInstance(target));
        return 1;
    }

    // fget_angle(process | type) -> angle from this process to target (degrees)
    int native_fget_angle(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("fget_angle expects 1 argument (process or type)");
            vm->pushDouble(0);
            return 1;
        }

        Process *target = resolveTarget(vm, proc, args[0]);
        if (!target)
        {
            vm->pushDouble(0);
            return 1;
        }

        double x1 = proc->privates[0].asNumber();
        double y1 = proc->privates[1].asNumber();
        double x2 = target->privates[0].asNumber();
        double y2 = target->privates[1].asNumber();

        double dx = x2 - x1;
        double dy = -(y2 - y1);
        double angle = atan2(dy, dx) * 180.0 / PI;
        vm->pushDouble(angle);
        return 1;
    }

    // fget_dist(process | type) -> distance from this process to target
    int native_fget_dist(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 1)
        {
            Error("fget_dist expects 1 argument (process or type)");
            vm->pushDouble(0);
            return 1;
        }

        Process *target = resolveTarget(vm, proc, args[0]);
        if (!target)
        {
            vm->pushDouble(0);
            return 1;
        }

        double x1 = proc->privates[0].asNumber();
        double y1 = proc->privates[1].asNumber();
        double x2 = target->privates[0].asNumber();
        double y2 = target->privates[1].asNumber();

        double dx = x2 - x1;
        double dy = y2 - y1;
        vm->pushDouble(sqrt(dx * dx + dy * dy));
        return 1;
    }

    // turn_to(process | type, step) -> rotate angle toward target by step degrees
    int native_turn_to(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 2)
        {
            Error("turn_to expects 2 arguments (target, step)");
            return 0;
        }

        double step = fabs(args[1].asNumber());
        Process *target = resolveTarget(vm, proc, args[0]);
        if (!target)
            return 0;

        double x1 = proc->privates[0].asNumber();
        double y1 = proc->privates[1].asNumber();
        double x2 = target->privates[0].asNumber();
        double y2 = target->privates[1].asNumber();

        double dx = x2 - x1;
        double dy = -(y2 - y1);
        double targetAngle = atan2(dy, dx) * 180.0 / PI;
        double current = proc->privates[4].asNumber();

        double diff = fmod(targetAngle - current, 360.0);
        if (diff > 180.0) diff -= 360.0;
        if (diff < -180.0) diff += 360.0;

        double newAngle;
        if (fabs(diff) <= step)
            newAngle = targetAngle;
        else
            newAngle = current + (diff > 0 ? step : -step);

        proc->privates[4] = vm->makeDouble(newAngle);
        return 0;
    }

    int native_let_me_alone(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        (void)argCount;
        (void)args;
        const auto &alive = vm->getAliveProcesses();
        for (size_t i = 0; i < alive.size(); i++)
        {
            Process *other = alive[i];
            if (other && other != proc)
                other->state = ProcessState::DEAD;
        }
        return 0;
    }

 

    int native_collision(Interpreter *vm, Process *proc, int argCount, Value *args)
    {
        if (argCount != 3 || !args[0].isInt() || !args[1].isNumber() || !args[2].isNumber())
        {
            vm->pushBool(false);
            return 1;
        }

        if(proc->state == ProcessState::FROZEN || proc->state == ProcessState::DEAD)
        {
           
            vm->pushBool(false);
            return 1;
        }

        Entity *entity = requireEntity(proc, "collision");
        if (!entity || !entity->shape || !(entity->flags & B_COLLISION) || !entity->ready)
        {
            vm->pushBool(false);
            return 1;
        }

        int targetBlueprint = args[0].asInt();
        double x = args[1].asNumber();
        double y = args[2].asNumber();

        // Guarda posição original
        double old_x = entity->x, old_y = entity->y;
        entity->x = x;
        entity->y = y;
        entity->markTransformDirty();
        entity->updateBounds();

        // Tilemap collision
        if (entity->collide_with_tiles(entity->getBounds()))
        {
            entity->x = old_x;
            entity->y = old_y;
            entity->markTransformDirty();
            entity->bounds_dirty = true;
            vm->pushBool(false);
            return 1;
        }

        // Broadphase: quadtree + dinâmicas, pré-filtradas por blueprint
        static std::vector<Entity *> nearby;
        nearby.clear();
        if (gScene.staticTree)
            gScene.staticTree->query(entity->getBounds(), nearby);
        for (Entity *dyn : gScene.dynamicEntities)
        {
            if (dyn != entity && dyn->blueprint == targetBlueprint)
                nearby.push_back(dyn);
        }

        for (Entity *other : nearby)
        {
            if (!other || other == entity) continue;
            if (!other->shape || !(other->flags & B_COLLISION)) continue;
            if (other->flags & B_DEAD) continue;
            if (other->procID < 0) continue;

            Process *otherProc = (Process*)other->userData;
            if (!otherProc || otherProc->state == ProcessState::DEAD) continue;
            if (otherProc->blueprint != targetBlueprint) continue;
 

            if (CheckCollisionRecs(entity->getBounds(), other->getBounds()))
            {
                if (entity->intersects(other))
                {
                    entity->x = old_x;
                    entity->y = old_y;
                    entity->markTransformDirty();
                    entity->bounds_dirty = true;
                    vm->push(vm->makeProcessInstance(otherProc));
                    return 1;
                }
            }
        }

        // Restaura posição
        entity->x = old_x;
        entity->y = old_y;
        entity->markTransformDirty();
        entity->bounds_dirty = true;

        vm->pushBool(false);
        return 1;
    }

    void registerAll(Interpreter &vm)
    {
        vm.registerNativeProcess("advance", native_advance, 1);
        vm.registerNativeProcess("xadvance", native_xadvance, 2);
        vm.registerNativeProcess("get_point", native_get_point, 1);
        vm.registerNativeProcess("get_real_point", native_get_real_point, 1);
        vm.registerNativeProcess("set_rect_shape", native_set_rect_shape, 4);
        vm.registerNativeProcess("get_local_point", native_get_local_point, 2);
        vm.registerNativeProcess("get_world_point", native_get_world_point, 2);
        vm.registerNativeProcess("set_circle_shape", native_set_circle_shape, 1);
        vm.registerNativeProcess("set_collision_layer", native_set_collision_layer, 1);
        vm.registerNativeProcess("set_collision_mask", native_set_collision_mask, 1);
        vm.registerNativeProcess("add_collision_mask", native_add_collision_mask, 1);
        vm.registerNativeProcess("remove_collision_mask", native_remove_collision_mask, 1);
        vm.registerNativeProcess("set_static", native_set_static, 0);
        vm.registerNativeProcess("enable_collision", native_enable_collision, 0);
        vm.registerNativeProcess("disable_collision", native_disable_collision, 0);
        vm.registerNativeProcess("place_free", native_place_free, 2);
        vm.registerNativeProcess("place_meeting", native_place_meeting, 2);
        vm.registerNativeProcess("collision", native_collision, 3);
        vm.registerNativeProcess("atach", native_atach, 2);
        vm.registerNativeProcess("out_screen", native_out_screen, 0);
        vm.registerNativeProcess("set_layer", native_set_layer, 1);
        vm.registerNativeProcess("get_layer", native_get_layer, 0);
        vm.registerNativeProcess("let_me_alone", native_let_me_alone, 0);

        vm.registerNativeProcess("flip_vertical", native_mirror_vertical, 1);
        vm.registerNativeProcess("flip_horizontal", native_mirror_horizontal, 1);
        vm.registerNativeProcess("set_visible", native_set_visible, 1);
        vm.registerNativeProcess("flip", native_flip, 2);

        // Game math (process-aware, DIV-style)
        vm.registerNativeProcess("get_nearest", native_get_nearest, 1);
        vm.registerNativeProcess("fget_angle", native_fget_angle, 1);
        vm.registerNativeProcess("fget_dist", native_fget_dist, 1);
        vm.registerNativeProcess("turn_to", native_turn_to, 2);

    }
}
