#include <imgui.h>

#include "mainwindow.hpp"

MainWindow::MainWindow(uint32_t magicID, const char *dbPathName)
    : ImGuiApp("Follow UID Magic Editor", 985, 645, "followuidmagiceditor.imgui.ini")
{
    m_drawArea.load(magicID, dbPathName);
}

bool MainWindow::onCloseRequested()
{
    m_confirmQuit = true;
    return false;
}

void MainWindow::update(double delta)
{
    m_frameTime += delta;
    while(m_frameTime >= 0.1){
        m_drawArea.updateFrame();
        m_frameTime -= 0.1;
    }
}

void MainWindow::draw()
{
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    const auto flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if(ImGui::Begin("Follow UID Magic Editor", nullptr, flags)){
        if(ImGui::BeginMenuBar()){
            if(ImGui::BeginMenu("Project")){
                if(ImGui::MenuItem("Reset")){ m_drawArea.reset(); }
                if(ImGui::MenuItem("Output")){
                    const auto output = m_drawArea.output();
                    ImGui::SetClipboardText(output.c_str());
                    alert("Generated offsets copied to the clipboard.", "Output");
                }
                ImGui::Separator();
                if(ImGui::MenuItem("Quit", "Ctrl+Q")){ close(); }
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("About")){
                if(ImGui::MenuItem("About Me")){ m_showAbout = true; }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        m_drawArea.draw(ImGui::GetCursorScreenPos(), ImGui::GetContentRegionAvail());
    }
    ImGui::End();

    if(m_showAbout){
        ImGui::OpenPopup("About Me");
        m_showAbout = false;
    }
    if(ImGui::BeginPopupModal("About Me", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextUnformatted("Name : Anhong\nEmail: anhonghe@gmail.com");
        if(ImGui::Button("OK", ImVec2(100, 0))){
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if(m_confirmQuit){
        ImGui::OpenPopup("Quit editor?");
        m_confirmQuit = false;
    }
    if(ImGui::BeginPopupModal("Quit editor?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        if(ImGui::Button("Cancel", ImVec2(100, 0))){
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Yes", ImVec2(100, 0))){
            ImGui::CloseCurrentPopup();
            close();
        }
        ImGui::EndPopup();
    }
}
