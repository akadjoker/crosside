#pragma once

#include "interpreter.hpp"

class b2World;
class b2Body;

namespace BindingsBox2DJoints
{
    void registerAll(Interpreter &vm);

    // Lifecycle hooks from Box2D core module
    void setWorld(b2World *world);
    void onWorldDestroying();
    void onBodyRemoving(b2Body *body);
    void flushPending();
}

