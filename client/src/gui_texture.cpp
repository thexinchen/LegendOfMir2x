#include "gui_texture.hpp"

// ===== pngtexdb.cpp =====
#include "gldevice.hpp"

extern GLDevice *g_glDevice;
std::optional<std::tuple<PNGTexElement, size_t>> PNGTexDB::loadResource(uint32_t key)
{
    char keyString[16];
    if(std::vector<uint8_t> dataBuf; m_zsdbPtr->decomp(hexstr::to_string<uint32_t, 4>(key, keyString, true), 8, &dataBuf)){
        if(auto texPtr = g_glDevice->loadPNGTexture(dataBuf.data(), dataBuf.size())){
            return std::make_tuple(PNGTexElement
            {
                .texture = texPtr,
            }, 1);
        }
    }
    return {};
}

void PNGTexDB::freeResource(PNGTexElement &element)
{
    if(element.texture){
        g_glDevice->destroyTexture(element.texture);
        element.texture = nullptr;
    }
}

// ===== pngtexoffdb.cpp =====

extern GLDevice *g_glDevice;
std::optional<std::tuple<PNGTexOffElement, size_t>> PNGTexOffDB::loadResource(uint32_t key)
{
    char keyString[16];
    std::vector<uint8_t> dataBuf;

    if(const auto fontFileName = m_zsdbPtr->decomp(hexstr::to_string<uint32_t, 4>(key, keyString, true), 8, &dataBuf); fontFileName && (std::strlen(fontFileName) >= 18)){
        //
        // [0 ~ 7] [8] [9] [10 ~ 13] [14 ~ 17]
        //  <KEY>  <S> <S>   <+DX>     <+DY>
        //    4    1/2 1/2     2         2
        //
        //   KEY: 3 bytes
        //   S  : sign of DX, take 1 char, 1/2 byte, + for 1, - for 0
        //   S  : sign of DY, take 1 char, 1/2 byte
        //   +DX: abs(DX) take 4 chars, 2 bytes
        //   +DY: abs(DY) take 4 chars, 2 bytes

        if(auto texPtr = g_glDevice->loadPNGTexture(dataBuf.data(), dataBuf.size())){
            return std::make_tuple(PNGTexOffElement
            {
                .dx = to_d((fontFileName[8] != '0') ? 1 : (-1)) * to_d(hexstr::to_hex<uint32_t, 2>(fontFileName + 10)),
                .dy = to_d((fontFileName[9] != '0') ? 1 : (-1)) * to_d(hexstr::to_hex<uint32_t, 2>(fontFileName + 14)),

                .texture = texPtr,
            }, 1);
        }
    }
    return {};
}

void PNGTexOffDB::freeResource(PNGTexOffElement &element)
{
    if(element.texture){
        g_glDevice->destroyTexture(element.texture);
        element.texture = nullptr;
    }
}

// ===== emojidb.cpp =====

extern GLDevice *g_glDevice;
GLTexID EmojiDB::retrieve(uint32_t key, int *srcXPtr, int *srcYPtr, int *srcWPtr, int *srcHPtr, int *frameH1Ptr, int *fpsPtr, int *frameCountPtr)
{
    if(auto p = innLoad(key & 0XFFFFFF00)){
        const int texW = GLDeviceHelper::getTextureWidth(p->texture);
        const int gridXCount = texW / p->frameW;
        const int frameIndex = to_d(key & 0X000000FF);

        if(      srcXPtr){       *srcXPtr = (frameIndex % gridXCount) * p->frameW;     }
        if(      srcYPtr){       *srcYPtr = (frameIndex / gridXCount) * p->frameH;     }
        if(      srcWPtr){       *srcWPtr =                             p->frameW;     }
        if(      srcHPtr){       *srcHPtr =                             p->frameH;     }
        if(   frameH1Ptr){    *frameH1Ptr =                             p->frameH1;    }
        if(       fpsPtr){        *fpsPtr =                             p->fps;        }
        if(frameCountPtr){ *frameCountPtr =                             p->frameCount; }

        return p->texture;
    }
    return nullptr;
}

GLTexID EmojiDB::retrieve(uint8_t emojiSet, uint16_t emojiSubset, uint8_t emojiIndex, int *srcXPtr, int *srcYPtr, int *srcWPtr, int *srcHPtr, int *fpsPtr, int *frameH1Ptr, int *frameCountPtr)
{
    return retrieve((to_u32(emojiSet) << 24) + to_u32(emojiSubset << 8) + to_u32(emojiIndex), srcXPtr, srcYPtr, srcWPtr, srcHPtr, frameH1Ptr, fpsPtr, frameCountPtr);
}

std::optional<std::tuple<EmojiElement, size_t>> EmojiDB::loadResource(uint32_t key)
{
    char keyString[16];
    std::vector<uint8_t> dataBuf;

    if(auto fileName = m_zsdbPtr->decomp(hexstr::to_string<uint32_t, 4>(key, keyString, true), 8, &dataBuf); fileName && (std::strlen(fileName) >= 22)){
        if(auto texPtr = g_glDevice->loadPNGTexture(dataBuf.data(), dataBuf.size())){
            return std::make_tuple(EmojiElement
            {
                .frameW     = to_d(hexstr::to_hex<uint16_t, 2>(fileName + 12)),
                .frameH     = to_d(hexstr::to_hex<uint16_t, 2>(fileName + 16)),
                .frameH1    = to_d(hexstr::to_hex<uint16_t, 2>(fileName + 20)),
                .fps        = to_d(hexstr::to_hex< uint8_t, 1>(fileName + 10)),
                .frameCount = to_d(hexstr::to_hex< uint8_t, 1>(fileName +  8)),
                .texture    = texPtr,
            }, 1);
        }
    }
    return {};
}

void EmojiDB::freeResource(EmojiElement &element)
{
    if(element.texture){
        g_glDevice->destroyTexture(element.texture);
        element.texture = nullptr;
    }
}
