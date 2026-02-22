#include "engine.hpp"

SoundLib gSoundLib;


void InitSound()
{
    if (!IsAudioDeviceReady())
    {
        InitAudioDevice();
    }
}

void DestroySound()
{
    gSoundLib.destroy();
    CloseAudioDevice();
}

 

int SoundLib::load(const char *name, const char *soundPath)
{
    if (!IsAudioDeviceReady())
    {
        InitAudioDevice();
    }

    Sound snd = LoadSound(soundPath);
    SoundData sd;
    sd.id = (int)sounds.size();
    sd.sound = snd;
    strncpy(sd.name, name, MAXNAME - 1);
    sd.name[MAXNAME - 1] = '\0';

    sounds.push_back(sd);

    return sd.id;
}

Sound *SoundLib::getSound(int id)
{
    if (id < 0 || id >= (int)sounds.size())
        return nullptr;
    return &sounds[id].sound;
}

SoundData *SoundLib::getSoundData(int id)
{
    if (id < 0 || id >= (int)sounds.size())
        return nullptr;
    return &sounds[id];
}

void SoundLib::play(int id, float volume, float pitch)
{
    if (id < 0 || id >= (int)sounds.size())
        return;

    Sound *snd = getSound(id);
    if (!snd)
        return;

    SetSoundVolume(*snd, volume);
    SetSoundPitch(*snd, pitch);
    PlaySound(*snd);
}

void SoundLib::stop(int id)
{
    if (id < 0 || id >= (int)sounds.size())
        return;
    StopSound(sounds[id].sound);
}

void SoundLib::pause(int id)
{
    if (id < 0 || id >= (int)sounds.size())
        return;
    PauseSound(sounds[id].sound);
}

void SoundLib::resume(int id)
{
    if (id < 0 || id >= (int)sounds.size())
        return;
    ResumeSound(sounds[id].sound);
}

bool SoundLib::isSoundPlaying(int id)
{
    if (id < 0 || id >= (int)sounds.size())
        return false;
    return IsSoundPlaying(sounds[id].sound);
}

void SoundLib::destroy()
{
    for (size_t i = 0; i < sounds.size(); i++)
    {
        UnloadSound(sounds[i].sound);
    }
    sounds.clear();
    for (size_t i = 0; i < musics.size(); i++)
    {
        UnloadMusicStream(musics[i].music);
    }
    musics.clear();
}

// --- Music Implementation in SoundLib ---

int SoundLib::loadMusic(const char *name, const char *musicPath)
{
    if (!IsAudioDeviceReady())
    {
        InitAudioDevice();
    }

    Music mus = LoadMusicStream(musicPath);
    MusicData md;
    md.id = (int)musics.size();
    md.music = mus;
    strncpy(md.name, name, MAXNAME - 1);
    md.name[MAXNAME - 1] = '\0';
    md.music.looping = true; // Default to looping for music

    musics.push_back(md);

    return md.id;
}

MusicData *SoundLib::getMusicData(int id)
{
    if (id < 0 || id >= (int)musics.size())
        return nullptr;
    return &musics[id];
}

void SoundLib::playMusic(int id)
{
    if (id < 0 || id >= (int)musics.size()) return;
    PlayMusicStream(musics[id].music);
}

void SoundLib::stopMusic(int id)
{
    if (id < 0 || id >= (int)musics.size()) return;
    StopMusicStream(musics[id].music);
}

void SoundLib::pauseMusic(int id)
{
    if (id < 0 || id >= (int)musics.size()) return;
    PauseMusicStream(musics[id].music);
}

void SoundLib::resumeMusic(int id)
{
    if (id < 0 || id >= (int)musics.size()) return;
    ResumeMusicStream(musics[id].music);
}

void SoundLib::setMusicVolume(int id, float volume)
{
    if (id < 0 || id >= (int)musics.size()) return;
    SetMusicVolume(musics[id].music, volume);
}

bool SoundLib::isMusicPlaying(int id)
{
    if (id < 0 || id >= (int)musics.size()) return false;
    return IsMusicStreamPlaying(musics[id].music);
}

void SoundLib::updateMusicStreams()
{
    for (size_t i = 0; i < musics.size(); i++)
    {
        if (IsMusicStreamPlaying(musics[i].music))
        {
            UpdateMusicStream(musics[i].music);
        }
    }
}

void SoundLib::destroyMusic()
{
    for (size_t i = 0; i < musics.size(); i++)
    {
        UnloadMusicStream(musics[i].music);
    }
    musics.clear();
}
