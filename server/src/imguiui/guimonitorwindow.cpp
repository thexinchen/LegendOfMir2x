#include <array>
#include <string>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif
#include <GLFW/glfw3.h>
#include <imgui.h>

#include "totype.hpp"
#include "uidf.hpp"
#include "actormsg.hpp"
#include "actorpool.hpp"
#include "guicore.hpp"
#include "guimonitorwindow.hpp"

extern ActorPool *g_actorPool;

static constexpr double MONITOR_REFRESH_SEC = 1.370; // legacy Fl::add_timeout cadence

static std::string fnTimeString(uint64_t msec)
{
    if(msec < 1000ULL){
        return std::to_string(msec) + "ms";
    }
    if(msec < 1000ULL * 60){
        return str_printf("%.2fs", to_df(msec) / 1000.0);
    }
    return str_printf("%.2fm", to_df(msec) / 60000.0);
}

void GUIMonitorWindow::openPodMonitor(uint64_t uid)
{
    m_podUID = uid;
    m_lastRefresh = -1.0; // force refresh
    m_core->setPodMonitorOpen(true);
}

void GUIMonitorWindow::refreshSnapshots()
{
    const auto now = glfwGetTime();
    if(now - m_lastRefresh < MONITOR_REFRESH_SEC){
        return;
    }
    m_lastRefresh = now;

    try{
        m_actorList = g_actorPool->getActorMonitor();
        if(m_podUID){
            m_podMonitor = g_actorPool->getPodMonitor(m_podUID);
        }
    }
    catch(const std::exception &){
        // actor pool not launched yet or mid-restart: keep stale data
    }
}

void GUIMonitorWindow::drawActorMonitor()
{
    refreshSnapshots();

    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(1030, 445), ImGuiCond_Appearing);
    if(!ImGui::Begin("Actor Monitor", &open, ImGuiWindowFlags_MenuBar)){
        ImGui::End();
        if(!open){
            m_core->setActorMonitorOpen(false);
        }
        return;
    }

    if(ImGui::BeginMenuBar()){
        if(ImGui::BeginMenu("Monitor")){
            if(ImGui::MenuItem("Exit")){
                open = false;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    const float footerHeight = ImGui::GetFrameHeight();
    if(ImGui::BeginTable("ActorTable", 8,
                ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_SizingStretchProp, ImVec2(0, -footerHeight))){
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("UID"        , ImGuiTableColumnFlags_DefaultSort, 1.6f, 0);
        ImGui::TableSetupColumn("TYPE"       , ImGuiTableColumnFlags_DefaultSort, 1.0f, 1);
        ImGui::TableSetupColumn("GROUP"      , ImGuiTableColumnFlags_DefaultSort, 0.6f, 2);
        ImGui::TableSetupColumn("LIVE"       , ImGuiTableColumnFlags_DefaultSort, 0.8f, 3);
        ImGui::TableSetupColumn("BUSY"       , ImGuiTableColumnFlags_DefaultSort, 0.8f, 4);
        ImGui::TableSetupColumn("MSG_DONE"   , ImGuiTableColumnFlags_DefaultSort, 0.9f, 5);
        ImGui::TableSetupColumn("MSG_PENDING", ImGuiTableColumnFlags_DefaultSort, 0.9f, 6);
        ImGui::TableSetupColumn("MSG_AVGDLY" , ImGuiTableColumnFlags_DefaultSort, 0.9f, 7);
        ImGui::TableHeadersRow();

        // sorting (replaces fltableimpl header-click sort)
        if(ImGuiTableSortSpecs *sortSpecs = ImGui::TableGetSortSpecs(); sortSpecs && sortSpecs->SpecsDirty){
            const auto col = sortSpecs->Specs->ColumnUserID;
            const bool asc = sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending;
            std::sort(m_actorList.begin(), m_actorList.end(), [col, asc](const ActorMonitor &lhs, const ActorMonitor &rhs) -> bool
            {
                const auto lv = [col, &lhs]() -> uint64_t
                {
                    switch(col){
                        case 0 : return lhs.uid;
                        case 1 : return to_u64(uidf::getUIDType(lhs.uid));
                        case 2 : return to_u64(g_actorPool->getBucketID(lhs.uid));
                        case 3 : return lhs.liveTick;
                        case 4 : return lhs.busyTick;
                        case 5 : return lhs.messageDone;
                        case 6 : return lhs.messagePending;
                        default: return lhs.avgDelay;
                    }
                }();
                const auto rv = [col, &rhs]() -> uint64_t
                {
                    switch(col){
                        case 0 : return rhs.uid;
                        case 1 : return to_u64(uidf::getUIDType(rhs.uid));
                        case 2 : return to_u64(g_actorPool->getBucketID(rhs.uid));
                        case 3 : return rhs.liveTick;
                        case 4 : return rhs.busyTick;
                        case 5 : return rhs.messageDone;
                        case 6 : return rhs.messagePending;
                        default: return rhs.avgDelay;
                    }
                }();
                return asc ? (lv < rv) : (lv > rv);
            });
            sortSpecs->SpecsDirty = false;
        }

        ImGuiListClipper clipper;
        clipper.Begin(to_d(m_actorList.size()));
        while(clipper.Step()){
            for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row){
                const auto &monitor = m_actorList.at(row);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                // full-row selectable gives row hover + double-click detection
                const auto uidStr = str_printf("%016llx", to_llu(monitor.uid));
                if(ImGui::Selectable(uidStr.c_str(), m_selectedActorUID == monitor.uid,
                            ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)){
                    m_selectedActorUID = monitor.uid;
                    if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){
                        openPodMonitor(monitor.uid); // drills into the pod monitor (legacy behavior)
                    }
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(uidf::getUIDTypeCStr(monitor.uid));
                ImGui::TableNextColumn();
                ImGui::Text("%d", g_actorPool->getBucketID(monitor.uid));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(fnTimeString(monitor.liveTick).c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(fnTimeString(monitor.busyTick).c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%u", monitor.messageDone);
                ImGui::TableNextColumn();
                ImGui::Text("%u", monitor.messagePending);
                ImGui::TableNextColumn();
                ImGui::Text("%u", monitor.avgDelay);
            }
        }
        ImGui::EndTable();
    }

    std::array<size_t, UID_END> uidTypeCounts {};
    for(const auto &monitor: m_actorList){
        if(const int uidType = uidf::getUIDType(monitor.uid); uidType >= UID_BEGIN && uidType < UID_END){
            ++uidTypeCounts.at(uidType);
        }
    }
    ImGui::TextDisabled("ACTORS: %zu, COR: %zu, MAP: %zu, NPC: %zu, MON: %zu, PLY: %zu, RCV: %zu, QST: %zu, SLO: %zu",
            m_actorList.size(),
            uidTypeCounts.at(UID_COR),
            uidTypeCounts.at(UID_MAP),
            uidTypeCounts.at(UID_NPC),
            uidTypeCounts.at(UID_MON),
            uidTypeCounts.at(UID_PLY),
            uidTypeCounts.at(UID_RCV),
            uidTypeCounts.at(UID_QST),
            uidTypeCounts.at(UID_SLO));
    ImGui::End();
    if(!open){
        m_core->setActorMonitorOpen(false);
    }
}

void GUIMonitorWindow::drawPodMonitor()
{
    refreshSnapshots();

    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(900, 445), ImGuiCond_Appearing);

    const auto title = str_printf("Actor Pod Monitor - %016llx", to_llu(m_podUID));
    if(!ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_MenuBar)){
        ImGui::End();
        if(!open){
            m_core->setPodMonitorOpen(false);
        }
        return;
    }

    if(ImGui::BeginMenuBar()){
        if(ImGui::BeginMenu("Monitor")){
            if(ImGui::MenuItem("Exit")){
                open = false;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    int messageTypeCount = 0;
    const float footerHeight = ImGui::GetFrameHeight();
    if(m_podMonitor && m_podMonitor.uid == m_podUID){
        if(ImGui::BeginTable("PodTable", 5,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp, ImVec2(0, -footerHeight))){
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("TYPE"     , ImGuiTableColumnFlags_None, 2.2f);
            ImGui::TableSetupColumn("BUSY"     , ImGuiTableColumnFlags_None, 1.0f);
            ImGui::TableSetupColumn("SEND"     , ImGuiTableColumnFlags_None, 0.8f);
            ImGui::TableSetupColumn("RECV"     , ImGuiTableColumnFlags_None, 0.8f);
            ImGui::TableSetupColumn("RECV_AVG" , ImGuiTableColumnFlags_None, 1.0f);
            ImGui::TableHeadersRow();

            for(int amType = 0; const auto &procMonitor: m_podMonitor.amProcMonitorList){
                if(procMonitor.sendCount || procMonitor.recvCount){
                    ++messageTypeCount;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if(ImGui::Selectable(mpkName(amType), m_selectedAMType == amType,
                                ImGuiSelectableFlags_SpanAllColumns)){
                        m_selectedAMType = amType;
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(fnTimeString(procMonitor.procTick / 1000000ULL).c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", procMonitor.sendCount);
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", procMonitor.recvCount);
                    ImGui::TableNextColumn();
                    ImGui::Text("%llu", to_llu(procMonitor.recvCount ? (procMonitor.procTick / 1000ULL / procMonitor.recvCount) : 0));
                }
                ++amType;
            }
            ImGui::EndTable();
        }
    }
    else{
        ImGui::TextDisabled("waiting for snapshot...");
    }

    ImGui::TextDisabled("MSG_TYPE: %d", messageTypeCount);
    ImGui::End();
    if(!open){
        m_core->setPodMonitorOpen(false);
    }
}
