#include "ImRuntimeConfigBoard.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

#include "audiodevice.hpp"
#include "client.hpp"
#include "fflerror.hpp"
#include "gldevice.hpp"
#include "gui_font.hpp"
#include "gui_texture.hpp"
#include "imeboard.hpp"
#include "processrun.hpp"
#include "serdesmsg.hpp"
#include "strf.hpp"
#include "totype.hpp"

extern Client *g_client;
extern PNGTexDB *g_progUseDB;
extern FontexDB *g_fontexDB;
extern GLDevice *g_glDevice;
extern AudioDevice *g_audioDevice;

namespace
{
    constexpr ImU32 frameColor = IM_COL32(231, 231, 189, 100);

    void drawTexture(ImDrawList *list, const GLTexID texture, const ImVec2 pos, const ImU32 tint = IM_COL32_WHITE)
    {
        if(texture){
            list->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h}, {0, 0}, {1, 1}, tint);
        }
    }

    ImVec2 drawText(ImDrawList *list, const ImVec2 pos, const std::string &text, const uint8_t font = 1, const uint8_t size = 12, const ImU32 color = IM_COL32_WHITE)
    {
        if(const auto texture = g_fontexDB->retrieve(font, size, 0, text.c_str()); texture){
            drawTexture(list, texture, pos, color);
            return {to_f(texture.w), to_f(texture.h)};
        }
        return {};
    }

    void drawFrame(ImDrawList *list, const ImVec2 pos, const ImVec2 size)
    {
        const auto texture = g_progUseDB->retrieve(0X00000450);
        if(!texture){
            return;
        }
        constexpr float edge = 58.0f;
        const std::array<float, 4> sx {{0, edge, to_f(texture.w) - edge, to_f(texture.w)}};
        const std::array<float, 4> sy {{0, edge, to_f(texture.h) - edge, to_f(texture.h)}};
        const std::array<float, 4> dx {{pos.x, pos.x + edge, pos.x + size.x - edge, pos.x + size.x}};
        const std::array<float, 4> dy {{pos.y, pos.y + edge, pos.y + size.y - edge, pos.y + size.y}};
        for(int y = 0; y < 3; ++y){
            for(int x = 0; x < 3; ++x){
                list->AddImage(
                    texture,
                    {dx[x], dy[y]},
                    {dx[x + 1], dy[y + 1]},
                    {sx[x] / texture.w, sy[y] / texture.h},
                    {sx[x + 1] / texture.w, sy[y + 1] / texture.h});
            }
        }
    }

    void drawTiledNineSlice(
            ImDrawList *list,
            const GLTexID texture,
            const ImVec2 pos,
            const ImVec2 size,
            const float left,
            const float top,
            const float centerW,
            const float centerH,
            const ImU32 tint = IM_COL32_WHITE)
    {
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
                const float srcW = sx[x + 1] - sx[x];
                const float srcH = sy[y + 1] - sy[y];
                const float dstW = dx[x + 1] - dx[x];
                const float dstH = dy[y + 1] - dy[y];
                if(srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0){
                    continue;
                }
                for(float tileY = 0; tileY < dstH; tileY += srcH){
                    const float tileH = std::min(srcH, dstH - tileY);
                    for(float tileX = 0; tileX < dstW; tileX += srcW){
                        const float tileW = std::min(srcW, dstW - tileX);
                        list->AddImage(
                            texture,
                            {dx[x] + tileX, dy[y] + tileY},
                            {dx[x] + tileX + tileW, dy[y] + tileY + tileH},
                            {sx[x] / texture.w, sy[y] / texture.h},
                            {(sx[x] + tileW) / texture.w, (sy[y] + tileH) / texture.h},
                            tint);
                    }
                }
            }
        }
    }

    bool textButton(
            const char *id,
            ImDrawList *list,
            const ImVec2 pos,
            const std::string &text,
            const bool selected = false,
            const ImU32 baseColor = IM_COL32_WHITE)
    {
        const auto label = g_fontexDB->retrieve(1, 12, 0, text.c_str());
        if(!label){
            return false;
        }
        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {to_f(label.w), to_f(label.h)});
        const auto color = ImGui::IsItemHovered() ? IM_COL32(255, 0, 0, 255) : baseColor;
        if(selected){
            list->AddRectFilled({pos.x - 4, pos.y - 2}, {pos.x + label.w + 4, pos.y + label.h + 2}, frameColor);
        }
        drawTexture(list, label, pos, color);
        return clicked;
    }

    bool checkControl(const char *id, ImDrawList *list, const ImVec2 pos, const std::string &label, const bool checked)
    {
        const auto text = g_fontexDB->retrieve(1, 12, 0, label.c_str());
        const float width = 16.0f + 8.0f + (text ? to_f(text.w) : 0.0f);
        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {width, 18});
        const ImU32 color = ImGui::IsItemHovered() ? IM_COL32(255, 0, 0, 255) : IM_COL32(231, 231, 189, 128);
        list->AddRect(pos, {pos.x + 16, pos.y + 16}, color);
        list->AddLine({pos.x + 1, pos.y + 15}, {pos.x + 15, pos.y + 15}, IM_COL32(115, 115, 94, 64));
        list->AddLine({pos.x + 15, pos.y + 1}, {pos.x + 15, pos.y + 15}, IM_COL32(115, 115, 94, 64));
        if(checked){
            if(const auto mark = g_progUseDB->retrieve(0X00000480); mark){
                drawTexture(list, mark, {pos.x + (16 - mark.w) * 0.5f, pos.y + (16 - mark.h) * 0.5f});
            }
        }
        if(text){
            drawTexture(list, text, {pos.x + 24, pos.y + (16 - text.h) * 0.5f}, ImGui::IsItemHovered() ? IM_COL32(255, 0, 0, 255) : IM_COL32_WHITE);
        }
        return clicked;
    }

    bool overlayButton(const char *id, const ImVec2 pos, const uint32_t hoverID, const uint32_t downID)
    {
        const auto hover = g_progUseDB->retrieve(hoverID);
        if(!hover){
            return false;
        }
        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {to_f(hover.w), to_f(hover.h)});
        if(ImGui::IsItemHovered() || ImGui::IsItemActive()){
            const auto down = g_progUseDB->retrieve(downID);
            drawTexture(ImGui::GetWindowDrawList(), ImGui::IsItemActive() && down ? down : hover, pos);
        }
        if(clicked){ playButtonClickSound(); }
        return clicked;
    }
}

ImRuntimeConfigBoard::ImRuntimeConfigBoard(const int x, const int y, const int w, const int h, ProcessRun *processRun)
    : ImBoard("##runtime-config-board")
    , m_processRun(fflcheck(processRun))
    , m_boardSize {to_f(w), to_f(h)}
{
    std::strncpy(m_englishPreview.data(), "The quick brown fox jumps over the lazy dog.", m_englishPreview.size() - 1);
    std::strncpy(m_chinesePreview.data(), "快速的棕色狐狸跳过了懒狗。", m_chinesePreview.size() - 1);
    moveTo(to_f(x), to_f(y));
    updateWindowSize(g_glDevice->getRendererSize(), false);
    updateIME(IME_DISABLE, false);
}

void ImRuntimeConfigBoard::draw() const
{
    if(!show()){
        return;
    }
    if(beginWindow(m_boardSize, false)){
        const auto pos = ImGui::GetWindowPos();
        auto *list = ImGui::GetWindowDrawList();
        drawFrame(list, pos, m_boardSize);

        const ImVec2 menuMin {pos.x + 30, pos.y + 30};
        const ImVec2 menuMax {menuMin.x + 80, menuMin.y + m_boardSize.y - 60};
        list->AddRectFilled(menuMin, menuMax, IM_COL32(0, 0, 0, 128), 10);
        list->AddRect(menuMin, menuMax, frameColor, 10);

        const std::array<const char *, 5> mainLabels {{"系 统", "社 交", "网 络", "游 戏", "帮 助"}};
        for(int i = 0; i < to_d(mainLabels.size()); ++i){
            ImGui::PushID(i);
            if(textButton(
                    "##runtime-main-page",
                    list,
                    {menuMin.x + 18, menuMin.y + 135 + i * 30.0f},
                    mainLabels[i],
                    false,
                    IM_COL32(255, 255, 0, 255))){
                m_mainPage = i;
            }
            ImGui::PopID();
        }

        const ImVec2 pagePos {pos.x + 140, pos.y + 40};
        const auto tabs = [&](const std::initializer_list<const char *> labels, int &selected)
        {
            float x = pagePos.x;
            int index = 0;
            for(const auto label: labels){
                ImGui::PushID(index);
                const auto text = g_fontexDB->retrieve(1, 12, 0, label);
                if(textButton("##runtime-tab", list, {x, pagePos.y}, label, selected == index)){
                    selected = index;
                }
                x += (text ? text.w : 0) + 10.0f;
                ImGui::PopID();
                ++index;
            }
            list->AddLine({pagePos.x, pagePos.y + 18}, {pos.x + m_boardSize.x - 50, pagePos.y + 18}, frameColor);
        };

        const auto configCheck = [&]<int Config>(const char *label, const float x, const float y)
        {
            ImGui::PushID(Config);
            const bool value = SDRuntimeConfig_getConfig<Config>(m_config);
            if(checkControl("##runtime-check", list, {pagePos.x + x, pagePos.y + 40 + y}, label, value)){
                SDRuntimeConfig_setConfig<Config>(m_config, !value);
                if constexpr(Config == RTCFG_BGM || Config == RTCFG_SEFF){
                    applyAudioConfig();
                }
                reportRuntimeConfig(Config);
            }
            ImGui::PopID();
        };

        if(m_mainPage == 0){
            tabs({"系统", "外观"}, m_systemTab);
            if(m_systemTab == 0){
                const auto windowSize = m_displayWindowSize;
                drawText(list, {pagePos.x, pagePos.y + 40}, "分辨率");
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(48,48,48,255));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(64,64,64,255));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(80,80,80,255));
                ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(24,24,24,255));
                ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(80,80,64,255));
                ImGui::SetCursorScreenPos({pagePos.x + 45, pagePos.y + 35});
                ImGui::SetNextItemWidth(80);
                const auto windowLabel = str_printf("%d×%d", windowSize.first, windowSize.second);
                if(ImGui::BeginCombo("##runtime-resolution", windowLabel.c_str(), ImGuiComboFlags_NoArrowButton)){
                    for(const auto size: std::array<std::pair<int, int>, 6>{{{800,600},{960,600},{1024,768},{1280,720},{1280,768},{1280,800}}}){
                        const auto label = str_printf("%d×%d", size.first, size.second);
                        if(ImGui::Selectable(label.c_str(), size == windowSize)){
                            const_cast<ImRuntimeConfigBoard *>(this)->updateWindowSize(size, true);
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopStyleColor(5);

                const int ime = m_displayIME;
                drawText(list, {pagePos.x, pagePos.y + 70}, "输入法");
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(48,48,48,255));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(64,64,64,255));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(80,80,80,255));
                ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(24,24,24,255));
                ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(80,80,64,255));
                ImGui::SetCursorScreenPos({pagePos.x + 45, pagePos.y + 65});
                ImGui::SetNextItemWidth(120);
                const std::array<const char *, 3> imeLabels {{"禁用", "使用内置输入法", "使用系统输入法"}};
                if(ImGui::BeginCombo("##runtime-ime", imeLabels.at(ime), ImGuiComboFlags_NoArrowButton)){
                    for(int value = IME_BEGIN; value < IME_END; ++value){
                        if(ImGui::Selectable(imeLabels.at(value), value == ime)){
                            const_cast<ImRuntimeConfigBoard *>(this)->updateIME(value, true);
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopStyleColor(5);

                configCheck.template operator()<RTCFG_FULLSCREEN>("全屏显示", 0, 75);
                configCheck.template operator()<RTCFG_SHOWFPS>("显示FPS", 0, 100);
                configCheck.template operator()<RTCFG_BGM>("背景音乐", 0, 140);
                configCheck.template operator()<RTCFG_SEFF>("动作声效", 0, 200);

                const auto slider = [&](const char *id, const char *label, const float y, const bool active, float value, const int config)
                {
                    drawText(list, {pagePos.x, pagePos.y + 40 + y}, label, 1, 12, active ? IM_COL32_WHITE : IM_COL32(128,128,128,255));
                    const ImVec2 controlPos {pagePos.x + 65, pagePos.y + 43 + y};
                    ImGui::SetCursorScreenPos({controlPos.x, controlPos.y - 5});
                    ImGui::InvisibleButton(id, {84, 18});
                    bool changed = false;
                    if(active && ImGui::IsItemActive()){
                        value = std::clamp((ImGui::GetIO().MousePos.x - controlPos.x - 2.0f) / 80.0f, 0.0f, 1.0f);
                        changed = true;
                    }

                    const ImU32 tint = active ? IM_COL32_WHITE : IM_COL32(128,128,128,255);
                    const auto input = g_progUseDB->retrieve(0X00000460);
                    drawTiledNineSlice(list, input, controlPos, {84, 9}, 3, 3, input ? input.w - 6.0f : 0.0f, 2, tint);
                    if(const auto fill = g_progUseDB->retrieve(0X00000470); fill){
                        const float fillW = 80.0f * value;
                        for(float x = 0; x < fillW; x += fill.w){
                            const float tileW = std::min(to_f(fill.w), fillW - x);
                            list->AddImage(
                                fill,
                                {controlPos.x + 2 + x, controlPos.y + 2},
                                {controlPos.x + 2 + x + tileW, controlPos.y + 2 + fill.h},
                                {0, 0},
                                {tileW / fill.w, 1},
                                tint);
                        }
                    }
                    if(const auto knob = g_progUseDB->retrieve(0X00000081); knob){
                        drawTexture(
                            list,
                            knob,
                            {controlPos.x + 2 + 80.0f * value - 8, controlPos.y + 4.5f - 8},
                            tint);
                    }

                    if(changed){
                        if(config == RTCFG_BGMVALUE){
                            SDRuntimeConfig_setConfig<RTCFG_BGMVALUE>(m_config, value);
                        }
                        else{
                            SDRuntimeConfig_setConfig<RTCFG_SEFFVALUE>(m_config, value);
                        }
                        applyAudioConfig();
                    }
                    if(ImGui::IsItemDeactivatedAfterEdit()){
                        reportRuntimeConfig(config);
                    }
                };
                slider("##runtime-bgm-volume", "音乐音量", 165, SDRuntimeConfig_getConfig<RTCFG_BGM>(m_config), SDRuntimeConfig_getConfig<RTCFG_BGMVALUE>(m_config), RTCFG_BGMVALUE);
                slider("##runtime-seff-volume", "声效音量", 225, SDRuntimeConfig_getConfig<RTCFG_SEFF>(m_config), SDRuntimeConfig_getConfig<RTCFG_SEFFVALUE>(m_config), RTCFG_SEFFVALUE);
            }
            else{
                drawText(list, {pagePos.x, pagePos.y + 40}, "控件");
                drawText(list, {pagePos.x + 140, pagePos.y + 40}, "字体");
                drawText(list, {pagePos.x + 280, pagePos.y + 40}, "字号");
                ImGui::SetCursorScreenPos({pagePos.x, pagePos.y + 58});
                ImGui::SetNextItemWidth(120);
                ImGui::Combo("##runtime-preview-widget", &m_previewWidget, "系统信息\0命令行\0");
                ImGui::SetCursorScreenPos({pagePos.x + 140, pagePos.y + 58});
                ImGui::SetNextItemWidth(130);
                const auto previewFontName = std::get<0>(g_fontexDB->fontName(to_u8(m_previewFont)));
                if(ImGui::BeginCombo("##runtime-preview-font", previewFontName.c_str())){
                    for(uint8_t font = 0, found = 0; found < g_fontexDB->fontCount(); ++font){
                        if(g_fontexDB->hasFont(font)){
                            ++found;
                            const auto [name, style] = g_fontexDB->fontName(font);
                            if(ImGui::Selectable((name + " " + style).c_str(), font == m_previewFont)){
                                m_previewFont = font;
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SetCursorScreenPos({pagePos.x + 280, pagePos.y + 58});
                ImGui::SetNextItemWidth(80);
                ImGui::SliderInt("##runtime-preview-size", &m_previewFontSize, 5, 25);
                list->AddRect({pagePos.x, pagePos.y + 100}, {pagePos.x + 410, pagePos.y + 180}, IM_COL32(128,128,128,255));
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
                ImGui::SetCursorScreenPos({pagePos.x + 4, pagePos.y + 105});
                ImGui::SetNextItemWidth(402);
                ImGui::InputText("##runtime-preview-english", m_englishPreview.data(), m_englishPreview.size());
                ImGui::SetCursorScreenPos({pagePos.x + 4, pagePos.y + 135});
                ImGui::SetNextItemWidth(402);
                ImGui::InputText("##runtime-preview-chinese", m_chinesePreview.data(), m_chinesePreview.size());
                ImGui::PopStyleColor(2);
                drawText(list, {pagePos.x + 5, pagePos.y + 110}, m_englishPreview.data(), to_u8(m_previewFont), to_u8(m_previewFontSize));
                drawText(list, {pagePos.x + 5, pagePos.y + 140}, m_chinesePreview.data(), to_u8(m_previewFont), to_u8(m_previewFontSize));
            }
        }
        else if(m_mainPage == 1){
            tabs({"社交", "好友"}, m_socialTab);
            if(m_socialTab == 0){
#define DRAW_SOCIAL(cfg, label, x, y) configCheck.template operator()<cfg>(label, x, y)
                DRAW_SOCIAL(RTCFG_允许私聊, "允许私聊", 0, 0);
                DRAW_SOCIAL(RTCFG_允许白字聊天, "允许白字聊天", 0, 25);
                DRAW_SOCIAL(RTCFG_允许地图聊天, "允许地图聊天", 0, 50);
                DRAW_SOCIAL(RTCFG_允许行会聊天, "允许行会聊天", 0, 75);
                DRAW_SOCIAL(RTCFG_允许全服聊天, "允许全服聊天", 0, 100);
                DRAW_SOCIAL(RTCFG_允许加入队伍, "允许加入队伍", 200, 0);
                DRAW_SOCIAL(RTCFG_允许加入行会, "允许加入行会", 200, 25);
                DRAW_SOCIAL(RTCFG_允许回生术, "允许回生术", 200, 50);
                DRAW_SOCIAL(RTCFG_允许天地合一, "允许天地合一", 200, 75);
                DRAW_SOCIAL(RTCFG_允许交易, "允许交易", 200, 100);
                DRAW_SOCIAL(RTCFG_允许添加好友, "允许添加好友", 200, 125);
                DRAW_SOCIAL(RTCFG_允许行会召唤, "允许行会召唤", 200, 150);
                DRAW_SOCIAL(RTCFG_允许行会杀人提示, "允许行会杀人提示", 200, 175);
                DRAW_SOCIAL(RTCFG_允许拜师, "允许拜师", 200, 200);
                DRAW_SOCIAL(RTCFG_允许好友上线提示, "允许好友上线提示", 200, 225);
#undef DRAW_SOCIAL
            }
            else{
                drawText(list, {pagePos.x, pagePos.y + 40}, "当加我为好友时：");
                const std::array<const char *, 3> labels {{"允许任何人加我微好友", "拒绝任何人加我为好友", "好友申请验证"}};
                int value = SDRuntimeConfig_getConfig<RTCFG_好友申请>(m_config);
                for(int i = 0; i < 3; ++i){
                    ImGui::PushID(i);
                    if(checkControl("##runtime-friend-radio", list, {pagePos.x, pagePos.y + 65 + i * 25.0f}, labels[i], value == i)){
                        SDRuntimeConfig_setConfig<RTCFG_好友申请>(m_config, i);
                        reportRuntimeConfig(RTCFG_好友申请);
                    }
                    ImGui::PopID();
                }
            }
        }
        else if(m_mainPage == 3){
            tabs({"常用", "辅助", "保护"}, m_gameTab);
            if(m_gameTab == 0){
#define DRAW_GAME(cfg, label, y) configCheck.template operator()<cfg>(label, 0, y)
                DRAW_GAME(RTCFG_强制攻击, "强制攻击", 0);
                DRAW_GAME(RTCFG_显示体力变化, "显示体力变化", 25);
                DRAW_GAME(RTCFG_满血不显血, "满血不显血", 50);
                DRAW_GAME(RTCFG_显示血条, "显示血条", 75);
                DRAW_GAME(RTCFG_数字显血, "数字显血", 100);
                DRAW_GAME(RTCFG_综合数字显示, "综合数字显示", 125);
                DRAW_GAME(RTCFG_标记攻击目标, "标记攻击目标", 150);
                DRAW_GAME(RTCFG_单击解除锁定, "单击解除锁定", 175);
                DRAW_GAME(RTCFG_显示BUFF图标, "显示BUFF图标", 200);
                DRAW_GAME(RTCFG_显示BUFF计时, "显示BUFF计时", 225);
                DRAW_GAME(RTCFG_显示角色名字, "显示角色名字", 250);
                DRAW_GAME(RTCFG_关闭组队血条, "关闭组队血条", 275);
                DRAW_GAME(RTCFG_队友染色, "队友染色", 300);
                DRAW_GAME(RTCFG_显示队友位置, "显示队友位置", 325);
#undef DRAW_GAME
            }
            else if(m_gameTab == 1){
                configCheck.template operator()<RTCFG_持续盾>("持续盾", 0, 0);
                configCheck.template operator()<RTCFG_持续移花接木>("持续移花接木", 0, 25);
                configCheck.template operator()<RTCFG_持续金刚>("持续金刚", 0, 50);
                configCheck.template operator()<RTCFG_持续破血>("持续破血", 0, 100);
                configCheck.template operator()<RTCFG_持续铁布衫>("持续铁布衫", 0, 125);
            }
            else{
                configCheck.template operator()<RTCFG_自动喝红>("自动喝红", 0, 0);
                configCheck.template operator()<RTCFG_保持满血>("保持满血", 0, 25);
                configCheck.template operator()<RTCFG_自动喝蓝>("自动喝蓝", 0, 50);
                configCheck.template operator()<RTCFG_保持满蓝>("保持满蓝", 0, 75);
                drawText(list, {pagePos.x, pagePos.y + 150}, "等待");
                ImGui::SetCursorScreenPos({pagePos.x + 40, pagePos.y + 145});
                ImGui::SetNextItemWidth(50);
                ImGui::InputInt("##runtime-unused-wait", &m_unusedWaitSeconds, 0);
                drawText(list, {pagePos.x + 95, pagePos.y + 150}, "秒");
            }
        }

        if(overlayButton("##runtime-close", {pos.x + m_boardSize.x - 51, pos.y + m_boardSize.y - 53}, 0X0000001C, 0X0000001D)){
            setShow(false);
        }

        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()){
            m_dragging = true;
        }
        if(!ImGui::IsMouseDown(ImGuiMouseButton_Left)){
            m_dragging = false;
        }
        if(m_dragging){
            const auto delta = ImGui::GetIO().MouseDelta;
            moveTo(pos.x + delta.x, pos.y + delta.y);
        }
    }
    endWindow();
}

bool ImRuntimeConfigBoard::processEvent(const MirEvent &event) const
{
    if(!show()){
        return false;
    }
    if(event.type == MIR_EVENT_KEY_DOWN){
        if(event.key.key == MIRK_ESCAPE){
            setShow(false);
            return false;
        }
        return true;
    }
    return ImBoard::processEvent(event);
}

void ImRuntimeConfigBoard::setConfig(const SDRuntimeConfig &config)
{
    m_config = config;
    applyAudioConfig();
    updateWindowSize(SDRuntimeConfig_getConfig<RTCFG_WINDOWSIZE>(m_config), false);
    updateIME(SDRuntimeConfig_getConfig<RTCFG_IME>(m_config), false);
}

void ImRuntimeConfigBoard::applyAudioConfig() const
{
    const float bgm = SDRuntimeConfig_getConfig<RTCFG_BGM>(m_config) ? SDRuntimeConfig_getConfig<RTCFG_BGMVALUE>(m_config) : 0.0f;
    const float seff = SDRuntimeConfig_getConfig<RTCFG_SEFF>(m_config) ? SDRuntimeConfig_getConfig<RTCFG_SEFFVALUE>(m_config) : 0.0f;
    g_audioDevice->setBGMVolume(bgm);
    g_audioDevice->setSoundEffectVolume(seff);
}

void ImRuntimeConfigBoard::reportRuntimeConfig(const int config) const
{
    fflassert(config >= RTCFG_BEGIN && config < RTCFG_END, config);
    CMSetRuntimeConfig message {};
    message.type = config;
    message.buf.assign(m_config.getConfig(config).value_or(std::string()));
    g_client->send({CM_SETRUNTIMECONFIG, message});
}

void ImRuntimeConfigBoard::updateWindowSize(const std::pair<int, int> size, const bool saveConfig)
{
    fflassert(size.first >= 0 && size.second >= 0, size);
    m_displayWindowSize = size;
    g_glDevice->setWindowSize(size.first, size.second);
    if(saveConfig){
        SDRuntimeConfig_setConfig<RTCFG_WINDOWSIZE>(m_config, size);
        reportRuntimeConfig(RTCFG_WINDOWSIZE);
    }
}

void ImRuntimeConfigBoard::updateIME(const int ime, const bool saveConfig)
{
    fflassert(ime >= IME_BEGIN && ime < IME_END, ime);
    m_displayIME = ime;
    if(saveConfig){
        SDRuntimeConfig_setConfig<RTCFG_IME>(m_config, ime);
        reportRuntimeConfig(RTCFG_IME);
    }
}
