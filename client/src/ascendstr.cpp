#include <cstdint>
#include <cinttypes>
#include "log.hpp"
#include "colorf.hpp"
#include "pngtexdb.hpp"
#include "ascendstr.hpp"
#include "gldevice.hpp"
#include "totype.hpp"

extern Log *g_mir2xLog;
extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

AscendStr::AscendStr(int argType, int argValue, int argX, int argY)
    : m_type(argType)
    , m_value(argValue)
    , m_x(argX)
    , m_y(argY)
{
    switch(m_type){
        case ASCENDSTR_MISS:
            {
                m_value = 0;
                break;
            }
        case ASCENDSTR_RED:
        case ASCENDSTR_BLUE:
        case ASCENDSTR_GREEN:
            {
                break;
            }
        default:
            {
                throw fflreach();
            }
    }
}

void AscendStr::draw(int viewX, int viewY)
{
    if(ratio() < 1.0){
        /* */ auto currX = x();
        const auto currY = y();
        const auto currA = to_u8(std::lround(255 * (1.0 - ratio())));

        switch(type()){
            case ASCENDSTR_MISS:
                {
                    if(auto texPtr = g_progUseDB->retrieve(0X03000030)){
                        GLDeviceHelper::EnableTextureModColor enableColor(texPtr, colorf::RGBA(255, 255, 255, currA));
                        g_glDevice->drawTexture(texPtr, currX - viewX, currY - viewY);
                    }
                    break;
                }
            case ASCENDSTR_RED:
            case ASCENDSTR_BLUE:
            case ASCENDSTR_GREEN:
                {
                    if(value()){
                        const uint32_t baseKey = 0X03000000 | ((type() - ASCENDSTR_BEGIN) << 4);
                        if(auto texPtr = g_progUseDB->retrieve(baseKey | ((value() < 0) ? 0X0A : 0X0B))){
                            GLDeviceHelper::EnableTextureModColor enableColor(texPtr, colorf::RGBA(255, 255, 255, currA));
                            g_glDevice->drawTexture(texPtr, currX - viewX, currY - viewY + ((value() < 0) ? 4 : 1));
                            currX += GLDeviceHelper::getTextureWidth(texPtr);
                        }

                        for(const auto chNum: std::to_string(std::labs(value()))){
                            if(auto texPtr = g_progUseDB->retrieve(baseKey | (chNum - '0'))){
                                GLDeviceHelper::EnableTextureModColor enableColor(texPtr, colorf::RGBA(255, 255, 255, currA));
                                g_glDevice->drawTexture(texPtr, currX - viewX, currY - viewY);
                                currX += GLDeviceHelper::getTextureWidth(texPtr);
                            }
                        }
                    }
                    break;
                }
            default:
                {
                    break;
                }
        }
    }
}
