#include <fstream>
#include <sstream>

#include <imgui.h>

#include "log.hpp"
#include "guicore.hpp"
#include "guiscriptwindow.hpp"

extern Log *g_mir2xLog;

void GUIScriptWindow::draw()
{
    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(830, 505), ImGuiCond_Appearing);
    if(ImGui::Begin("Script", &open, ImGuiWindowFlags_MenuBar)){
        const auto loadFile = [this](const char *fileName)
        {
            std::ifstream ifs(fileName, std::ios::binary);
            if(ifs){
                std::stringstream ss;
                ss << ifs.rdbuf();
                m_text = ss.str();
                m_fileName = fileName;
            }
            else{
                g_mir2xLog->addLog(LOGTYPE_WARNING, "failed to read script file: %s", fileName);
            }
        };

        if(ImGui::BeginMenuBar()){
            if(ImGui::BeginMenu("File")){
                if(ImGui::MenuItem("Load...")){
                    m_fileDialog.open("Load Lua script", m_fileName.c_str(), GUIFileDialog::Mode::File, ".lua");
                }
                if(ImGui::MenuItem("Update", nullptr, false, !m_fileName.empty())){
                    loadFile(m_fileName.c_str());
                }
                if(ImGui::MenuItem("Exit")){
                    open = false;
                }
                ImGui::EndMenu();
            }

            if(ImGui::BeginMenu("Execute")){
                if(ImGui::MenuItem("Run", nullptr, false, !m_text.empty())){
                    m_core->execScriptInCommandWindow(m_text);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if(open){
            if(ImGui::BeginChild("ScriptText", ImVec2(0, 0), ImGuiChildFlags_None)){
                ImGui::TextUnformatted(m_text.c_str());
            }
            ImGui::EndChild();
        }

        std::string selectedPath;
        if(m_fileDialog.draw(selectedPath)){
            loadFile(selectedPath.c_str());
        }
    }
    ImGui::End();

    if(!open){
        m_core->setScriptOpen(false);
    }
}
