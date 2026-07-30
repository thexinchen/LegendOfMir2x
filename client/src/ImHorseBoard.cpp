#include "ImHorseBoard.hpp"

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
        return clicked;
    }
}

ImHorseBoard::ImHorseBoard(ProcessRun *processRun)
    : ImBoard("##horse-board")
    , m_processRun(fflcheck(processRun))
{
    moveTo(
        static_cast<float>(g_glDevice->getRendererWidth()  / 2 - 128),
        static_cast<float>(g_glDevice->getRendererHeight() / 2 - 161));
}

void ImHorseBoard::draw() const
{
    if(!show()){
        return;
    }

    constexpr ImVec2 boardSize {257, 322};
    if(beginWindow(boardSize)){
        const auto pos = ImGui::GetWindowPos();
        auto *drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(pos, {pos.x + boardSize.x, pos.y + boardSize.y}, IM_COL32(128, 128, 128, 255));
        drawList->AddRectFilled(
            {pos.x + 15, pos.y + 39},
            {pos.x + 242, pos.y + 227},
            IM_COL32_BLACK);
        if(const auto background = g_progUseDB->retrieve(0X00000700); background){
            drawList->AddImage(background, pos, {pos.x + background.w, pos.y + background.h});
        }

        if(overlayButton("##horse-up", 0X00000710, 0X00000711, {pos.x + 20, pos.y + 231})){
        }
        if(overlayButton("##horse-down", 0X00000720, 0X00000721, {pos.x + 77, pos.y + 231})){
        }
        if(overlayButton("##horse-hide", 0X00000730, 0X00000731, {pos.x + 134, pos.y + 231})){
        }
        if(overlayButton("##horse-show", 0X00000740, 0X00000741, {pos.x + 191, pos.y + 231})){
        }
        if(overlayButton("##horse-close", 0X0000001C, 0X0000001D, {pos.x + 217, pos.y + 278})){
            setShow(false);
        }
    }
    endWindow();
}

bool ImHorseBoard::processEvent(const MirEvent &event) const
{
    if(show() && event.type == MIR_EVENT_KEY_DOWN && event.key.key == MIRK_ESCAPE){
        setShow(false);
        return true;
    }
    return ImBoard::processEvent(event);
}
