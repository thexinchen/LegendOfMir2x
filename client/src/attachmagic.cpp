#include "colorf.hpp"
#include "dbcomid.hpp"
#include "gldevice.hpp"
#include "attachmagic.hpp"
#include "pngtexoffdb.hpp"

extern GLDevice *g_glDevice;
extern PNGTexOffDB *g_magicDB;

void AttachMagic::drawShift(int shiftX, int shiftY, uint32_t modColor) const
{
    if(m_gfxEntry->gfxID == SYS_U32NIL){
        return;
    }

    const auto texID = [this]() -> uint32_t
    {
        switch(m_gfxEntry->gfxDirType){
            case  1: return m_gfxEntry->gfxID + frame();
            case  4:
            case  8:
            case 16: return m_gfxEntry->gfxID + frame() + m_gfxDirIndex * m_gfxEntry->gfxIDCount;
            default: throw fflpanic("invalid gfxDirType: {}", m_gfxEntry->gfxDirType);
        }
    }();

    if(auto [texPtr, offX, offY] = g_magicDB->retrieve(texID); texPtr){
        GLDeviceHelper::EnableTextureModColor enableModColor(texPtr, colorf::modRGBA(m_gfxEntryRef ? m_gfxEntryRef->modColor : m_gfxEntry->modColor, modColor));
        GLDeviceHelper::EnableTextureBlendMode enableBlendMode(texPtr, MIR_BLENDMODE_BLEND);
        g_glDevice->drawTexture(texPtr, shiftX + offX, shiftY + offY);
    }
}

void Thunderbolt::drawShift(int shiftX, int shiftY, uint32_t modColor) const
{
    if(m_gfxEntry->gfxID == SYS_U32NIL){
        return;
    }

    const auto texID = [this]() -> uint32_t
    {
        switch(m_gfxEntry->gfxDirType){
            case  1: return m_gfxEntry->gfxID + frame();
            case  4:
            case  8:
            case 16: return m_gfxEntry->gfxID + frame() + m_gfxDirIndex * m_gfxEntry->gfxIDCount;
            default: throw fflpanic("invalid gfxDirType: {}", m_gfxEntry->gfxDirType);
        }
    }();

    if(auto [texPtr, offX, offY] = g_magicDB->retrieve(texID); texPtr){
        const auto [texW, texH] = GLDeviceHelper::getTextureSize(texPtr);
        GLDeviceHelper::EnableTextureModColor enableModColor(texPtr, colorf::modRGBA(m_gfxEntryRef ? m_gfxEntryRef->modColor : m_gfxEntry->modColor, modColor));
        GLDeviceHelper::EnableTextureBlendMode enableBlendMode(texPtr, MIR_BLENDMODE_BLEND);

        // thunder bolt has 5 frames
        // frame 0 ~ 3 are long, last frame is short
        g_glDevice->drawTexture(texPtr, shiftX + offX, shiftY + offY);
        if(frame() <= 3){
            g_glDevice->drawTextureEx(texPtr, 0, 0, texW, texH, shiftX + offX, shiftY + offY - texH, texW, texH, 0, 0, 0, MIR_FLIP_VERTICAL);
        }
    }
}

void TaoYellowBlueRing::drawShift(int shiftX, int shiftY, uint32_t modColor) const
{
    const auto r = std::fabs(std::cos(m_accuTime / 800.0));
    const auto timedModColor = colorf::RGBA(0XFF, 0XFF, 0XFF, std::max<uint8_t>(colorf::round255(r * 255), 32));
    AttachMagic::drawShift(shiftX, shiftY, colorf::modRGBA(timedModColor, modColor));
}

void AntHealing::drawShift(int shiftX, int shiftY, uint32_t modColor) const
{
    AttachMagic::drawShift(shiftX, shiftY - frame() * 3, modColor);
}
