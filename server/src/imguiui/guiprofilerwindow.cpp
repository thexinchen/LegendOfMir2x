#include <GLFW/glfw3.h>
#include <imgui.h>

#include "logprof.hpp"
#include "guicore.hpp"
#include "guiprofilerwindow.hpp"

void GUIProfilerWindow::draw()
{
    const auto now = glfwGetTime();
    if(now - m_lastRefresh >= 1.370){
        m_lastRefresh = now;
        m_text.clear();
        logProfiling([this](const std::string &s)
        {
            m_text += s;
        });
    }

    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(640, 420), ImGuiCond_FirstUseEver);
    if(ImGui::Begin("Profiler", &open)){
        ImGui::TextUnformatted(m_text.c_str());
    }
    ImGui::End();

    if(!open){
        m_core->setProfilerOpen(false);
    }
}
