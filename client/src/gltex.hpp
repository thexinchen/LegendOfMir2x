#pragma once
//
// Stage 1b of the SDL3 -> GLFW/OpenGL/Dear ImGui migration.
//
// GLTexID replaces SDL_Texture* as the client-wide texture handle: a plain GL
// texture name plus its pixel size (the old code queried sizes via
// SDL_QueryTexture all over the place). Zero is "no texture", mirroring the
// nullptr checks on SDL_Texture*.

#include <cstddef>
#include <cstdint>
#include <imgui.h>

struct GLTexID
{
    uint32_t id = 0; // GL texture name
    int      w  = 0;
    int      h  = 0;

    /* ctor */ GLTexID() = default;
    /* ctor */ GLTexID(std::nullptr_t) {}
    /* ctor */ GLTexID(uint32_t argID, int argW, int argH)
        : id(argID)
        , w(argW)
        , h(argH)
    {}

    explicit operator bool() const
    {
        return id != 0;
    }

    operator ImTextureID() const
    {
        return static_cast<ImTextureID>(id);
    }

    // imgui 1.92 draw-list APIs take ImTextureRef; convert directly so
    // drawTexture(tex, ...) call sites work without a double conversion
    operator ImTextureRef() const
    {
        return ImTextureRef(static_cast<ImTextureID>(id));
    }
};

inline bool operator ==(const GLTexID &lhs, const GLTexID &rhs)
{
    return lhs.id == rhs.id;
}
