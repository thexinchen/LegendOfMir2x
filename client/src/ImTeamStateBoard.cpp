#include "ImTeamStateBoard.hpp"

#include <algorithm>
#include <string>

#include "fflerror.hpp"
#include "gldevice.hpp"
#include "gui_font.hpp"
#include "gui_texture.hpp"
#include "processrun.hpp"
#include "strf.hpp"
#include "totype.hpp"
#include "uidf.hpp"

extern PNGTexDB *g_progUseDB;
extern FontexDB *g_fontexDB;
extern GLDevice *g_glDevice;

namespace
{
    void drawTextureCrop(ImDrawList *drawList, GLTexID texture, ImVec2 pos, ImVec2 size, ImVec2 srcPos)
    {
        drawList->AddImage(
                texture,
                pos,
                {pos.x + size.x, pos.y + size.y},
                {srcPos.x / texture.w, srcPos.y / texture.h},
                {(srcPos.x + size.x) / texture.w, (srcPos.y + size.y) / texture.h});
    }

    bool textureButton(const char *id, uint32_t offID, uint32_t downID, ImVec2 pos, bool active = true)
    {
        const auto off = g_progUseDB->retrieve(offID);
        if(!off){
            return false;
        }
        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {to_f(off.w), to_f(off.h)});
        const auto down = g_progUseDB->retrieve(downID);
        const auto shown = active && ImGui::IsItemActive() && down ? down : off;
        ImGui::GetWindowDrawList()->AddImage(
                shown,
                pos,
                {pos.x + shown.w, pos.y + shown.h},
                {0, 0},
                {1, 1},
                active ? IM_COL32_WHITE : IM_COL32(128, 128, 128, 255));
        return active && clicked;
    }

    bool overlayButton(const char *id, uint32_t hoverID, uint32_t downID, ImVec2 pos)
    {
        const auto hover = g_progUseDB->retrieve(hoverID);
        if(!hover){
            return false;
        }
        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {to_f(hover.w), to_f(hover.h)});
        if(ImGui::IsItemHovered() || ImGui::IsItemActive()){
            const auto down = g_progUseDB->retrieve(downID);
            const auto shown = ImGui::IsItemActive() && down ? down : hover;
            ImGui::GetWindowDrawList()->AddImage(shown, pos, {pos.x + shown.w, pos.y + shown.h});
        }
        return clicked;
    }

    void drawGameText(ImDrawList *drawList, ImVec2 pos, const std::string &text, ImU32 color = IM_COL32_WHITE)
    {
        if(const auto texture = g_fontexDB->retrieve(1, 12, 0, text.c_str()); texture){
            drawList->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h}, {0, 0}, {1, 1}, color);
        }
    }

    void drawCenteredGameText(ImDrawList *drawList, ImVec2 center, const char *text, ImU32 color)
    {
        if(const auto texture = g_fontexDB->retrieve(1, 12, 0, text); texture){
            const ImVec2 pos {center.x - texture.w * 0.5f, center.y};
            drawList->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h}, {0, 0}, {1, 1}, color);
        }
    }
}

ImTeamStateBoard::ImTeamStateBoard(ProcessRun *processRun)
    : ImBoard("##team-state-board")
    , m_processRun(fflcheck(processRun))
{
    moveTo(
        to_f(g_glDevice->getRendererWidth()  / 2 - 129),
        to_f(g_glDevice->getRendererHeight() / 2 - 122));
}

size_t ImTeamStateBoard::lineCount() const
{
    return m_showCandidateList ? m_teamCandidateList.size() : m_teamMemberList.memberList.size();
}

size_t ImTeamStateBoard::lineShowCount() const
{
    return std::clamp(lineCount(), to_uz(uidMinCount), to_uz(uidMaxCount));
}

const SDTeamPlayer &ImTeamStateBoard::getSDTeamPlayer(int index) const
{
    return m_showCandidateList
        ? m_teamCandidateList.at(index).first.player
        : m_teamMemberList.memberList.at(index);
}

void ImTeamStateBoard::draw() const
{
    if(!show()){
        return;
    }

    const auto background = g_progUseDB->retrieve(0X00000150);
    if(!background){
        return;
    }

    const int visibleCount = to_d(lineShowCount());
    const int boardH = background.h - uidRegionH + visibleCount * lineH;
    if(beginWindow({to_f(background.w), to_f(boardH)})){
        const auto pos = ImGui::GetWindowPos();
        auto *drawList = ImGui::GetWindowDrawList();
        const int repeatStartY = uidRegionY + (uidRegionH - texRepeatH) / 2;
        drawTextureCrop(drawList, background, pos, {to_f(background.w), to_f(repeatStartY)}, {0, 0});

        const int neededUIDRegionH = visibleCount * lineH;
        const int neededRepeatTexH = neededUIDRegionH - (uidRegionH - texRepeatH);
        int drawnRepeatH = 0;
        while(drawnRepeatH < neededRepeatTexH){
            const int copyH = std::min(texRepeatH, neededRepeatTexH - drawnRepeatH);
            drawTextureCrop(
                    drawList,
                    background,
                    {pos.x, pos.y + repeatStartY + drawnRepeatH},
                    {to_f(background.w), to_f(copyH)},
                    {0, to_f(repeatStartY)});
            drawnRepeatH += copyH;
        }
        const int repeatEndY = repeatStartY + texRepeatH;
        drawTextureCrop(
                drawList,
                background,
                {pos.x, pos.y + repeatStartY + neededRepeatTexH},
                {to_f(background.w), to_f(background.h - repeatEndY)},
                {0, to_f(repeatEndY)});

        drawCenteredGameText(
                drawList,
                {pos.x + background.w * 0.5f, pos.y + 57.0f},
                m_showCandidateList ? "申请加入" : "当前队伍",
                IM_COL32(255, 255, 0, 255));

        overlayButton("##team-enable", 0X00000200, 0X00000201, {pos.x + 24, pos.y + 47});

        const int modeIndex = m_showCandidateList ? 1 : 0;
        const int maxStart = std::max(0, to_d(lineCount()) - visibleCount);
        m_startIndex[modeIndex] = std::clamp(m_startIndex[modeIndex], 0, maxStart);
        for(int i = 0; i < visibleCount && i + m_startIndex[modeIndex] < to_d(lineCount()); ++i){
            const int itemIndex = i + m_startIndex[modeIndex];
            const ImVec2 linePos {pos.x + uidRegionX, pos.y + uidRegionY + i * lineH};
            ImGui::SetCursorScreenPos(linePos);
            ImGui::PushID(i);
            if(ImGui::InvisibleButton("##team-line", {to_f(uidRegionW), to_f(lineH)})){
                m_selectedIndex[modeIndex] = itemIndex;
            }
            if(m_selectedIndex[modeIndex] == itemIndex){
                drawList->AddRectFilled(linePos, {linePos.x + uidRegionW, linePos.y + lineH}, IM_COL32(255, 0, 0, 100));
            }
            if(ImGui::IsItemHovered()){
                drawList->AddRectFilled(linePos, {linePos.x + uidRegionW, linePos.y + lineH}, IM_COL32(0, 0, 255, 100));
                if(ImGui::GetIO().MouseWheel != 0.0f){
                    m_startIndex[modeIndex] = std::clamp(
                            m_startIndex[modeIndex] - to_d(ImGui::GetIO().MouseWheel),
                            0,
                            maxStart);
                }
            }
            ImGui::PopID();

            const auto &player = getSDTeamPlayer(itemIndex);
            const auto name = player.name.empty() ? uidf::getUIDString(player.uid) : player.name;
            drawGameText(drawList, {linePos.x + 5.0f, linePos.y + 2.0f}, str_printf("%d %s", itemIndex, name.c_str()));
        }

        const int buttonY = uidRegionY + visibleCount * lineH + 14;
        if(overlayButton("##team-switch", 0X00000160, 0X00000161, {pos.x + 19, pos.y + buttonY})){
            m_showCandidateList = !m_showCandidateList;
        }
        if(textureButton("##team-add", 0X00000170, 0X00000171, {pos.x + 72, pos.y + buttonY}, m_showCandidateList)){
            const int selected = m_selectedIndex[1];
            if(selected >= 0 && selected < to_d(lineCount())){
                m_processRun->requestJoinTeam(getSDTeamPlayer(selected).uid);
            }
        }
        if(textureButton("##team-delete", 0X00000180, 0X00000181, {pos.x + 125, pos.y + buttonY}, !m_showCandidateList)){
            const int selected = m_selectedIndex[0];
            if(selected >= 0 && selected < to_d(lineCount())){
                m_processRun->requestLeaveTeam(getSDTeamPlayer(selected).uid);
            }
        }
        if(overlayButton("##team-refresh", 0X00000190, 0X00000191, {pos.x + 177, pos.y + buttonY})){
            refresh();
        }
        if(overlayButton("##team-close", 0X0000001C, 0X0000001D, {pos.x + 217, pos.y + buttonY + 8})){
            setShow(false);
        }
    }
    endWindow();
}

bool ImTeamStateBoard::processEvent(const MirEvent &event) const
{
    if(show() && event.type == MIR_EVENT_KEY_DOWN && event.key.key == MIRK_ESCAPE){
        setShow(false);
        return true;
    }
    return ImBoard::processEvent(event);
}

void ImTeamStateBoard::refresh() const
{
    m_startIndex[m_showCandidateList ? 1 : 0] = 0;
}

void ImTeamStateBoard::addTeamCandidate(SDTeamCandidate candidate)
{
    if(std::ranges::any_of(m_teamMemberList.memberList, [&candidate](const auto &player)
    {
        return player.uid == candidate.player.uid;
    })){
        return;
    }

    std::erase_if(m_teamCandidateList, [&candidate](const auto &entry)
    {
        return entry.first.player.uid == candidate.player.uid;
    });
    m_teamCandidateList.push_front({std::move(candidate), hres_timer()});
}

void ImTeamStateBoard::setTeamMemberList(SDTeamMemberList list)
{
    m_teamMemberList = std::move(list);
    std::erase_if(m_teamCandidateList, [this](const auto &entry)
    {
        return std::ranges::any_of(m_teamMemberList.memberList, [&entry](const auto &member)
        {
            return member.uid == entry.first.player.uid;
        });
    });
    if(m_selectedIndex[0] >= to_d(m_teamMemberList.memberList.size())){
        m_selectedIndex[0] = -1;
    }
}
