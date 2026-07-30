#include "ImMainUI.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <imgui.h>

#include "client.hpp"
#include "clientmonster.hpp"
#include "clientmsg.hpp"
#include "colorf.hpp"
#include "dbcomid.hpp"
#include "gldevice.hpp"
#include "gui_font.hpp"
#include "gui_textengine.hpp"
#include "gui_texture.hpp"
#include "imeboard.hpp"
#include "log.hpp"
#include "ImMiniMapBoard.hpp"
#include "processrun.hpp"
#include "strf.hpp"
#include "tinyxml2.h"
#include "totype.hpp"
#include "uidf.hpp"

extern Client *g_client;
extern Log *g_mir2xLog;
extern GLDevice *g_glDevice;
extern PNGTexDB *g_itemDB;
extern PNGTexDB *g_progUseDB;
extern FontexDB *g_fontexDB;
extern IMEBoard *g_imeBoard;

namespace
{
    constexpr float leftWidth = 178.0f;
    constexpr float rightWidth = 166.0f;
    constexpr float compactHeight = 152.0f;
    constexpr float textureHeight = 133.0f;

    ImDrawList *background()
    {
        return ImGui::GetBackgroundDrawList();
    }

    void drawTexture(uint32_t textureID, ImVec2 pos, ImU32 tint = IM_COL32_WHITE)
    {
        if(const auto texture = g_progUseDB->retrieve(textureID); texture){
            background()->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h}, {0, 0}, {1, 1}, tint);
        }
    }

    void drawTextureCrop(uint32_t textureID, ImVec2 pos, ImVec2 size, ImVec2 srcPos, ImVec2 srcSize, ImU32 tint = IM_COL32_WHITE)
    {
        if(const auto texture = g_progUseDB->retrieve(textureID); texture){
            background()->AddImage(
                texture,
                pos,
                {pos.x + size.x, pos.y + size.y},
                {srcPos.x / texture.w, srcPos.y / texture.h},
                {(srcPos.x + srcSize.x) / texture.w, (srcPos.y + srcSize.y) / texture.h},
                tint);
        }
    }

    void drawTextureNineSlice(
            const uint32_t textureID,
            const ImVec2 pos,
            const ImVec2 size,
            const float left,
            const float top,
            const float centerW,
            const float centerH)
    {
        const auto texture = g_progUseDB->retrieve(textureID);
        if(!texture){
            return;
        }

        const float right = texture.w - left - centerW;
        const float bottom = texture.h - top - centerH;
        const std::array<float, 4> sx {{0, left, left + centerW, to_f(texture.w)}};
        const std::array<float, 4> sy {{0, top, top + centerH, to_f(texture.h)}};
        const std::array<float, 4> dx {{pos.x, pos.x + left, pos.x + size.x - right, pos.x + size.x}};
        const std::array<float, 4> dy {{pos.y, pos.y + top, pos.y + size.y - bottom, pos.y + size.y}};
        for(int y = 0; y < 3; ++y){
            for(int x = 0; x < 3; ++x){
                if(dx[x + 1] <= dx[x] || dy[y + 1] <= dy[y]){
                    continue;
                }
                background()->AddImage(
                    texture,
                    {dx[x], dy[y]},
                    {dx[x + 1], dy[y + 1]},
                    {sx[x] / texture.w, sy[y] / texture.h},
                    {sx[x + 1] / texture.w, sy[y + 1] / texture.h});
            }
        }
    }

    GLTexID gameTextTexture(const char *text, uint8_t font, uint8_t size)
    {
        if(!(text && *text)){
            return nullptr;
        }
        return g_fontexDB->retrieve(font, size, 0, text);
    }

    ImVec2 drawGameText(
            ImVec2 pos,
            const char *text,
            uint8_t font = 11,
            uint8_t size = 15,
            ImU32 color = IM_COL32_WHITE)
    {
        if(const auto texture = gameTextTexture(text, font, size); texture){
            background()->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h}, {0, 0}, {1, 1}, color);
            return {to_f(texture.w), to_f(texture.h)};
        }
        return {};
    }

    bool textureButton(const char *id, uint32_t offID, uint32_t downID, ImVec2 pos, bool visible = true)
    {
        const auto off = g_progUseDB->retrieve(offID);
        if(!off){
            return false;
        }

        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {to_f(off.w), to_f(off.h)});
        if(visible){
            const auto down = g_progUseDB->retrieve(downID);
            const auto shown = ImGui::IsItemActive() && down ? down : off;
            background()->AddImage(shown, pos, {pos.x + shown.w, pos.y + shown.h});
        }
        return clicked;
    }

    bool overlayButton(const char *id, uint32_t hoverID, uint32_t downID, ImVec2 pos)
    {
        const auto hover = g_progUseDB->retrieve(hoverID);
        if(!hover){
            return false;
        }

        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {to_f(hover.w), to_f(hover.h)});
        if(ImGui::IsItemHovered() || ImGui::IsItemActive()){
            const auto down = g_progUseDB->retrieve(downID);
            const auto shown = ImGui::IsItemActive() && down ? down : hover;
            background()->AddImage(shown, pos, {pos.x + shown.w, pos.y + shown.h});
        }
        return clicked;
    }

    bool beginOverlay(const char *name, ImVec2 pos, ImVec2 size)
    {
        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        return ImGui::Begin(
            name,
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
    }

    void endOverlay()
    {
        ImGui::End();
        ImGui::PopStyleVar();
    }
}

ImMainUI::ImMainUI(ProcessRun *processRun)
    : m_processRun(processRun)
    , m_NPCChatBoard(processRun)
    , m_friendChatBoard(
        g_glDevice->getRendererWidth()  / 2 - 250,
        g_glDevice->getRendererHeight() / 2 - 250,
        processRun)
    , m_horseBoard(processRun)
    , m_skillBoard(
        g_glDevice->getRendererWidth()  / 2 - 180,
        g_glDevice->getRendererHeight() / 2 - 224,
        processRun)
    , m_guildBoard(processRun)
    , m_miniMapBoard(processRun)
    , m_acutionBoard(processRun)
    , m_purchaseBoard(processRun)
    , m_teamStateBoard(processRun)
    , m_inventoryBoard(processRun)
    , m_questStateBoard(processRun)
    , m_playerStateBoard(processRun)
    , m_inputStringBoard {}
    , m_runtimeConfigBoard(
        g_glDevice->getRendererWidth()  / 2 - 255,
        g_glDevice->getRendererHeight() / 2 - 234,
        600,
        480,
        processRun)
    , m_securedItemListBoard(processRun)
{
    fflassert(m_processRun);
    g_imeBoard->dropFocus();
}

void ImMainUI::update(double deltaMS)
{
    m_accuTimeMS += deltaMS;
    for(auto &[name, blink]: m_buttonBlink){
        if(!blink.active){
            continue;
        }
        blink.elapsedMS += deltaMS;
        if(blink.remainingMS > 0.0){
            blink.remainingMS -= deltaMS;
            if(blink.remainingMS <= 0.0){
                blink.active = false;
            }
        }
    }

    m_purchaseBoard.update(deltaMS);
    m_acutionBoard.update(deltaMS);
    m_horseBoard.update(deltaMS);
    m_guildBoard.update(deltaMS);
    m_inputStringBoard.update(deltaMS);
    m_questStateBoard.update(deltaMS);
    m_teamStateBoard.update(deltaMS);
    m_securedItemListBoard.update(deltaMS);
    m_inventoryBoard.update(deltaMS);
    m_playerStateBoard.update(deltaMS);
    m_miniMapBoard.update(deltaMS);
    m_NPCChatBoard.update(deltaMS);
    m_friendChatBoard.update(deltaMS);
    m_skillBoard.update(deltaMS);
    g_imeBoard->update(deltaMS);
}

void ImMainUI::draw() const
{
    m_miniMapBoard.draw();
    m_NPCChatBoard.draw();
    m_friendChatBoard.draw();
    m_skillBoard.draw();
    drawHUD();
    if(m_quickAccessShown){
        drawQuickAccess();
    }
    m_acutionBoard.draw();
    m_horseBoard.draw();
    m_guildBoard.draw();
    m_inputStringBoard.draw();
    m_runtimeConfigBoard.draw();
    m_questStateBoard.draw();
    m_teamStateBoard.draw();
    m_securedItemListBoard.draw();
    m_inventoryBoard.draw();
    m_playerStateBoard.draw();
    m_purchaseBoard.draw();

    if(g_imeBoard->show()){
        g_imeBoard->draw();
    }
}

void ImMainUI::drawHUD() const
{
    const float screenW = to_f(g_glDevice->getRendererWidth());
    const float screenH = to_f(g_glDevice->getRendererHeight());
    const float middleW = std::max(1.0f, screenW - leftWidth - rightWidth);
    const float hudH = m_minimize ? 31.0f : (m_expand ? std::min(421.0f, screenH) : compactHeight);
    const float hudTop = screenH - hudH;
    const float baseY = screenH - textureHeight;
    const float expandedPanelH = std::max(0.0f, hudH - 21.0f);
    const float panelTop = m_expand ? screenH - expandedPanelH : baseY;

    if(!m_minimize){
        drawTextureCrop(0X00000012, {0, baseY}, {leftWidth, textureHeight}, {0, 0}, {leftWidth, textureHeight});
        if(m_expand){
            background()->AddRectFilled(
                {leftWidth, panelTop},
                {screenW - rightWidth, screenH},
                IM_COL32(0, 0, 0, 220));
            drawTextureNineSlice(0X00000027, {leftWidth, panelTop}, {middleW, expandedPanelH}, 50, 47, 287, 196);
        }
        else{
            background()->AddRectFilled({leftWidth, panelTop}, {screenW - rightWidth, screenH}, IM_COL32_BLACK);
            drawTextureNineSlice(0X00000013, {leftWidth, baseY}, {middleW, 131.0f}, 50, 0, 287, 131);
        }
        drawTextureCrop(
            0X00000012,
            {screenW - rightWidth, baseY},
            {rightWidth, textureHeight},
            {800.0f - rightWidth, 0},
            {rightWidth, textureHeight});
    }
    drawTexture(0X00000022, {leftWidth + (middleW - 127.0f) * 0.5f, hudTop});
    {
        const auto titleX = leftWidth + (middleW - 127.0f) * 0.5f;
        const auto decimalFrame = m_accuTimeMS / 1000.0;
        const auto currentFrame = to_d(std::floor(decimalFrame)) % 4;
        const auto nextFrame = (currentFrame + 1) % 4;
        const auto alpha = to_u8(std::clamp(to_dround(255.0 * (decimalFrame - std::floor(decimalFrame))), 0, 255));
        const ImVec2 arcPos {titleX + 46.0f, hudTop + 8.0f};
        if(const auto texture = g_progUseDB->retrieve(0X04000000 + currentFrame); texture){
            background()->AddImage(
                texture,
                arcPos,
                {arcPos.x + texture.w, arcPos.y + texture.h},
                {0, 0},
                {1, 1},
                IM_COL32(255,255,255,255 - alpha));
        }
        if(const auto texture = g_progUseDB->retrieve(0X04000000 + nextFrame); texture){
            background()->AddImage(
                texture,
                arcPos,
                {arcPos.x + texture.w, arcPos.y + texture.h},
                {0, 0},
                {1, 1},
                IM_COL32(255,255,255,alpha));
        }
    }

    if(!beginOverlay("##game-main-hud", {0, hudTop}, {screenW, hudH})){
        endOverlay();
        return;
    }

    const float localBaseY = baseY;
    const float rightX = screenW - rightWidth;
    const float middleX = leftWidth;

    const ImVec2 titlePos {leftWidth + (middleW - 127.0f) * 0.5f, hudTop};
    ImGui::SetCursorScreenPos(titlePos);
    ImGui::InvisibleButton("##hud-title-toggle", {127, 41});
    if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){
        m_minimize = !m_minimize;
    }

    if(m_minimize){
        endOverlay();
        return;
    }

    const auto hero = m_processRun->getMyHero();
    if(hero){
        const auto drawBottomCrop = [](uint32_t id, float x, float bottomY, double ratio)
        {
            if(const auto texture = g_progUseDB->retrieve(id); texture){
                const int cropH = std::clamp(to_dround(texture.h * ratio), 0, texture.h);
                if(cropH > 0){
                    drawTextureCrop(
                        id,
                        {x, bottomY - cropH},
                        {to_f(texture.w), to_f(cropH)},
                        {0, to_f(texture.h - cropH)},
                        {to_f(texture.w), to_f(cropH)});
                }
            }
        };
        drawBottomCrop(0X00000018, 33.0f, localBaseY + 95.0f, hero->getHealthRatio().at(0));
        drawBottomCrop(0X00000019, 73.0f, localBaseY + 95.0f, hero->getHealthRatio().at(1));

        if(const auto bar = g_progUseDB->retrieve(0X000000A0); bar){
            const float levelX = 153.0f - bar.w * 0.5f;
            const float loadX = 166.0f - bar.w * 0.5f;
            drawBottomCrop(0X000000A0, levelX, localBaseY + 115.0f, hero->getLevelRatio());
            drawBottomCrop(0X000000A0, loadX, localBaseY + 115.0f, hero->getInventoryRatio());
        }

        if(!m_expand){
            const auto focusCreature = m_processRun->findUID(m_processRun->getFocusUID(FOCUS_MOUSE));
            const auto faceCreature = focusCreature && (focusCreature->type() == UID_PLY || focusCreature->type() == UID_MON)
                                    ? focusCreature
                                    : hero;
            const uint32_t faceTextureID = [&]() -> uint32_t
            {
                if(faceCreature->type() == UID_PLY){
                    return dynamic_cast<const Hero *>(faceCreature)->faceGfxID();
                }
                if(faceCreature->type() == UID_MON){
                    if(const auto lookID = dynamic_cast<const ClientMonster *>(faceCreature)->lookID(); lookID >= 0){
                        return UINT32_C(0X01000000) + (lookID - LID_BEGIN);
                    }
                    return SYS_U32NIL;
                }
                return hero->faceGfxID();
            }();

            const float faceX = middleX + middleW - 101.0f;
            const float faceY = localBaseY + 14.0f;
            auto face = g_progUseDB->retrieve(faceTextureID);
            if(!face){
                face = g_progUseDB->retrieve(0X010007CF);
            }
            if(face){
                const float faceW = to_f(std::max(0, face.w - 2));
                const float faceH = to_f(face.h);
                background()->AddImage(
                    face,
                    {faceX, faceY + 3},
                    {faceX + faceW, faceY + 3 + faceH},
                    {0, 0},
                    {faceW / face.w, faceH / face.h});
                background()->AddRectFilled(
                    {faceX, faceY},
                    {faceX + 82.0f * to_f(faceCreature->getHealthRatio().at(0)), faceY + 3.0f},
                    IM_COL32(255, 0, 0, 255));
            }

            if(const auto &buffList = faceCreature->getSDBuffIDListOpt(); buffList){
                int drawCount = 0;
                for(const auto id: buffList->idList){
                    const auto &record = DBCOM_BUFFRECORD(id);
                    if(!record || record.icon.gfxID == SYS_U32NIL){
                        continue;
                    }
                    const auto icon = g_progUseDB->retrieve(record.icon.gfxID);
                    if(!icon){
                        continue;
                    }
                    const ImVec2 iconPos {
                        faceX + 20.0f + (drawCount % 5) * 16.0f,
                        faceY + 79.0f - (drawCount / 5) * 16.0f,
                    };
                    background()->AddImage(icon, iconPos, {iconPos.x + 16.0f, iconPos.y + 16.0f});
                    const auto color = record.favor > 0 ? IM_COL32(0,255,0,255)
                                     : record.favor == 0 ? IM_COL32(255,255,0,255)
                                                        : IM_COL32(255,0,0,255);
                    background()->AddRect(iconPos, {iconPos.x + 16.0f, iconPos.y + 16.0f}, color);
                    ++drawCount;
                }
            }
        }

        const auto levelText = std::to_string(hero->getLevel());
        const auto levelTexture = gameTextTexture(levelText.c_str(), 0, 12);
        if(levelTexture){
            drawGameText(
                {titlePos.x + 63.0f - levelTexture.w * 0.5f, titlePos.y + 25.0f - levelTexture.h * 0.5f},
                levelText.c_str(),
                0,
                12,
                IM_COL32(255, 255, 0, 255));
        }

        if(uidf::isMap(m_processRun->mapUID())){
            if(const auto &mapRecord = DBCOM_MAPRECORD(m_processRun->mapID())){
                const std::string mapNameFull(to_cstr(mapRecord.name));
                const auto mapName = mapNameFull.substr(0, mapNameFull.find('_'));
                const auto location = str_printf("%s: %d %d", mapName.c_str(), hero->x(), hero->y());
                drawGameText({4, localBaseY + 110}, location.c_str(), 10, 15);
            }
        }
    }

    if(overlayButton("##hud-quick", 0X0B000000, 0X0B000001, {148, localBaseY + 2})){
        m_quickAccessShown = !m_quickAccessShown;
        if(m_quickY < 0.0f){
            m_quickY = hudTop - 48.0f;
        }
    }
    if(overlayButton("##hud-close", 0X0000001E, 0X0000001F, {8, localBaseY + 72})){
        std::exit(0);
    }
    overlayButton("##hud-minimize", 0X00000020, 0X00000021, {109, localBaseY + 72});

    if(overlayButton("##hud-exchange", 0X00000042, 0X00000042, {rightX + 4, localBaseY + 6})){
        const_cast<ImMainUI *>(this)->addLog(0, "exchange doesn't implemented yet");
    }
    if(overlayButton("##hud-minimap", 0X00000043, 0X00000043, {rightX + 4, localBaseY + 40})){
        auto map = getMiniMapBoard();
        if(map && map->getMiniMapTexture()){
            map->flipShow();
        }
        else{
            const_cast<ImMainUI *>(this)->addLog(3, to_cstr(u8"没有可用的地图"));
        }
    }
    if(overlayButton("##hud-magic-key", 0X00000044, 0X00000044, {rightX + 4, localBaseY + 75})){
        m_processRun->flipDrawMagicKey();
    }

    struct BoardButton
    {
        const char *id;
        const char *name;
        const char *board;
        uint32_t off;
        uint32_t down;
        float x;
        float y;
    };
    constexpr BoardButton buttons[]
    {
        {"##hud-inventory", "Inventory", "InventoryBoard", 0X00000030, 0X00000031, 48, 33},
        {"##hud-state", "HeroState", "PlayerStateBoard", 0X00000033, 0X00000032, 77, 31},
        {"##hud-skill", "HeroMagic", "SkillBoard", 0X00000035, 0X00000034, 105, 33},
        {"##hud-guild", "Guild", "GuildBoard", 0X00000036, 0X00000037, 40, 11},
        {"##hud-team", "Team", "TeamStateBoard", 0X00000038, 0X00000039, 72, 8},
        {"##hud-quest", "Quest", "QuestStateBoard", 0X0000003A, 0X0000003B, 108, 11},
        {"##hud-horse", "Horse", "HorseBoard", 0X0000003C, 0X0000003D, 40, 61},
        {"##hud-config", "RuntimeConfig", "RuntimeConfigBoard", 0X0000003E, 0X0000003F, 72, 72},
        {"##hud-friend", "FriendChat", "FriendChatBoard", 0X00000040, 0X00000041, 108, 61},
    };

    for(const auto &button: buttons){
        if(textureButton(
                    button.id,
                    button.off,
                    button.down,
                    {rightX + button.x, localBaseY + button.y},
                    blinkVisible(button.name))){
            stopButtonBlink(button.name);
            if(std::string_view(button.name) == "Team"){
                auto teamBoard = m_processRun->getTeamStateBoard();
                if(hero && hero->hasTeam()){
                    teamBoard->flipShow();
                    if(teamBoard->show()){
                        teamBoard->refresh();
                    }
                }
                else{
                    m_processRun->setCursor(ProcessRun::CURSOR_TEAMFLAG);
                }
            }
            else{
                const_cast<ImMainUI *>(this)->flipBoard(button.board);
            }
        }
    }

    ImGui::SetCursorScreenPos({rightX + 1, localBaseY + 105});
    if(ImGui::InvisibleButton("##hud-ac", {82, 24})){
        m_acMagic = !m_acMagic;
    }
    ImGui::SetCursorScreenPos({rightX + 84, localBaseY + 105});
    if(ImGui::InvisibleButton("##hud-dc", {82, 24})){
        m_dcMagic = !m_dcMagic;
    }

    if(hero){
        const auto [acMin, acMax] = m_processRun->getACNum(m_acMagic ? "MA" : "AC");
        const auto [dcMin, dcMax] = m_processRun->getACNum(m_dcMagic ? "MC" : "DC");
        const auto acText = str_printf("%d-%d", acMin, acMax);
        const auto dcText = str_printf("%d-%d", dcMin, dcMax);
        const auto acIcon = g_progUseDB->retrieve(m_acMagic ? 0X00000048 : 0X00000046);
        const auto dcIcon = g_progUseDB->retrieve(m_dcMagic ? 0X00000049 : 0X00000047);
        if(acIcon){
            drawTexture(m_acMagic ? 0X00000048 : 0X00000046, {rightX + 1, localBaseY + 105});
            drawGameText({rightX + 6.0f + acIcon.w, localBaseY + 105}, acText.c_str(), 11, 15, IM_COL32(255, 255, 0, 255));
        }
        if(dcIcon){
            drawTexture(m_dcMagic ? 0X00000049 : 0X00000047, {rightX + 84, localBaseY + 105});
            drawGameText({rightX + 89.0f + dcIcon.w, localBaseY + 105}, dcText.c_str(), 11, 15, IM_COL32(255, 255, 0, 255));
        }
    }

    const float logX = middleX + (m_expand ? 7.0f : 7.0f);
    const float logY = panelTop + 15.0f;
    const float logW = std::max(20.0f, middleW - (m_expand ? 24.0f : 112.0f));
    const float logH = m_expand ? std::max(84.0f, expandedPanelH - 70.0f) : 84.0f;

    struct DrawLogLine
    {
        const LogLine *line = nullptr;
        float y = 0.0f;
        float h = 15.0f;
    };
    std::vector<DrawLogLine> drawLines;
    drawLines.reserve(m_logList.size());
    float totalLogH = 0.0f;
    for(const auto &line: m_logList){
        float height = 15.0f;
        if(!line.xml.empty()){
            const int layoutWidth = std::max(1, to_d(logW));
            if(!line.layout || line.layoutWidth != layoutWidth){
                line.layout = std::make_shared<XMLTypeset>(
                    layoutWidth,
                    LALIGN_JUSTIFY,
                    true,
                    false,
                    11,
                    15,
                    0,
                    colorf::WHITE_A255);
                line.layout->loadXML(line.xml.c_str());
                line.layoutWidth = layoutWidth;
            }
            height = to_f(std::max(line.layout->ph(), line.layout->getDefaultFontHeight()));
        }
        drawLines.push_back({std::addressof(line), totalLogH, height});
        totalLogH += height;
    }

    const float lineH = 15.0f;
    const float logReach = std::max(0.0f, totalLogH - logH);
    if(ImGui::IsMouseHoveringRect({logX, logY}, {logX + logW, logY + logH}) && ImGui::GetIO().MouseWheel != 0.0f && logReach > 0.0f){
        m_logScroll = std::clamp(m_logScroll - ImGui::GetIO().MouseWheel * lineH * 3.0f / logReach, 0.0f, 1.0f);
    }
    background()->PushClipRect({logX, logY}, {logX + logW, logY + logH}, true);
    for(const auto &drawLine: drawLines){
        const float textY = logY + drawLine.y - m_logScroll * logReach;
        if(textY + drawLine.h < logY || textY >= logY + logH){
            continue;
        }
        if(drawLine.line->layout){
            drawLine.line->layout->drawImGui(background(), to_d(logX), to_d(textY));
        }
        else{
            drawGameText({logX, textY}, drawLine.line->text.c_str(), 11, 15, drawLine.line->color);
        }
    }
    background()->PopClipRect();

    {
        const float barX = middleX + middleW - 10.0f;
        const float barY = panelTop + 40.0f;
        const float barH = m_expand ? std::max(1.0f, expandedPanelH - 75.0f) : 60.0f;
        ImGui::SetCursorScreenPos({barX - 9.0f, barY - 10.0f});
        ImGui::InvisibleButton("##hud-log-slider", {18.0f, barH + 20.0f});
        if(logReach > 0.0f && ImGui::IsItemActive()){
            m_logScroll = std::clamp((ImGui::GetIO().MousePos.y - barY) / std::max(1.0f, barH - 1.0f), 0.0f, 1.0f);
        }
        if(const auto slider = g_progUseDB->retrieve(0X00000088); slider){
            const ImVec2 sliderPos {
                barX - 4.5f,
                barY - 8.0f + m_logScroll * (barH - 1.0f),
            };
            background()->AddImage(
                slider,
                sliderPos,
                {sliderPos.x + slider.w, sliderPos.y + slider.h},
                {0, 0},
                {1, 1},
                ImGui::IsItemActive() ? IM_COL32_WHITE : IM_COL32(128,128,128,255));
        }
    }

    const float inputY = m_expand ? screenH - 50.0f : localBaseY + 106.0f;
    ImGui::SetCursorScreenPos({logX, inputY});
    ImGui::SetNextItemWidth(logW);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_Text, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, {0, 0, 0, 0});
    if(m_focusCommand){
        ImGui::SetKeyboardFocusHere();
        m_focusCommand = false;
    }
    if(ImGui::InputText(
                "##hud-command",
                m_command.data(),
                m_command.size(),
                ImGuiInputTextFlags_EnterReturnsTrue)){
        submitCommand();
    }
    const bool commandActive = ImGui::IsItemActive();
    background()->PushClipRect({logX, inputY}, {logX + logW, inputY + 40.0f}, true);
    const auto commandSize = drawGameText({logX, inputY}, m_command.data(), 11, 15);
    if(commandActive && std::fmod(m_accuTimeMS, 1000.0) < 500.0){
        background()->AddLine(
            {logX + commandSize.x + 1.0f, inputY},
            {logX + commandSize.x + 1.0f, inputY + std::max(15.0f, commandSize.y)},
            IM_COL32_WHITE);
    }
    background()->PopClipRect();
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);

    const ImVec2 switchPos {middleX + middleW - 15.0f, panelTop + 3.0f};
    if(overlayButton("##hud-expand", 0X00000028, 0X00000029, switchPos)){
        m_expand = !m_expand;
    }
    if(m_expand){
        overlayButton("##hud-emoji", 0X00000023, 0X00000024, {middleX + middleW - 94.0f, screenH - 44.0f});
        overlayButton("##hud-mute", 0X00000025, 0X00000026, {middleX + middleW - 54.0f, screenH - 44.0f});
    }

    endOverlay();
}

void ImMainUI::drawQuickAccess() const
{
    const auto texture = g_progUseDB->retrieve(0X00000060);
    if(!texture){
        return;
    }
    if(m_quickY < 0.0f){
        m_quickY = to_f(g_glDevice->getRendererHeight()) - compactHeight - 48.0f;
    }
    m_quickX = std::clamp(m_quickX, 0.0f, std::max(0.0f, to_f(g_glDevice->getRendererWidth() - texture.w)));
    m_quickY = std::clamp(m_quickY, 0.0f, std::max(0.0f, to_f(g_glDevice->getRendererHeight() - texture.h)));

    const ImVec2 pos {m_quickX, m_quickY};
    drawTexture(0X00000060, pos);
    if(!beginOverlay("##quick-access", pos, {to_f(texture.w), to_f(texture.h)})){
        endOverlay();
        return;
    }

    for(int slot = 0; slot < 6; ++slot){
        const ImVec2 gridPos {pos.x + 17.0f + 42.0f * slot, pos.y + 6.0f};
        ImGui::SetCursorScreenPos(gridPos);
        ImGui::InvisibleButton(str_printf("##quick-slot-%d", slot).c_str(), {36, 36});
        if(ImGui::IsItemHovered()){
            background()->AddRectFilled(gridPos, {gridPos.x + 36, gridPos.y + 36}, IM_COL32(255, 255, 255, 64));
        }
        if(ImGui::IsItemClicked(ImGuiMouseButton_Left)){
            if(const auto grabbed = m_processRun->getMyHero()->getInvPack().getGrabbedItem()){
                const auto &record = DBCOM_ITEMRECORD(grabbed.itemID);
                if(record.beltable()){
                    m_processRun->requestEquipBelt(grabbed.itemID, grabbed.seqID, slot);
                }
                else{
                    m_processRun->getMyHero()->getInvPack().add(grabbed);
                    m_processRun->getMyHero()->getInvPack().setGrabbedItem({});
                }
            }
            else if(m_processRun->getMyHero()->getBelt(slot)){
                m_processRun->requestGrabBelt(slot);
            }
        }
        if(ImGui::IsItemClicked(ImGuiMouseButton_Right)){
            consumeQuickSlot(slot);
        }

        if(const auto &item = m_processRun->getMyHero()->getBelt(slot)){
            const auto &record = DBCOM_ITEMRECORD(item.itemID);
            if(const auto itemTexture = g_itemDB->retrieve(record.pkgGfxID | 0X01000000); itemTexture){
                const ImVec2 itemPos {
                    gridPos.x + (36.0f - itemTexture.w) * 0.5f,
                    gridPos.y + (36.0f - itemTexture.h) * 0.5f,
                };
                background()->AddImage(itemTexture, itemPos, {itemPos.x + itemTexture.w, itemPos.y + itemTexture.h});
            }
            if(item.count > 1){
                const auto count = std::to_string(item.count);
                const auto countTexture = gameTextTexture(count.c_str(), 1, 10);
                if(countTexture){
                    drawGameText({gridPos.x + 35.0f - countTexture.w, gridPos.y}, count.c_str(), 1, 10);
                }
            }
        }
    }

    if(textureButton("##quick-close", 0X00000061, 0X00000062, {pos.x + 263, pos.y + 32})){
        m_quickAccessShown = false;
    }
    endOverlay();
}

bool ImMainUI::processEvent(const MirEvent &event)
{
    switch(event.type){
        case MIR_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case MIR_EVENT_WINDOW_RESIZED:
            {
                afterResize();
                return true;
            }
        default:
            {
                break;
            }
    }

    bool tookEvent = false;
    if(g_imeBoard->show()){
        tookEvent |= g_imeBoard->processEvent(event);
    }
    tookEvent |= !tookEvent && m_purchaseBoard.processEvent(event);
    tookEvent |= !tookEvent && processHUDEvent(event);
    tookEvent |= !tookEvent && m_acutionBoard.processEvent(event);
    tookEvent |= !tookEvent && m_horseBoard.processEvent(event);
    tookEvent |= !tookEvent && m_guildBoard.processEvent(event);
    tookEvent |= !tookEvent && m_inputStringBoard.processEvent(event);
    tookEvent |= !tookEvent && m_runtimeConfigBoard.processEvent(event);
    tookEvent |= !tookEvent && m_questStateBoard.processEvent(event);
    tookEvent |= !tookEvent && m_teamStateBoard.processEvent(event);
    tookEvent |= !tookEvent && m_securedItemListBoard.processEvent(event);
    tookEvent |= !tookEvent && m_inventoryBoard.processEvent(event);
    tookEvent |= !tookEvent && m_playerStateBoard.processEvent(event);
    tookEvent |= !tookEvent && m_NPCChatBoard.processEvent(event);
    tookEvent |= !tookEvent && m_friendChatBoard.processEvent(event);
    tookEvent |= !tookEvent && m_skillBoard.processEvent(event);
    tookEvent |= !tookEvent && m_miniMapBoard.processEvent(event);
    return tookEvent;
}

bool ImMainUI::processHUDEvent(const MirEvent &event)
{
    switch(event.type){
        case MIR_EVENT_KEY_DOWN:
            {
                if(event.key.key == MIRK_RETURN){
                    m_focusCommand = true;
                    return true;
                }
                if(m_quickAccessShown){
                    const auto ch = GLDeviceHelper::getKeyChar(event, false);
                    if(ch >= '1' && ch <= '6'){
                        consumeQuickSlot(ch - '1');
                        return true;
                    }
                }
                break;
            }
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
            {
                const float x = to_f(event.button.x);
                const float y = to_f(event.button.y);
                if(m_quickAccessShown){
                    if(const auto texture = g_progUseDB->retrieve(0X00000060); texture){
                        if(x >= m_quickX && x < m_quickX + texture.w && y >= m_quickY && y < m_quickY + texture.h){
                            m_quickDragging = event.button.button == MIR_BUTTON_LEFT;
                            return true;
                        }
                    }
                }
                const float hudH = m_minimize ? 31.0f : (m_expand ? std::min(421.0f, to_f(g_glDevice->getRendererHeight())) : compactHeight);
                return y >= g_glDevice->getRendererHeight() - hudH;
            }
        case MIR_EVENT_MOUSE_BUTTON_UP:
            {
                if(event.button.button == MIR_BUTTON_LEFT){
                    m_quickDragging = false;
                }
                break;
            }
        case MIR_EVENT_MOUSE_MOTION:
            {
                if(m_quickDragging){
                    m_quickX += to_f(event.motion.xrel);
                    m_quickY += to_f(event.motion.yrel);
                    return true;
                }
                const float hudH = m_minimize ? 31.0f : (m_expand ? std::min(421.0f, to_f(g_glDevice->getRendererHeight())) : compactHeight);
                return event.motion.y >= g_glDevice->getRendererHeight() - hudH;
            }
        default:
            {
                break;
            }
    }
    return false;
}

void ImMainUI::flipBoard(std::string_view name)
{
    if(name == "HorseBoard"){ m_horseBoard.flipShow(); return; }
    if(name == "GuildBoard"){ m_guildBoard.flipShow(); return; }
    if(name == "QuestStateBoard"){ m_questStateBoard.flipShow(); return; }
    if(name == "TeamStateBoard"){ m_teamStateBoard.flipShow(); return; }
    if(name == "InventoryBoard"){ m_inventoryBoard.flipShow(); return; }
    if(name == "PlayerStateBoard"){ m_playerStateBoard.flipShow(); return; }
    if(name == "FriendChatBoard"){ m_friendChatBoard.flipShow(); return; }
    if(name == "SkillBoard"){ m_skillBoard.flipShow(); return; }
    if(name == "RuntimeConfigBoard"){ m_runtimeConfigBoard.flipShow(); return; }
    throw fflvalue(name);
}

void ImMainUI::afterResize()
{
    m_runtimeConfigBoard.updateWindowSize({
        to_f(g_glDevice->getRendererWidth()),
        to_f(g_glDevice->getRendererHeight()),
    }, true);
}

void ImMainUI::addXMLLog(const char *log)
{
    if(!(log && *log)){
        return;
    }

    tinyxml2::XMLDocument document(true, tinyxml2::PEDANTIC_WHITESPACE);
    if(document.Parse(log) != tinyxml2::XML_SUCCESS){
        addLog(0, log);
        return;
    }

    const auto root = document.RootElement();
    if(root && to_sv(root->Name()) == "layout"){
        for(auto paragraph = root->FirstChildElement("par"); paragraph; paragraph = paragraph->NextSiblingElement("par")){
            tinyxml2::XMLPrinter printer;
            paragraph->Accept(&printer);
            addParLog(printer.CStr());
        }
        return;
    }
    addParLog(log);
}

void ImMainUI::addParLog(const char *log)
{
    if(!(log && *log)){
        return;
    }
    m_logList.push_back({.xml = log});
    m_logScroll = 1.0f;
    while(m_logList.size() > 200){
        m_logList.pop_front();
    }
}

void ImMainUI::addLog(int logType, const char *log)
{
    if(!log){
        return;
    }
    const ImU32 color = [logType]
    {
        switch(logType){
            case 1: return IM_COL32(0, 255, 0, 255);
            case 2: return IM_COL32(64, 128, 255, 255);
            case 3: return IM_COL32(255, 64, 64, 255);
            default: return IM_COL32_WHITE;
        }
    }();
    m_logList.push_back({.text = log, .color = color});
    m_logScroll = 1.0f;
    while(m_logList.size() > 200){
        m_logList.pop_front();
    }

    if(logType == 3){
        g_mir2xLog->addLog(LOGTYPE_WARNING, "%s", log);
    }
    else{
        g_mir2xLog->addLog(LOGTYPE_INFO, "%s", log);
    }
}

void ImMainUI::startButtonBlink(std::string_view name, double durationMS)
{
    auto &blink = m_buttonBlink[std::string(name)];
    blink.elapsedMS = 0.0;
    blink.remainingMS = durationMS;
    blink.active = true;
}

bool ImMainUI::blinkVisible(std::string_view name) const
{
    if(const auto p = m_buttonBlink.find(std::string(name)); p != m_buttonBlink.end() && p->second.active){
        return std::fmod(p->second.elapsedMS, 200.0) < 100.0;
    }
    return true;
}

void ImMainUI::stopButtonBlink(std::string_view name) const
{
    if(auto p = m_buttonBlink.find(std::string(name)); p != m_buttonBlink.end()){
        p->second.active = false;
    }
}

void ImMainUI::submitCommand() const
{
    const std::string fullText = str_trim(m_command.data(), true, false);
    m_command.fill('\0');
    if(fullText.empty()){
        return;
    }

    switch(fullText.front()){
        case '!':
            {
                const auto content = str_trim(fullText.substr(1), true, false);
                if(!content.empty()){
                    CMPlayerBroadcast message {};
                    std::memcpy(message.content, content.data(), std::min(content.size(), sizeof(message.content) - 1));
                    g_client->send({CM_PLAYERBROADCAST, message});
                }
                break;
            }
        case '@':
            {
                m_processRun->userCommand(fullText.c_str() + 1);
                break;
            }
        case '$':
            {
                m_processRun->luaCommand(fullText.c_str() + 1);
                break;
            }
        default:
            {
                const_cast<ImMainUI *>(this)->addLog(0, fullText.c_str());
                CMPlayerSay message {};
                std::memcpy(message.content, fullText.data(), std::min(fullText.size(), sizeof(message.content) - 1));
                g_client->send({CM_PLAYERSAY, message});
                break;
            }
    }
}

void ImMainUI::consumeQuickSlot(int slot) const
{
    if(slot >= 0 && slot < 6){
        if(const auto &item = m_processRun->getMyHero()->getBelt(slot)){
            InvPack::playItemSoundEffect(item.itemID, true);
            m_processRun->requestConsumeItem(item.itemID, item.seqID, 1);
        }
    }
}

int ImMainUI::shiftHeight() const
{
    return m_minimize ? 0 : 131;
}
