#include "ImSkillBoard.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <string>

#include "dbcomid.hpp"
#include "fflerror.hpp"
#include "gldevice.hpp"
#include "gui_font.hpp"
#include "gui_texture.hpp"
#include "magicrecord.hpp"
#include "processrun.hpp"
#include "skillboard.hpp"
#include "strf.hpp"
#include "totype.hpp"

extern GLDevice *g_glDevice;
extern PNGTexDB *g_progUseDB;
extern FontexDB *g_fontexDB;

namespace
{
    constexpr ImVec2 pagePos {18.0f, 44.0f};
    constexpr ImVec2 pageSize {304.0f, 329.0f};

    void drawTexture(ImDrawList *list, const GLTexID texture, const ImVec2 pos, const ImU32 tint = IM_COL32_WHITE)
    {
        if(texture){
            list->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h}, {0, 0}, {1, 1}, tint);
        }
    }

    void drawText(ImDrawList *list, ImVec2 pos, const std::string &text, const uint8_t font, const uint8_t size, const ImU32 color)
    {
        if(const auto texture = g_fontexDB->retrieve(font, size, 0, text.c_str()); texture){
            list->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h}, {0, 0}, {1, 1}, color);
        }
    }

    bool textureButton(const char *id, const uint32_t hoverID, const uint32_t downID, const ImVec2 pos, const bool selected = false)
    {
        const auto hover = g_progUseDB->retrieve(hoverID);
        const auto down = g_progUseDB->retrieve(downID);
        const auto sizeTexture = hover ? hover : down;
        if(!sizeTexture){
            return false;
        }

        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {to_f(sizeTexture.w), to_f(sizeTexture.h)});
        const auto shown = selected || ImGui::IsItemActive()
                         ? (down ? down : hover)
                         : (ImGui::IsItemHovered() ? hover : GLTexID {});
        drawTexture(ImGui::GetWindowDrawList(), shown, pos);
        return clicked;
    }

    int pageIndex(const uint32_t magicID)
    {
        if(const auto &record = DBCOM_MAGICRECORD(magicID)){
            const int elemID = magicElemID(record.elem);
            if(elemID == MET_NONE){
                return 7;
            }
            if(elemID >= MET_BEGIN && elemID < MET_END){
                return elemID - MET_BEGIN;
            }
        }
        return -1;
    }
}

struct ImSkillBoard::Impl
{
    ProcessRun *processRun = nullptr;
    SkillBoardConfig config;

    int selectedTab = 0;
    mutable int hoveredTab = -1;
    mutable uint32_t hoveredMagicID = 0;
    mutable float scroll = 0.0f;
    mutable bool dragging = false;
};

ImSkillBoard::ImSkillBoard(const int x, const int y, ProcessRun *processRun)
    : ImBoard("##skill-board")
    , m_impl(std::make_unique<Impl>())
{
    m_impl->processRun = fflcheck(processRun);
    moveTo(to_f(x), to_f(y));
}

ImSkillBoard::~ImSkillBoard() = default;

void ImSkillBoard::draw() const
{
    if(!show()){
        return;
    }

    const auto background = g_progUseDB->retrieve(0X05000000);
    if(!background){
        return;
    }

    if(beginWindow({to_f(background.w), to_f(background.h)})){
        const ImVec2 pos = ImGui::GetWindowPos();
        auto *drawList = ImGui::GetWindowDrawList();
        drawTexture(drawList, background, pos);

        m_impl->hoveredTab = -1;
        for(int i = 0; i < 8; ++i){
            ImGui::PushID(i);
            const ImVec2 tabPos {pos.x + 45.0f + 34.0f * i, pos.y + 10.0f};
            if(textureButton("##skill-tab", 0X05000020 + to_u32(i), 0X05000030 + to_u32(i), tabPos, i == m_impl->selectedTab)){
                if(m_impl->selectedTab != i){
                    m_impl->selectedTab = i;
                    m_impl->scroll = 0.0f;
                }
            }
            if(ImGui::IsItemHovered()){
                m_impl->hoveredTab = i;
            }
            ImGui::PopID();
        }

        const auto pageTexture = g_progUseDB->retrieve(0X05000010 + to_u32(m_impl->selectedTab));
        float maxButtonReachY = 0.0f;
        for(const auto &gfx: SkillBoard::m_iconGfxList){
            if(pageIndex(gfx.magicID) == m_impl->selectedTab){
                if(const auto icon = g_progUseDB->retrieve(gfx.magicIcon); icon){
                    maxButtonReachY = std::max(maxButtonReachY, to_f(gfx.y * 65 + 13 + icon.h + 8));
                }
            }
        }
        const float backgroundHeight = pageTexture ? to_f(pageTexture.h) : pageSize.y;
        const float pageHeight = std::clamp(
            maxButtonReachY + 10.0f,
            std::min(backgroundHeight, pageSize.y),
            std::max(backgroundHeight, pageSize.y));
        const float scrollReach = std::max(0.0f, pageHeight - pageSize.y);
        const float pageOffsetY = -scrollReach * m_impl->scroll;

        const ImVec2 clipMin {pos.x + pagePos.x, pos.y + pagePos.y};
        const ImVec2 clipMax {clipMin.x + pageSize.x, clipMin.y + pageSize.y};
        drawList->PushClipRect(clipMin, clipMax, true);
        drawTexture(drawList, pageTexture, {clipMin.x, clipMin.y + pageOffsetY});

        m_impl->hoveredMagicID = 0;
        for(const auto &gfx: SkillBoard::m_iconGfxList){
            if(pageIndex(gfx.magicID) != m_impl->selectedTab){
                continue;
            }

            const auto icon = g_progUseDB->retrieve(gfx.magicIcon);
            if(!icon){
                continue;
            }

            const ImVec2 iconPos
            {
                clipMin.x + to_f(gfx.x * 60 + 12),
                clipMin.y + pageOffsetY + to_f(gfx.y * 65 + 13),
            };
            if(iconPos.y + icon.h < clipMin.y || iconPos.y >= clipMax.y){
                continue;
            }

            ImGui::PushID(to_d(gfx.magicID));
            ImGui::SetCursorScreenPos(iconPos);
            ImGui::InvisibleButton("##magic-icon", {to_f(icon.w + 8), to_f(icon.h + 8)});
            if(ImGui::IsItemHovered()){
                m_impl->hoveredMagicID = gfx.magicID;
            }

            if(const auto level = m_impl->config.getMagicLevel(gfx.magicID)){
                drawTexture(drawList, icon, iconPos);
                drawText(
                    drawList,
                    {iconPos.x + icon.w - 2.0f, iconPos.y + icon.h - 1.0f},
                    std::to_string(level.value()),
                    3,
                    12,
                    IM_COL32(255, 255, 0, 255));

                if(const auto key = m_impl->config.getMagicKey(gfx.magicID)){
                    const std::string keyText(1, static_cast<char>(std::toupper(static_cast<unsigned char>(key.value()))));
                    drawText(drawList, {iconPos.x + 4, iconPos.y + 4}, keyText, 3, 20, IM_COL32(0, 0, 0, 224));
                    drawText(drawList, {iconPos.x + 2, iconPos.y + 2}, keyText, 3, 20, IM_COL32(255, 128, 0, 224));
                }
            }
            ImGui::PopID();
        }
        drawList->PopClipRect();

        if(ImGui::IsMouseHoveringRect(clipMin, clipMax) && ImGui::GetIO().MouseWheel != 0.0f){
            m_impl->scroll = std::clamp(m_impl->scroll - ImGui::GetIO().MouseWheel * 0.1f, 0.0f, 1.0f);
        }

        const ImVec2 sliderMin {pos.x + 326.0f, pos.y + 74.0f};
        ImGui::SetCursorScreenPos({sliderMin.x - 7.0f, sliderMin.y - 7.0f});
        ImGui::InvisibleButton("##skill-slider", {20.0f, 280.0f});
        if(ImGui::IsItemActive()){
            m_impl->scroll = std::clamp((ImGui::GetIO().MousePos.y - sliderMin.y) / 266.0f, 0.0f, 1.0f);
        }
        if(const auto thumb = g_progUseDB->retrieve(0X00000080); thumb){
            const ImVec2 center {sliderMin.x + 3.0f, sliderMin.y + m_impl->scroll * 266.0f};
            drawTexture(drawList, thumb, {center.x - 7.0f, center.y - 7.0f}, IM_COL32_WHITE);
        }

        if(textureButton("##skill-close", 0X0000001C, 0X0000001D, {pos.x + 317.0f, pos.y + 402.0f})){
            setShow(false);
        }

        std::string tabName;
        if(m_impl->hoveredTab >= 0){
            tabName = str_printf("元素【%s】", to_cstr(magicElemName(SkillBoard::tabElem(m_impl->hoveredTab))));
        }
        else if(m_impl->hoveredMagicID){
            if(const auto &record = DBCOM_MAGICRECORD(m_impl->hoveredMagicID)){
                tabName = str_printf("元素【%s】%s", str_haschar(record.elem) ? to_cstr(record.elem) : "无", to_cstr(record.name));
            }
        }
        if(tabName.empty()){
            tabName = str_printf("元素【%s】", to_cstr(magicElemName(SkillBoard::tabElem(m_impl->selectedTab))));
        }
        drawText(drawList, {pos.x + 30.0f, pos.y + 400.0f}, tabName, 1, 12, IM_COL32_WHITE);

        const auto mousePos = ImGui::GetIO().MousePos;
        const bool mouseOnPage = mousePos.x >= clipMin.x && mousePos.x < clipMax.x
                              && mousePos.y >= clipMin.y && mousePos.y < clipMax.y;
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                && ImGui::IsWindowHovered()
                && !ImGui::IsAnyItemHovered()
                && !mouseOnPage){
            m_impl->dragging = true;
        }
        if(!ImGui::IsMouseDown(ImGuiMouseButton_Left)){
            m_impl->dragging = false;
        }
        if(m_impl->dragging){
            const auto delta = ImGui::GetIO().MouseDelta;
            moveTo(pos.x + delta.x, pos.y + delta.y);
        }
    }
    endWindow();
}

void ImSkillBoard::update(double)
{}

bool ImSkillBoard::processEvent(const MirEvent &event) const
{
    if(!show()){
        return false;
    }

    if(event.type == MIR_EVENT_KEY_DOWN && m_impl->hoveredMagicID){
        const char key = GLDeviceHelper::getKeyChar(event, false);
        if((key >= '0' && key <= '9') || (key >= 'a' && key <= 'z')){
            if(m_impl->config.hasMagicID(m_impl->hoveredMagicID)){
                const auto gfx = SkillBoard::getMagicIconGfx(m_impl->hoveredMagicID);
                if(gfx && gfx->passive){
                    m_impl->processRun->addCBLog(
                        CBLOG_SYS,
                        u8"无法为被动技能设置快捷键：%s",
                        to_cstr(DBCOM_MAGICRECORD(m_impl->hoveredMagicID).name));
                }
                else{
                    m_impl->config.setMagicKey(m_impl->hoveredMagicID, key);
                    m_impl->processRun->requestSetMagicKey(m_impl->hoveredMagicID, key);
                }
            }
            return true;
        }
    }
    if(event.type == MIR_EVENT_KEY_DOWN && event.key.key == MIRK_ESCAPE){
        setShow(false);
        return true;
    }
    return ImBoard::processEvent(event);
}

SkillBoardConfig &ImSkillBoard::getConfig()
{
    return m_impl->config;
}

const SkillBoardConfig &ImSkillBoard::getConfig() const
{
    return m_impl->config;
}
