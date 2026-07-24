#include <cmath>
#include <cstring>
#include <algorithm>
#include <unordered_map>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "totype.hpp"
#include "fflerror.hpp"
#include "glfont.hpp"

struct GLFontFace::Impl
{
    std::vector<uint8_t> data;  // owned font bytes (stb keeps a pointer into it)
    stbtt_fontinfo info {};
    float scale   = 0.0f;
    int   ascent  = 0;          // px, > 0
    int   descent = 0;          // px, <= 0
    int   lineGap = 0;          // px
    int   style   = 0;
    bool  kerning = true;
};

GLFontFace::GLFontFace()
    : impl(std::make_unique<Impl>())
{}

GLFontFace::~GLFontFace() = default;

GLFontFace *glfont::createFont(const void *data, size_t size, uint8_t ptSize)
{
    fflassert(data);
    fflassert(size > 0);
    fflassert(ptSize > 0);

    auto face = std::make_unique<GLFontFace>();
    face->impl->data.assign(static_cast<const uint8_t *>(data), static_cast<const uint8_t *>(data) + size);

    const auto offset = stbtt_GetFontOffsetForIndex(face->impl->data.data(), 0);
    if(!stbtt_InitFont(&face->impl->info, face->impl->data.data(), offset)){
        return nullptr;
    }

    // SDL_ttf: point size at 72 DPI == pixel size
    face->impl->scale = stbtt_ScaleForPixelHeight(&face->impl->info, to_f(ptSize));

    int ascentU = 0, descentU = 0, lineGapU = 0;
    stbtt_GetFontVMetrics(&face->impl->info, &ascentU, &descentU, &lineGapU);
    face->impl->ascent  = to_d(std::lround(to_df(ascentU ) * face->impl->scale));
    face->impl->descent = to_d(std::lround(to_df(descentU) * face->impl->scale));
    face->impl->lineGap = to_d(std::lround(to_df(lineGapU ) * face->impl->scale));

    return face.release();
}

GLFontFace *glfont::defaultFont(uint8_t ptSize)
{
    constexpr static uint8_t ttfData[]
    {
        #embed "monaco.ttf"
    };

    static std::unordered_map<uint8_t, std::unique_ptr<GLFontFace>> cache;
    if(auto p = cache.find(ptSize); p != cache.end()){
        return p->second.get();
    }

    if(auto *face = createFont(std::data(ttfData), std::size(ttfData), ptSize)){
        return cache.emplace(ptSize, std::unique_ptr<GLFontFace>(face)).first->second.get();
    }
    throw fflpanic("can't build default ttf with point: {}", ptSize);
}

void glfont::setStyle(GLFontFace *face, int styleFlags)
{
    if(face){
        face->impl->style = styleFlags;
    }
}

void glfont::setKerning(GLFontFace *face, bool enable)
{
    if(face){
        face->impl->kerning = enable;
    }
}

int glfont::getFontAscent(const GLFontFace *face)
{
    return face ? face->impl->ascent : 0;
}

int glfont::getFontDescent(const GLFontFace *face)
{
    return face ? face->impl->descent : 0;
}

int glfont::getFontHeight(const GLFontFace *face)
{
    return face ? (face->impl->ascent - face->impl->descent + face->impl->lineGap) : 0;
}

int glfont::getFontLineSkip(const GLFontFace *face)
{
    return getFontHeight(face);
}

bool glfont::fontIsFixedWidth(const GLFontFace *face)
{
    if(!face){
        return false;
    }
    const auto [ig1, ig2, ig3, ig4, advI] = getGlyphMetrics(face, 'i');
    const auto [ig5, ig6, ig7, ig8, advM] = getGlyphMetrics(face, 'm');
    return advI == advM && advI > 0;
}

static std::string fnFontNameString(const GLFontFace *face, int nameID)
{
    if(!face){
        return {};
    }

    // prefer Microsoft Unicode BMP English, fall back to Mac Roman
    struct NameQuery { int platformID; int encodingID; int languageID; };
    static const NameQuery queryList[]
    {
        {3, 1, 0x0409},
        {3, 1, 0x0000},
        {1, 0, 0x0000},
    };

    for(const auto &query: queryList){
        int length = 0;
        const auto *bytes = stbtt_GetFontNameString(&face->impl->info, &length, query.platformID, query.encodingID, query.languageID, nameID);
        if(!(bytes && length > 0)){
            continue;
        }

        if(query.platformID == 3){
            // UTF-16BE -> UTF-8
            std::string result;
            for(int i = 0; i + 1 < length; i += 2){
                auto unit = (uint16_t)(((uint8_t)bytes[i] << 8) | (uint8_t)bytes[i + 1]);
                if(unit < 0x80){
                    result += (char)unit;
                }
                else if(unit < 0x800){
                    result += (char)(0xC0 | (unit >> 6));
                    result += (char)(0x80 | (unit & 0x3F));
                }
                else{
                    result += (char)(0xE0 | (unit >> 12));
                    result += (char)(0x80 | ((unit >> 6) & 0x3F));
                    result += (char)(0x80 | (unit & 0x3F));
                }
            }
            if(!result.empty()){
                return result;
            }
        }
        else{
            return std::string(bytes, length); // Mac Roman ~ ASCII for our purposes
        }
    }
    return {};
}

std::string glfont::getFontFamilyName(const GLFontFace *face)
{
    return fnFontNameString(face, 1);
}

std::string glfont::getFontStyleName(const GLFontFace *face)
{
    return fnFontNameString(face, 2);
}

bool glfont::fontHasGlyph(const GLFontFace *face, uint32_t codePoint)
{
    if(!face){
        return false;
    }
    return stbtt_FindGlyphIndex(&face->impl->info, to_d(codePoint)) != 0;
}

std::tuple<int, int, int, int, int> glfont::getGlyphMetrics(const GLFontFace *face, uint32_t codePoint)
{
    if(!face){
        throw fflpanic("failed to get glyph metrics: {}", codePoint);
    }

    int advanceWidth = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(&face->impl->info, to_d(codePoint), &advanceWidth, &lsb);

    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetCodepointBitmapBox(&face->impl->info, to_d(codePoint), face->impl->scale, face->impl->scale, &x0, &y0, &x1, &y1);

    // stb bitmap box is y-down from baseline; FT/SDL metrics are y-up
    const auto minx    = x0;
    const auto maxx    = x1;
    const auto miny    = -y1;
    const auto maxy    = -y0;
    const auto advance = to_d(std::lround(to_df(advanceWidth) * face->impl->scale));

    return {minx, maxx, miny, maxy, advance};
}

static std::vector<uint8_t> fnGlyphAlphaBitmap(const GLFontFace::Impl &impl, uint32_t codePoint, int &w, int &h, int &xoff, int &yoff)
{
    std::vector<uint8_t> result;
    unsigned char *bmp = stbtt_GetCodepointBitmap(&impl.info, impl.scale, impl.scale, to_d(codePoint), &w, &h, &xoff, &yoff);
    if(bmp){
        result.assign(bmp, bmp + (size_t)(std::max(w, 0) * std::max(h, 0)));

        if(impl.style & GLFONT_STYLE_BOLD){
            // fake bold: spread each pixel one pixel to the right (SDL_ttf behavior)
            for(int y = 0; y < h; ++y){
                for(int x = w - 1; x >= 1; --x){
                    result[y * w + x] = std::max(result[y * w + x], result[y * w + x - 1]);
                }
            }
        }
        stbtt_FreeBitmap(bmp, nullptr);
    }
    return result;
}

static uint32_t fnPackPixel(GLFontRenderMode mode, uint8_t alpha)
{
    switch(mode){
        case GLFONT_SOLID  : return alpha >= 128 ? 0X00FFFFFF : 0X00000000;
        case GLFONT_SHADED : return ((uint32_t)(alpha) << 16) | ((uint32_t)(alpha) << 8) | (uint32_t)(alpha);
        case GLFONT_BLENDED:
        default            : return ((uint32_t)(alpha) << 24) | 0X00FFFFFF;
    }
}

std::unique_ptr<GLSurface> glfont::renderGlyph(const GLFontFace *face, uint32_t codePoint, GLFontRenderMode mode)
{
    if(!face){
        return nullptr;
    }

    int w = 0, h = 0, xoff = 0, yoff = 0;
    const auto bmp = fnGlyphAlphaBitmap(*face->impl, codePoint, w, h, xoff, yoff);
    if(bmp.empty() || w <= 0 || h <= 0){
        return nullptr;
    }

    auto surf = createSurface(w, h);
    for(int i = 0; i < w * h; ++i){
        surf->pixels[i] = fnPackPixel(mode, bmp[i]);
    }
    return surf;
}

static size_t fnUTF8Decode(const char *s, size_t length, std::vector<uint32_t> &out)
{
    size_t i = 0;
    while(i < length){
        const auto c0 = (uint8_t)s[i];
        uint32_t cp = 0;
        size_t n = 0;

        if(c0 < 0x80){ cp = c0; n = 1; }
        else if((c0 >> 5) == 0x6 && i + 1 < length){ cp = ((c0 & 0x1F) << 6) | ((uint8_t)s[i+1] & 0x3F); n = 2; }
        else if((c0 >> 4) == 0xE && i + 2 < length){ cp = ((c0 & 0x0F) << 12) | (((uint8_t)s[i+1] & 0x3F) << 6) | ((uint8_t)s[i+2] & 0x3F); n = 3; }
        else if((c0 >> 3) == 0x1E && i + 3 < length){ cp = ((c0 & 0x07) << 18) | (((uint8_t)s[i+1] & 0x3F) << 12) | (((uint8_t)s[i+2] & 0x3F) << 6) | ((uint8_t)s[i+3] & 0x3F); n = 4; }
        else { cp = '?'; n = 1; }

        out.push_back(cp);
        i += n;
    }
    return out.size();
}

std::unique_ptr<GLSurface> glfont::renderText(const GLFontFace *face, const char *utf8, size_t length, GLFontRenderMode mode)
{
    if(!(face && utf8)){
        return nullptr;
    }

    if(length == 0){
        length = std::strlen(utf8);
    }
    if(length == 0){
        return nullptr;
    }

    std::vector<uint32_t> codeList;
    fnUTF8Decode(utf8, length, codeList);
    if(codeList.empty()){
        return nullptr;
    }

    auto &impl = *face->impl;
    const auto fnKern = [&impl](uint32_t a, uint32_t b) -> int
    {
        if(!impl.kerning){
            return 0;
        }
        return to_d(std::lround(to_df(stbtt_GetCodepointKernAdvance(&impl.info, to_d(a), to_d(b))) * impl.scale));
    };

    // first pass: width (SDL_ttf keeps the last glyph's advance and any
    // positive left bearing of the first glyph as leading transparent pixels)
    const auto [firstMinX, ig1, ig2, ig3, ig4] = getGlyphMetrics(face, codeList.front());
    const auto xstart = std::max<int>(0, -firstMinX);

    int cursor = 0;
    for(size_t i = 0; i < codeList.size(); ++i){
        const auto [igA, igB, igC, igD, advance] = getGlyphMetrics(face, codeList[i]);
        cursor += advance;
        if(i + 1 < codeList.size()){
            cursor += fnKern(codeList[i], codeList[i + 1]);
        }
    }

    const int lineHeight = std::max<int>(1, impl.ascent - impl.descent);
    const int italicExtra = (impl.style & GLFONT_STYLE_ITALIC) ? std::max<int>(1, lineHeight / 4) : 0;
    const int width = std::max<int>(1, xstart + cursor + italicExtra);

    auto surf = createSurface(width, lineHeight);

    // second pass: rasterize glyphs onto the line (baseline at row = ascent)
    int pen = 0;
    for(size_t i = 0; i < codeList.size(); ++i){
        int gw = 0, gh = 0, xoff = 0, yoff = 0;
        const auto bmp = fnGlyphAlphaBitmap(impl, codeList[i], gw, gh, xoff, yoff);

        if(!bmp.empty() && gw > 0 && gh > 0){
            for(int yy = 0; yy < gh; ++yy){
                const int shear = (impl.style & GLFONT_STYLE_ITALIC) ? ((gh - 1 - yy) / 4) : 0;
                for(int xx = 0; xx < gw; ++xx){
                    const auto alpha = bmp[yy * gw + xx];
                    if(!alpha){
                        continue;
                    }

                    const auto pix = fnPackPixel(mode, alpha);
                    const int dy = impl.ascent + yoff + yy;
                    if(dy < 0 || dy >= lineHeight){
                        continue;
                    }

                    const int dx0 = xstart + pen + xoff + xx + shear;
                    if(dx0 >= 0 && dx0 < width){
                        surf->pixels[dy * width + dx0] = pix;
                    }
                    if((impl.style & GLFONT_STYLE_BOLD) && dx0 + 1 < width){
                        surf->pixels[dy * width + dx0 + 1] = pix;
                    }
                }
            }
        }

        const auto [igA, igB, igC, igD, advance] = getGlyphMetrics(face, codeList[i]);
        pen += advance;
        if(i + 1 < codeList.size()){
            pen += fnKern(codeList[i], codeList[i + 1]);
        }
    }

    // underline / strikethrough span the whole line
    const auto drawHLine = [&surf, width](int y, uint32_t pix)
    {
        if(y >= 0 && y < surf->h){
            for(int x = 0; x < width; ++x){
                surf->pixels[y * width + x] = pix;
            }
        }
    };

    if(impl.style & (GLFONT_STYLE_UNDERLINE | GLFONT_STYLE_STRIKETHROUGH)){
        const auto pix = fnPackPixel(mode, 255);
        if(impl.style & GLFONT_STYLE_UNDERLINE){
            drawHLine(std::min(impl.ascent, lineHeight - 1), pix);
        }
        if(impl.style & GLFONT_STYLE_STRIKETHROUGH){
            drawHLine(std::max(0, impl.ascent / 2), pix);
        }
    }

    return surf;
}

std::unique_ptr<GLSurface> glfont::createSurface(int w, int h)
{
    auto surf = std::make_unique<GLSurface>();
    surf->w = std::max(w, 0);
    surf->h = std::max(h, 0);
    surf->pixels.assign((size_t)(surf->w) * surf->h, 0);
    return surf;
}

void glfont::fillSurface(GLSurface &surf, uint32_t argb)
{
    std::fill(surf.pixels.begin(), surf.pixels.end(), argb);
}

void glfont::fillSurfaceRect(GLSurface &surf, const GLRect &rect, uint32_t argb)
{
    const int x0 = std::max(0, rect.x);
    const int y0 = std::max(0, rect.y);
    const int x1 = std::min(surf.w, rect.x + rect.w);
    const int y1 = std::min(surf.h, rect.y + rect.h);

    for(int y = y0; y < y1; ++y){
        for(int x = x0; x < x1; ++x){
            surf.pixels[y * surf.w + x] = argb;
        }
    }
}

bool glfont::blitSurface(const GLSurface &src, const GLRect *srcRect, GLSurface &dst, int dstX, int dstY)
{
    GLRect full {0, 0, src.w, src.h};
    const auto &sr = srcRect ? *srcRect : full;

    for(int y = 0; y < sr.h; ++y){
        const int sy = sr.y + y;
        const int dy = dstY + y;
        if(sy < 0 || sy >= src.h || dy < 0 || dy >= dst.h){
            continue;
        }
        for(int x = 0; x < sr.w; ++x){
            const int sx = sr.x + x;
            const int dx = dstX + x;
            if(sx < 0 || sx >= src.w || dx < 0 || dx >= dst.w){
                continue;
            }
            dst.pixels[dy * dst.w + dx] = src.pixels[sy * src.w + sx];
        }
    }
    return true;
}
