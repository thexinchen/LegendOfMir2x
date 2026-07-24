#pragma once
//
// Stage 2b: stb_truetype-based replacement for SDL_ttf + SDL_Surface used by
// FontexDB/InitView. Pixel format matches the old ARGB8888 surfaces
// (uint32 = 0xAARRGGBB) so FontexDB's pixel-level logic is unchanged; the
// device converts to GL RGBA on upload.
//
// Render modes mirror SDL_ttf conventions relied on by FontexDB:
//   SOLID  : pixels are (0,0,0,0) or (255,255,255,0)      (color in RGB, alpha 0)
//   SHADED : pixels are (x,x,x,0)                          (gray in RGB, alpha 0)
//   BLENDED: pixels are (255,255,255,x)                    (coverage in alpha)

#include <tuple>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

// style flags (SDL_ttf-compatible values)
constexpr int GLFONT_STYLE_BOLD          = 0x01;
constexpr int GLFONT_STYLE_ITALIC        = 0x02;
constexpr int GLFONT_STYLE_UNDERLINE     = 0x04;
constexpr int GLFONT_STYLE_STRIKETHROUGH = 0x08;

enum GLFontRenderMode: int
{
    GLFONT_SOLID = 0,
    GLFONT_SHADED,
    GLFONT_BLENDED,
};

struct GLRect
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct GLSurface
{
    int w = 0;
    int h = 0;
    std::vector<uint32_t> pixels; // 0xAARRGGBB, size = w * h
};

class GLFontFace // TTF_Font replacement
{
    public:
        struct Impl;
        std::unique_ptr<Impl> impl;

        /* ctor */  GLFontFace();
        /* dtor */ ~GLFontFace();
};

namespace glfont
{
    // ptSize: point size at 72 DPI (pixels == points), same as SDL_ttf
    GLFontFace *createFont(const void *data, size_t size, uint8_t ptSize);

    // the embedded monaco.ttf default font, cached per size
    GLFontFace *defaultFont(uint8_t ptSize);

    void setStyle(GLFontFace *, int styleFlags);
    void setKerning(GLFontFace *, bool enable);

    int  getFontAscent(const GLFontFace *);
    int  getFontDescent(const GLFontFace *);
    int  getFontHeight(const GLFontFace *);
    int  getFontLineSkip(const GLFontFace *);
    bool fontIsFixedWidth(const GLFontFace *);

    // TTF name table entries (UTF-16BE decoded to UTF-8); empty when absent
    std::string getFontFamilyName(const GLFontFace *);
    std::string getFontStyleName(const GLFontFace *);

    bool fontHasGlyph(const GLFontFace *, uint32_t codePoint);

    // {minx, maxx, miny, maxy, advance} in pixels, FT coordinate convention
    // (y up, origin at baseline), matching TTF_GetGlyphMetrics
    std::tuple<int, int, int, int, int> getGlyphMetrics(const GLFontFace *, uint32_t codePoint);

    // tight glyph bitmap (no full-line-height padding); nullptr if no bitmap
    std::unique_ptr<GLSurface> renderGlyph(const GLFontFace *, uint32_t codePoint, GLFontRenderMode);

    // laid-out UTF-8 line with kerning and styles; nullptr on empty/invalid face
    std::unique_ptr<GLSurface> renderText(const GLFontFace *, const char *utf8, size_t length, GLFontRenderMode);

    // surface helpers (SDL_CreateSurface/SDL_FillSurfaceRect/SDL_BlitSurface)
    std::unique_ptr<GLSurface> createSurface(int w, int h);
    void fillSurface(GLSurface &, uint32_t argb);
    void fillSurfaceRect(GLSurface &, const GLRect &, uint32_t argb);
    bool blitSurface(const GLSurface &src, const GLRect *srcRect, GLSurface &dst, int dstX, int dstY); // raw copy

    constexpr uint32_t mapRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        return ((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b);
    }
}
