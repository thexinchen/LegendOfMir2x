#include "ImMiniMapBoard.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "fflerror.hpp"
#include "gldevice.hpp"
#include "gui_font.hpp"
#include "gui_texture.hpp"
#include "maprecord.hpp"
#include "processrun.hpp"
#include "strf.hpp"
#include "totype.hpp"

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;
extern FontexDB *g_fontexDB;

namespace
{
    bool inRect(const ImVec2 point, const ImVec2 min, const ImVec2 max)
    {
        return point.x >= min.x && point.x < max.x && point.y >= min.y && point.y < max.y;
    }

    void drawText(ImDrawList *drawList, const ImVec2 pos, const std::string &text, const ImU32 color)
    {
        if(const auto texture = g_fontexDB->retrieve(1, 12, 0, text.c_str()); texture){
            drawList->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h}, {0, 0}, {1, 1}, color);
        }
    }

    bool textureButton(const char *id, const GLTexID texture, const ImVec2 pos)
    {
        if(!texture){
            return false;
        }

        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {to_f(texture.w), to_f(texture.h)});
        ImGui::GetWindowDrawList()->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h});
        return clicked;
    }
}

ImMiniMapBoard::ImMiniMapBoard(ProcessRun *processRun)
    : ImBoard("##mini-map-board", true)
    , m_processRun(fflcheck(processRun))
{}

void ImMiniMapBoard::draw() const
{
    if(!show()){
        return;
    }

    const auto mapTexture = getMiniMapTexture();
    if(!mapTexture){
        return;
    }

    const int canvasW = m_extended ? to_dround(g_glDevice->getRendererWidth() * 0.8) : 200;
    const int canvasH = m_extended ? to_dround(g_glDevice->getRendererHeight() * 0.5) : 200;
    moveTo(
        m_extended ? (g_glDevice->getRendererWidth()  - canvasW) * 0.5f : to_f(g_glDevice->getRendererWidth() - canvasW),
        m_extended ? (g_glDevice->getRendererHeight() - canvasH) * 0.5f : 0.0f);

    if(beginWindow({to_f(canvasW), to_f(canvasH)})){
        const ImVec2 pos = ImGui::GetWindowPos();
        const ImVec2 end {pos.x + canvasW, pos.y + canvasH};
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        auto *drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(pos, end, IM_COL32(0, 0, 0, 255));

        const auto [imageW, imageH] = imageSize();
        const auto [imageDX, imageDY] = imageOffset(canvasW, canvasH);
        const ImVec2 imagePos {pos.x + imageDX, pos.y + imageDY};
        drawList->PushClipRect(pos, end, true);
        drawList->AddImage(
            mapTexture,
            imagePos,
            {imagePos.x + imageW, imagePos.y + imageH},
            {0, 0},
            {1, 1},
            IM_COL32(255, 255, 255, m_alphaOn ? 128 : 255));

        for(const auto &[uid, creature]: m_processRun->getCOList()){
            const auto [color, radius] = [this](const uint64_t value) -> std::tuple<ImU32, float>
            {
                switch(uidf::getUIDType(value)){
                    case UID_PLY:
                        return value == m_processRun->getMyHeroUID()
                             ? std::tuple<ImU32, float>{IM_COL32(255, 0, 255, 255), 3.0f}
                             : std::tuple<ImU32, float>{IM_COL32(200, 0, 200, 255), 2.0f};
                    case UID_NPC:
                        return {IM_COL32(0, 0, 255, 255), 2.0f};
                    case UID_MON:
                        return {IM_COL32(255, 0, 0, 255), 1.0f};
                    default:
                        return {0, 0.0f};
                }
            }(uid);

            if(color){
                const auto [x, y] = creature->location();
                const auto [canvasX, canvasY] = canvasLocationFromMap(x, y, canvasW, canvasH);
                if(canvasX >= 0 && canvasX < canvasW && canvasY >= 0 && canvasY < canvasH){
                    drawList->AddCircleFilled({pos.x + canvasX, pos.y + canvasY}, radius, color);
                }
            }
        }
        drawList->PopClipRect();

        drawList->AddRect(pos, end, IM_COL32(231, 231, 189, 128));
        if(const auto texture = g_progUseDB->retrieve(0X09000006); texture){
            drawList->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h});
        }
        if(const auto texture = g_progUseDB->retrieve(0X09000007); texture){
            const ImVec2 corner {end.x - texture.w, pos.y};
            drawList->AddImage(texture, corner, {corner.x + texture.w, corner.y + texture.h});
        }
        if(const auto texture = g_progUseDB->retrieve(0X09000008); texture){
            const ImVec2 corner {pos.x, end.y - texture.h};
            drawList->AddImage(texture, corner, {corner.x + texture.w, corner.y + texture.h});
        }

        const auto zoomText = str_printf("%d%%", to_dround(m_zoomFactor * 100));
        const auto zoomTexture = g_fontexDB->retrieve(1, 12, 0, zoomText.c_str());
        const std::array<GLTexID, 4> buttonTextures
        {{
            g_progUseDB->retrieve(m_alphaOn       ? 0X09000011 : 0X09000010),
            g_progUseDB->retrieve(m_extended      ? 0X09000021 : 0X09000020),
            g_progUseDB->retrieve(m_autoCenterActive ? 0X09000031 : 0X09000030),
            g_progUseDB->retrieve(m_configActive  ? 0X09000041 : 0X09000040),
        }};

        int toolbarW = zoomTexture ? zoomTexture.w + 4 : 0;
        int toolbarH = zoomTexture ? zoomTexture.h : 0;
        for(const auto texture: buttonTextures){
            if(texture){
                toolbarW += texture.w + (toolbarW ? 1 : 0);
                toolbarH = std::max(toolbarH, texture.h);
            }
        }

        const ImVec2 toolbarPos {end.x - toolbarW, end.y - toolbarH};
        float itemX = toolbarPos.x;
        if(zoomTexture){
            drawList->AddRectFilled({itemX, end.y - zoomTexture.h}, {itemX + zoomTexture.w + 4, end.y}, IM_COL32(0, 0, 0, 255));
            drawList->AddImage(
                zoomTexture,
                {itemX + 2, end.y - zoomTexture.h},
                {itemX + 2 + zoomTexture.w, end.y},
                {0, 0},
                {1, 1},
                IM_COL32(255, 255, 0, 255));
            itemX += zoomTexture.w + 4;
        }

        const auto drawButton = [&](const int index, const char *id)
        {
            const auto texture = buttonTextures.at(index);
            if(!texture){
                return false;
            }
            if(itemX > toolbarPos.x){
                itemX += 1;
            }
            const bool clicked = textureButton(id, texture, {itemX, end.y - texture.h});
            itemX += texture.w;
            return clicked;
        };

        if(drawButton(0, "##minimap-alpha")){ flipAlpha(); }
        if(drawButton(1, "##minimap-extend")){ flipExtended(); }
        if(drawButton(2, "##minimap-center")){
            flipAutoCenter();
            m_autoCenterActive = m_autoCenter;
        }
        if(drawButton(3, "##minimap-config")){ m_configActive = m_autoCenter; }

        const bool onCanvas = inRect(mouse, pos, end);
        const bool onToolbar = inRect(mouse, toolbarPos, end);
        if(onCanvas && !onToolbar && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
            m_dragStarted = true;
        }
        if(!ImGui::IsMouseDown(ImGuiMouseButton_Left)){
            m_dragStarted = false;
        }
        if(m_dragStarted && ImGui::IsMouseDown(ImGuiMouseButton_Left)){
            const ImVec2 delta = ImGui::GetIO().MouseDelta;
            if(delta.x != 0.0f || delta.y != 0.0f){
                if(m_autoCenter){
                    m_mapImageDX = imageDX;
                    m_mapImageDY = imageDY;
                    m_autoCenter = false;
                }
                m_mapImageDX += to_dround(delta.x);
                m_mapImageDY += to_dround(delta.y);
                fixMapImageOffset(canvasW, canvasH);
            }
        }

        if(onCanvas && ImGui::GetIO().MouseWheel != 0.0f){
            zoomOnCanvasAt(
                to_dround(mouse.x - pos.x),
                to_dround(mouse.y - pos.y),
                canvasW,
                canvasH,
                m_zoomFactor * std::pow(1.1, ImGui::GetIO().MouseWheel));
        }

        if(onCanvas && !onToolbar && ImGui::IsMouseClicked(ImGuiMouseButton_Right)){
            const auto [mapX, mapY] = mapLocationFromCanvas(
                to_dround(mouse.x - pos.x),
                to_dround(mouse.y - pos.y),
                canvasW,
                canvasH);
            m_processRun->requestSpaceMove(std::get<0>(m_processRun->getMap()), mapX, mapY);
        }

        if(onCanvas && inRect(mouse, imagePos, {imagePos.x + imageW, imagePos.y + imageH})){
            const auto [mapX, mapY] = mapLocationFromCanvas(
                to_dround(mouse.x - pos.x),
                to_dround(mouse.y - pos.y),
                canvasW,
                canvasH);
            const auto text = str_printf("[%d,%d]", mapX, mapY);
            if(const auto textTexture = g_fontexDB->retrieve(1, 12, 0, text.c_str()); textTexture){
                const ImU32 bgColor = m_processRun->canMove(true, 0, mapX, mapY)
                                    ? IM_COL32(0, 0, 0, 200)
                                    : IM_COL32(255, 0, 0, 200);
                const ImVec2 textPos {mouse.x - textTexture.w - 2, mouse.y - textTexture.h - 1};
                auto *foreground = ImGui::GetForegroundDrawList();
                foreground->AddRectFilled(textPos, mouse, bgColor);
                drawText(foreground, {textPos.x + 1, textPos.y}, text, IM_COL32(255, 255, 0, 255));
            }
        }
    }
    endWindow();
}

bool ImMiniMapBoard::processEvent(const MirEvent &event) const
{
    if(!show() || !getMiniMapTexture()){
        return false;
    }

    if(event.type == MIR_EVENT_MOUSE_WHEEL){
        const auto pos = position();
        const auto windowSize = size();
        return event.wheel.mouse_x >= pos.x &&
               event.wheel.mouse_x <  pos.x + windowSize.x &&
               event.wheel.mouse_y >= pos.y &&
               event.wheel.mouse_y <  pos.y + windowSize.y;
    }
    return ImBoard::processEvent(event);
}

void ImMiniMapBoard::flipAlpha() const
{
    m_alphaOn = !m_alphaOn;
}

void ImMiniMapBoard::flipExtended() const
{
    m_extended = !m_extended;
    m_autoCenter = true;
}

void ImMiniMapBoard::flipAutoCenter() const
{
    if(m_autoCenter){
        const int canvasW = m_extended ? to_dround(g_glDevice->getRendererWidth() * 0.8) : 200;
        const int canvasH = m_extended ? to_dround(g_glDevice->getRendererHeight() * 0.5) : 200;
        const auto [imageDX, imageDY] = imageOffset(canvasW, canvasH);
        m_mapImageDX = imageDX;
        m_mapImageDY = imageDY;
    }
    m_autoCenter = !m_autoCenter;
}

GLTexID ImMiniMapBoard::getMiniMapTexture() const
{
    if(const auto id = DBCOM_MAPRECORD(m_processRun->mapID()).miniMapID; id.has_value()){
        return g_progUseDB->retrieve(id.value());
    }
    return nullptr;
}

std::tuple<int, int> ImMiniMapBoard::imageSize() const
{
    if(const auto texture = getMiniMapTexture()){
        return
        {
            to_dround(texture.w * m_zoomFactor),
            to_dround(texture.h * m_zoomFactor),
        };
    }
    return {0, 0};
}

std::tuple<int, int> ImMiniMapBoard::imageOffset(const int canvasW, const int canvasH) const
{
    if(!m_autoCenter){
        return {m_mapImageDX, m_mapImageDY};
    }

    const auto [imageW, imageH] = imageSize();
    const auto [mapUID, mapW, mapH] = m_processRun->getMap();
    const auto [heroX, heroY] = m_processRun->getMyHero()->location();
    return
    {
        imageW <= canvasW ? (canvasW - imageW) / 2 : std::clamp(canvasW / 2 - to_dround(heroX * 1.0 * imageW / mapW), canvasW - imageW, 0),
        imageH <= canvasH ? (canvasH - imageH) / 2 : std::clamp(canvasH / 2 - to_dround(heroY * 1.0 * imageH / mapH), canvasH - imageH, 0),
    };
}

std::tuple<int, int> ImMiniMapBoard::mapLocationFromCanvas(const int canvasX, const int canvasY, const int canvasW, const int canvasH) const
{
    const auto [imageW, imageH] = imageSize();
    const auto [imageDX, imageDY] = imageOffset(canvasW, canvasH);
    const auto [mapUID, mapW, mapH] = m_processRun->getMap();
    return
    {
        to_dround((canvasX - imageDX) * 1.0 * mapW / imageW),
        to_dround((canvasY - imageDY) * 1.0 * mapH / imageH),
    };
}

std::tuple<int, int> ImMiniMapBoard::canvasLocationFromMap(const int mapX, const int mapY, const int canvasW, const int canvasH) const
{
    const auto [imageW, imageH] = imageSize();
    const auto [imageDX, imageDY] = imageOffset(canvasW, canvasH);
    const auto [mapUID, mapW, mapH] = m_processRun->getMap();
    return
    {
        imageDX + to_dround(mapX * 1.0 * imageW / mapW),
        imageDY + to_dround(mapY * 1.0 * imageH / mapH),
    };
}

void ImMiniMapBoard::fixMapImageOffset(const int canvasW, const int canvasH) const
{
    const auto [imageW, imageH] = imageSize();
    m_mapImageDX = imageW <= canvasW ? (canvasW - imageW) / 2 : std::clamp(m_mapImageDX, canvasW - imageW, 0);
    m_mapImageDY = imageH <= canvasH ? (canvasH - imageH) / 2 : std::clamp(m_mapImageDY, canvasH - imageH, 0);
}

void ImMiniMapBoard::zoomOnCanvasAt(const int canvasX, const int canvasY, const int canvasW, const int canvasH, const double zoomFactor) const
{
    const auto [oldImageW, oldImageH] = imageSize();
    const auto [oldImageDX, oldImageDY] = imageOffset(canvasW, canvasH);
    const double imageXRatio = (canvasX - oldImageDX) * 1.0 / oldImageW;
    const double imageYRatio = (canvasY - oldImageDY) * 1.0 / oldImageH;

    m_zoomFactor = std::clamp(zoomFactor, 0.1, 10.0);
    const auto [newImageW, newImageH] = imageSize();
    m_autoCenter = false;
    m_mapImageDX = oldImageDX + (canvasX - oldImageDX) - to_dround(newImageW * imageXRatio);
    m_mapImageDY = oldImageDY + (canvasY - oldImageDY) - to_dround(newImageH * imageYRatio);
    fixMapImageOffset(canvasW, canvasH);
}
