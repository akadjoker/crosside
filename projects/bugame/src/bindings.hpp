#pragma once

#include "interpreter.hpp"

class b2Draw;

namespace Bindings
{
    void registerAll(Interpreter &vm);
}
namespace BindingsInput
{
    void registerAll(Interpreter &vm);
    void update();
    void drawVirtualKeys();
}

namespace BindingsImage
{
    void registerAll(Interpreter &vm);
}

namespace BindingsProcess
{
    void registerAll(Interpreter &vm);
}

namespace BindingsBox2D
{
    void registerAll(Interpreter &vm);
    void onProcessDestroy(Process *proc);
    void shutdownPhysics();
    void renderDebug();
    b2Draw *getDebugDraw();
}

namespace BindingsDraw
{
    void registerAll(Interpreter &vm);
    void RenderWorldCommands();
    void RenderScreenCommands();
    void resetDrawCommands();
    void unloadFonts();
   
    void addLineCommand(int x1, int y1, int x2, int y2, bool screenSpace);
    void addTextCommand(String *text, int x, int y, int size, bool screenSpace);
    void addCircleCommand(int centerX, int centerY, int radius, bool fill, bool screenSpace);
    void addRectangleCommand(int x, int y, int width, int height, bool fill, bool screenSpace);
}

namespace BindingsParticles
{
    void registerAll(Interpreter &vm);
}

namespace BindingsFont
{
    void registerAll(Interpreter &vm);
}

namespace BindingsEase
{
    void registerAll(Interpreter &vm);
}

namespace BindingsMessage
{
    void registerAll(Interpreter &vm);
    void clearAllMessages();
}
