#pragma once
//
// Stage 1b of the SDL3 -> GLFW/OpenGL/Dear ImGui migration.
//
// AudioClip replaces MIX_Audio: compressed audio bytes (MP3/WAV straight from
// the zsdb package) decoded at play time by miniaudio. The bytes must outlive
// playback; the DB element owns them inside the clip.

#include <memory>
#include <vector>
#include <cstdint>

struct AudioClip
{
    std::vector<uint8_t> data;
};
