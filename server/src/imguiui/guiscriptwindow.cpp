#include <fstream>
#include <sstream>

#include <imgui.h>
#include <tinyfiledialogs/tinyfiledialogs.h>

#include "log.hpp"
#include "guicore.hpp"
#include "guiscriptwindow.hpp"

extern Log *g_mir2xLog;

void GUIScriptWindow::draw()
{
    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(560, 420), ImGuiCond_FirstUseEver);
    if(ImGui::Begin("Script", &open)){
        if(ImGui::Button("Load file...")){
            if(const char *picked = tinyfd_openFileDialog("Load Lua script", "", 1, (const char * const[]){"*.lua"}, "Lua script (*.lua)", 0)){
                std::ifstream ifs(picked, std::ios::binary);
                if(ifs){
                    std::stringstream ss;
                    ss << ifs.rdbuf();
                    m_text = ss.str();
                    m_fileName = picked;
                }
                else{
                    g_mir2xLog->addLog(LOGTYPE_WARNING, "failed to read script file: %s", picked);
                }
            }
        }
        ImGui::SameLine();
        const bool canExec = !m_text.empty();
        if(!canExec){
            ImGui::BeginDisabled();
        }
        if(ImGui::Button("Execute")){
            m_core->execScriptInCommandWindow(m_text);
        }
        if(!canExec){
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_fileName.empty() ? "(no file loaded)" : m_fileName.c_str());
        ImGui::Separator();

        if(ImGui::BeginChild("ScriptText", ImVec2(0, 0), ImGuiChildFlags_Borders)){
            ImGui::TextUnformatted(m_text.c_str());
        }
        ImGui::EndChild();
    }
    ImGui::End();

    if(!open){
        m_core->setScriptOpen(false);
    }
}
