#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <imgui.h>

#include "filesys.hpp"
#include "imgf.hpp"
#include "strf.hpp"
#include "totype.hpp"
#include "mainwindow.hpp"
#include "previewwindow.hpp"

MainWindow::MainWindow()
    : ImGuiApp("pkgviewer", 1200, 800, "pkgviewer.imgui.ini")
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
            const auto layoutFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV;
            if(m_package && ImGui::BeginTable("BrowserLayout", 2, layoutFlags)){
                ImGui::TableSetupColumn("Images", ImGuiTableColumnFlags_WidthStretch, 0.40f);
                ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch, 0.60f);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                if(ImGui::BeginChild("ImageTablePane")){
                    const auto tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
                    if(ImGui::BeginTable("Images", 5, tableFlags)){
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableSetupColumn("Index");
                        ImGui::TableSetupColumn("W");
                        ImGui::TableSetupColumn("H");
                        ImGui::TableSetupColumn("PX");
                        ImGui::TableSetupColumn("PY");
                        ImGui::TableHeadersRow();

                        bool scrollToSelected = false;
                        if(!m_entries.empty() && ImGui::IsWindowFocused()){
                            int nextEntry = m_selectedEntry;
                            if(ImGui::IsKeyPressed(ImGuiKey_UpArrow)){
                                nextEntry = m_selectedEntry > 0 ? m_selectedEntry - 1 : 0;
                            }
                            else if(ImGui::IsKeyPressed(ImGuiKey_DownArrow)){
                                nextEntry = std::min(m_selectedEntry + 1, static_cast<int>(m_entries.size()) - 1);
                            }
                            if(nextEntry != m_selectedEntry){
                                selectEntry(nextEntry);
                                scrollToSelected = true;
                            }
                        }

                        ImGuiListClipper clipper;
                        clipper.Begin(static_cast<int>(m_entries.size()));
                        if(scrollToSelected){
                            clipper.IncludeItemByIndex(m_selectedEntry);
                        }
                        while(clipper.Step()){
                            for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i){
                                const auto &entry = m_entries.at(i);
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::PushID(i);
                                if(ImGui::Selectable(std::to_string(entry.index).c_str(), m_selectedEntry == i,
                                    ImGuiSelectableFlags_SpanAllColumns)){
                                    selectEntry(i);
                                }
                                if(scrollToSelected && m_selectedEntry == i){
                                    ImGui::SetScrollHereY();
                                }
                                ImGui::PopID();
                                ImGui::TableSetColumnIndex(1); ImGui::Text("%d", entry.width);
                                ImGui::TableSetColumnIndex(2); ImGui::Text("%d", entry.height);
                                ImGui::TableSetColumnIndex(3); ImGui::Text("%d", entry.px);
                                ImGui::TableSetColumnIndex(4); ImGui::Text("%d", entry.py);
                            }
                        }
                        ImGui::EndTable();
                    }
                }
                ImGui::EndChild();

                ImGui::TableSetColumnIndex(1);
                if(ImGui::BeginChild("PreviewPane")){
                    m_preview->draw();
                }
                ImGui::EndChild();
                ImGui::EndTable();
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
            m_preview->clear();
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
    const auto [parentPath, stem, extension] = filesys::decompFileName(fileName.c_str(), true);
    m_fileFullName = fileName;
    m_package = std::make_unique<WilImagePackage>(parentPath.c_str(), stem.c_str());
    m_entries.clear();
    m_selectedEntry = -1;
    m_scanIndex = 0;
    m_progress = 0;
    m_busy = true;
    m_status = "Loading " + stem + "." + extension;
}

void MainWindow::processJobs()
{
    if(m_busy && m_package && m_scanIndex < m_package->indexCount()){
        const size_t end = std::min(m_scanIndex + 100, m_package->indexCount());
        for(; m_scanIndex < end; ++m_scanIndex){
            if(m_package->setIndex(to_d(m_scanIndex))){
                const auto *imageInfo = m_package->currImageInfo();
                m_entries.push_back({
                    to_u32(m_scanIndex),
                    imageInfo->width,
                    imageInfo->height,
                    imageInfo->px,
                    imageInfo->py,
                });
            }
        }
        m_progress = m_package->indexCount() ? to_d(std::lround(100.0 * m_scanIndex / m_package->indexCount())) : 100;
        if(m_scanIndex >= m_package->indexCount()){
            const auto fileParts = filesys::decompFileName(m_fileFullName.c_str(), true);
            const auto &stem = std::get<1>(fileParts);
            const auto &extension = std::get<2>(fileParts);
            m_status = "FileName: " + stem + "." + extension +
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
    const auto base = filePath + "/" + indexText;
    if(const auto [layer0, layer1, layer2] = m_package->decode(true, m_removeShadowMosaic, m_autoAlpha); layer0){
        imgf::saveImageBuffer(reinterpret_cast<const uint8_t *>(layer0), width, height, (base + "_M.PNG").c_str());
    }
    if(m_saveLayers){
        const auto [layer0, layer1, layer2] = m_package->decode(false, m_removeShadowMosaic, m_autoAlpha);
        if(layer0){ imgf::saveImageBuffer(reinterpret_cast<const uint8_t *>(layer0), width, height, (base + "_0.PNG").c_str()); }
        if(layer1){ imgf::saveImageBuffer(reinterpret_cast<const uint8_t *>(layer1), width, height, (base + "_1.PNG").c_str()); }
        if(layer2){ imgf::saveImageBuffer(reinterpret_cast<const uint8_t *>(layer2), width, height, (base + "_2.PNG").c_str()); }
    }
}
