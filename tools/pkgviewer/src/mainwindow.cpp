#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

#include <imgui.h>

#include "imgf.hpp"
#include "strf.hpp"
#include "totype.hpp"
#include "mainwindow.hpp"
#include "previewwindow.hpp"

MainWindow::MainWindow()
    : ImGuiApp("pkgviewer", 555, 605, "pkgviewer.imgui.ini")
    , m_preview(std::make_unique<PreviewWindow>(this))
{}

MainWindow::~MainWindow() = default;

uint32_t MainWindow::selectedImageIndex() const
{
    if(m_selectedEntry >= 0 && m_selectedEntry < static_cast<int>(m_entries.size())){
        return m_entries.at(m_selectedEntry).index;
    }
    return m_package ? to_u32(m_package->imageCount()) : 0;
}

void MainWindow::draw()
{
    processJobs();
    drawMainWindow();
    m_preview->draw();
    drawDialogs();
    drawProgress();

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
}

void MainWindow::drawMainWindow()
{
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    const auto flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if(ImGui::Begin("pkgviewer", nullptr, flags)){
        drawMenu();
        const float statusHeight = ImGui::GetTextLineHeightWithSpacing();
        ImGui::BeginDisabled(m_busy);
        if(ImGui::BeginChild("ImageBrowser", ImVec2(0, -statusHeight), ImGuiChildFlags_Borders)){
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(m_entries.size()));
            while(clipper.Step()){
                for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i){
                    if(ImGui::Selectable(m_entries.at(i).text.c_str(), m_selectedEntry == i)){
                        selectEntry(i);
                    }
                }
            }
        }
        ImGui::EndChild();
        ImGui::EndDisabled();
        ImGui::TextUnformatted(m_status.c_str());
    }
    ImGui::End();
}

void MainWindow::drawMenu()
{
    if(!ImGui::BeginMenuBar()){
        return;
    }
    if(ImGui::BeginMenu("File")){
        if(ImGui::MenuItem("Open", "Ctrl+O", false, !m_busy)){
            m_preview->hide();
            m_openDialog.open("Select .WIL File", ".", ImGuiFileDialog::Mode::File, ".wil");
        }
        const bool canExport = m_package && m_selectedEntry >= 0 && !m_busy;
        if(ImGui::MenuItem("Export", nullptr, false, canExport)){
            m_exportDialog.open("Save", ".", ImGuiFileDialog::Mode::Directory);
        }
        if(ImGui::MenuItem("Export All", nullptr, false, m_package && !m_entries.empty() && !m_busy)){
            m_exportAllDialog.open("Save All Wil Images", ".", ImGuiFileDialog::Mode::Directory);
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Exit", "Ctrl+E")){
            close();
        }
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Config")){
        ImGui::MenuItem("Show Offset Cross", nullptr, &m_showOffsetCross);
        ImGui::Separator();
        for(int i = 0; i < 3; ++i){
            const auto label = "Show Layer " + std::to_string(i);
            if(ImGui::MenuItem(label.c_str(), nullptr, &m_showLayer[i])){
                m_preview->loadImage();
            }
        }
        ImGui::Separator();
        ImGui::MenuItem("Save Index", nullptr, &m_saveIndex);
        ImGui::MenuItem("Save Layers", nullptr, &m_saveLayers);
        ImGui::MenuItem("Save Offset", nullptr, &m_saveOffset);
        ImGui::Separator();
        if(ImGui::MenuItem("Auto Alpha", nullptr, &m_autoAlpha)){ m_preview->loadImage(); }
        if(ImGui::MenuItem("Remove Shadow Mosaic", nullptr, &m_removeShadowMosaic)){ m_preview->loadImage(); }
        ImGui::Separator();
        ImGui::MenuItem("Offset Draw", nullptr, &m_offsetDraw);
        ImGui::Separator();
        ImGui::MenuItem("Clear Background", nullptr, &m_clearBackground);
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("About")){
        if(ImGui::MenuItem("About Me")){
            m_showAbout = true;
        }
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}

void MainWindow::drawDialogs()
{
    std::string selected;
    if(m_openDialog.draw(selected)){
        try{
            openPackage(selected);
        }
        catch(const std::exception &e){
            m_busy = false;
            alert(e.what(), "Open failed");
        }
    }
    if(m_exportDialog.draw(selected)){
        try{
            saveImage(selectedImageIndex(), selected);
        }
        catch(const std::exception &e){
            alert(e.what(), "Export failed");
        }
    }
    if(m_exportAllDialog.draw(selected)){
        m_exportPath = selected;
        m_exportAll = true;
        m_exportIndex = 0;
        m_progress = 0;
        m_busy = true;
    }
}

void MainWindow::openPackage(const std::string &fileName)
{
    const auto path = std::filesystem::path(fileName);
    m_fileFullName = path.generic_string();
    m_package = std::make_unique<WilImagePackage>(path.parent_path().string().c_str(), path.stem().string().c_str());
    m_entries.clear();
    m_selectedEntry = -1;
    m_scanIndex = 0;
    m_progress = 0;
    m_busy = true;
    m_status = "Loading " + path.filename().string();
}

void MainWindow::processJobs()
{
    if(m_busy && m_package && m_scanIndex < m_package->indexCount()){
        const size_t end = std::min(m_scanIndex + 100, m_package->indexCount());
        int maxLen = 1;
        for(auto count = m_package->indexCount(); count >= 10; count /= 10){ ++maxLen; }
        const auto format = "Index: %0" + std::to_string(maxLen) + "d       W:%4d       H:%4d       PX:%4d      PY:%4d";
        for(; m_scanIndex < end; ++m_scanIndex){
            if(m_package->setIndex(to_d(m_scanIndex))){
                char text[128];
                std::snprintf(text, sizeof(text), format.c_str(), to_d(m_scanIndex),
                    m_package->currImageInfo()->width, m_package->currImageInfo()->height,
                    m_package->currImageInfo()->px, m_package->currImageInfo()->py);
                m_entries.push_back({to_u32(m_scanIndex), text});
            }
        }
        m_progress = m_package->indexCount() ? to_d(std::lround(100.0 * m_scanIndex / m_package->indexCount())) : 100;
        if(m_scanIndex >= m_package->indexCount()){
            const auto path = std::filesystem::path(m_fileFullName);
            m_status = "FileName: " + path.filename().string() +
                "    ImageCount: " + std::to_string(m_entries.size()) +
                "    Version: " + std::to_string(m_package->version());
            m_busy = false;
        }
    }

    if(m_busy && m_exportAll){
        const size_t end = std::min(m_exportIndex + 10, m_entries.size());
        try{
            for(; m_exportIndex < end; ++m_exportIndex){
                saveImage(m_entries.at(m_exportIndex).index, m_exportPath);
            }
        }
        catch(const std::exception &e){
            m_exportAll = false;
            m_busy = false;
            alert(e.what(), "Export failed");
            return;
        }
        m_progress = m_entries.empty() ? 100 : to_d(std::lround(100.0 * m_exportIndex / m_entries.size()));
        if(m_exportIndex >= m_entries.size()){
            m_exportAll = false;
            m_busy = false;
        }
    }
}

void MainWindow::drawProgress()
{
    if(!m_busy){
        return;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(250, 0));
    if(ImGui::Begin("Progress", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)){
        const auto overlay = "Progress: " + std::to_string(m_progress) + "%";
        ImGui::ProgressBar(m_progress / 100.0f, ImVec2(-1, 0), overlay.c_str());
    }
    ImGui::End();
}

void MainWindow::selectEntry(int entry)
{
    m_selectedEntry = entry;
    if(m_package && m_package->setIndex(selectedImageIndex())){
        m_preview->loadImage();
        m_preview->show();
    }
}

std::string MainWindow::imageIndexString(int index) const
{
    return m_saveIndex ? str_printf("%06d_%08X", index, index) : str_printf("%08X", index);
}

void MainWindow::saveImage(uint32_t imageIndex, const std::string &filePath)
{
    if(!m_package || !m_package->setIndex(imageIndex)){
        return;
    }
    const auto width = m_package->currImageInfo()->width;
    const auto height = m_package->currImageInfo()->height;
    const auto dx = m_package->currImageInfo()->px;
    const auto dy = m_package->currImageInfo()->py;
    char indexText[128];
    if(m_saveOffset){
        std::snprintf(indexText, sizeof(indexText), "TMP%s_%s%s%04X%04X", imageIndexString(imageIndex).c_str(),
            dx > 0 ? "1" : "0", dy > 0 ? "1" : "0", static_cast<unsigned>(std::labs(dx)), static_cast<unsigned>(std::labs(dy)));
    }
    else{
        std::snprintf(indexText, sizeof(indexText), "TMP%s", imageIndexString(imageIndex).c_str());
    }
    const auto base = std::filesystem::path(filePath) / indexText;
    if(const auto [layer0, layer1, layer2] = m_package->decode(true, m_removeShadowMosaic, m_autoAlpha); layer0){
        imgf::saveImageBuffer(reinterpret_cast<const uint8_t *>(layer0), width, height, (base.string() + "_M.PNG").c_str());
    }
    if(m_saveLayers){
        const auto [layer0, layer1, layer2] = m_package->decode(false, m_removeShadowMosaic, m_autoAlpha);
        if(layer0){ imgf::saveImageBuffer(reinterpret_cast<const uint8_t *>(layer0), width, height, (base.string() + "_0.PNG").c_str()); }
        if(layer1){ imgf::saveImageBuffer(reinterpret_cast<const uint8_t *>(layer1), width, height, (base.string() + "_1.PNG").c_str()); }
        if(layer2){ imgf::saveImageBuffer(reinterpret_cast<const uint8_t *>(layer2), width, height, (base.string() + "_2.PNG").c_str()); }
    }
}

