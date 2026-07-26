#include "colorf.hpp"
#include "fontexdb.hpp"
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
