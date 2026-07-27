#pragma once

// ===== fontstyle.hpp =====
#include <cstdint>

enum FontStyle: uint8_t
{
    FONTSTYLE_BOLD          = 0B0000'0001,
    FONTSTYLE_ITALIC        = 0B0000'0010,
    FONTSTYLE_UNDERLINE     = 0B0000'0100,
    FONTSTYLE_STRIKETHROUGH = 0B0000'1000,
    FONTSTYLE_SOLID         = 0B0001'0000,
    FONTSTYLE_SHADED        = 0B0010'0000,
    FONTSTYLE_BLENDED       = 0B0100'0000,
};

// ===== glfont.hpp =====
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

// ===== fontexdb.hpp =====
#include <cstring>
#include <utility>
#include <unordered_map>

#include "zsdb.hpp"
#include "inndb.hpp"
#include "utf8f.hpp"
#include "fflerror.hpp"
#include "hexstr.hpp"
#include "gldevice.hpp"

struct FontexElement
{
    uint32_t textEncode = 0; // this is part of the resource because textEncode can refer to a long-text-string

    int32_t left  :  8 = 0;
    int32_t right :  8 = 0;
    int32_t ascent: 16 = 0;

    GLTexID texture = nullptr;
};

// key & 0X00FF000000000000) >> 48: font index --+
// key & 0X0000FF0000000000) >> 40: font size  --+---> combined as ttf index
// key & 0X000000FF00000000) >> 32: font style
// key & 0X00000000FFFFFFFF) >>  0: utf8 code or long text encode

// use lower 4 bytes:
// 1. used as a single unicode point returned by utf8f::str2code(), max value is: 0x0010_FFFF
// 2. used as a valid utf8-string buffer:
//
//        const std::string buf = get_valid_utf_8_string_with_lenght_less_than_or_equal_to_4();
//        assert(buf.size() <= 4);
//
//        uint32_t encode = 0;
//        std::memcpy(&encode, buf.data(), buf.size());
//
//    in this way, encode has maximal possible value: 0xBFDF_BFDF
//    it's from a valid UTF-8 string with two chars, packed with little-endian: U+07FF U+07FF
//
// 3. used as a long-string index

class FontexDB: public innDB<uint64_t, FontexElement>
{
    private:
        constexpr static uint32_t R1_MAX = 0x0010FFFFU; // single unicode point
        constexpr static uint32_t R2_MAX = 0xBFDFBFDFU; // raw utf-8 buffer with size <= 4
        constexpr static uint32_t R3_MAX = 0x400F401FU; // long string index

        constexpr static uint32_t R1_BASE = 0;
        constexpr static uint32_t R2_BASE = R1_BASE + R1_MAX + 1;
        constexpr static uint32_t R3_BASE = R2_BASE + R2_MAX + 1;

        static_assert(R3_BASE + R3_MAX == 0xFFFFFFFFU);

    private:
        std::unique_ptr<ZSDB> m_zsdbPtr;
        std::vector<ZSDB::Entry> m_entryList;

    private:
        // 0XFF00 : font index
        // 0X00FF : font point size
        std::unordered_map<uint16_t, std::unique_ptr<GLFontFace>> m_ttfCache;

    private:
        std::unordered_map<uint8_t, std::vector<uint8_t>> m_fontDataCache;

    private:
        uint32_t m_longTextIndexMax = 0;
        std::vector<uint32_t> m_longTextIndexList;

    private:
        std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> m_longText2Encode;
        std::unordered_map<uint32_t, const char *>                     m_encode2LongText;

    public:
        FontexDB(size_t resMax)
            : innDB<uint64_t, FontexElement>(resMax)
        {}

        virtual ~FontexDB() = default;

    private:
        const std::vector<uint8_t> &findFontData(uint8_t fontIndex)
        {
            if(auto p = m_fontDataCache.find(fontIndex); p != m_fontDataCache.end()){
                return p->second;
            }

            return m_fontDataCache[fontIndex] = [this, fontIndex]() -> std::vector<uint8_t>
            {
                char fontIndexString[8];
                std::vector<uint8_t> fontDataBuf;

                if(m_zsdbPtr->decomp(hexstr::to_string<uint8_t, 1>(fontIndex, fontIndexString, true), 2, &fontDataBuf)){
                    return fontDataBuf;
                }
                return {};
            }();
        }

    private:
        GLFontFace *findTTF(uint16_t);
        GLFontFace *findTTF(uint8_t, uint8_t);

    public:
        void load(const char *fontDBName)
        {
            m_zsdbPtr = std::make_unique<ZSDB>(fontDBName);
            m_entryList = m_zsdbPtr->getEntryList();
        }

    public:
        GLTexID retrieve(uint64_t key, int *left = nullptr, int *right = nullptr, int *ascent = nullptr)
        {
            if(auto p = innLoad(key)){
                if(left  ) *  left = p->  left;
                if(right ) * right = p-> right;
                if(ascent) *ascent = p->ascent;
                return p->texture;
            }
            return nullptr;
        }

        GLTexID retrieve(uint8_t fontIndex, uint8_t fontSize, uint8_t fontStyle, const char *utf8String, int *left = nullptr, int *right = nullptr, int *ascent = nullptr)
        {
            return retrieve(utf8f::buildU64Key(fontIndex, fontSize, fontStyle, encodeString(utf8String)), left, right, ascent);
        }

    public:
        uint8_t findFontName(const char *fontName)
        {
            fflassert(str_haschar(fontName));
            const auto fileName = str_printf("%s.TTF", fontName);

            for(const auto &entry: m_entryList){
                if(fileName == entry.fileName + 3){
                    return hexstr::to_hex<uint8_t, 1>(entry.fileName);
                }
            }
            return 0;
        }

        bool hasFont(uint8_t font)
        {
            return !findFontData(font).empty();
        }

        size_t fontCount() const
        {
            return m_entryList.size();
        }

        std::tuple<std::string, std::string> fontName(uint8_t font)
        {
            if(!hasFont(font)){
                throw fflpanic("invalid font index: {}", font);
            }

            if(auto ttfPtr = findTTF(font, 16)){
                const auto familyName = glfont::getFontFamilyName(ttfPtr);
                const auto  styleName = glfont::getFontStyleName (ttfPtr);

                fflassert(str_haschar(familyName.c_str()));
                fflassert(str_haschar( styleName.c_str()));

                return {familyName, styleName};
            }
            throw fflpanic("failed to load font: {}", font);
        }

        int fontAscent(uint8_t font, uint8_t fontSize)
        {
            if(!hasFont(font)){
                throw fflpanic("invalid font index: {}", font);
            }

            if(auto ttfPtr = findTTF(font, fontSize)){
                return glfont::getFontAscent(ttfPtr);
            }
            throw fflpanic("failed to load font: {}", font);
        }

        int fontDescent(uint8_t font, uint8_t fontSize)
        {
            if(!hasFont(font)){
                throw fflpanic("invalid font index: {}", font);
            }

            if(auto ttfPtr = findTTF(font, fontSize)){
                return glfont::getFontDescent(ttfPtr);
            }
            throw fflpanic("failed to load font: {}", font);
        }

        int fontHeight(uint8_t font, uint8_t fontSize)
        {
            if(!hasFont(font)){
                throw fflpanic("invalid font index: {}", font);
            }

            if(auto ttfPtr = findTTF(font, fontSize)){
                return glfont::getFontHeight(ttfPtr);
            }
            throw fflpanic("failed to load font: {}", font);
        }

        int fontLineSkip(uint8_t font, uint8_t fontSize)
        {
            if(!hasFont(font)){
                throw fflpanic("invalid font index: {}", font);
            }

            if(auto ttfPtr = findTTF(font, fontSize)){
                return glfont::getFontLineSkip(ttfPtr);
            }
            throw fflpanic("failed to load font: {}", font);
        }

        bool fontMono(uint8_t font, uint8_t fontSize)
        {
            if(!hasFont(font)){
                throw fflpanic("invalid font index: {}", font);
            }

            if(auto ttfPtr = findTTF(font, fontSize)){
                return glfont::fontIsFixedWidth(ttfPtr);
            }
            throw fflpanic("failed to load font: {}", font);
        }

    public:
        std::optional<std::tuple<FontexElement, size_t>> loadResource(uint64_t) override;

    public:
        void freeResource(FontexElement &) override;

    private:
        uint32_t encodeString(const char *utf8String)
        {
            fflassert(utf8String);

            if(const auto size = std::strlen(utf8String); size <= 4){
                uint32_t utf8Code = 0;
                std::memcpy(&utf8Code, utf8String, size);
                return encodeRange(2, utf8Code);
            }

            if(auto p = m_longText2Encode.find(utf8String); p != m_longText2Encode.end()){
                return p->second.first;
            }

            const auto currIndex = [this] -> uint32_t
            {
                if(m_longTextIndexList.empty()){
                    return ++m_longTextIndexMax;
                }
                else{
                    const auto index = m_longTextIndexList.back();
                    m_longTextIndexList.pop_back();
                    return index;
                }
            }();

            if(currIndex >= R3_MAX){
                throw fflpanic("long text count exceeds limit: {}", currIndex);
            }

            const auto encodedIndex = encodeRange(3, currIndex);
            const auto insertedString = m_longText2Encode.try_emplace(utf8String, std::make_pair(encodedIndex, 0)); // allocate slot only, no resource refers to it now
            const auto insertedEncode = m_encode2LongText.try_emplace(encodedIndex, insertedString.first->first.c_str());

            fflassert(insertedString.second);
            fflassert(insertedEncode.second);

            return encodedIndex;
        }

    private:
        static uint32_t encodeRange(int range, uint32_t val)
        {
            switch(range){
                case 1 : fflassert(val <= R1_MAX); return R1_BASE + val;
                case 2 : fflassert(val <= R2_MAX); return R2_BASE + val;
                case 3 : fflassert(val <= R3_MAX); return R3_BASE + val;
                default:                           throw  fflvalue(range, val);
            }
        }

        static std::pair<size_t, uint32_t> decodeRange(uint32_t val)
        {
            if     (val <= R1_BASE + R1_MAX) return {1, val - R1_BASE};
            else if(val <= R2_BASE + R2_MAX) return {2, val - R2_BASE};
            else                             return {3, val - R3_BASE};
        }

    private:
        // SDL3_ttf only reports presence (not the glyph index)
        static bool hasGlphy(GLFontFace *, uint32_t);

    public:
        bool hasGlphy(uint16_t,          uint32_t);
        bool hasGlphy( uint8_t, uint8_t, uint32_t);

    private:
        static bool isTransparant(GLFontFace *, uint32_t);
        static bool isTransparant(const std::tuple<int, int, int, int, int> &);

    public:
        bool isTransparant(uint16_t,          uint32_t);
        bool isTransparant(uint8_t , uint8_t, uint32_t);

    private:
        static std::tuple<int, int, int, int, int> getGlyphMetrics(GLFontFace *, uint32_t); // returns everything

    private:
        static std::tuple<int, int, int> getGlyphPadding  (const std::tuple<int, int, int, int, int> &);
        static std::pair <int, int>      getGlyphPixelSize(const std::tuple<int, int, int, int, int> &);

    private:
        static std::tuple<int, int, int> getGlyphPadding  (GLFontFace *font, uint32_t codePoint){ return getGlyphPadding  (getGlyphMetrics(font, codePoint)); }
        static std::pair <int, int>      getGlyphPixelSize(GLFontFace *font, uint32_t codePoint){ return getGlyphPixelSize(getGlyphMetrics(font, codePoint)); }
};

// ===== fontselector.hpp =====
#include "gui_core.hpp"
#include "gui_widgets.hpp"
#include "layoutboard.hpp"

class ProcessRun;
class FontSelector: public Widget
{
    private:
        constexpr static int GAP = 10;
        constexpr static int LAYOUT_WIDTH = 410;

    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            ProcessRun *runProc = nullptr;
            Widget::WADPair parent {};
        };

    private:
        ProcessRun *m_runProc;

    private:
        PullMenu        m_widget;
        PullMenu        m_font;
        IntegerSelector m_size;

    private:
        LayoutBoard m_english;
        LayoutBoard m_chinese;

    private:
        ItemFlex m_vflex;
        GfxShapeBoard m_vflexFrame;

    public:
        FontSelector(FontSelector::InitArgs);
};
