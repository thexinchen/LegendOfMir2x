#pragma once
//
// Stage 1b: MIX_Audio replaced by an AudioClip (miniaudio decodes at play
// time, see audiodevice.hpp). chunkFileData kept for struct-shape parity;
// the bytes now live inside the clip.

#include <memory>
#include <vector>
#include <cstdint>

#include "audioclip.hpp"

struct SoundEffectHandle
{
    std::shared_ptr<AudioClip> audio = nullptr;
    std::vector<uint8_t> chunkFileData;
};
