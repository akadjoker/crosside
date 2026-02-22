#pragma once

#include "interpreter.hpp"

namespace BindingsSound
{
    void registerAll(Interpreter &vm);
    void updateMusicStreams();
    void shutdown();
}