#pragma once
//
// Stage 1b of the SDL3 -> GLFW/OpenGL/Dear ImGui migration.
//
// AudioDevice replaces the SDL3_mixer half of SDLDevice with miniaudio:
//   - one ma_engine
//   - BGM as a single looping ma_sound fed from an in-memory decoder
//   - a 128-slot ma_sound pool for positioned sound effects, recycled by the
//     sound-end callback (mirrors the old recycleSoundEffectTrack hook)
//
// The public method names mirror SDLDevice's audio API so call sites
// (processrun, boards) stay untouched.
//
// distance:    0 : overlaps with listener
//            255 : far but not fully silent
//          > 255 : culled, will not play
//
// angle:   0 : north, 90 : east, 180 : south, 270 : west

#include <mutex>
#include <array>
#include <memory>
#include <cstdint>

#include "audioclip.hpp"
#include "soundeffecthandle.hpp"

class AudioDevice;

class SoundEffectChannel final // controller of one playing slot
{
    private:
        friend class AudioDevice;

    private:
        AudioDevice *m_device = nullptr;
        int          m_slot   = -1; // -1 after halt()

    private:
        SoundEffectChannel(AudioDevice *, int);

    public:
        ~SoundEffectChannel();

    public:
        bool halted() const
        {
            return m_slot < 0;
        }

    public:
        void halt();
        void pause();
        void resume();
        void setPosition(int distance, int angle);
};

class AudioDevice final
{
    private:
        friend class SoundEffectChannel;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;

    private:
        static constexpr size_t TRACK_COUNT = 128;

    public:
        /* ctor */  explicit AudioDevice(bool enabled = true);
        /* dtor */ ~AudioDevice();

    public:
        void stopBGM();
        void setBGMVolume(float);                      // by initial max volume
        void playBGM(std::shared_ptr<AudioClip>, size_t repeats = 0); // 0 = forever

    public:
        size_t channelCount() const
        {
            return TRACK_COUNT;
        }

        std::shared_ptr<SoundEffectChannel> playSoundEffect(std::shared_ptr<SoundEffectHandle>, int distance = 0, int angle = 0, size_t repeats = 1);
        void stopSoundEffect();
        void setSoundEffectVolume(float);

    public:
        void recycleSlotBySound(void *sound); // ma_sound*, void to keep miniaudio out of the header

    private:
        void recycleSlot(int slot);
        void applySlotPosition(int slot, int distance, int angle);
};
