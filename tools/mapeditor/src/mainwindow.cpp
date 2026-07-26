#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include <imgui.h>

#include "filesys.hpp"
#include "colorf.hpp"
#include "imgf.hpp"
#include "raiitimer.hpp"
#include "strf.hpp"
#include "sysconst.hpp"
#include "totype.hpp"
#include "mainwindow.hpp"

MainWindow *g_mainWindow = nullptr;
ImageMapDB *g_imageMapDB = nullptr;
EditorMap g_editorMap;
LayerBrowserWindow *g_layerBrowserWindow = nullptr;

namespace
{
    int depthIndex(int depth)
    {
        switch(depth){
            case OBJD_GROUND:      return 0;
            case OBJD_OVERGROUND0: return 1;
            case OBJD_OVERGROUND1: return 2;
            case OBJD_SKY:         return 3;
            default:               return -1;
        }
    }

    ImU32 depthColor(int depth)
    {
        switch(depth){
            case OBJD_GROUND:      return IM_COL32(255, 190, 0, 255);
            case OBJD_OVERGROUND0: return IM_COL32(0, 220, 255, 255);
            case OBJD_OVERGROUND1: return IM_COL32(100, 255, 100, 255);
            case OBJD_SKY:         return IM_COL32(220, 120, 255, 255);
            default:               return IM_COL32_WHITE;
        }
    }

    template<typename Map>
    std::tuple<int, int, int, int> visibleRange(const Map &map, float offsetX, float offsetY, const ImVec2 &size)
    {
        return {
            std::max(0, static_cast<int>(offsetX) / SYS_MAPGRIDXP - 2),
            std::max(0, static_cast<int>(offsetY) / SYS_MAPGRIDYP - 2),
            std::min(to_d(map.w()), static_cast<int>(offsetX + size.x) / SYS_MAPGRIDXP + 3),
            std::min(to_d(map.h()), static_cast<int>(offsetY + size.y) / SYS_MAPGRIDYP + 8),
        };
    }

    std::vector<uint32_t> scaleImage(const uint32_t *src, int srcW, int srcH, int dstW, int dstH)
    {
        std::vector<uint32_t> dst(to_uz(dstW) * dstH);
        for(int y = 0; y < dstH; ++y){
            const double srcY0 = to_df(y) * srcH / dstH;
            const double srcY1 = to_df(y + 1) * srcH / dstH;
            for(int x = 0; x < dstW; ++x){
                const double srcX0 = to_df(x) * srcW / dstW;
                const double srcX1 = to_df(x + 1) * srcW / dstW;
                double sumR = 0.0;
                double sumG = 0.0;
                double sumB = 0.0;
                double sumA = 0.0;
                double sumWeight = 0.0;
                for(int srcY = to_d(std::floor(srcY0)); srcY < to_d(std::ceil(srcY1)); ++srcY){
                    const double weightY = std::min(srcY1, to_df(srcY + 1)) - std::max(srcY0, to_df(srcY));
                    for(int srcX = to_d(std::floor(srcX0)); srcX < to_d(std::ceil(srcX1)); ++srcX){
                        const double weightX = std::min(srcX1, to_df(srcX + 1)) - std::max(srcX0, to_df(srcX));
                        const double weight = weightX * weightY;
                        const uint32_t pixel = src[to_uz(srcY) * srcW + srcX];
                        const double alpha = colorf::A(pixel) / 255.0;
                        sumR += colorf::R(pixel) * alpha * weight;
                        sumG += colorf::G(pixel) * alpha * weight;
                        sumB += colorf::B(pixel) * alpha * weight;
                        sumA += alpha * weight;
                        sumWeight += weight;
                    }
                }
                const auto alpha = colorf::round255(sumA * 255.0 / sumWeight);
                dst[to_uz(y) * dstW + x] = sumA > 0.0
                    ? colorf::RGBA(
                        colorf::round255(sumR / sumA),
                        colorf::round255(sumG / sumA),
                        colorf::round255(sumB / sumA),
                        alpha)
                    : 0;
            }
        }
        return dst;
    }

    std::string parentPath(const std::string &path)
    {
        const std::u8string utf8Path(
            reinterpret_cast<const char8_t *>(path.data()),
            reinterpret_cast<const char8_t *>(path.data() + path.size()));
        const auto parent = std::filesystem::path(utf8Path).parent_path().generic_u8string();
        return {
            reinterpret_cast<const char *>(parent.data()),
            parent.size(),
        };
    }
}

void AttributeSelector::draw(const char *title)
{
    if(!open){
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(360, 250), ImGuiCond_Appearing);
    if(ImGui::Begin(title, &open)){
        ImGui::Checkbox("CanWalk", &walkable);
        ImGui::SameLine();
        ImGui::Checkbox("CanFly", &flyable);
        ImGui::Separator();
        ImGui::BeginDisabled(walkable || flyable);
        if(ImGui::BeginTable("LandTypes", 3)){
            ImGui::TableNextColumn(); ImGui::Checkbox("Grass", &grass);
            ImGui::TableNextColumn(); ImGui::Checkbox("Stone", &stone);
            ImGui::TableNextColumn(); ImGui::Checkbox("Pond", &pond);
            ImGui::TableNextColumn(); ImGui::Checkbox("Sand", &sand);
            ImGui::TableNextColumn(); ImGui::Checkbox("Ocean", &ocean);
            for(int i = 0; i < 10; ++i){
                ImGui::TableNextColumn();
                bool unused = false;
                ImGui::BeginDisabled();
                ImGui::Checkbox(("Unused##" + std::to_string(i)).c_str(), &unused);
                ImGui::EndDisabled();
            }
            ImGui::EndTable();
        }
        ImGui::EndDisabled();
        ImGui::Separator();
        if(ImGui::Button("OK", ImVec2(100, 0))){
            open = false;
        }
    }
    ImGui::End();
}

bool AttributeSelector::testLand(const Mir2xMapData::LAND &land) const
{
    if(walkable || flyable){
        return land.canWalk || land.canFly;
    }
    switch(land.type){
        case LANDTYPE_GRASS: return grass;
        case LANDTYPE_STONE: return stone;
        case LANDTYPE_POND:  return pond;
        case LANDTYPE_SAND:  return sand;
        case LANDTYPE_OCEAN: return ocean;
        default:             return false;
    }
}

MainWindow::MainWindow()
    : ImGuiApp("mapeditor-version-0.0.1", 1200, 800, "mapeditor.imgui.ini")
{
    g_mainWindow = this;
    g_layerBrowserWindow = &m_layerBrowser;
    m_animationDB = std::make_unique<AnimationDB>();
}

MainWindow::~MainWindow()
{
    g_imageMapDB = nullptr;
    g_layerBrowserWindow = nullptr;
    g_mainWindow = nullptr;
}

bool MainWindow::onCloseRequested()
{
    m_confirmQuit = true;
    return false;
}

void MainWindow::update(double delta)
{
    m_aniTimer.update(to_u32(std::lround(delta * 1000.0)));
    updateOverviewBuild();
}

void MainWindow::draw()
{
    drawEditor();
    drawOverview();
    drawDialogs();
    m_attributeSelect.draw("Attribute Select");
    m_attributeGrid.draw("Attribute Grid");
    m_layerBrowser.drawBrowser(this);
    m_layerBrowser.drawView(this);
    drawModals();
}

void MainWindow::drawEditor()
{
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    const auto flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if(ImGui::Begin("mapeditor-version-0.0.1", nullptr, flags)){
        drawMenu();
        const auto available = ImGui::GetContentRegionAvail();
        const bool mapLoaded = g_editorMap.valid();
        const float scrollbar = mapLoaded ? ImGui::GetFrameHeight() : 0.0f;
        const float statusHeight = ImGui::GetFrameHeightWithSpacing();
        const ImVec2 canvasSize(std::max(1.0f, available.x - scrollbar), std::max(1.0f, available.y - scrollbar - statusHeight));
        m_editorCanvasSize = canvasSize;
        const auto canvasPos = ImGui::GetCursorScreenPos();
        renderEditorCanvas(canvasPos, canvasSize);

        if(mapLoaded){
            ImGui::SetCursorScreenPos(ImVec2(canvasPos.x + canvasSize.x, canvasPos.y));
            ImGui::VSliderFloat("##MapVScroll", ImVec2(scrollbar, canvasSize.y), &m_scrollY, 0.0f, 1.0f, "");
            ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + canvasSize.y));
            ImGui::SetNextItemWidth(canvasSize.x);
            ImGui::SliderFloat("##MapHScroll", &m_scrollX, 0.0f, 1.0f, "");
        }
        const float statusY = canvasPos.y + canvasSize.y + scrollbar;
        ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, statusY + (statusHeight - ImGui::GetTextLineHeight()) / 2));
        ImGui::TextUnformatted(m_status.c_str());
    }
    ImGui::End();
}

void MainWindow::drawMenu()
{
    if(!ImGui::BeginMenuBar()){
        return;
    }
    if(ImGui::BeginMenu("Project")){
        if(ImGui::MenuItem("New")){
            m_workingDialog.open("Set Working Folder...", ".", ImGuiFileDialog::Mode::Directory);
            alert("Haven't implement yet!", "New");
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Load Layer")){ requestLoad(PendingLoad::Layer); }
        if(ImGui::MenuItem("Load Mir2Map")){ requestLoad(PendingLoad::Mir2Map); }
        if(ImGui::MenuItem("Load Mir2xMapData", "Ctrl+O")){ requestLoad(PendingLoad::Mir2xMapData); }
        ImGui::Separator();
        if(ImGui::MenuItem("Save Mir2xMapData")){ saveMir2xMapData(); }
        ImGui::Separator();
        for(const int ratio: {1, 2, 4, 8, 16, 32}){
            const auto label = "Export Overview 1:" + std::to_string(ratio);
            if(ImGui::MenuItem(label.c_str())){ extractOverview(ratio); }
        }
        ImGui::Separator();
        ImGui::MenuItem("Save As", "Ctrl+Shift+S", false, false);
        ImGui::Separator();
        if(ImGui::MenuItem("Quit", "Ctrl+Q")){ close(); }
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Grid")){
        ImGui::MenuItem("Step", nullptr, &m_gridLine);
        if(ImGui::MenuItem("Attribute", nullptr, &m_attributeLine) && m_attributeLine){ m_attributeGrid.open = true; }
        ImGui::Separator();
        ImGui::MenuItem("Light", nullptr, &m_lightLine);
        ImGui::Separator();
        ImGui::MenuItem("Tile", nullptr, &m_tileLine);
        ImGui::Separator();
        const char *depthNames[] = {"Object Ground", "Object Over Ground 0", "Object Over Ground 1", "Object Sky"};
        for(int i = 0; i < 4; ++i){ ImGui::MenuItem(depthNames[i], nullptr, &m_objectLine[i]); }
        ImGui::Separator();
        ImGui::MenuItem("Object Index 0", nullptr, &m_objectIndexLine[0]);
        ImGui::MenuItem("Object Index 1", nullptr, &m_objectIndexLine[1]);
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Show")){
        ImGui::MenuItem("Overview", nullptr, &m_overviewOpen);
        ImGui::Separator();
        ImGui::MenuItem("Light", nullptr, &m_showLight);
        ImGui::Separator();
        ImGui::MenuItem("Tile", nullptr, &m_showTile);
        ImGui::Separator();
        const char *depthNames[] = {"Object Ground", "Object Over Ground 0", "Object Over Ground 1", "Object Sky"};
        for(int i = 0; i < 4; ++i){ ImGui::MenuItem(depthNames[i], nullptr, &m_showObject[i]); }
        ImGui::Separator();
        ImGui::MenuItem("Object Index 0", nullptr, &m_showObjectIndex[0]);
        ImGui::MenuItem("Object Index 1", nullptr, &m_showObjectIndex[1]);
        ImGui::Separator();
        if(ImGui::MenuItem("Remove Shadow Mosaic", nullptr, &m_removeShadowMosaic)){ clearImageCache(); }
        ImGui::Separator();
        ImGui::MenuItem("Clear Background", nullptr, &m_clearBackground);
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Edit")){
        ImGui::MenuItem("Enable Edit", nullptr, &m_enableEdit);
        ImGui::MenuItem("Edit Ground Info", nullptr, &m_editGround);
        ImGui::Separator();
        if(ImGui::MenuItem("Optimize", nullptr, false, g_editorMap.valid())){ g_editorMap.optimize(); }
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Select")){
        ImGui::MenuItem("Enable Select", nullptr, &m_enableSelect);
        ImGui::Separator();
        ImGui::BeginDisabled(!m_enableSelect);
        const char *modeNames[] = {"Attribute", "Tile", "Object Ground", "Object Over Ground 0",
            "Object Over Ground 1", "Object Sky", "Object Index 0", "Object Index 1"};
        for(int i = 0; i < 8; ++i){
            if(ImGui::MenuItem(modeNames[i], nullptr, m_selectMode == i)){
                m_selectMode = i;
                if(i == 0){ m_attributeSelect.open = true; }
            }
        }
        ImGui::Separator();
        ImGui::MenuItem("Reversed", nullptr, &m_reversed);
        ImGui::MenuItem("Deselect", nullptr, &m_deselect);
        ImGui::Separator();
        if(ImGui::MenuItem("Clear All Select")){ g_editorMap.clearSelect(); }
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Layer")){
        ImGui::MenuItem("Import Layer", nullptr, false, false);
        if(ImGui::MenuItem("Export Layer", nullptr, false, g_editorMap.valid())){
            m_layerBrowser.addEntry(g_editorMap.exportLayer());
            m_layerBrowser.browserOpen = true;
        }
        ImGui::Separator();
        const char *modeNames[] = {"Reject", "Replace", "Substitute"};
        for(int i = 0; i < 3; ++i){
            if(ImGui::MenuItem(modeNames[i], nullptr, m_layerMode == i)){ m_layerMode = i; }
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Layer Browser")){ m_layerBrowser.browserOpen = true; }
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Test")){
        ImGui::MenuItem("Enable Test", nullptr, &m_enableTest);
        ImGui::Separator();
        for(int i = 0; i < 3; ++i){
            const auto label = "Animation " + std::to_string(i);
            if(ImGui::MenuItem(label.c_str(), nullptr, m_animationIndex == i)){ m_animationIndex = i; }
        }
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("About")){
        if(ImGui::MenuItem("About Me")){ m_showAbout = true; }
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}

void MainWindow::drawOverview()
{
    if(!m_overviewOpen){
        return;
    }
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(380.0f, 300.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 10.0f, viewport->WorkPos.y + 10.0f),
        ImGuiCond_FirstUseEver,
        ImVec2(1.0f, 0.0f));
    if(ImGui::Begin("Overview###MapOverviewV2", &m_overviewOpen)){
        const auto windowPos = ImGui::GetWindowPos();
        const auto windowSize = ImGui::GetWindowSize();
        const ImVec2 clampedPos(
            std::clamp(
                windowPos.x,
                viewport->WorkPos.x,
                std::max(viewport->WorkPos.x, viewport->WorkPos.x + viewport->WorkSize.x - windowSize.x)),
            std::clamp(
                windowPos.y,
                viewport->WorkPos.y,
                std::max(viewport->WorkPos.y, viewport->WorkPos.y + viewport->WorkSize.y - windowSize.y)));
        if(clampedPos.x != windowPos.x || clampedPos.y != windowPos.y){
            ImGui::SetWindowPos(clampedPos);
        }
        if(m_overviewBuild.active){
            const float progress = m_overviewBuild.totalCells
                ? to_f(m_overviewBuild.nextCell) / m_overviewBuild.totalCells
                : 0.0f;
            ImGui::TextUnformatted("Generating overview...");
            ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
            ImGui::End();
            return;
        }
        if(!g_editorMap.valid() || !m_overviewTexture.valid()){
            ImGui::TextUnformatted("No map overview available");
            ImGui::End();
            return;
        }

        const auto available = ImGui::GetContentRegionAvail();
        const float fitScale = std::min(available.x / m_overviewTexture.width(), available.y / m_overviewTexture.height());
        const ImVec2 imageSize(
            std::max(1.0f, m_overviewTexture.width() * fitScale),
            std::max(1.0f, m_overviewTexture.height() * fitScale));
        const auto cursor = ImGui::GetCursorScreenPos();
        const ImVec2 imagePos(
            cursor.x + std::max(0.0f, (available.x - imageSize.x) * 0.5f),
            cursor.y + std::max(0.0f, (available.y - imageSize.y) * 0.5f));

        ImGui::SetCursorScreenPos(imagePos);
        ImGui::InvisibleButton("OverviewCanvas", imageSize, ImGuiButtonFlags_MouseButtonLeft);

        const float mapWidth = to_f(g_editorMap.w()) * SYS_MAPGRIDXP;
        const float mapHeight = to_f(g_editorMap.h()) * SYS_MAPGRIDYP;
        const float viewRatioX = std::min(1.0f, m_editorCanvasSize.x / mapWidth);
        const float viewRatioY = std::min(1.0f, m_editorCanvasSize.y / mapHeight);
        const ImVec2 rectSize(
            std::min(imageSize.x, std::max(2.0f, imageSize.x * viewRatioX)),
            std::min(imageSize.y, std::max(2.0f, imageSize.y * viewRatioY)));
        const float travelX = std::max(0.0f, imageSize.x - rectSize.x);
        const float travelY = std::max(0.0f, imageSize.y - rectSize.y);
        ImVec2 rectMin(imagePos.x + m_scrollX * travelX, imagePos.y + m_scrollY * travelY);

        const auto mouse = ImGui::GetIO().MousePos;
        if(ImGui::IsItemActivated()){
            const bool inside = mouse.x >= rectMin.x && mouse.x <= rectMin.x + rectSize.x
                && mouse.y >= rectMin.y && mouse.y <= rectMin.y + rectSize.y;
            m_overviewDragOffset = inside
                ? ImVec2(mouse.x - rectMin.x, mouse.y - rectMin.y)
                : ImVec2(rectSize.x * 0.5f, rectSize.y * 0.5f);
        }
        if(ImGui::IsItemActive()){
            m_scrollX = travelX > 0.0f
                ? std::clamp((mouse.x - m_overviewDragOffset.x - imagePos.x) / travelX, 0.0f, 1.0f)
                : 0.0f;
            m_scrollY = travelY > 0.0f
                ? std::clamp((mouse.y - m_overviewDragOffset.y - imagePos.y) / travelY, 0.0f, 1.0f)
                : 0.0f;
            rectMin = ImVec2(imagePos.x + m_scrollX * travelX, imagePos.y + m_scrollY * travelY);
        }
        if(ImGui::IsItemHovered() || ImGui::IsItemActive()){
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        auto *drawList = ImGui::GetWindowDrawList();
        drawList->AddImage(m_overviewTexture.id(), imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y));
        const ImVec2 rectMax(rectMin.x + rectSize.x, rectMin.y + rectSize.y);
        drawList->AddRectFilled(rectMin, rectMax, IM_COL32(255, 80, 40, 35));
        drawList->AddRect(rectMin, rectMax, IM_COL32(255, 80, 40, 255), 0.0f, 0, 2.0f);
    }
    ImGui::End();
}

MainWindow::CachedImage *MainWindow::retrieveImage(uint32_t texID)
{
    auto &image = m_imageCache.try_emplace(texID).first->second;
    if(image.attempted){
        return image.texture.valid() ? &image : nullptr;
    }
    image.attempted = true;
    if(!m_imageMapDB){
        return nullptr;
    }
    try{
        if(const auto [buffer, width, height] = m_imageMapDB->decode(texID, m_removeShadowMosaic); buffer){
            image.texture.loadRGBA(buffer, to_d(width), to_d(height));
        }
    }
    catch(...){
        return nullptr;
    }
    return image.texture.valid() ? &image : nullptr;
}

void MainWindow::renderEditorCanvas(const ImVec2 &canvasPos, const ImVec2 &canvasSize)
{
    auto *drawList = ImGui::GetWindowDrawList();
    const ImVec2 canvasMax(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);
    if(m_clearBackground){
        drawList->AddRectFilled(canvasPos, canvasMax, IM_COL32_BLACK);
    }
    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("EditorCanvas", canvasSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    if(!g_editorMap.valid()){
        return;
    }
    const float maxOffsetX = std::max(0.0f, to_f(g_editorMap.w() * SYS_MAPGRIDXP) - canvasSize.x);
    const float maxOffsetY = std::max(0.0f, to_f(g_editorMap.h() * SYS_MAPGRIDYP) - canvasSize.y);
    const float offsetX = m_scrollX * maxOffsetX;
    const float offsetY = m_scrollY * maxOffsetY;
    handleEditorInput(canvasPos, canvasSize, maxOffsetX, maxOffsetY);

    const auto [x0, y0, x1, y1] = visibleRange(g_editorMap, offsetX, offsetY, canvasSize);
    drawList->PushClipRect(canvasPos, canvasMax, true);
    if(m_showTile){
        for(int y = y0; y < y1; ++y){
            for(int x = x0; x < x1; ++x){
                if((x % 2) || (y % 2) || !g_editorMap.tile(x, y).valid){ continue; }
                if(auto *image = retrieveImage(g_editorMap.tile(x, y).texID)){
                    const ImVec2 p0(canvasPos.x + x * SYS_MAPGRIDXP - offsetX, canvasPos.y + y * SYS_MAPGRIDYP - offsetY);
                    const ImVec2 p1(p0.x + image->texture.width(), p0.y + image->texture.height());
                    drawList->AddImage(image->texture.id(), p0, p1);
                    if(m_tileLine){ drawList->AddRect(p0, p1, IM_COL32(255, 0, 0, 255)); }
                }
            }
        }
    }

    for(const int depth: {OBJD_GROUND, OBJD_OVERGROUND0, OBJD_OVERGROUND1, OBJD_SKY}){
        const int di = depthIndex(depth);
        for(int y = y0; y < y1; ++y){
            for(int x = x0; x < x1; ++x){
                for(int objIndex = 0; objIndex < 2; ++objIndex){
                    const auto &obj = g_editorMap.cell(x, y).obj[objIndex];
                    if(!obj.valid || obj.depth != depth || !(m_showObject[di] || m_showObjectIndex[objIndex])){ continue; }
                    uint32_t texID = obj.texID;
                    if(obj.animated){
                        texID = (texID & 0XFFFF0000) | to_u16(to_u16(texID) + to_u16(m_aniTimer.frame(obj.tickType, obj.frameCount)));
                    }
                    if(auto *image = retrieveImage(texID)){
                        const ImVec2 p0(canvasPos.x + x * SYS_MAPGRIDXP - offsetX,
                            canvasPos.y + y * SYS_MAPGRIDYP + SYS_MAPGRIDYP - image->texture.height() - offsetY);
                        const ImVec2 p1(p0.x + image->texture.width(), p0.y + image->texture.height());
                        drawList->AddImage(image->texture.id(), p0, p1);
                        if(m_objectLine[di] || m_objectIndexLine[objIndex]){ drawList->AddRect(p0, p1, depthColor(depth)); }
                    }
                }
            }
        }
    }

    if(m_attributeLine){
        for(int y = y0; y < y1; ++y){
            for(int x = x0; x < x1; ++x){
                if(!m_attributeGrid.testLand(g_editorMap.cell(x, y).land)){ continue; }
                const ImVec2 p0(canvasPos.x + x * SYS_MAPGRIDXP - offsetX, canvasPos.y + y * SYS_MAPGRIDYP - offsetY);
                const ImVec2 p1(p0.x + SYS_MAPGRIDXP, p0.y + SYS_MAPGRIDYP);
                drawList->AddRect(p0, p1, IM_COL32(255, 0, 0, 255));
                drawList->AddLine(p0, p1, IM_COL32(255, 0, 0, 255));
                drawList->AddLine(ImVec2(p1.x, p0.y), ImVec2(p0.x, p1.y), IM_COL32(255, 0, 0, 255));
            }
        }
    }
    if(m_showLight){
        for(int y = y0; y < y1; ++y){
            for(int x = x0; x < x1; ++x){
                if(!g_editorMap.cell(x, y).light.valid){ continue; }
                const ImVec2 center(canvasPos.x + x * SYS_MAPGRIDXP + SYS_MAPGRIDXP / 2 - offsetX,
                    canvasPos.y + y * SYS_MAPGRIDYP + SYS_MAPGRIDYP / 2 - offsetY);
                drawList->AddCircleFilled(center, 100.0f, IM_COL32(0x12, 0x86, 0xFF, 45));
                if(m_lightLine){ drawList->AddCircle(center, 20.0f, IM_COL32(255, 0, 255, 255)); }
            }
        }
    }
    if(m_gridLine){
        for(int x = x0; x <= x1; ++x){
            const float px = canvasPos.x + x * SYS_MAPGRIDXP - offsetX;
            drawList->AddLine(ImVec2(px, canvasPos.y), ImVec2(px, canvasMax.y), IM_COL32(255, 0, 255, 255));
        }
        for(int y = y0; y <= y1; ++y){
            const float py = canvasPos.y + y * SYS_MAPGRIDYP - offsetY;
            drawList->AddLine(ImVec2(canvasPos.x, py), ImVec2(canvasMax.x, py), IM_COL32(255, 0, 255, 255));
        }
    }

    for(int y = y0; y < y1; ++y){
        for(int x = x0; x < x1; ++x){
            bool selected = false;
            if(m_selectMode == 0){ selected = g_editorMap.cellSelect(x, y).attribute != m_reversed; }
            else if(m_selectMode == 1 && !(x % 2) && !(y % 2)){ selected = g_editorMap.tileSelect(x, y).tile; }
            else if(m_selectMode >= 2){
                for(int oi = 0; oi < 2; ++oi){ selected = selected || g_editorMap.cellSelect(x, y).obj[oi]; }
            }
            if(selected){
                const ImVec2 p0(canvasPos.x + x * SYS_MAPGRIDXP - offsetX, canvasPos.y + y * SYS_MAPGRIDYP - offsetY);
                drawList->AddRectFilled(p0, ImVec2(p0.x + SYS_MAPGRIDXP, p0.y + SYS_MAPGRIDYP),
                    m_deselect ? IM_COL32(255, 0, 0, 128) : IM_COL32(0, 255, 0, 128));
            }
        }
    }

    if(ImGui::IsItemHovered()){
        const auto mouse = ImGui::GetIO().MousePos;
        const int gx = static_cast<int>((mouse.x - canvasPos.x + offsetX) / SYS_MAPGRIDXP);
        const int gy = static_cast<int>((mouse.y - canvasPos.y + offsetY) / SYS_MAPGRIDYP);
        if(g_editorMap.validC(gx, gy)){
            const ImVec2 p0(canvasPos.x + gx * SYS_MAPGRIDXP - offsetX, canvasPos.y + gy * SYS_MAPGRIDYP - offsetY);
            drawList->AddRectFilled(p0, ImVec2(p0.x + SYS_MAPGRIDXP, p0.y + SYS_MAPGRIDYP),
                m_deselect ? IM_COL32(255, 0, 0, 96) : IM_COL32(0, 255, 0, 96));
            if(m_enableTest && m_animationDB){
                if(auto [dx, dy, frame] = m_animationDB->getFrame(); frame && frame->valid()){
                    const ImVec2 fp(canvasPos.x + gx * SYS_MAPGRIDXP - offsetX + dx, canvasPos.y + gy * SYS_MAPGRIDYP - offsetY + dy);
                    drawList->AddImage(frame->id(), fp, ImVec2(fp.x + frame->width(), fp.y + frame->height()));
                }
            }
        }
    }
    drawList->PopClipRect();

    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + 250, canvasPos.y + 110), IM_COL32(0, 0, 0, 192));
    const auto mouse = ImGui::GetIO().MousePos;
    const int mousePX = std::max(0, static_cast<int>(mouse.x - canvasPos.x + offsetX));
    const int mousePY = std::max(0, static_cast<int>(mouse.y - canvasPos.y + offsetY));
    drawList->AddText(ImVec2(canvasPos.x + 10, canvasPos.y + 10), IM_COL32(255, 0, 0, 255),
        str_printf("OffsetX: %d %d\nOffsetY: %d %d\nMouseGX: %d %d\nMouseGY: %d %d",
            to_d(offsetX) / SYS_MAPGRIDXP, to_d(offsetX), to_d(offsetY) / SYS_MAPGRIDYP, to_d(offsetY),
            mousePX / SYS_MAPGRIDXP, mousePX, mousePY / SYS_MAPGRIDYP, mousePY).c_str());
}

void MainWindow::handleEditorInput(const ImVec2 &canvasPos, const ImVec2 &, float maxOffsetX, float maxOffsetY)
{
    if(!ImGui::IsItemHovered()){
        return;
    }
    const auto &io = ImGui::GetIO();
    if(io.MouseWheel != 0.0f && maxOffsetY > 0.0f){
        m_scrollY = std::clamp(m_scrollY - io.MouseWheel * SYS_MAPGRIDYP / maxOffsetY, 0.0f, 1.0f);
    }
    if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && maxOffsetX > 0){ m_scrollX = std::clamp(m_scrollX - SYS_MAPGRIDXP / maxOffsetX, 0.0f, 1.0f); }
    if(ImGui::IsKeyPressed(ImGuiKey_RightArrow) && maxOffsetX > 0){ m_scrollX = std::clamp(m_scrollX + SYS_MAPGRIDXP / maxOffsetX, 0.0f, 1.0f); }
    if(ImGui::IsKeyPressed(ImGuiKey_UpArrow) && maxOffsetY > 0){ m_scrollY = std::clamp(m_scrollY - SYS_MAPGRIDYP / maxOffsetY, 0.0f, 1.0f); }
    if(ImGui::IsKeyPressed(ImGuiKey_DownArrow) && maxOffsetY > 0){ m_scrollY = std::clamp(m_scrollY + SYS_MAPGRIDYP / maxOffsetY, 0.0f, 1.0f); }

    if(m_enableSelect && ImGui::IsMouseDown(ImGuiMouseButton_Left)){
        const int gx = static_cast<int>((io.MousePos.x - canvasPos.x + m_scrollX * maxOffsetX) / SYS_MAPGRIDXP);
        const int gy = static_cast<int>((io.MousePos.y - canvasPos.y + m_scrollY * maxOffsetY) / SYS_MAPGRIDYP);
        addSelection(gx, gy, io.MousePos.x - canvasPos.x, io.MousePos.y - canvasPos.y, maxOffsetX, maxOffsetY);
    }
    else if(!m_enableSelect && ImGui::IsMouseDragging(ImGuiMouseButton_Left)){
        if(maxOffsetX > 0){ m_scrollX = std::clamp(m_scrollX - io.MouseDelta.x / maxOffsetX, 0.0f, 1.0f); }
        if(maxOffsetY > 0){ m_scrollY = std::clamp(m_scrollY - io.MouseDelta.y / maxOffsetY, 0.0f, 1.0f); }
    }
}

void MainWindow::addSelection(int gx, int gy, float mouseX, float mouseY, float maxOffsetX, float maxOffsetY)
{
    if(!g_editorMap.validC(gx, gy)){
        return;
    }
    if(m_selectMode == 0){
        if(m_attributeSelect.testLand(g_editorMap.cell(gx, gy).land)){
            g_editorMap.cellSelect(gx, gy).attribute = !m_deselect;
        }
        return;
    }
    if(m_selectMode == 1){
        g_editorMap.tileSelect(gx, gy).tile = !m_deselect;
        return;
    }
    const float offsetX = m_scrollX * maxOffsetX;
    const float offsetY = m_scrollY * maxOffsetY;
    for(int y = gy; y < std::min(gy + 26, to_d(g_editorMap.h())); ++y){
        for(int oi = 0; oi < 2; ++oi){
            const auto &obj = g_editorMap.cell(gx, y).obj[oi];
            if(!obj.valid){ continue; }
            const int modeDepth = m_selectMode >= 2 && m_selectMode <= 5 ? m_selectMode - 2 : -1;
            const int modeIndex = m_selectMode >= 6 ? m_selectMode - 6 : -1;
            if((modeDepth >= 0 && depthIndex(obj.depth) != modeDepth) || (modeIndex >= 0 && oi != modeIndex)){ continue; }
            if(auto *image = retrieveImage(obj.texID)){
                const float x = gx * SYS_MAPGRIDXP - offsetX;
                const float y0 = y * SYS_MAPGRIDYP + SYS_MAPGRIDYP - image->texture.height() - offsetY;
                if(mouseX >= x && mouseX < x + image->texture.width() && mouseY >= y0 && mouseY < y0 + image->texture.height()){
                    g_editorMap.cellSelect(gx, y).obj[oi] = !m_deselect;
                    return;
                }
            }
        }
    }
}

bool MainWindow::testAttribute(const Mir2xMapData::LAND &land) const
{
    return m_attributeSelect.testLand(land);
}

void MainWindow::requestLoad(PendingLoad load)
{
    m_pendingLoad = load;
    if(load == PendingLoad::Mir2Map){
        makeWorkingFolder();
    }
    if(m_wilPath.empty()){
        m_wilDialog.open("Set *.wil File Path...", ".", ImGuiFileDialog::Mode::Directory);
    }
    else{
        beginPendingLoad();
    }
}

void MainWindow::beginPendingLoad()
{
    const auto mapPath = parentPath(m_wilPath);
    switch(m_pendingLoad){
        case PendingLoad::Layer:
        case PendingLoad::Mir2Map:
            m_mapDialog.open("Select .map file", mapPath.c_str(), ImGuiFileDialog::Mode::File, ".map");
            break;
        case PendingLoad::Mir2xMapData:
            m_mapDataDialog.open("Set Map File Path...", ".", ImGuiFileDialog::Mode::Directory);
            break;
        default:
            break;
    }
}

void MainWindow::drawDialogs()
{
    if(m_continuePendingLoad){
        m_continuePendingLoad = false;
        beginPendingLoad();
    }

    std::string selected;
    try{
        if(m_wilDialog.draw(selected)){
            m_wilPath = selected;
            m_imageMapDB = std::make_unique<ImageMapDB>(m_wilPath.c_str());
            g_imageMapDB = m_imageMapDB.get();
            clearImageCache();
            m_continuePendingLoad = true;
        }
        if(m_workingDialog.draw(selected)){
            m_workingPath = selected;
            filesys::makeDir(m_workingPath.c_str());
        }
        if(m_mapDialog.draw(selected)){ loadMir2Map(selected); }
        if(m_layerDialog.draw(selected)){ loadLayer(selected); }
        if(m_mapDataDialog.draw(selected)){ loadMir2xMapData(selected); }
    }
    catch(const std::exception &e){
        alert(e.what(), "Load failed");
    }
}

void MainWindow::loadMir2Map(const std::string &fileName)
{
    if(!g_editorMap.loadMir2Map(fileName.c_str())){
        throw std::runtime_error("Load mir2 map failed: " + fileName);
    }
    afterLoadMap(str_printf("Mir2Map %s, width %zu, height %zu", fileName.c_str(), g_editorMap.w(), g_editorMap.h()));
}

void MainWindow::loadLayer(const std::string &fileName)
{
    if(!g_editorMap.loadLayer(fileName.c_str())){
        throw std::runtime_error("Load layer failed: " + fileName);
    }
    afterLoadMap(str_printf("Layer %s, width %zu, height %zu", fileName.c_str(), g_editorMap.w(), g_editorMap.h()));
}

void MainWindow::loadMir2xMapData(const std::string &directory)
{
    const auto fileName = directory + "/DESC.BIN";
    if(!filesys::hasFile(fileName.c_str()) || !g_editorMap.loadMir2xMapData(fileName.c_str())){
        throw std::runtime_error("Invalid Mir2xMapData folder: " + directory);
    }
    makeWorkingFolder();
    afterLoadMap(str_printf("Mir2xMapData %s, width %zu, height %zu", fileName.c_str(), g_editorMap.w(), g_editorMap.h()));
}

void MainWindow::afterLoadMap(const std::string &status)
{
    m_scrollX = m_scrollY = 0.0f;
    m_status = status;
    clearImageCache();
    startOverviewBuild();
    m_overviewOpen = true;
    m_pendingLoad = PendingLoad::None;
}

void MainWindow::startOverviewBuild()
{
    m_overviewTexture.clear();
    m_overviewBuild = {};
    if(!g_editorMap.valid() || !m_imageMapDB){
        return;
    }

    constexpr float maxOverviewSize = 1024.0f;
    const float mapWidth = to_f(g_editorMap.w()) * SYS_MAPGRIDXP;
    const float mapHeight = to_f(g_editorMap.h()) * SYS_MAPGRIDYP;
    const float scale = std::min({1.0f, maxOverviewSize / mapWidth, maxOverviewSize / mapHeight});
    m_overviewBuild.width = std::max(1, to_d(std::lround(mapWidth * scale)));
    m_overviewBuild.height = std::max(1, to_d(std::lround(mapHeight * scale)));
    m_overviewBuild.scaleX = m_overviewBuild.width / mapWidth;
    m_overviewBuild.scaleY = m_overviewBuild.height / mapHeight;
    m_overviewBuild.totalCells = g_editorMap.w() * g_editorMap.h() * 5;
    m_overviewBuild.pixels.assign(
        to_uz(m_overviewBuild.width) * m_overviewBuild.height,
        colorf::BLACK_A255);
    m_overviewBuild.active = true;
}

void MainWindow::updateOverviewBuild()
{
    if(!m_overviewBuild.active){
        return;
    }

    constexpr auto frameBudget = std::chrono::milliseconds(4);
    const auto deadline = std::chrono::steady_clock::now() + frameBudget;
    const size_t mapW = g_editorMap.w();
    const size_t mapH = g_editorMap.h();
    const size_t mapCells = mapW * mapH;
    constexpr int depths[] = {OBJD_GROUND, OBJD_OVERGROUND0, OBJD_OVERGROUND1, OBJD_SKY};

    do{
        const size_t index = m_overviewBuild.nextCell++;
        const size_t phase = index / mapCells;
        const size_t cellIndex = index % mapCells;
        const int x = to_d(cellIndex % mapW);
        const int y = to_d(cellIndex / mapW);
        if(phase == 0){
            const auto &tile = g_editorMap.tile(x, y);
            if(!(x % 2) && !(y % 2) && tile.valid){
                blendOverviewImage(tile.texID, x, y, false);
            }
        }
        else{
            for(const auto &obj: g_editorMap.cell(x, y).obj){
                if(obj.valid && obj.depth == depths[phase - 1]){
                    blendOverviewImage(obj.texID, x, y, true);
                }
            }
        }
    }while(m_overviewBuild.nextCell < m_overviewBuild.totalCells
        && std::chrono::steady_clock::now() < deadline);

    if(m_overviewBuild.nextCell >= m_overviewBuild.totalCells){
        m_overviewTexture.loadRGBA(
            m_overviewBuild.pixels.data(),
            m_overviewBuild.width,
            m_overviewBuild.height);
        glBindTexture(GL_TEXTURE_2D, m_overviewTexture.id());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        m_overviewBuild.pixels.clear();
        m_overviewBuild.imageCache.clear();
        m_overviewBuild.sourceSizes.clear();
        m_overviewBuild.active = false;
    }
}

void MainWindow::blendOverviewImage(uint32_t texID, int x, int y, bool object)
{
    try{
        auto [sizeIt, sizeInserted] = m_overviewBuild.sourceSizes.try_emplace(texID);
        if(sizeInserted){
            if(const auto *imageInfo = m_imageMapDB->setIndex(texID)){
                sizeIt->second = {to_d(imageInfo->width), to_d(imageInfo->height)};
            }
        }
        const auto [srcW, srcH] = sizeIt->second;
        if(srcW <= 0 || srcH <= 0){
            return;
        }
        const int worldX = x * SYS_MAPGRIDXP;
        const int worldY = y * SYS_MAPGRIDYP + (object ? SYS_MAPGRIDYP - srcH : 0);
        const int dstX0 = to_d(std::lround(worldX * m_overviewBuild.scaleX));
        const int dstY0 = to_d(std::lround(worldY * m_overviewBuild.scaleY));
        const int dstX1 = to_d(std::lround((worldX + srcW) * m_overviewBuild.scaleX));
        const int dstY1 = to_d(std::lround((worldY + srcH) * m_overviewBuild.scaleY));
        const int dstW = dstX1 - dstX0;
        const int dstH = dstY1 - dstY0;
        if(dstW <= 0 || dstH <= 0){
            return;
        }

        const uint64_t cacheKey = (to_u64(texID) << 32) | (to_u64(dstW) << 16) | to_u64(dstH);
        auto [it, inserted] = m_overviewBuild.imageCache.try_emplace(cacheKey);
        auto &image = it->second;
        if(inserted){
            if(const auto [src, decodedW, decodedH] = m_imageMapDB->decode(texID, true); src){
                image.width = dstW;
                image.height = dstH;
                image.pixels = scaleImage(src, to_d(decodedW), to_d(decodedH), dstW, dstH);
            }
        }
        if(image.pixels.empty()){
            return;
        }
        imgf::blendImageBuffer(
            m_overviewBuild.pixels.data(),
            m_overviewBuild.width,
            m_overviewBuild.height,
            image.pixels.data(),
            image.width,
            image.height,
            dstX0,
            dstY0);
    }
    catch(...){
    }
}

void MainWindow::makeWorkingFolder()
{
    if(m_workingPath.empty() || m_workingPath.front() == '.'){
        m_workingPath = "./" + std::to_string(hres_tstamp::localtime());
    }
    filesys::makeDir(m_workingPath.c_str());
}

void MainWindow::saveMir2xMapData()
{
    if(!g_editorMap.valid()){
        alert("Currently no operating map!");
        return;
    }
    makeWorkingFolder();
    const auto fileName = m_workingPath + "/DESC.BIN";
    if(g_editorMap.saveMir2xMapData(fileName.c_str())){
        alert("Save map file in mir2xmapdata format successfully!", "Saved");
    }
}

void MainWindow::extractOverview(int ratio)
{
    if(!g_editorMap.valid() || !m_imageMapDB){
        alert("Current editor map or image database is invalid");
        return;
    }
    makeWorkingFolder();
    const int width = to_d(g_editorMap.w()) * SYS_MAPGRIDXP;
    const int height = to_d(g_editorMap.h()) * SYS_MAPGRIDYP;
    if(ratio <= 0 || width % ratio || height % ratio){
        alert(str_printf("Overview ratio doesn't match map size: ratio %d, w %zu h %zu", ratio, g_editorMap.w(), g_editorMap.h()));
        return;
    }
    std::vector<uint32_t> full(to_uz(width) * height, 0);
    g_editorMap.exportOverview([&](uint32_t texID, int x, int y, bool object)
    {
        if(const auto [src, srcW, srcH] = m_imageMapDB->decode(texID, true); src){
            imgf::blendImageBuffer(full.data(), width, height, src, to_d(srcW), to_d(srcH),
                x * SYS_MAPGRIDXP, y * SYS_MAPGRIDYP + (object ? SYS_MAPGRIDYP - to_d(srcH) : 0));
        }
    }, nullptr);
    int exportW = width;
    int exportH = height;
    const uint32_t *exportData = full.data();
    std::vector<uint32_t> reduced;
    if(ratio != 1){
        exportW /= ratio;
        exportH /= ratio;
        reduced.resize(to_uz(exportW) * exportH);
        for(int y = 0; y < exportH; ++y){
            for(int x = 0; x < exportW; ++x){
                reduced[to_uz(y) * exportW + x] = full[to_uz(y * ratio) * width + x * ratio];
            }
        }
        exportData = reduced.data();
    }
    const auto name = std::to_string(hres_tstamp::localtime()) + ".PNG";
    const auto fullName = m_workingPath + "/" + name;
    if(imgf::saveImageBuffer(reinterpret_cast<const uint8_t *>(exportData), exportW, exportH, fullName.c_str())){
        alert("Done overview map image: " + name, "Exported");
    }
    else{
        alert("Export overview map image failed");
    }
}

void MainWindow::drawModals()
{
    if(m_showAbout){
        ImGui::OpenPopup("About Me");
        m_showAbout = false;
    }
    if(ImGui::BeginPopupModal("About Me", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextUnformatted("Name : Anhong\nEmail: anhonghe@gmail.com");
        if(ImGui::Button("OK", ImVec2(100, 0))){ ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if(m_confirmQuit){
        ImGui::OpenPopup("Quit map editor?");
        m_confirmQuit = false;
    }
    if(ImGui::BeginPopupModal("Quit map editor?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        if(ImGui::Button("No", ImVec2(100, 0))){ ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if(ImGui::Button("Yes", ImVec2(100, 0))){ ImGui::CloseCurrentPopup(); close(); }
        ImGui::EndPopup();
    }
}

void LayerBrowserWindow::addEntry(Mir2xMapData data)
{
    if(data.valid()){
        m_layers.push_back(std::move(data));
        m_selected = static_cast<int>(m_layers.size()) - 1;
    }
}

bool LayerBrowserWindow::importObject(int depthType) const
{
    const int index = depthIndex(depthType);
    return index >= 0 && m_importObject[index];
}

const Mir2xMapData *LayerBrowserWindow::getLayer() const
{
    return m_selected >= 0 && m_selected < static_cast<int>(m_layers.size()) ? &m_layers.at(m_selected) : nullptr;
}

void LayerBrowserWindow::drawBrowser(MainWindow *owner)
{
    if(!browserOpen){
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(555, 605), ImGuiCond_Appearing);
    if(ImGui::Begin("Layer Browser", &browserOpen, ImGuiWindowFlags_MenuBar)){
        if(ImGui::BeginMenuBar()){
            if(ImGui::BeginMenu("Layer")){
                if(ImGui::MenuItem("Merge", nullptr, false, getLayer() != nullptr)){ viewOpen = true; }
                ImGui::MenuItem("Export", nullptr, false, false);
                ImGui::MenuItem("Export All", nullptr, false, false);
                ImGui::Separator();
                if(ImGui::MenuItem("Exit")){ browserOpen = false; }
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Import")){
                ImGui::MenuItem("Attribute", nullptr, &m_importAttribute);
                ImGui::MenuItem("Light", nullptr, &m_importLight);
                ImGui::MenuItem("Tile", nullptr, &m_importTile);
                ImGui::Separator();
                ImGui::MenuItem("Object Ground", nullptr, &m_importObject[0]);
                ImGui::MenuItem("Object Over Ground 0", nullptr, &m_importObject[1]);
                ImGui::MenuItem("Object Over Ground 1", nullptr, &m_importObject[2]);
                ImGui::MenuItem("Object Sky", nullptr, &m_importObject[3]);
                ImGui::Separator();
                if(ImGui::MenuItem("Import", "Ctrl+I", false, g_editorMap.valid())){
                    addEntry(g_editorMap.exportLayer());
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        const float footer = ImGui::GetTextLineHeightWithSpacing();
        if(ImGui::BeginChild("Layers", ImVec2(0, -footer), ImGuiChildFlags_Borders)){
            for(int i = 0; i < static_cast<int>(m_layers.size()); ++i){
                const auto &layer = m_layers.at(i);
                const auto label = str_printf("layer: %zu x %zu", layer.w(), layer.h());
                if(ImGui::Selectable(label.c_str(), m_selected == i, ImGuiSelectableFlags_AllowDoubleClick)){
                    m_selected = i;
                    if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){ viewOpen = true; }
                }
            }
        }
        ImGui::EndChild();
        ImGui::TextUnformatted("Version 0.0.1");
    }
    ImGui::End();
    (void)(owner);
}

void LayerBrowserWindow::drawView(MainWindow *owner)
{
    if(viewOpen && getLayer()){
        owner->drawLayerCanvas(*getLayer(), scrollX, scrollY);
    }
}

void MainWindow::drawLayerCanvas(const Mir2xMapData &layer, float &scrollX, float &scrollY)
{
    ImGui::SetNextWindowSize(ImVec2(985, 690), ImGuiCond_Appearing);
    if(ImGui::Begin("LayerViewWindow", &m_layerBrowser.viewOpen, ImGuiWindowFlags_MenuBar)){
        if(ImGui::BeginMenuBar()){
            if(ImGui::BeginMenu("Layer")){
                ImGui::MenuItem("Attribute", nullptr, &m_layerBrowser.m_viewDrawAttribute);
                ImGui::MenuItem("Light", nullptr, &m_layerBrowser.m_viewDrawLight);
                ImGui::MenuItem("Tile", nullptr, &m_layerBrowser.m_viewDrawTile);
                ImGui::Separator();
                const char *names[] = {"Object Ground", "Object Over Ground 0", "Object Over Ground 1", "Object Sky"};
                for(int i = 0; i < 4; ++i){ ImGui::MenuItem(names[i], nullptr, &m_layerBrowser.m_viewDrawObject[i]); }
                ImGui::Separator();
                ImGui::MenuItem("Clear Background", nullptr, &m_layerBrowser.m_viewClearBackground);
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Lines")){
                ImGui::MenuItem("Grid", nullptr, &m_layerBrowser.m_viewGridLine);
                ImGui::MenuItem("Attribute", nullptr, &m_layerBrowser.m_viewAttributeLine);
                ImGui::MenuItem("Light", nullptr, &m_layerBrowser.m_viewLightLine);
                ImGui::MenuItem("Tile", nullptr, &m_layerBrowser.m_viewTileLine);
                const char *names[] = {"Object Ground", "Object Over Ground 0", "Object Over Ground 1", "Object Sky"};
                for(int i = 0; i < 4; ++i){ ImGui::MenuItem(names[i], nullptr, &m_layerBrowser.m_viewObjectLine[i]); }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        const auto canvasPos = ImGui::GetCursorScreenPos();
        const auto canvasSize = ImGui::GetContentRegionAvail();
        const ImVec2 canvasMax(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);
        auto *drawList = ImGui::GetWindowDrawList();
        if(m_layerBrowser.m_viewClearBackground){ drawList->AddRectFilled(canvasPos, canvasMax, IM_COL32_BLACK); }
        ImGui::InvisibleButton("LayerCanvas", canvasSize, ImGuiButtonFlags_MouseButtonLeft);
        const float maxX = std::max(0.0f, to_f(layer.w() * SYS_MAPGRIDXP) - canvasSize.x);
        const float maxY = std::max(0.0f, to_f(layer.h() * SYS_MAPGRIDYP) - canvasSize.y);
        if(ImGui::IsItemHovered()){
            const auto &io = ImGui::GetIO();
            if(io.MouseWheel && maxY > 0){ scrollY = std::clamp(scrollY - io.MouseWheel * SYS_MAPGRIDYP / maxY, 0.0f, 1.0f); }
            if(ImGui::IsMouseDragging(ImGuiMouseButton_Left)){
                if(maxX > 0){ scrollX = std::clamp(scrollX - io.MouseDelta.x / maxX, 0.0f, 1.0f); }
                if(maxY > 0){ scrollY = std::clamp(scrollY - io.MouseDelta.y / maxY, 0.0f, 1.0f); }
            }
        }
        const float ox = scrollX * maxX;
        const float oy = scrollY * maxY;
        const auto [x0, y0, x1, y1] = visibleRange(layer, ox, oy, canvasSize);
        drawList->PushClipRect(canvasPos, canvasMax, true);
        if(m_layerBrowser.m_viewDrawTile){
            for(int y = y0; y < y1; ++y){
                for(int x = x0; x < x1; ++x){
                    if((x % 2) || (y % 2) || !layer.tile(x, y).valid){ continue; }
                    if(auto *image = retrieveImage(layer.tile(x, y).texID)){
                        const ImVec2 p0(canvasPos.x + x * SYS_MAPGRIDXP - ox, canvasPos.y + y * SYS_MAPGRIDYP - oy);
                        const ImVec2 p1(p0.x + image->texture.width(), p0.y + image->texture.height());
                        drawList->AddImage(image->texture.id(), p0, p1);
                        if(m_layerBrowser.m_viewTileLine){ drawList->AddRect(p0, p1, IM_COL32(255, 0, 0, 255)); }
                    }
                }
            }
        }
        for(const int depth: {OBJD_GROUND, OBJD_OVERGROUND0, OBJD_OVERGROUND1, OBJD_SKY}){
            const int di = depthIndex(depth);
            if(!m_layerBrowser.m_viewDrawObject[di]){ continue; }
            for(int y = y0; y < y1; ++y){
                for(int x = x0; x < x1; ++x){
                    for(int oi = 0; oi < 2; ++oi){
                        const auto &obj = layer.cell(x, y).obj[oi];
                        if(!obj.valid || obj.depth != depth){ continue; }
                        if(auto *image = retrieveImage(obj.texID)){
                            const ImVec2 p0(canvasPos.x + x * SYS_MAPGRIDXP - ox,
                                canvasPos.y + y * SYS_MAPGRIDYP + SYS_MAPGRIDYP - image->texture.height() - oy);
                            const ImVec2 p1(p0.x + image->texture.width(), p0.y + image->texture.height());
                            drawList->AddImage(image->texture.id(), p0, p1);
                            if(m_layerBrowser.m_viewObjectLine[di]){ drawList->AddRect(p0, p1, depthColor(depth)); }
                        }
                    }
                }
            }
        }
        if(m_layerBrowser.m_viewGridLine){
            for(int x = x0; x <= x1; ++x){
                const float px = canvasPos.x + x * SYS_MAPGRIDXP - ox;
                drawList->AddLine(ImVec2(px, canvasPos.y), ImVec2(px, canvasMax.y), IM_COL32(255, 0, 255, 255));
            }
            for(int y = y0; y <= y1; ++y){
                const float py = canvasPos.y + y * SYS_MAPGRIDYP - oy;
                drawList->AddLine(ImVec2(canvasPos.x, py), ImVec2(canvasMax.x, py), IM_COL32(255, 0, 255, 255));
            }
        }
        drawList->PopClipRect();
    }
    ImGui::End();
}
