#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "fflerror.hpp"
#include "mathf.hpp"
#include "strf.hpp"
#include "sysconst.hpp"
#include "totype.hpp"
#include "magicdrawarea.hpp"

namespace
{
    constexpr ImU32 GREEN   = IM_COL32(0, 255, 0, 255);
    constexpr ImU32 RED     = IM_COL32(255, 0, 0, 255);
    constexpr ImU32 BLUE    = IM_COL32(0, 80, 255, 255);
    constexpr ImU32 MAGENTA = IM_COL32(255, 0, 255, 255);

    struct RunGfxConfig
    {
        uint32_t magicID = 0;
        uint32_t gfxID = SYS_U32NIL;
        int frameCount = 0;
        int gfxIDCount = 0;
        int gfxDirType = 0;
    };

    // MSVC doesn't preserve the backing arrays of the nested constexpr
    // initializer_list members used by MagicRecord for runtime traversal.
    // Keep the scalar "运行/跟随" fields here so this editor also works on
    // Windows; the values mirror common/src/magicrecord.inc.
    constexpr RunGfxConfig RUN_GFX_CONFIGS[]
    {
        {DBCOM_MAGICID(u8"魔法特效_护身符"), 0X000003D4, 3, 10, 16},
        {DBCOM_MAGICID(u8"魔法特效_火球术"), 0X000001A4, 5, 10, 16},
        {DBCOM_MAGICID(u8"魔法特效_大火球"), 0X00000668, 6, 10, 16},
        {DBCOM_MAGICID(u8"月魂断玉"),        0X00000D02, 6, 10,  1},
        {DBCOM_MAGICID(u8"月魂灵波"),        0X00000D70, 6, 10,  1},
        {DBCOM_MAGICID(u8"冰月神掌"),        0X00000A8C, 3, 10, 16},
        {DBCOM_MAGICID(u8"冰月震天"),        0X00000B90, 6,  6,  1},
        {DBCOM_MAGICID(u8"霹雳掌"),          0X00000BFE, 6, 10, 16},
        {DBCOM_MAGICID(u8"风掌"),            0X010001AE, 5, 10, 16},
        {DBCOM_MAGICID(u8"沙漠树魔_喷刺"),   0X030003C0, 1,  1,  1},
        {DBCOM_MAGICID(u8"掷斧骷髅_掷斧"),   0X03000320, 6, 10,  8},
        {DBCOM_MAGICID(u8"暗黑战士_喷刺"),   0X030004D8, 1, 10, 16},
        {DBCOM_MAGICID(u8"祖玛弓箭手_射箭"), 0X03000578, 1, 10, 16},
        {DBCOM_MAGICID(u8"爆毒蚂蚁_喷毒"),   0X03000050, 6,  6,  1},
    };

    const RunGfxConfig *findRunGfxConfig(uint32_t magicID)
    {
        for(const auto &config: RUN_GFX_CONFIGS){
            if(config.magicID == magicID){
                return &config;
            }
        }
        return nullptr;
    }
}

int MagicDrawArea::magicDirCount() const
{
    return m_gfxDirType;
}

std::tuple<GLTexture *, int, int> MagicDrawArea::getFrameImage(int gfxDirIndex)
{
    if(m_gfxID == SYS_U32NIL){
        return {nullptr, 0, 0};
    }
    return m_frameDBPtr->retrieve(m_gfxID + m_frame + gfxDirIndex * m_gfxIDCount);
}

void MagicDrawArea::draw(const ImVec2 &canvasPos, const ImVec2 &canvasSize)
{
    auto *drawList = ImGui::GetWindowDrawList();
    const ImVec2 canvasMax(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);
    drawList->AddRectFilled(canvasPos, canvasMax, IM_COL32_BLACK);
    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("MagicCanvas", canvasSize, ImGuiButtonFlags_MouseButtonLeft);

    const int width = static_cast<int>(canvasSize.x);
    const int height = static_cast<int>(canvasSize.y);
    const int centerX = width / 2;
    const int centerY = height / 2;
    const ImVec2 center(canvasPos.x + centerX, canvasPos.y + centerY);

    if(ImGui::IsItemHovered()){
        const auto mouse = ImGui::GetIO().MousePos;
        const int mouseX = static_cast<int>(mouse.x - canvasPos.x);
        const int mouseY = static_cast<int>(mouse.y - canvasPos.y);
        const bool control = ImGui::GetIO().KeyCtrl;
        m_adjustR = control && ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if(m_adjustR){
            m_r = mathf::LDistance<int>(centerX, centerY, mouseX, mouseY);
        }

        if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !control){
            for(int i = 0; i < magicDirCount(); ++i){
                const auto [dstX, dstY] = getGfxDirPLoc(i, width, height);
                const auto [image, dx, dy] = getFrameImage(i);
                if(image && image->valid() &&
                        mathf::pointInRectangle(mouseX, mouseY, dstX + dx, dstY + dy, image->width(), image->height())){
                    m_adjustTargetOff = i;
                    break;
                }
            }
        }
        else if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !control && m_adjustTargetOff >= 0){
            const auto [dstX, dstY] = getGfxDirPLoc(m_adjustTargetOff, width, height);
            m_offList.at(m_adjustTargetOff) = {mouseX - dstX, mouseY - dstY};
            m_adjustTargetOff = -1;
        }
    }
    else{
        m_adjustR = false;
    }

    drawList->PushClipRect(canvasPos, canvasMax, true);
    drawList->AddCircle(center, static_cast<float>(m_r), m_adjustR ? RED : GREEN);
    for(int i = 0; i < magicDirCount(); ++i){
        const auto [dstX, dstY] = getGfxDirPLoc(i, width, height);
        drawList->AddLine(center, ImVec2(canvasPos.x + dstX, canvasPos.y + dstY), BLUE);
    }
    for(int i = 0; i < magicDirCount(); ++i){
        const auto [dstX, dstY] = getGfxDirPLoc(i, width, height);
        const auto [image, dx, dy] = getFrameImage(i);
        if(!image || !image->valid()){
            continue;
        }
        const ImVec2 anchor(canvasPos.x + dstX, canvasPos.y + dstY);
        const ImVec2 imageMin(anchor.x + dx, anchor.y + dy);
        const ImVec2 imageMax(imageMin.x + image->width(), imageMin.y + image->height());
        drawList->AddImage(image->id(), imageMin, imageMax);
        drawList->AddRect(imageMin, imageMax, BLUE);
        drawList->AddLine(anchor, imageMin, RED);
        const auto [offX, offY] = m_offList.at(i);
        drawList->AddLine(anchor, ImVec2(anchor.x + offX, anchor.y + offY), RED);
    }
    if(m_adjustTargetOff >= 0){
        const auto mouse = ImGui::GetIO().MousePos;
        const auto [dstX, dstY] = getGfxDirPLoc(m_adjustTargetOff, width, height);
        const ImVec2 anchor(canvasPos.x + dstX, canvasPos.y + dstY);
        drawList->AddLine(anchor, mouse, MAGENTA);
        drawList->AddLine(ImVec2(mouse.x - 8, mouse.y), ImVec2(mouse.x + 8, mouse.y), MAGENTA);
        drawList->AddLine(ImVec2(mouse.x, mouse.y - 8), ImVec2(mouse.x, mouse.y + 8), MAGENTA);
    }
    drawList->PopClipRect();
}

void MagicDrawArea::load(uint32_t magicID, const char *dbPathName)
{
    const auto *config = findRunGfxConfig(magicID);
    fflassert(config);

    m_magicID = magicID;
    m_gfxID = config->gfxID;
    m_frameCount = config->frameCount;
    m_gfxIDCount = config->gfxIDCount;
    m_gfxDirType = config->gfxDirType;
    m_frameDBPtr = std::make_unique<MagicFrameDB>(dbPathName);
    m_offList.resize(magicDirCount());
}

void MagicDrawArea::reset()
{
    m_r = 160;
    std::fill(m_offList.begin(), m_offList.end(), std::tuple<int, int>(0, 0));
}

std::string MagicDrawArea::output() const
{
    std::string result = str_printf("case DBCOM_MAGICID(u8\"%s\"):\n{\nswitch(gfxDirIndex()){\n",
            to_cstr(DBCOM_MAGICRECORD(m_magicID).name));
    for(int i = 0; const auto &[dx, dy]: m_offList){
        result += str_printf("case %2d: return {%3d, %3d};\n", i++, dx, dy);
    }
    result += "default: throw bad_reach();\n}\n}\n";
    return result;
}

void MagicDrawArea::updateFrame()
{
    m_frame = (m_frame + 1) % m_frameCount;
}

std::tuple<int, int> MagicDrawArea::getGfxDirPLoc(int gfxDirIndex, int width, int height) const
{
    fflassert(gfxDirIndex >= 0);
    constexpr float pi = 3.1415926535f;
    constexpr float angle16 = 2.0f * pi / 16.0f;
    const float angle = pi / 2.0f - gfxDirIndex * angle16;
    return {
        to_d(width / 2.0f + to_f(m_r) * std::cos(angle)),
        to_d(height / 2.0f - to_f(m_r) * std::sin(angle)),
    };
}
