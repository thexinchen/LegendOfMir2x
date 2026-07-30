#include "ImGuildBoard.hpp"

#include <algorithm>

#include "fflerror.hpp"
#include "gldevice.hpp"
#include "gui_texture.hpp"
#include "processrun.hpp"

extern GLDevice *g_glDevice;
extern PNGTexDB *g_progUseDB;

namespace
{
    bool overlayButton(const char *id, uint32_t hoverID, uint32_t downID, ImVec2 pos)
    {
        const auto hover = g_progUseDB->retrieve(hoverID);
        if(!hover){
            return false;
        }

        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {static_cast<float>(hover.w), static_cast<float>(hover.h)});
        if(ImGui::IsItemHovered() || ImGui::IsItemActive()){
            const auto down = g_progUseDB->retrieve(downID);
            const auto shown = ImGui::IsItemActive() && down ? down : hover;
            ImGui::GetWindowDrawList()->AddImage(shown, pos, {pos.x + shown.w, pos.y + shown.h});
        }
        if(clicked){ playButtonClickSound(); }
        return clicked;
    }
}

ImGuildBoard::ImGuildBoard(ProcessRun *processRun)
    : ImBoard("##guild-board")
    , m_processRun(fflcheck(processRun))
{
    moveTo(
        static_cast<float>(g_glDevice->getRendererWidth()  / 2 - 297),
        static_cast<float>(g_glDevice->getRendererHeight() / 2 - 222));
}

void ImGuildBoard::draw() const
{
    if(!show()){
        return;
    }

    constexpr ImVec2 boardSize {594, 444};
    if(beginWindow(boardSize)){
        const auto pos = ImGui::GetWindowPos();
        auto *drawList = ImGui::GetWindowDrawList();
        if(const auto background = g_progUseDB->retrieve(0X00000500); background){
            drawList->AddImage(background, pos, {pos.x + background.w, pos.y + background.h});
        }

        struct Button
        {
            const char *id;
            uint32_t hover;
            uint32_t down;
            float x;
        };
        constexpr Button buttonList[]
        {
            {"##guild-announcement", 0X00000510, 0X00000511,  40},
            {"##guild-members",      0X00000520, 0X00000521,  90},
            {"##guild-chat",         0X00000530, 0X00000531, 140},
            {"##guild-edit",         0X00000540, 0X00000541, 290},
            {"##guild-remove",       0X00000550, 0X00000551, 340},
            {"##guild-disband",      0X00000560, 0X00000561, 390},
            {"##guild-position",     0X00000570, 0X00000571, 440},
            {"##guild-covenant",     0X00000580, 0X00000581, 490},
        };
        for(const auto &button: buttonList){
            if(overlayButton(button.id, button.hover, button.down, {pos.x + button.x, pos.y + 385})){
            }
        }
        if(overlayButton("##guild-close", 0X0000001C, 0X0000001D, {pos.x + 554, pos.y + 399})){
            setShow(false);
        }

        constexpr float barX = 564.0f;
        constexpr float barY = 53.0f;
        constexpr float barW = 9.0f;
        constexpr float barH = 294.0f;
        ImGui::SetCursorScreenPos({pos.x + barX - 10.0f, pos.y + barY - 12.0f});
        ImGui::InvisibleButton("##guild-slider", {29.0f, barH + 24.0f});
        if(ImGui::IsItemActive()){
            m_scrollValue = std::clamp((ImGui::GetIO().MousePos.y - (pos.y + barY)) / (barH - 1.0f), 0.0f, 1.0f);
        }
        if(const auto slider = g_progUseDB->retrieve(0X00000089); slider){
            const ImVec2 sliderPos
            {
                pos.x + barX + barW * 0.5f - 10.0f,
                pos.y + barY + m_scrollValue * (barH - 1.0f) - 12.0f,
            };
            drawList->AddImage(
                slider,
                sliderPos,
                {sliderPos.x + slider.w, sliderPos.y + slider.h},
                {0, 0},
                {1, 1},
                ImGui::IsItemActive() ? IM_COL32_WHITE : IM_COL32(128, 128, 128, 255));
        }
    }
    endWindow();
}

bool ImGuildBoard::processEvent(const MirEvent &event) const
{
    if(show() && event.type == MIR_EVENT_KEY_DOWN && event.key.key == MIRK_ESCAPE){
        setShow(false);
        return true;
    }
    return ImBoard::processEvent(event);
}
