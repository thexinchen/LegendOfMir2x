#include "ImAcutionBoard.hpp"

#include "fflerror.hpp"
#include "gui_texture.hpp"
#include "processrun.hpp"

extern PNGTexDB *g_progUseDB;

ImAcutionBoard::ImAcutionBoard(ProcessRun *processRun)
    : ImBoard("##auction-board", false)
    , m_processRun(fflcheck(processRun))
{}

void ImAcutionBoard::draw() const
{
    if(!show()){
        return;
    }

    const auto background = g_progUseDB->retrieve(0X00001400);
    if(!background){
        return;
    }

    if(beginWindow({static_cast<float>(background.w), static_cast<float>(background.h)})){
        const auto pos = ImGui::GetWindowPos();
        ImGui::GetWindowDrawList()->AddImage(
            background,
            pos,
            {pos.x + background.w, pos.y + background.h});
    }
    endWindow();
}
