#pragma once

// ===== gltex.hpp =====
//
// Stage 1b of the SDL3 -> GLFW/OpenGL/Dear ImGui migration.
//
// GLTexID replaces SDL_Texture* as the client-wide texture handle: a plain GL
// texture name plus its pixel size (the old code queried sizes via
// SDL_QueryTexture all over the place). Zero is "no texture", mirroring the
// nullptr checks on SDL_Texture*.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <imgui.h>
#include "totype.hpp"

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

// ===== pngtexdb.hpp =====
#include <vector>
#include <memory>
#include <unordered_map>

#include "zsdb.hpp"
#include "inndb.hpp"
#include "hexstr.hpp"

struct PNGTexElement
{
    GLTexID texture = nullptr;
};

class PNGTexDB: public innDB<uint32_t, PNGTexElement>
{
    private:
        std::unique_ptr<ZSDB> m_zsdbPtr;

    public:
        PNGTexDB(size_t resMax)
            : innDB<uint32_t, PNGTexElement>(resMax)
        {}

    public:
        void load(const char *texDBName)
        {
            m_zsdbPtr = std::make_unique<ZSDB>(texDBName);
        }

    public:
        GLTexID retrieve(uint32_t key)
        {
            if(auto p = innLoad(key)){
                return p->texture;
            }
            return nullptr;
        }

        GLTexID retrieve(uint8_t fileIndex, uint16_t imageIndex)
        {
            return retrieve(to_u32((to_u32(fileIndex) << 16) + imageIndex));
        }

    public:
        std::optional<std::tuple<PNGTexElement, size_t>> loadResource(uint32_t) override;

    public:
        void freeResource(PNGTexElement &) override;
};

// ===== pngtexoffdb.hpp =====
#include <tuple>
#include <cstring>
#include "mirevent.hpp"

#include "totype.hpp"

struct PNGTexOffElement
{
    int dx = 0;
    int dy = 0;
    GLTexID texture = nullptr;
};

class PNGTexOffDB: public innDB<uint32_t, PNGTexOffElement>
{
    private:
        std::unique_ptr<ZSDB> m_zsdbPtr;

    public:
        PNGTexOffDB(size_t resMax)
            : innDB<uint32_t, PNGTexOffElement>(resMax)
        {}

    public:
        void load(const char *texOffDBName)
        {
            m_zsdbPtr = std::make_unique<ZSDB>(texOffDBName);
        }

    public:
        std::tuple<GLTexID , int, int> retrieve(uint32_t key)
        {
            int dx = 0;
            int dy = 0;
            auto texPtr = retrieve(key, &dx, &dy);
            return {texPtr, dx, dy};
        }

        GLTexID retrieve(uint32_t key, int *pdx, int *pdy)
        {
            if(auto p = innLoad(key)){
                if(pdx){
                    *pdx = p->dx;
                }

                if(pdy){
                    *pdy = p->dy;
                }
                return p->texture;
            }
            return nullptr;
        }

        GLTexID retrieve(uint8_t fileIndex, uint16_t imageIndex, int *pdx, int *pdy)
        {
            return retrieve(to_u32((to_u32(fileIndex) << 16) + imageIndex), pdx, pdy);
        }

    public:
        std::optional<std::tuple<PNGTexOffElement, size_t>> loadResource(uint32_t) override;

    public:
        void freeResource(PNGTexOffElement &) override;
};

// ===== emojidb.hpp =====


// layout of an emoji on a texture
// on a single picture, from left to right
// |
// V  ->| FW |<-
// ---- +----+----+----+----+-+
//      |    |    |    |    | |
// FH   | 0  | 1  | 2  | 3  | |
//      |    |    |    |    | |
// ---- +----+----+----+----+-+
// ^    |    |    |    |    | |
// |    | 4  | 5  | 6  | 7  | |
//      |    |    |    |    | |
//      +----+----+----+----+-+
//      |    |    |    |    | |
//      +----+----+----+----+-+
//
//  FileName format:
//
//  [0 ~ 1] [2 ~ 5] [6 ~ 7] [8 ~ 9]  [10 ~ 11] [12 ~ 15] [16 ~ 19] [20 ~ 23].PNG
//
//  ESet    ESubset EFrame  FrameCnt fps       frameW    frameH    frameH1
//  1Byte   2Bytes  1Byte   1Byte    1Byte     2Bytes    2Bytes    2Bytes
//                  +-----
//                  ^
//                  |
//                  +------- always set it as zero
//                             and when retrieving we can just plus the frame index here

struct EmojiElement
{
    int frameW     = 0;
    int frameH     = 0;
    int frameH1    = 0;
    int fps        = 0;
    int frameCount = 0;

    GLTexID texture = nullptr;
};

class EmojiDB: public innDB<uint32_t, EmojiElement>
{
    private:
        std::unique_ptr<ZSDB> m_zsdbPtr;

    public:
        static uint32_t u32Key(uint8_t emojiSet, uint16_t emojiSubset)
        {
            return (to_u32(emojiSet) << 24) | (to_u32(emojiSubset) << 8);
        }

        static uint32_t u32Key(uint8_t emojiSet, uint16_t emojiSubset, uint8_t emojiIndex)
        {
            return u32Key(emojiSet, emojiSubset) | to_u32(emojiIndex);
        }

    public:
        EmojiDB()
            : innDB<uint32_t, EmojiElement>(1024)
        {}

    public:
        void load(const char *emojiDBName)
        {
            m_zsdbPtr = std::make_unique<ZSDB>(emojiDBName);
        }

    public:
        GLTexID retrieve(uint32_t,                   int *, int *, int *, int *, int *, int *, int *);
        GLTexID retrieve(uint8_t, uint16_t, uint8_t, int *, int *, int *, int *, int *, int *, int *);

    public:
        std::optional<std::tuple<EmojiElement, size_t>> loadResource(uint32_t) override;

    public:
        void freeResource(EmojiElement &) override;
};

// ===== game text drawing utilities =====
//
// Shared text-drawing helpers used by all ImGui-based boards/panels.
//
// The old SDL/KGUI widget system rendered text per-character via XMLTypeset,
// where each glyph was positioned relative to the font baseline. The new ImGui
// code renders the entire string as one fontex texture (long-text mode). These
// helpers provide a consistent API that matches the old positioning semantics.
//
// pos semantics:
//   - default (align=0): pos is the top-left corner of the text texture
//   - hcenter: pos.x is the horizontal center of the text
//   - vcenter: pos.y is the vertical center of the text texture
//   - center:  pos is the center of the text texture (hcenter + vcenter)
//   - right:   pos.x is the right edge of the text texture

enum GameTextAlign : int
{
    GTEXT_ALIGN_NONE    = 0,
    GTEXT_ALIGN_HCENTER = 1,
    GTEXT_ALIGN_VCENTER = 2,
    GTEXT_ALIGN_CENTER  = 3,
    GTEXT_ALIGN_RIGHT   = 4,
};

struct GameTextResult
{
    float w = 0.0f;
    float h = 0.0f;
};

class FontexDB;
extern FontexDB *g_fontexDB;

GameTextResult drawGameText(
    ImDrawList    *drawList,
    ImVec2         pos,
    const char    *text,
    uint8_t        font    = 1,
    uint8_t        size    = 12,
    ImU32          color   = IM_COL32_WHITE,
    int            align   = GTEXT_ALIGN_NONE);

GameTextResult drawGameText(
    ImDrawList        *drawList,
    ImVec2             pos,
    const std::string &text,
    uint8_t            font    = 1,
    uint8_t            size    = 12,
    ImU32              color   = IM_COL32_WHITE,
    int                align   = GTEXT_ALIGN_NONE);

// play the default button click sound effect (0X01020000 + 105)
void playButtonClickSound();
