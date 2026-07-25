#include <fstream>
#include <sstream>

#include <imgui.h>

#include "log.hpp"
#include "guicore.hpp"
#include "guiscriptwindow.hpp"

extern Log *g_mir2xLog;

bool GUIScriptWindow::loadFile(const char *fileName)
{
    std::ifstream ifs(fileName, std::ios::binary);
    if(!ifs){
        g_mir2xLog->addLog(LOGTYPE_WARNING, "failed to read script file: %s", fileName ? fileName : "");
        return false;
    }

    std::stringstream ss;
    ss << ifs.rdbuf();
    m_text = ss.str();
    m_fileName = fileName;
    return true;
}

void GUIScriptWindow::runFile()
{
    if(m_fileName.empty()){
        return;
    }

    std::ifstream ifs(m_fileName, std::ios::binary);
    if(!ifs){
        g_mir2xLog->addLog(LOGTYPE_WARNING, "failed to read script file: %s", m_fileName.c_str());
        return;
    }

    std::stringstream ss;
    ss << ifs.rdbuf();
    m_core->execScriptInCommandWindow(ss.str());
}

void GUIScriptWindow::openFileDialog(bool fromMainMenu)
{
    m_loadRequestedFromMainMenu = fromMainMenu;
    m_fileDialog.open("Load Lua script", m_fileName.c_str(), GUIFileDialog::Mode::File, ".lua");
}

void GUIScriptWindow::drawFileDialog()
{
    std::string selectedPath;
    bool cancelled = false;
    if(m_fileDialog.draw(selectedPath, &cancelled)){
        if(loadFile(selectedPath.c_str()) && m_loadRequestedFromMainMenu){
            m_core->setScriptOpen(true);
        }
        m_loadRequestedFromMainMenu = false;
    }
    else if(cancelled){
        m_loadRequestedFromMainMenu = false;
    }
}

void GUIScriptWindow::draw()
{
    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(830, 505), ImGuiCond_Appearing);
    if(ImGui::Begin("Script", &open, ImGuiWindowFlags_MenuBar)){
        if(ImGui::BeginMenuBar()){
            if(ImGui::BeginMenu("File")){
                if(ImGui::MenuItem("Load...")){
                    openFileDialog(false);
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
                ImGui::MenuItem("Configure");
                ImGui::Separator();
                if(ImGui::MenuItem("Run", nullptr, false, !m_fileName.empty())){
                    runFile();
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

    }
    ImGui::End();

    if(!open){
        m_core->setScriptOpen(false);
    }
}
