#include "sound.hpp"
#include "engine.hpp"
#include <raylib.h>
#include <vector>
#include <string>
 

extern SoundLib gSoundLib;
 

namespace BindingsSound
{
    // --- Sound Functions ---
    int native_load_sound(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isString())
        {
            Error("load_sound expects 1 string argument (path)");
            vm->pushInt(-1);
            return 1;
        }
        const char *path = args[0].asStringChars();
        int soundId = gSoundLib.load(GetFileNameWithoutExt(path), path);
        if (soundId < 0)
        {
            Error("Failed to load sound from path: %s", path);
            vm->pushInt(-1);
            return 1;
        }
        vm->pushInt(soundId);
        return 1;
    }

    int native_play_sound(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount < 1 || argCount > 3 || !args[0].isInt())
        {
            Error("play_sound expects (soundId, [volume=1.0], [pitch=1.0])");
            return 0;
        }
        int soundId = args[0].asInt();
        float volume = (argCount > 1) ? (float)args[1].asNumber() : 1.0f;
        float pitch = (argCount > 2) ? (float)args[2].asNumber() : 1.0f;
        gSoundLib.play(soundId, volume, pitch);
        return 0;
    }

    int native_stop_sound(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isInt()) { Error("stop_sound expects 1 int argument (soundId)"); return 0; }
        gSoundLib.stop(args[0].asInt());
        return 0;
    }

    int native_is_sound_playing(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isInt()) { Error("is_sound_playing expects 1 int argument (soundId)"); vm->pushBool(false); return 1; }
        vm->pushBool(gSoundLib.isSoundPlaying(args[0].asInt()));
        return 1;
    }

    int native_pause_sound(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isInt()) { Error("pause_sound expects 1 int argument (soundId)"); return 0; }
        gSoundLib.pause(args[0].asInt());
        return 0;
    }

    int native_resume_sound(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isInt()) { Error("resume_sound expects 1 int argument (soundId)"); return 0; }
        gSoundLib.resume(args[0].asInt());
        return 0;
    }

    // --- Music Functions ---
    int native_load_music(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isString()) { Error("load_music expects 1 string argument (path)"); vm->pushInt(-1); return 1; }
        const char *path = args[0].asStringChars();
        int musicId = gSoundLib.loadMusic(GetFileNameWithoutExt(path), path);
        if (musicId < 0) { Error("Failed to load music from path: %s", path); vm->pushInt(-1); return 1; }
        vm->pushInt(musicId);
        return 1;
    }

    int native_play_music(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isInt()) { Error("play_music expects 1 int argument (musicId)"); return 0; }
        gSoundLib.playMusic(args[0].asInt());
        return 0;
    }

    int native_stop_music(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isInt()) { Error("stop_music expects 1 int argument (musicId)"); return 0; }
        gSoundLib.stopMusic(args[0].asInt());
        return 0;
    }

    int native_pause_music(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isInt()) { Error("pause_music expects 1 int argument (musicId)"); return 0; }
        gSoundLib.pauseMusic(args[0].asInt());
        return 0;
    }

    int native_resume_music(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isInt()) { Error("resume_music expects 1 int argument (musicId)"); return 0; }
        gSoundLib.resumeMusic(args[0].asInt());
        return 0;
    }

    int native_set_music_volume(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 2 || !args[0].isInt() || !args[1].isNumber()) { Error("set_music_volume expects (musicId, volume)"); return 0; }
        gSoundLib.setMusicVolume(args[0].asInt(), (float)args[1].asNumber());
        return 0;
    }

    int native_is_music_playing(Interpreter *vm, int argCount, Value *args)
    {
        if (argCount != 1 || !args[0].isInt()) { Error("is_music_playing expects 1 int argument (musicId)"); vm->pushBool(false); return 1; }
        vm->pushBool(gSoundLib.isMusicPlaying(args[0].asInt()));
        return 1;
    }

    int native_update_music_streams(Interpreter *vm, int argCount, Value *args)
    {
        (void)vm; (void)argCount; (void)args;
        gSoundLib.updateMusicStreams();
        return 0;
    }

    void registerAll(Interpreter &vm)
    {
        vm.registerNative("load_sound", native_load_sound, 1);
        vm.registerNative("play_sound", native_play_sound, -1);
        vm.registerNative("stop_sound", native_stop_sound, 1);
        vm.registerNative("is_sound_playing", native_is_sound_playing, 1);
        vm.registerNative("pause_sound", native_pause_sound, 1);
        vm.registerNative("resume_sound", native_resume_sound, 1);

        vm.registerNative("load_music", native_load_music, 1);
        vm.registerNative("play_music", native_play_music, 1);
        vm.registerNative("stop_music", native_stop_music, 1);
        vm.registerNative("pause_music", native_pause_music, 1);
        vm.registerNative("resume_music", native_resume_music, 1);
        vm.registerNative("set_music_volume", native_set_music_volume, 2);
        vm.registerNative("is_music_playing", native_is_music_playing, 1);
        vm.registerNative("update_music_streams", native_update_music_streams, 0);
    }

    void updateMusicStreams() { gSoundLib.updateMusicStreams(); }
    void shutdown() { gSoundLib.destroy(); }
}