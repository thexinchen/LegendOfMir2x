#include "ImQuestStateBoard.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "fflerror.hpp"
#include "gldevice.hpp"
#include "gui_font.hpp"
#include "gui_texture.hpp"
#include "processrun.hpp"
#include "strf.hpp"
#include "totype.hpp"

extern PNGTexDB *g_progUseDB;
extern FontexDB *g_fontexDB;
extern GLDevice *g_glDevice;

namespace
{
    constexpr float despX = 40.0f;
    constexpr float despY = 100.0f;
    constexpr float despW = 270.0f;
    constexpr float despH = 300.0f;
    constexpr float lineH = 17.0f;

    GLTexID gameTextTexture(const char *text)
    {
        return text && *text ? g_fontexDB->retrieve(1, 12, 0, text) : nullptr;
    }

    float gameTextWidth(const std::string &text)
    {
        if(const auto texture = gameTextTexture(text.c_str()); texture){
            return to_f(texture.w);
        }
        return 0.0f;
    }

    std::vector<std::string> wrapText(const std::string &text, float maxWidth)
    {
        std::vector<std::string> lines;
        std::string line;
        for(size_t i = 0; i < text.size();){
            const size_t charBytes = (static_cast<unsigned char>(text[i]) < 0X80) ? 1 :
                                     ((static_cast<unsigned char>(text[i]) & 0XE0) == 0XC0) ? 2 :
                                     ((static_cast<unsigned char>(text[i]) & 0XF0) == 0XE0) ? 3 : 4;
            const auto glyph = text.substr(i, std::min(charBytes, text.size() - i));
            if(glyph == "\n"){
                lines.push_back(line);
                line.clear();
            }
            else{
                const auto candidate = line + glyph;
                if(!line.empty() && gameTextWidth(candidate) > maxWidth){
                    lines.push_back(line);
                    line = glyph;
                }
                else{
                    line = candidate;
                }
            }
            i += charBytes;
        }
        if(!line.empty() || lines.empty()){
            lines.push_back(line);
        }
        return lines;
    }

    bool textureButton(const char *id, uint32_t offID, uint32_t downID, ImVec2 pos)
    {
        const auto off = g_progUseDB->retrieve(offID);
        if(!off){
            return false;
        }
        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {to_f(off.w), to_f(off.h)});
        const auto down = g_progUseDB->retrieve(downID);
        const auto shown = ImGui::IsItemActive() && down ? down : off;
        ImGui::GetWindowDrawList()->AddImage(shown, pos, {pos.x + shown.w, pos.y + shown.h});
        if(clicked){ playButtonClickSound(); }
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
            ImGui::GetWindowDrawList()->AddImage(shown, pos, {pos.x + shown.w, pos.y + shown.h});
        }
        if(clicked){ playButtonClickSound(); }
        return clicked;
    }
}

ImQuestStateBoard::ImQuestStateBoard(ProcessRun *processRun)
    : ImBoard("##quest-state-board")
    , m_processRun(fflcheck(processRun))
{
    moveTo(
        to_f(g_glDevice->getRendererWidth()  / 2 - 145),
        to_f(g_glDevice->getRendererHeight() / 2 - 223));
}

void ImQuestStateBoard::draw() const
{
    if(!show()){
        return;
    }

    const auto background = g_progUseDB->retrieve(0X00000350);
    if(!background){
        return;
    }

    if(beginWindow({to_f(background.w), to_f(background.h)})){
        const auto pos = ImGui::GetWindowPos();
        auto *drawList = ImGui::GetWindowDrawList();
        drawList->AddImage(background, pos, {pos.x + background.w, pos.y + background.h});

        textureButton("##quest-lr", 0X00000300, 0X00000302, {pos.x + 315, pos.y + 76});
        if(overlayButton("##quest-close", 0X0000001C, 0X0000001D, {pos.x + 316, pos.y + 108})){
            setShow(false);
        }

        struct DisplayLine
        {
            std::string text;
            std::string quest;
            bool heading = false;
        };
        std::vector<DisplayLine> lines;
        for(const auto &[quest, state]: m_questDesp){
            lines.push_back({quest, quest, true});
            if(!state.folded){
                const auto mainDesp = (state.desp.contains(SYS_QSTFSM) && str_haschar(state.desp.at(SYS_QSTFSM)))
                    ? state.desp.at(SYS_QSTFSM)
                    : "暂无任务描述";
                for(auto &line: wrapText("    " + mainDesp, despW)){
                    lines.push_back({std::move(line), {}, false});
                }
                for(const auto &[fsm, desp]: state.desp){
                    if(fsm == SYS_QSTFSM){
                        continue;
                    }
                    for(auto &line: wrapText("    * " + fsm, despW)){
                        lines.push_back({std::move(line), {}, false});
                    }
                    for(auto &line: wrapText("      " + (str_haschar(desp) ? desp : "暂无任务描述"), despW)){
                        lines.push_back({std::move(line), {}, false});
                    }
                }
            }
        }

        const float contentH = std::max(despH, to_f(lines.size()) * lineH + 5.0f);
        const float maxOffset = std::max(0.0f, contentH - despH);
        ImGui::SetCursorScreenPos({pos.x + despX, pos.y + despY});
        ImGui::InvisibleButton("##quest-description", {despW, despH});
        if(ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0.0f && maxOffset > 0.0f){
            m_scrollValue = std::clamp(m_scrollValue - ImGui::GetIO().MouseWheel * lineH / maxOffset, 0.0f, 1.0f);
        }

        const float offsetY = m_scrollValue * maxOffset;
        drawList->PushClipRect({pos.x + despX, pos.y + despY}, {pos.x + despX + despW, pos.y + despY + despH}, true);
        for(size_t i = 0; i < lines.size(); ++i){
            const float y = pos.y + despY + to_f(i) * lineH - offsetY;
            if(y + lineH < pos.y + despY || y >= pos.y + despY + despH){
                continue;
            }
            const auto &line = lines.at(i);
            if(line.heading){
                ImGui::SetCursorScreenPos({pos.x + despX, y});
                ImGui::PushID(to_d(i));
                if(ImGui::InvisibleButton("##quest-heading", {despW, lineH})){
                    m_questDesp.at(line.quest).folded = !m_questDesp.at(line.quest).folded;
                }
                ImGui::PopID();
            }
            if(const auto text = gameTextTexture(line.text.c_str()); text){
                const ImVec2 textPos {pos.x + despX, y};
                drawList->AddImage(text, textPos, {textPos.x + text.w, textPos.y + text.h});
            }
        }
        drawList->PopClipRect();

        constexpr float barX = 326.0f;
        constexpr float barY = 160.0f;
        constexpr float barH = 214.0f;
        ImGui::SetCursorScreenPos({pos.x + barX - 10.0f, pos.y + barY - 12.0f});
        ImGui::InvisibleButton("##quest-slider", {29.0f, barH + 24.0f});
        if(ImGui::IsItemActive()){
            m_scrollValue = std::clamp((ImGui::GetIO().MousePos.y - (pos.y + barY)) / (barH - 1.0f), 0.0f, 1.0f);
        }
        if(const auto slider = g_progUseDB->retrieve(0X00000089); slider){
            const ImVec2 sliderPos {pos.x + barX - 5.5f, pos.y + barY + m_scrollValue * (barH - 1.0f) - 12.0f};
            drawList->AddImage(slider, sliderPos, {sliderPos.x + slider.w, sliderPos.y + slider.h}, {0, 0}, {1, 1},
                    ImGui::IsItemActive() ? IM_COL32_WHITE : IM_COL32(128, 128, 128, 255));
        }
    }
    endWindow();
}

bool ImQuestStateBoard::processEvent(const MirEvent &event) const
{
    if(show() && event.type == MIR_EVENT_KEY_DOWN && event.key.key == MIRK_ESCAPE){
        setShow(false);
        return true;
    }
    return ImBoard::processEvent(event);
}

void ImQuestStateBoard::updateQuestDesp(SDQuestDespUpdate update)
{
    if(update.desp.has_value()){
        m_questDesp[update.name].desp[update.fsm] = update.desp.value();
    }
    else if(update.fsm == SYS_QSTFSM){
        m_questDesp.erase(update.name);
    }
    else{
        m_questDesp[update.name].desp.erase(update.fsm);
    }

    if(!show()){
        m_processRun->getMainUI()->startButtonBlink("Quest");
    }
}

void ImQuestStateBoard::setQuestDesp(SDQuestDespList list)
{
    m_questDesp.clear();
    for(const auto &[quest, desps]: list){
        for(const auto &[fsm, desp]: desps){
            m_questDesp[quest].desp[fsm] = desp;
        }
    }
    m_scrollValue = 0.0f;
}
