#include "bindings.hpp"
#include "engine.hpp"
#include <raylib.h>

extern ParticleSystem gParticleSystem;
namespace BindingsParticles
{


    int native_set_position(Interpreter *vm, void *data, int argCount, Value *args)

    {
        Emitter *emitter = static_cast<Emitter *>(data);
        if (argCount != 2)
        {
            Error("set_position expects 2 arguments (x, y)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("set_position expects 2 number arguments (x, y)");
            return 0;
        }
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        emitter->setPosition(x, y);

        return 0;
    }

    int native_set_direction(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Emitter *emitter = static_cast<Emitter *>(data);
        if (argCount != 2)
        {
            Error("set_direction expects 2 arguments (x, y)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("set_direction expects 2 number arguments (x, y)");
            return 0;
        }
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        emitter->setDirection(x, y);

        return 0;
    }

    int native_set_emission_rate(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Emitter *emitter = static_cast<Emitter *>(data);
        if (argCount != 1)
        {
            Error("set_emission_rate expects 1 argument (rate)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("set_emission_rate expects a number argument (rate)");
            return 0;
        }
        float rate = (float)args[0].asNumber();
        emitter->setEmissionRate(rate);

        return 0;
    }

    int native_set_life(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Emitter *emitter = static_cast<Emitter *>(data);
        if (argCount != 1)
        {
            Error("set_life expects 1 argument (life)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("set_life expects a number argument (life)");
            return 0;
        }
        float life = (float)args[0].asNumber();
        emitter->setLife(life);

        return 0;
    }

    int native_set_speed_range(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Emitter *emitter = static_cast<Emitter *>(data);
        if (argCount != 2)
        {
            Error("set_speed_range expects 2 arguments (min, max)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("set_speed_range expects 2 number arguments (min, max)");
            return 0;
        }
        float min = (float)args[0].asNumber();
        float max = (float)args[1].asNumber();
        emitter->setSpeedRange(min, max);

        return 0;
    }

    int native_set_spread(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Emitter *emitter = static_cast<Emitter *>(data);
        if (argCount != 1)
        {
            Error("set_spread expects 1 argument (radians)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("set_spread expects a number argument (radians)");
            return 0;
        }
        float radians = (float)args[0].asNumber();
        emitter->setSpread(radians);

        return 0;
    }

    int native_set_color_curve(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Emitter *emitter = static_cast<Emitter *>(data);
        if (argCount != 2)
        {
            Error("set_color_curve expects 2 arguments (startColor, endColor)");
            return 0;
        }
        if (!args[0].isNativeStructInstance() || !args[1].isNativeStructInstance())
        {
            Error("set_color_curve expects 2     arguments (startColor, endColor)");
            return 0;
        }
        auto *inst = args[0].asNativeStructInstance();
        Color *startColor = (Color *)inst->data;
        inst = args[1].asNativeStructInstance();
        Color *endColor = (Color *)inst->data;
        emitter->setColorCurve(*startColor, *endColor);

        return 0;
    }

    int native_set_size_curve(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Emitter *emitter = static_cast<Emitter *>(data);
        if (argCount != 2)
        {
            Error("set_size_curve expects 2 arguments (startSize, endSize)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("set_size_curve expects 2 number arguments (startSize, endSize)");
            return 0;
        }
        float startSize = (float)args[0].asNumber();
        float endSize = (float)args[1].asNumber();
        emitter->setSizeCurve(startSize, endSize);

        return 0;
    }

    int native_set_spawn_zone(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Emitter *emitter = static_cast<Emitter *>(data);
        if (argCount != 4)
        {
            Error("set_spawn_zone expects 4 arguments (x, y, w, h)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber() || !args[3].isNumber())
        {
            Error("set_spawn_zone expects 4 number arguments (x, y, w, h)");
            return 0;
        }
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        float w = (float)args[2].asNumber();
        float h = (float)args[3].asNumber();
        emitter->setSpawnZone(x, y, w, h);

        return 0;
    }

    int native_set_lifetime(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Emitter *emitter = static_cast<Emitter *>(data);
        if (argCount != 1)
        {
            Error("set_lifetime expects 1 argument (time)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("set_lifetime expects a number argument (time)");
            return 0;
        }
        float time = (float)args[0].asNumber();
        emitter->setLifeTime(time);

        return 0;
    }

    int native_set_gravity(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Emitter *emitter = static_cast<Emitter *>(data);
        if (argCount != 2)
        {
            Error("set_gravity expects 2 arguments (x, y)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("set_gravity expects 2 number arguments (x, y)");
            return 0;
        }
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        emitter->setGravity(x, y);

        return 0;
    }

    int native_set_drag(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Emitter *emitter = static_cast<Emitter *>(data);
        if (argCount != 1)
        {
            Error("set_drag expects 1 argument (drag)");
            return 0;
        }
        if (!args[0].isNumber())
        {
            Error("set_drag expects a number argument (drag)");
            return 0;
        }
        float drag = (float)args[0].asNumber();
        emitter->setDrag(drag);

        return 0;
    }

    int native_set_rotation_range(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Emitter *emitter = static_cast<Emitter *>(data);
        if (argCount != 2)
        {
            Error("set_rotation_range expects 2 arguments (min, max)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("set_rotation_range expects 2 number arguments (min, max)");
            return 0;
        }
        float min = (float)args[0].asNumber();
        float max = (float)args[1].asNumber();
        emitter->setRotationRange(min, max);

        return 0;
    }

    int native_set_angular_vel_range(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Emitter *emitter = static_cast<Emitter *>(data);
        if (argCount != 2)
        {
            Error("set_angular_vel_range expects 2 arguments (min, max)");
            return 0;
        }
        if (!args[0].isNumber() || !args[1].isNumber())
        {
            Error("set_angular_vel_range expects 2 number arguments (min, max)");
            return 0;
        }
        float min = (float)args[0].asNumber();
        float max = (float)args[1].asNumber();
        emitter->setAngularVelRange(min, max);

        return 0;
    }

    int native_set_blend_mode(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Emitter *emitter = static_cast<Emitter *>(data);
        if (argCount != 1)
        {
            Error("set_blend_mode expects 1 argument (blendMode)");
            return 0;
        }
        if (!args[0].isInt())
        {
            Error("set_blend_mode expects an integer argument (blendMode)");
            return 0;
        }
        int mode = (int)args[0].asInt();
        emitter->setBlendMode(static_cast<BlendMode>(mode));

        return 0;
    }

    int native_set_layer(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Emitter *emitter = static_cast<Emitter *>(data);
        if (argCount != 1)
        {
            Error("set_layer expects 1 argument (layer)");
            return 0;
        }
        if (!args[0].isInt())
        {
            Error("set_layer expects an integer argument (layer)");
            return 0;
        }
        int layer = (int)args[0].asInt();
        emitter->setLayer(layer);

        return 0;
    }

    int native_create_sparks(Interpreter *vm, int argCount, Value *args)
    {
         NativeClassDef * emitterClassDef  = nullptr;
        if (!vm->tryGetNativeClassDef("Emitter", &emitterClassDef))
        {
            Error("Emitter class is not registered !");
            vm->pushNil();
            return 1;
        }

        // Implementação de exemplo para criar faíscas
        if (argCount != 4)
        {
            Error("create_sparks expects 4 arguments (x, y, graph, color)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isInt() || !args[3].isNativeStructInstance())
        {
            Error("create_sparks expects arguments ( x,  y,  graph,  color)");
            vm->pushNil();
            return 1;
        }
       
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        int graph = (int)args[2].asInt();
        auto *inst = args[3].asNativeStructInstance();
        Color *color = (Color *)inst->data;

        Emitter *emitter = gParticleSystem.createSparks({x, y}, graph, *color);
       
         Value nodeValue = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = nodeValue.asNativeClassInstance();
        instance->klass = emitterClassDef;
        instance->userData = (void *)emitter;
        vm->push(nodeValue);

        return 1;
    }

   
    int native_create_fire(Interpreter *vm, int argCount, Value *args)
    {
        // Implementação de exemplo para criar fogo
        if (argCount != 3)
        {
            Error("create_fire expects 3 arguments (x, y, graph)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isInt())
        {
            Error("create_fire expects arguments (x, y, graph)");
            vm->pushNil();
            return 1;
        }
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        int graph = (int)args[2].asInt();
        Emitter *emitter = gParticleSystem.createFire({x, y}, graph);

        // Retorna a instância do emissor para o script
        NativeClassDef * emitterClassDef  = nullptr;
        if (!vm->tryGetNativeClassDef("Emitter", &emitterClassDef))
        {
            Error("Emitter class is not registered !");
            vm->pushNil();
            return 1;
        }

         Value nodeValue = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = nodeValue.asNativeClassInstance();
        instance->klass = emitterClassDef;
        instance->userData = (void *)emitter;
        vm->push(nodeValue);

        return 1;
    }

    int native_create_smoke(Interpreter *vm, int argCount, Value *args)
    {
       
        if (argCount != 3)
        {
            Error("create_smoke expects 3 arguments (x, y, graph)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isInt())
        {
            Error("create_smoke expects arguments (x, y, graph)");
            vm->pushNil();
            return 1;
        }
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        int graph = (int)args[2].asInt();
        Emitter *emitter = gParticleSystem.createSmoke({x, y}, graph);

        // Retorna a instância do emissor para o script
        NativeClassDef * emitterClassDef  = nullptr;
        if (!vm->tryGetNativeClassDef("Emitter", &emitterClassDef))
        {
            Error("Emitter class is not registered !");
            vm->pushNil();
            return 1;
        }

         Value nodeValue = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = nodeValue.asNativeClassInstance();
        instance->klass = emitterClassDef;
        instance->userData = (void *)emitter;
        vm->push(nodeValue);

        return 1;
    }

    int native_create_explosion(Interpreter *vm, int argCount, Value *args)
    {
         NativeClassDef * emitterClassDef  = nullptr;
        if (!vm->tryGetNativeClassDef("Emitter", &emitterClassDef))
        {
            Error("Emitter class is not registered !");
            vm->pushNil();
            return 1;
        }

        // Implementação de exemplo para criar uma explosão
        if (argCount != 4)
        {
            Error("create_explosion expects 4 arguments (x, y, graph, color)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isInt() || !args[3].isNativeStructInstance())
        {
            Error("create_explosion expects arguments ( x,  y,  graph,  color)");
            vm->pushNil();
            return 1;
        }
       
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        int graph = (int)args[2].asInt();
        auto *inst = args[3].asNativeStructInstance();
        Color *color = (Color *)inst->data;

        Emitter *emitter = gParticleSystem.createExplosion({x, y}, graph, *color);
       
         Value nodeValue = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = nodeValue.asNativeClassInstance();
        instance->klass = emitterClassDef;
        instance->userData = (void *)emitter;
        vm->push(nodeValue);

        return 1;
    }

    int native_create_muzzle_flash(Interpreter *vm, int argCount, Value *args)
    {
         NativeClassDef * emitterClassDef  = nullptr;
        if (!vm->tryGetNativeClassDef("Emitter", &emitterClassDef))
        {
            Error("Emitter class is not registered !");
            vm->pushNil();
            return 1;
        }

        // Implementação de exemplo para criar um flash de arma
        if (argCount != 4)
        {
            Error("create_muzzle_flash expects 4 arguments (x, y, graph, shootDirection)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isInt() || !args[3].isNativeStructInstance())
        {
            Error("create_muzzle_flash expects arguments ( x,  y,  graph, shootDirection)");
            vm->pushNil();
            return 1;
        }
       
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        int graph = (int)args[2].asInt();
        auto *inst = args[3].asNativeStructInstance();
        Vector2 *shootDirection = (Vector2 *)inst->data;

        Emitter *emitter = gParticleSystem.createMuzzleFlash({x, y}, graph, *shootDirection);
       
         Value nodeValue = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = nodeValue.asNativeClassInstance();
        instance->klass = emitterClassDef;
        instance->userData = (void *)emitter;
        vm->push(nodeValue);

        return 1;
    }

    int native_create_shell_ejection(Interpreter *vm, int argCount, Value *args)
    {
         NativeClassDef * emitterClassDef  = nullptr;
        if (!vm->tryGetNativeClassDef("Emitter", &emitterClassDef))
        {
            Error("Emitter class is not registered !");
            vm->pushNil();
            return 1;
        }

        // Implementação de exemplo para criar uma ejeção de cartucho
        if (argCount != 4)
        {
            Error("create_shell_ejection expects 4 arguments (x, y, graph, facingRight)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isInt() || !args[3].isBool())
        {
            Error("create_shell_ejection expects arguments ( x,  y,  graph, facingRight)");
            vm->pushNil();
            return 1;
        }
       
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        int graph = (int)args[2].asInt();
        bool facingRight = args[3].as.boolean;

        Emitter *emitter = gParticleSystem.createShellEjection({x, y}, graph, facingRight);
       
         Value nodeValue = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = nodeValue.asNativeClassInstance();
        instance->klass = emitterClassDef;
        instance->userData = (void *)emitter;
        vm->push(nodeValue);

        return 1;
    }

 

    int native_create_emitter(Interpreter *vm, int argCount, Value *args)
    {
         NativeClassDef * emitterClassDef  = nullptr;
        if (!vm->tryGetNativeClassDef("Emitter", &emitterClassDef))
        {
            Error("Emitter class is not registered !");
            vm->pushNil();
            return 1;
        }

        //persitente, graph, maxParticles
        if (argCount != 3)
        {
            Error("create_emitter expects 3 arguments (persistent, graph, maxParticles)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isBool() || !args[1].isInt() || !args[2].isInt())
        {
            Error("create_emitter expects arguments (bool persistent, int graph, int maxParticles)");
            vm->pushNil();
            return 1;
        }
        bool persistent = args[0].as.boolean;
        int graph = (int)args[1].asInt();
        int maxParticles = (int)args[2].asInt();
        EmitterType type = persistent ? EMITTER_CONTINUOUS : EMITTER_ONESHOT;
        Emitter *emitter = gParticleSystem.spawn(type, graph, maxParticles);

        Value nodeValue = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = nodeValue.asNativeClassInstance();
        instance->klass = emitterClassDef;
        instance->userData = (void *)emitter;
        vm->push(nodeValue);
        

        return 1;
    }
    // Impactos e Colisões

    int native_create_landing_dust(Interpreter *vm, int argCount, Value *args)
    {
       NativeClassDef * emitterClassDef  = nullptr;
        if (!vm->tryGetNativeClassDef("Emitter", &emitterClassDef))
        {
            Error("Emitter class is not registered !");
            vm->pushNil();
            return 1;
        }

       
        if (argCount != 4)
        {
            Error("create_landing_dust expects 4 arguments (x, y, graph, facingRight)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isInt() || !args[3].isBool())
        {
            Error("create_landing_dust expects arguments ( x,  y,  graph,  facingRight)");
            vm->pushNil();
            return 1;
        }
       
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        int graph = (int)args[2].asInt();
        bool facingRight = args[3].as.boolean;
        

        Emitter *emitter = gParticleSystem.createLandingDust({x, y}, graph,  facingRight);
       
         Value nodeValue = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = nodeValue.asNativeClassInstance();
        instance->klass = emitterClassDef;
        instance->userData = (void *)emitter;
        vm->push(nodeValue);

        return 1;
    }

    int native_create_wall_impact(Interpreter *vm, int argCount, Value *args)
    {
       NativeClassDef * emitterClassDef  = nullptr;
        if (!vm->tryGetNativeClassDef("Emitter", &emitterClassDef))
        {
            Error("Emitter class is not registered !");
            vm->pushNil();
            return 1;
        }

       
        if (argCount != 6)
        {
            Error("create_wall_impact expects 6 arguments (x, y, graph, hitFromLeft,size_start,size_end)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isInt() || !args[3].isBool() || !args[4].isNumber() || !args[5].isNumber())
        {
            Error("create_wall_impact expects arguments ( x,  y,  graph,  hitFromLeft)");
            vm->pushNil();
            return 1;
        }
       
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        int graph = (int)args[2].asInt();
        bool hitFromLeft = args[3].as.boolean;
        float size_start = (float)args[4].asNumber();
        float size_end = (float)args[5].asNumber();

        Emitter *emitter = gParticleSystem.createWallImpact({x, y}, graph,  hitFromLeft, size_start, size_end);
       
         Value nodeValue = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = nodeValue.asNativeClassInstance();
        instance->klass = emitterClassDef;
        instance->userData = (void *)emitter;
        vm->push(nodeValue);

        return 1;
    }

    int native_create_water_splash(Interpreter *vm, int argCount, Value *args)
    {
       NativeClassDef * emitterClassDef  = nullptr;
        if (!vm->tryGetNativeClassDef("Emitter", &emitterClassDef))
        {
            Error("Emitter class is not registered !");
            vm->pushNil();
            return 1;
        }

       
        if (argCount != 3)
        {
            Error("create_water_splash expects 3 arguments (x, y, graph)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isInt())
        {
            Error("create_water_splash expects arguments ( x,  y,  graph)");
            vm->pushNil();
            return 1;
        }
       
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        int graph = (int)args[2].asInt();
        

        Emitter *emitter = gParticleSystem.createWaterSplash({x, y}, graph);
       
         Value nodeValue = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = nodeValue.asNativeClassInstance();
        instance->klass = emitterClassDef;
        instance->userData = (void *)emitter;
        vm->push(nodeValue);

        return 1;
    }

 
    
    // // Movimento do Player


    int native_create_run_trail(Interpreter *vm, int argCount, Value *args)
    {
       NativeClassDef * emitterClassDef  = nullptr;
        if (!vm->tryGetNativeClassDef("Emitter", &emitterClassDef))
        {
            Error("Emitter class is not registered !");
            vm->pushNil();
            return 1;
        }

       
        if (argCount != 5)
        {
            Error("create_run_trail expects 5 arguments (x, y, graph, size_min, size_max)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isInt() || !args[3].isNumber() || !args[4].isNumber())
        {
            Error("create_run_trail expects arguments ( x,  y,  graph, size_min, size_max)");
            vm->pushNil();
            return 1;
        }
       
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        int graph = (int)args[2].asInt();
        float size_min = (float)args[3].asNumber();
        float size_max = (float)args[4].asNumber();
        

        Emitter *emitter = gParticleSystem.createRunTrail({x, y}, graph, size_min, size_max);
       
         Value nodeValue = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = nodeValue.asNativeClassInstance();
        instance->klass = emitterClassDef;
        instance->userData = (void *)emitter;
        vm->push(nodeValue);

        return 1;
    }

    int native_create_speed_lines(Interpreter *vm, int argCount, Value *args)
    {
       NativeClassDef * emitterClassDef  = nullptr;
        if (!vm->tryGetNativeClassDef("Emitter", &emitterClassDef))
        {
            Error("Emitter class is not registered !");
            vm->pushNil();
            return 1;
        }

       
        if (argCount != 4)
        {
            Error("create_speed_lines expects 4 arguments (x, y, graph, velX, velY)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isInt() || !args[3].isNumber() || !args[4].isNumber())
        {
            Error("create_speed_lines expects arguments ( x,  y,  graph, velX, velY)");
            vm->pushNil();
            return 1;
        }
       
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        int graph = (int)args[2].asInt();
        float velX = (float)args[3].asNumber();
        float velY = (float)args[4].asNumber();
        Vector2 velocity = {velX, velY};

        Emitter *emitter = gParticleSystem.createSpeedLines({x, y}, graph, velocity);
       
         Value nodeValue = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = nodeValue.asNativeClassInstance();
        instance->klass = emitterClassDef;
        instance->userData = (void *)emitter;
        vm->push(nodeValue);

        return 1;
    }

 
    // // Coleta e Power-ups


    int native_create_collect_effect(Interpreter *vm, int argCount, Value *args)
    {
       NativeClassDef * emitterClassDef  = nullptr;
        if (!vm->tryGetNativeClassDef("Emitter", &emitterClassDef))
        {
            Error("Emitter class is not registered !");
            vm->pushNil();
            return 1;
        }

       
        if (argCount != 4)
        {
            Error("create_collect_effect expects 4 arguments (x, y, graph, itemColor)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isInt() || !args[3].isNativeStructInstance())
        {
            Error("create_collect_effect expects arguments ( x,  y,  graph,  itemColor)");
            vm->pushNil();
            return 1;
        }
       
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        int graph = (int)args[2].asInt();
        auto *inst = args[3].asNativeStructInstance();
        Color *itemColor = (Color *)inst->data;
        

        Emitter *emitter = gParticleSystem.createCollectEffect({x, y}, graph,  *itemColor);
       
         Value nodeValue = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = nodeValue.asNativeClassInstance();
        instance->klass = emitterClassDef;
        instance->userData = (void *)emitter;
        vm->push(nodeValue);

        return 1;
    }

    int native_create_power_up_aura(Interpreter *vm, int argCount, Value *args)
    {
       NativeClassDef * emitterClassDef  = nullptr;
        if (!vm->tryGetNativeClassDef("Emitter", &emitterClassDef))
        {
            Error("Emitter class is not registered !");
            vm->pushNil();
            return 1;
        }

       
        if (argCount != 4)
        {
            Error("create_power_up_aura expects 4 arguments (x, y, graph, auraColor)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isInt() || !args[3].isNativeStructInstance())
        {
            Error("create_power_up_aura expects arguments ( x,  y,  graph,  auraColor)");
            vm->pushNil();
            return 1;
        }
       
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        int graph = (int)args[2].asInt();
        auto *inst = args[3].asNativeStructInstance();
        Color *auraColor = (Color *)inst->data;
        

        Emitter *emitter = gParticleSystem.createPowerUpAura({x, y}, graph,  *auraColor);
       
         Value nodeValue = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = nodeValue.asNativeClassInstance();
        instance->klass = emitterClassDef;
        instance->userData = (void *)emitter;
        vm->push(nodeValue);

        return 1;
    }

    int native_create_sparkle(Interpreter *vm, int argCount, Value *args)
    {
       NativeClassDef * emitterClassDef  = nullptr;
        if (!vm->tryGetNativeClassDef("Emitter", &emitterClassDef))
        {
            Error("Emitter class is not registered !");
            vm->pushNil();
            return 1;
        }

       
        if (argCount != 3)
        {
            Error("create_sparkle expects 3 arguments (x, y, graph)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isInt())
        {
            Error("create_sparkle expects arguments ( x,  y,  graph)");
            vm->pushNil();
            return 1;
        }
       
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        int graph = (int)args[2].asInt();
        

        Emitter *emitter = gParticleSystem.createSparkle({x, y}, graph);
       
         Value nodeValue = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = nodeValue.asNativeClassInstance();
        instance->klass = emitterClassDef;
        instance->userData = (void *)emitter;
        vm->push(nodeValue);

        return 1;
    }


    // // Dano e Combat

    int native_create_blood_splatter(Interpreter *vm, int argCount, Value *args)
    {
       NativeClassDef * emitterClassDef  = nullptr;
        if (!vm->tryGetNativeClassDef("Emitter", &emitterClassDef))
        {
            Error("Emitter class is not registered !");
            vm->pushNil();
            return 1;
        }

       
        if (argCount != 4)
        {
            Error("create_blood_splatter expects 4 arguments (x, y, graph, hitDirectionX, hitDirectionY)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isInt() || !args[3].isNumber() || !args[4].isNumber())
        {
            Error("create_blood_splatter expects arguments ( x,  y,  graph, hitDirectionX, hitDirectionY)");
            vm->pushNil();
            return 1;
        }
       
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        int graph = (int)args[2].asInt();
        float dirX = (float)args[3].asNumber();
        float dirY = (float)args[4].asNumber();
        Vector2 hitDirection = {dirX, dirY};

        Emitter *emitter = gParticleSystem.createBloodSplatter({x, y}, graph, hitDirection);
       
         Value nodeValue = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = nodeValue.asNativeClassInstance();
        instance->klass = emitterClassDef;
        instance->userData = (void *)emitter;
        vm->push(nodeValue);

        return 1;
    }

    
    
    // // Ambiente
    

    int native_create_rain(Interpreter *vm, int argCount, Value *args)
    {
       NativeClassDef * emitterClassDef  = nullptr;
        if (!vm->tryGetNativeClassDef("Emitter", &emitterClassDef))
        {
            Error("Emitter class is not registered !");
            vm->pushNil();
            return 1;
        }

       
        if (argCount != 4)
        {
            Error("create_rain expects 4 arguments (x, y, graph, width)");
            vm->pushNil();
            return 1;
        }
        if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isInt() || !args[3].isNumber())
        {
            Error("create_rain expects arguments ( x,  y,  graph, width)");
            vm->pushNil();
            return 1;
        }
       
        float x = (float)args[0].asNumber();
        float y = (float)args[1].asNumber();
        int graph = (int)args[2].asInt();
        float width = (float)args[3].asNumber();
        

        Emitter *emitter = gParticleSystem.createRain({x, y}, graph, width);
       
         Value nodeValue = vm->makeNativeClassInstance(false);
        NativeClassInstance *instance = nodeValue.asNativeClassInstance();
        instance->klass = emitterClassDef;
        instance->userData = (void *)emitter;
        vm->push(nodeValue);

        return 1;
    }

    void* ctor_native_create_emitter(Interpreter *vm, int argCount, Value *args)
    {
           
            //persitente, graph, maxParticles
            if (argCount != 3)
            {
                Error("create_emitter expects 3 arguments (persistent, graph, maxParticles)");
                vm->pushNil();
                return nullptr;
            }
            if (!args[0].isBool() || !args[1].isInt() || !args[2].isInt())
            {
                Error("create_emitter expects arguments (bool persistent, int graph, int maxParticles)");
                vm->pushNil();
                return nullptr;
            }
            bool persistent = args[0].as.boolean;
            int graph = (int)args[1].asInt();
            int maxParticles = (int)args[2].asInt();
            EmitterType type = persistent ? EMITTER_CONTINUOUS : EMITTER_ONESHOT;
            Emitter *emitter = gParticleSystem.spawn(type, graph, maxParticles);
   
            Warning("Emitter created from constructor, but it's recommended to use create_emitter native function for better error handling and integration with the scripting environment.");
        
            
            return emitter;
    }

    void dtor_native_destroy_emitter(Interpreter *vm, void* data)
    {
        
        
    }
    

    int native_stop(Interpreter *vm, void *data, int argCount, Value *args)
    {
        Emitter *emitter = static_cast<Emitter *>(data);
        emitter->stop();
        return 0;
    }

    void registerAll(Interpreter &vm)
    {

        NativeClassDef *emitter = vm.registerNativeClass(
            "Emitter",
            ctor_native_create_emitter,
            dtor_native_destroy_emitter,
            3,
            false);

        //   vm.addNativeProperty(animState, "enabled", animState_getEnabled, animState_setEnabledProp);
        // vm.addNativeProperty(animState, "loop", animState_getLoop, animState_setLoopProp);
        // vm.addNativeProperty(animState, "weight", animState_getWeight, animState_setWeight);

        vm.addNativeMethod(emitter, "set_position", native_set_position);
        vm.addNativeMethod(emitter, "set_direction", native_set_direction);
        vm.addNativeMethod(emitter, "set_emission_rate", native_set_emission_rate);
        vm.addNativeMethod(emitter, "set_life", native_set_life);
        vm.addNativeMethod(emitter, "set_speed_range", native_set_speed_range);
        vm.addNativeMethod(emitter, "set_spread", native_set_spread);
         vm.addNativeMethod(emitter, "set_color_curve", native_set_color_curve);
        vm.addNativeMethod(emitter, "set_size_curve", native_set_size_curve);
        vm.addNativeMethod(emitter, "set_spawn_zone", native_set_spawn_zone);
        vm.addNativeMethod(emitter, "set_lifetime", native_set_lifetime);
        vm.addNativeMethod(emitter, "set_gravity", native_set_gravity);
        vm.addNativeMethod(emitter, "set_drag", native_set_drag);
        vm.addNativeMethod(emitter, "set_rotation_range", native_set_rotation_range);
        vm.addNativeMethod(emitter, "set_angular_vel_range", native_set_angular_vel_range);
        vm.addNativeMethod(emitter, "set_blend_mode", native_set_blend_mode);
        vm.addNativeMethod(emitter, "set_layer", native_set_layer);
        vm.addNativeMethod(emitter, "stop", native_stop);

        vm.registerNative("create_emitter", native_create_emitter, 3);
        vm.registerNative("create_fire", native_create_fire, 3);
        vm.registerNative("create_smoke", native_create_smoke, 3);
        vm.registerNative("create_explosion", native_create_explosion, 4);
        vm.registerNative("create_sparks", native_create_sparks, 4);
        vm.registerNative("create_landing_dust", native_create_landing_dust, 4);
        vm.registerNative("create_wall_impact", native_create_wall_impact, 6);
        vm.registerNative("create_water_splash", native_create_water_splash, 3);
        vm.registerNative("create_run_trail", native_create_run_trail, 5);
        vm.registerNative("create_speed_lines", native_create_speed_lines, 5);
        vm.registerNative("create_collect_effect", native_create_collect_effect, 4);
        vm.registerNative("create_power_up_aura", native_create_power_up_aura, 4);
        vm.registerNative("create_sparkle", native_create_sparkle, 3);
        vm.registerNative("create_blood_splatter", native_create_blood_splatter, 5);
        vm.registerNative("create_rain", native_create_rain, 4);
        vm.registerNative("create_shell_ejection", native_create_shell_ejection, 4);
        vm.registerNative("create_muzzle_flash", native_create_muzzle_flash, 4);

    }
}