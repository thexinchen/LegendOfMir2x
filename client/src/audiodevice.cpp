#include <cmath>
#include <cstring>
#include <numbers>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "log.hpp"
#include "totype.hpp"
#include "audiodevice.hpp"

extern Log *g_mir2xLog;

struct AudioDevice::Impl
{
    struct Slot
    {
        ma_decoder decoder {};
        ma_sound   sound   {};
        bool inited = false;
        bool inUse  = false;
        std::shared_ptr<SoundEffectHandle> handle; // keeps the clip bytes alive
    };

    ma_engine engine {};
    bool engineOK = false;
    bool enabled  = true;

    // BGM
    ma_decoder bgmDecoder {};
    ma_sound   bgmSound   {};
    bool bgmInited = false;
    std::shared_ptr<AudioClip> bgmClip;

    float bgmVolume   = 1.0f;
    float seffVolume  = 1.0f;

    std::mutex slotLock;
    std::array<Slot, AudioDevice::TRACK_COUNT> slots;
};

// note: TRACK_COUNT is private in AudioDevice; re-expose for Impl via friend-defined struct above
// (Impl is a nested-friend struct, but the constant is referenced through AudioDevice::TRACK_COUNT
//  inside this translation unit through the class definition.)

static void fnSoundEndCallback(void *userData, ma_sound *sound)
{
    // audio thread: only flip the in-use flag; actual uninit happens lazily on
    // slot reuse from the main thread
    auto *device = static_cast<AudioDevice *>(userData);
    device->recycleSlotBySound(sound);
}

AudioDevice::AudioDevice(bool enabled)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->enabled = enabled;
    if(!enabled){
        return;
    }

    ma_engine_config engineConfig = ma_engine_config_init();
    if(ma_engine_init(&engineConfig, &m_impl->engine) != MA_SUCCESS){
        g_mir2xLog->addLog(LOGTYPE_WARNING, "Failed to initialize miniaudio engine, audio disabled");
        m_impl->enabled = false;
    }
}

AudioDevice::~AudioDevice()
{
    if(!m_impl->enabled){
        return;
    }

    stopBGM();
    stopSoundEffect();
    ma_engine_uninit(&m_impl->engine);
}

void AudioDevice::playBGM(std::shared_ptr<AudioClip> clip, size_t repeats)
{
    if(!m_impl->enabled || !clip || clip->data.empty()){
        return;
    }

    stopBGM();

    if(ma_decoder_init_memory(clip->data.data(), clip->data.size(), nullptr, &m_impl->bgmDecoder) != MA_SUCCESS){
        return;
    }

    if(ma_sound_init_from_data_source(&m_impl->engine, &m_impl->bgmDecoder, 0, nullptr, &m_impl->bgmSound) != MA_SUCCESS){
        ma_decoder_uninit(&m_impl->bgmDecoder);
        return;
    }

    m_impl->bgmInited = true;
    m_impl->bgmClip   = clip;

    ma_sound_set_looping(&m_impl->bgmSound, repeats == 0 ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(&m_impl->bgmSound, m_impl->bgmVolume);
    ma_sound_start(&m_impl->bgmSound);
}

void AudioDevice::stopBGM()
{
    if(m_impl->bgmInited){
        ma_sound_uninit(&m_impl->bgmSound);
        ma_decoder_uninit(&m_impl->bgmDecoder);
        m_impl->bgmInited = false;
        m_impl->bgmClip.reset();
    }
}

void AudioDevice::setBGMVolume(float volume)
{
    m_impl->bgmVolume = volume;
    if(m_impl->bgmInited){
        ma_sound_set_volume(&m_impl->bgmSound, volume);
    }
}

std::shared_ptr<SoundEffectChannel> AudioDevice::playSoundEffect(std::shared_ptr<SoundEffectHandle> handle, int distance, int angle, size_t repeats)
{
    if(!m_impl->enabled || !handle || !handle->audio || handle->audio->data.empty() || distance > 255){
        return nullptr;
    }

    std::lock_guard<std::mutex> lockGuard(m_impl->slotLock);
    for(size_t i = 0; i < m_impl->slots.size(); ++i){
        auto &slot = m_impl->slots[i];
        if(slot.inUse){
            continue;
        }

        if(slot.inited){ // lazy cleanup of a finished sound
            ma_sound_uninit(&slot.sound);
            ma_decoder_uninit(&slot.decoder);
            slot.inited = false;
            slot.handle.reset();
        }

        const auto &clipData = handle->audio->data;
        if(ma_decoder_init_memory(clipData.data(), clipData.size(), nullptr, &slot.decoder) != MA_SUCCESS){
            continue;
        }

        if(ma_sound_init_from_data_source(&m_impl->engine, &slot.decoder, 0, nullptr, &slot.sound) != MA_SUCCESS){
            ma_decoder_uninit(&slot.decoder);
            continue;
        }

        slot.inited = true;
        slot.inUse  = true;
        slot.handle = handle;

        ma_sound_set_looping(&slot.sound, repeats == 0 ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(&slot.sound, m_impl->seffVolume);

        if(distance > 0){
            ma_sound_set_attenuation_model(&slot.sound, ma_attenuation_model_linear);
            ma_sound_set_min_distance(&slot.sound, 1.0f);
            ma_sound_set_max_distance(&slot.sound, 128.0f);
            ma_sound_set_spatialization_enabled(&slot.sound, MA_TRUE);
            applySlotPosition(to_d(i), distance, angle);
        }

        ma_sound_set_end_callback(&slot.sound, fnSoundEndCallback, this);
        ma_sound_start(&slot.sound);

        return std::shared_ptr<SoundEffectChannel>(new SoundEffectChannel(this, to_d(i)));
    }
    return nullptr; // all slots busy
}

void AudioDevice::applySlotPosition(int slotIndex, int distance, int angle)
{
    auto &slot = m_impl->slots.at(slotIndex);
    if(!slot.inited){
        return;
    }

    // mir2x angle: 0 = north, 90 = east; map north to +z
    const double fRad = angle * std::numbers::pi / 180.0;
    const float  fDist = to_f(distance) * 0.5f;
    ma_sound_set_position(&slot.sound, to_f(std::sin(fRad) * fDist), 0.0f, to_f(std::cos(fRad) * fDist));
}

void AudioDevice::stopSoundEffect()
{
    std::lock_guard<std::mutex> lockGuard(m_impl->slotLock);
    for(auto &slot: m_impl->slots){
        if(slot.inited){
            ma_sound_uninit(&slot.sound);
            ma_decoder_uninit(&slot.decoder);
            slot.inited = false;
        }
        slot.inUse = false;
        slot.handle.reset();
    }
}

void AudioDevice::setSoundEffectVolume(float volume)
{
    std::lock_guard<std::mutex> lockGuard(m_impl->slotLock);
    m_impl->seffVolume = volume;
    for(auto &slot: m_impl->slots){
        if(slot.inited && slot.inUse){
            ma_sound_set_volume(&slot.sound, volume);
        }
    }
}

void AudioDevice::recycleSlot(int slotIndex)
{
    std::lock_guard<std::mutex> lockGuard(m_impl->slotLock);
    if(slotIndex >= 0 && slotIndex < to_d(m_impl->slots.size())){
        m_impl->slots.at(slotIndex).inUse = false;
    }
}

void AudioDevice::recycleSlotBySound(void *soundPtr)
{
    auto *sound = static_cast<ma_sound *>(soundPtr);
    std::lock_guard<std::mutex> lockGuard(m_impl->slotLock);
    for(auto &slot: m_impl->slots){
        if(slot.inited && &slot.sound == sound){
            slot.inUse = false;
            break;
        }
    }
}

SoundEffectChannel::SoundEffectChannel(AudioDevice *device, int slot)
    : m_device(device)
    , m_slot(slot)
{}

SoundEffectChannel::~SoundEffectChannel()
{
    halt();
}

void SoundEffectChannel::halt()
{
    if(m_slot >= 0){
        m_device->recycleSlot(m_slot);
        m_slot = -1;
    }
}

void SoundEffectChannel::pause()
{
    // miniaudio has no true pause; stopping is the closest behavior
    halt();
}

void SoundEffectChannel::resume()
{
    // see pause()
}

void SoundEffectChannel::setPosition(int distance, int angle)
{
    if(m_slot >= 0){
        m_device->applySlotPosition(m_slot, distance, angle);
    }
}
