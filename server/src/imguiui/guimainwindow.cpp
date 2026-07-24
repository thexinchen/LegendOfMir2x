#include <imgui.h>

#include "log.hpp"
#include "server.hpp"
#include "guicore.hpp"
#include "guimainwindow.hpp"

extern Server *g_server;

static ImU32 fnLogTypeColor(int type)
{
    switch(type){
        case Log::LOGTYPEV_INFO   : return IM_COL32(210, 210, 210, 255);
        case Log::LOGTYPEV_WARNING: return IM_COL32(240, 200,  80, 255);
        case Log::LOGTYPEV_FATAL  : return IM_COL32(240,  80,  80, 255);
        default:
            // command window output (type offset by 100, see GUICore::appendCWLog)
            if(type >= 100){
                return IM_COL32(140, 200, 240, 255);
            }
            return IM_COL32(160, 160, 160, 255);
    }
}

void GUIMainWindow::drawMenuBar()
{
    if(!ImGui::BeginMainMenuBar()){
        return;
    }

    if(ImGui::BeginMenu("Server")){
        if(ImGui::MenuItem("Launch", nullptr, false, !m_core->hasLaunched())){
            // legacy flow: password dialog first, then recreate + launch
            m_core->requestPassword([core = m_core](const std::string &)
            {
                core->queueLaunch();
            });
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Quit")){
            m_core->quit();
        }
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Configure")){
        bool showConfigure = m_core->isConfigureOpen();
        if(ImGui::MenuItem("Server...", nullptr, &showConfigure)){
            m_core->setConfigureOpen(showConfigure);
        }
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Command")){
        if(ImGui::MenuItem("Run...")){
            m_core->createCommandWindow();
        }
        ImGui::Separator();
        for(int cwid = 1; cwid <= 16; ++cwid){
            bool open = m_core->isCommandWindowOpen(cwid);
            if(ImGui::MenuItem(str_printf("Window %d", cwid).c_str(), nullptr, &open)){
                m_core->setCommandWindowOpen(cwid, open);
            }
        }
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Script")){
        bool showScript = m_core->isScriptOpen();
        if(ImGui::MenuItem("Load...", nullptr, &showScript)){
            m_core->setScriptOpen(showScript);
        }
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Monitor")){
        bool showActorMonitor = m_core->isActorMonitorOpen();
        bool showPodMonitor   = m_core->isPodMonitorOpen();
        bool showProfiler     = m_core->isProfilerOpen();

        if(ImGui::MenuItem("Actor", nullptr, &showActorMonitor)){
            m_core->setActorMonitorOpen(showActorMonitor);
        }
        if(ImGui::MenuItem("Actor Pod", nullptr, &showPodMonitor)){
            m_core->setPodMonitorOpen(showPodMonitor);
        }
        if(ImGui::MenuItem("Profiler", nullptr, &showProfiler)){
            m_core->setProfilerOpen(showProfiler);
        }
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void GUIMainWindow::drawConsole()
{
    ImGui::SetNextWindowSize(ImVec2(720, 480), ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Server Log")){
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Auto-scroll", &m_autoScroll);
    ImGui::SameLine();
    // note: entries are GUI-thread only, no locking needed here
    ImGui::TextDisabled("%zu line(s)", m_core->getLogEntries().size());
    ImGui::Separator();

    ImGui::BeginChild("LogScroll", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    for(const auto &entry: m_core->getLogEntries()){
        ImGui::PushStyleColor(ImGuiCol_Text, fnLogTypeColor(entry.type));
        ImGui::TextUnformatted(entry.text.c_str());
        ImGui::PopStyleColor();
    }

    if(m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f){
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    ImGui::End();
}
