#include "gui_font.hpp"

// ===== glfont.cpp =====
#include <cmath>
#include <cstring>
#include <algorithm>
#include <unordered_map>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "totype.hpp"
#include "fflerror.hpp"

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
        #include "monaco_ttf.hpp"
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

// ===== fontexdb.cpp =====
#include "colorf.hpp"
#include "gldevice.hpp"

extern GLDevice *g_glDevice;

GLFontFace *FontexDB::findTTF(uint16_t ttfIndex)
{
    if(auto p = m_ttfCache.find(ttfIndex); p != m_ttfCache.end()){
        return p->second.get();
    }

    auto face = [this, ttfIndex]() -> std::unique_ptr<GLFontFace>
    {
        const uint8_t fontIndex = to_u8((ttfIndex & 0XFF00) >> 8);
        const uint8_t fontSize  = to_u8((ttfIndex & 0X00FF) >> 0);

        if(auto &fontDataBuf = findFontData(fontIndex); !fontDataBuf.empty()){
            return std::unique_ptr<GLFontFace>(glfont::createFont(fontDataBuf.data(), fontDataBuf.size(), fontSize));
        }
        return nullptr;
    }();

    auto *result = face.get();
    m_ttfCache.emplace(ttfIndex, std::move(face));
    return result;
}

GLFontFace *FontexDB::findTTF(uint8_t ttfIndex, uint8_t ttfSize)
{
    return findTTF(utf8f::buildTTFIndex(ttfIndex, ttfSize));
}

std::optional<std::tuple<FontexElement, size_t>> FontexDB::loadResource(uint64_t key)
{
    const auto [fontIndex, fontSize, fontStyle, textEncode] = utf8f::extractU64Key(key);
    const auto ttfIndex = utf8f::buildTTFIndex(fontIndex, fontSize);
    const auto [range, index] = decodeRange(textEncode);

    const auto useMiniToken = (range == 1);
    const auto useLongText  = (range == 3);

    FontexElement result {.textEncode = textEncode};
    const auto fnReturnValue = [useLongText, &result, this] -> std::optional<std::tuple<FontexElement, size_t>>
    {
        // long-text is part of the resource
        // shall not return nullopt if the load has allocated a long-text

        if(useLongText){
            if(auto &refCount = m_longText2Encode.at(m_encode2LongText.at(result.textEncode)).second; ++refCount == 0){
                throw fflpanic("reference count for textEncode {} overflows", result.textEncode);
            }
        }

        if(result.texture){
            const auto [texW, texH] = GLDeviceHelper::getTextureSize(result.texture);
            return std::make_tuple(result, texW * texH + 50);
        }
        else if(useLongText){
            return std::make_tuple(result, 1);
        }
        else{
            return std::nullopt;
        }
    };

    auto ttf = findTTF(ttfIndex);
    if(!ttf){
        return fnReturnValue();
    }

    glfont::setKerning(ttf, !useMiniToken);
    {
        int styleFlags = 0;
        if(fontStyle & FONTSTYLE_BOLD){
            styleFlags |= GLFONT_STYLE_BOLD;
        }

        if(fontStyle & FONTSTYLE_ITALIC){
            styleFlags |= GLFONT_STYLE_ITALIC;
        }

        if(fontStyle & FONTSTYLE_UNDERLINE){
            styleFlags |= GLFONT_STYLE_UNDERLINE;
        }

        if(fontStyle & FONTSTYLE_STRIKETHROUGH){
            styleFlags |= GLFONT_STYLE_STRIKETHROUGH;
        }

        glfont::setStyle(ttf, styleFlags);
    }

    std::string strBuf;
    const char *utf8String;

    switch(range){
        case 1:
            {
                strBuf = utf8f::code2str(index);
                utf8String = strBuf.data();
                break;
            }
        case 2:
            {
                strBuf.assign(reinterpret_cast<const char *>(&index), 4);
                utf8String = strBuf.data();
                break;
            }
        default:
            {
                utf8String = m_encode2LongText.at(textEncode);
                break;
            }
    }

    std::unique_ptr<GLSurface> surf;
    if(useMiniToken){
        if(hasGlphy(ttf, index)){
            const auto metrics = getGlyphMetrics(ttf, index);

            const auto minx    = std::get<0>(metrics);
            const auto maxy    = std::get<3>(metrics);
            const auto advance = std::get<4>(metrics);

            const auto padding = getGlyphPadding  (metrics);
            const auto pixSize = getGlyphPixelSize(metrics);

            if(pixSize.first <= 0 || pixSize.second <= 0){
                fflassert(isTransparant(ttf, index), index);
                const auto emptyPixSize = getGlyphPixelSize(ttf, utf8f::str2code("a"));

                fflassert(emptyPixSize.first  > 0, emptyPixSize);
                fflassert(emptyPixSize.second > 0, emptyPixSize);

                surf = glfont::createSurface(advance, emptyPixSize.second);
                if(fontStyle & FONTSTYLE_SOLID){
                    glfont::fillSurface(*surf, glfont::mapRGBA(0, 0, 0, 0));
                }
                else if(fontStyle & FONTSTYLE_SHADED){
                    glfont::fillSurface(*surf, glfont::mapRGBA(0, 0, 0, 0));
                }
                else{
                    glfont::fillSurface(*surf, glfont::mapRGBA(255, 255, 255, 0));
                }

                result.left   = 0;
                result.right  = 0;
                result.ascent = surf->h;
            }
            else{
                if(fontStyle & FONTSTYLE_SOLID){
                    // texture with only two colors: RGBA (0,0,0,0) and (255,255,255,0)
                    // cannot be used with blend mode because alpha is always 0
                    surf = glfont::renderGlyph(ttf, index, GLFONT_SOLID);
                }
                else if(fontStyle & FONTSTYLE_SHADED){
                    // texture with color (x,x,x,0), x = 0~255
                    // cannot be used with blend mode because alpha is always 0
                    surf = glfont::renderGlyph(ttf, index, GLFONT_SHADED);
                }
                else{
                    // texture with color (255,255,255,x), x = 0~255
                    // cannot be used without blend mode, otherwise a white opaque block appears
                    surf = glfont::renderGlyph(ttf, index, GLFONT_BLENDED);
                }

                if(surf){
                    fflassert(pixSize.first  <= surf->w);
                    fflassert(pixSize.second <= surf->h);

                    // setup padding
                    // as default if crop failed or crop not happen
                    result.left   = 0;
                    result.right  = 0;
                    result.ascent = to_i32(std::get<2>(padding));

                    if(pixSize.first < surf->w || pixSize.second < surf->h){
                        auto minisurf = glfont::createSurface(pixSize.first, pixSize.second);

                        // glfont::renderGlyph returns a tight glyph bitmap; when the
                        // rasterizer produced a larger line-height surface (legacy
                        // SDL_ttf behavior), crop the actual bounding box:
                        // located at (max(0,minx), max(0, ascent - maxy))
                        GLRect src
                        {
                            std::max<int>(0, minx),
                            std::max<int>(0, glfont::getFontAscent(ttf) - maxy),
                            pixSize.first,
                            pixSize.second,
                        };

                        if(glfont::blitSurface(*surf, &src, *minisurf, 0, 0)){
                            surf = std::move(minisurf);
                            result.left  = to_i32(std::get<0>(padding));
                            result.right = to_i32(std::get<1>(padding));
                        }
                        // blit failed: keep the original surface
                    }
                }
            }
        }
        else{
            // font doesn't support this glyph
            // we manually create a texture with a white box frame: [x]
            const auto metrics = getGlyphMetrics(ttf, utf8f::str2code("a"));
            const auto padding = getGlyphPadding  (metrics);
            const auto pixSize = getGlyphPixelSize(metrics);

            fflassert(pixSize.first  > 0, pixSize);
            fflassert(pixSize.second > 0, pixSize);

            surf = glfont::createSurface(pixSize.first, pixSize.second);
            if(fontStyle & FONTSTYLE_SOLID){
                glfont::fillSurface(*surf, glfont::mapRGBA(0, 0, 0, 0));
            }
            else if(fontStyle & FONTSTYLE_SHADED){
                glfont::fillSurface(*surf, glfont::mapRGBA(0, 0, 0, 0));
            }
            else{
                glfont::fillSurface(*surf, glfont::mapRGBA(255, 255, 255, 0));
            }

            const int thickness = std::max<int>(1, std::min<int>(surf->w, surf->h) / 6);
            const auto xColor = [fontStyle]
            {
                if(fontStyle & FONTSTYLE_SOLID){
                    return glfont::mapRGBA(255, 255, 255, 0);
                }
                else if(fontStyle & FONTSTYLE_SHADED){
                    return glfont::mapRGBA(255, 255, 255, 0);
                }
                else{
                    return glfont::mapRGBA(255, 255, 255, 255);
                }
            }();

            const GLRect top    { 0                  ,                   0,   surf->w, thickness };
            const GLRect bottom { 0                  , surf->h - thickness,   surf->w, thickness };
            const GLRect left   { 0                  ,                   0, thickness,   surf->h };
            const GLRect right  { surf->w - thickness,                   0, thickness,   surf->h };

            glfont::fillSurfaceRect(*surf, top   , xColor);
            glfont::fillSurfaceRect(*surf, bottom, xColor);
            glfont::fillSurfaceRect(*surf, left  , xColor);
            glfont::fillSurfaceRect(*surf, right , xColor);

            int innerW = surf->w - (thickness * 2);
            int innerH = surf->h - (thickness * 2);

            for(int i = 0; i < innerW; i++){
                const auto y = thickness + (i * innerH) / innerW;

                const GLRect p1 {           thickness + i    , y, thickness, thickness }; // up-left  -> down-right
                const GLRect p2 { surf->w - thickness - i - 1, y, thickness, thickness }; // up-right -> down-left

                glfont::fillSurfaceRect(*surf, p1, xColor);
                glfont::fillSurfaceRect(*surf, p2, xColor);
            }

            result.left   = to_i32(std::get<0>(padding));
            result.right  = to_i32(std::get<1>(padding));
            result.ascent = to_i32(std::get<2>(padding));
        }
    }
    else{
        if(fontStyle & FONTSTYLE_SOLID){
            surf = glfont::renderText(ttf, utf8String, 0, GLFONT_SOLID);
        }
        else if(fontStyle & FONTSTYLE_SHADED){
            surf = glfont::renderText(ttf, utf8String, 0, GLFONT_SHADED);
        }
        else{
            surf = glfont::renderText(ttf, utf8String, 0, GLFONT_BLENDED);
        }

        // put same # of transparent pixels at left side and right side
        // the rasterizer does not guarantee that the first visible pixel starts at x=0.
        //
        // SDL_ttf only handles one specific case: if a glyph extends into x < 0,
        // the entire surface is shifted right to prevent the left side from being clipped. However,
        // if the first glyph has a positive left bearing (i.e., minx > 0), the leading
        // transparent pixels are preserved.
        //
        // as a result:
        //   - First glyph minx < 0  -> shifted right, the leftmost visible pixel usually aligns to x=0.
        //   - First glyph minx = 0  -> Usually starts exactly at x=0.
        //   - First glyph minx > 0  -> Leading transparent pixels will be present on the left.
        //   - First char is a space -> The left side will be entirely transparent, accounting only for the advance.
        //
        // but SDL_ttf extends the total surface width, using the advance width of the last character,
        // so transparent pixels may exist on both the left and right margins,
        // though trailing advance padding on the right is more common.

        // put same # of transparent pixels at left side and right side
        // skip if leading or tailing glyph is transparent

        if(surf){
            const auto firstCodePoint = utf8f::str2code(utf8f::peekFirst(utf8String));
            const auto  lastCodePoint = utf8f::str2code(utf8f::peekLast (utf8String));

            if(!isTransparant(ttf, firstCodePoint) && !isTransparant(ttf, lastCodePoint)){
                const auto [leftMinX,         u1, u2, u3, u4] = getGlyphMetrics(ttf, firstCodePoint);
                const auto [u5,  rightMaxX, u6, u7, rightAdvance] = getGlyphMetrics(ttf, lastCodePoint );

                const int padLeft  = std::max<int>(0,                 leftMinX);
                const int padRight = std::max<int>(0, rightAdvance - rightMaxX);

                const int addLeft  = std::max<int>(0, padRight - padLeft );
                const int addRight = std::max<int>(0, padLeft  - padRight);

                if(addLeft || addRight){
                    auto padded = glfont::createSurface(surf->w + addLeft + addRight, surf->h);
                    if(fontStyle & FONTSTYLE_SOLID){
                        glfont::fillSurface(*padded, glfont::mapRGBA(0, 0, 0, 0));
                    }
                    else if(fontStyle & FONTSTYLE_SHADED){
                        glfont::fillSurface(*padded, glfont::mapRGBA(0, 0, 0, 0));
                    }
                    else{
                        glfont::fillSurface(*padded, glfont::mapRGBA(255, 255, 255, 0));
                    }

                    if(glfont::blitSurface(*surf, nullptr, *padded, addLeft, 0)){
                        surf = std::move(padded);
                        result.left = 0;
                        result.right = 0;
                    }
                    else{
                        result.left = addLeft;
                        result.right = addRight;
                    }

                    result.ascent = to_u32(glfont::getFontAscent(ttf)); // can have 1 pixel shift for bitmap fonts
                }
            }
        }
    }

    if(surf){
        result.texture = g_glDevice->createTextureFromSurface(*surf);
    }

    return fnReturnValue(); // result.texture can be nullptr
}

void FontexDB::freeResource(FontexElement &element)
{
    if(element.texture){
        g_glDevice->destroyTexture(element.texture);
        element.texture = nullptr;
    }

    if(const auto [range, index] = decodeRange(element.textEncode); range == 3){
        auto p = m_encode2LongText.find(element.textEncode); fflassert(p != m_encode2LongText.end(), element.textEncode);
        auto q = m_longText2Encode.find(p->second         ); fflassert(q != m_longText2Encode.end(), *p                );

        if(q->second.second > 1){
            q->second.second--;
        }
        else{
            m_encode2LongText.erase(p);
            m_longText2Encode.erase(q);
            m_longTextIndexList.push_back(index);
        }
    }
}

bool FontexDB::hasGlphy(GLFontFace *font, uint32_t codePoint)
{
    return glfont::fontHasGlyph(font, codePoint);
}

bool FontexDB::hasGlphy(uint16_t ttfIndex, uint32_t codePoint)
{
    return hasGlphy(findTTF(ttfIndex), codePoint);
}

bool FontexDB::hasGlphy(uint8_t fontIndex, uint8_t fontSize, uint32_t codePoint)
{
    return hasGlphy(findTTF(fontIndex, fontSize), codePoint);
}

bool FontexDB::isTransparant(GLFontFace *font, uint32_t codePoint)
{
    return isTransparant(getGlyphMetrics(font, codePoint));
}

bool FontexDB::isTransparant(const std::tuple<int, int, int, int, int> &t)
{
    const auto [minx, maxx, miny, maxy, _] = t;
    return minx == 0
        && maxx == 0
        && miny == 0
        && maxy == 0; // TBD: should I check advance > 0 ?
}

bool FontexDB::isTransparant(uint16_t ttfIndex, uint32_t codePoint)
{
    return isTransparant(getGlyphMetrics(findTTF(ttfIndex), codePoint));
}

bool FontexDB::isTransparant(uint8_t fontIndex, uint8_t fontSize, uint32_t codePoint)
{
    return isTransparant(getGlyphMetrics(findTTF(fontIndex, fontSize), codePoint));
}

std::tuple<int, int, int, int, int> FontexDB::getGlyphMetrics(GLFontFace *font, uint32_t codePoint)
{
    return glfont::getGlyphMetrics(font, codePoint);
}

std::tuple<int, int, int> FontexDB::getGlyphPadding(const std::tuple<int, int, int, int, int> &t)
{
    const auto [minx, maxx, _, maxy, advance] = t;
    return {minx, advance - maxx, maxy};
}

std::pair<int, int> FontexDB::getGlyphPixelSize(const std::tuple<int, int, int, int, int> &t)
{
    const auto [minx, maxx, miny, maxy, _] = t;
    return {maxx - minx, maxy - miny};
}
