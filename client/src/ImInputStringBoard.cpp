#include "ImInputStringBoard.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include "gldevice.hpp"
#include "gui_font.hpp"
#include "gui_texture.hpp"
#include "totype.hpp"

extern PNGTexDB *g_progUseDB;
extern FontexDB *g_fontexDB;
extern GLDevice *g_glDevice;

namespace
{
    std::string plainLayoutText(const std::u8string &layout)
    {
        const std::string source = to_cstr(layout);
        std::string result;
        result.reserve(source.size());

        bool inTag = false;
        for(size_t i = 0; i < source.size(); ++i){
            if(source[i] == '<'){
                if(source.compare(i, 4, "<br>") == 0 || source.compare(i, 5, "<br/>") == 0){
                    result.push_back('\n');
                }
                inTag = true;
                continue;
            }
            if(source[i] == '>'){
                inTag = false;
                continue;
            }
            if(!inTag){
                result.push_back(source[i]);
            }
        }
        return result;
    }

    GLTexID gameTextTexture(const char *text, uint8_t font, uint8_t size)
    {
        return text && *text ? g_fontexDB->retrieve(font, size, 0, text) : nullptr;
    }

    void drawCenteredGameText(ImDrawList *drawList, ImVec2 center, const char *text, uint8_t font, uint8_t size)
    {
        if(const auto texture = gameTextTexture(text, font, size); texture){
            const ImVec2 pos {center.x - texture.w * 0.5f, center.y};
            drawList->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h});
        }
    }

    bool overlayButton(const char *id, uint32_t offID, uint32_t hoverID, uint32_t downID, ImVec2 pos)
    {
        const auto off = g_progUseDB->retrieve(offID);
        if(!off){
            return false;
        }

        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {to_f(off.w), to_f(off.h)});
        auto shown = off;
        if(ImGui::IsItemActive()){
            if(const auto down = g_progUseDB->retrieve(downID); down){
                shown = down;
            }
        }
        else if(ImGui::IsItemHovered()){
            if(const auto hover = g_progUseDB->retrieve(hoverID); hover){
                shown = hover;
            }
        }
        ImGui::GetWindowDrawList()->AddImage(shown, pos, {pos.x + shown.w, pos.y + shown.h});
        return clicked;
    }
}

ImInputStringBoard::ImInputStringBoard()
    : ImBoard("##input-string-board")
{
    moveTo(
        to_f(g_glDevice->getRendererWidth()  / 2 - 179),
        to_f(g_glDevice->getRendererHeight() / 2 - 134));
}

void ImInputStringBoard::draw() const
{
    if(!show()){
        return;
    }

    const auto background = g_progUseDB->retrieve(0X07000000);
    if(!background){
        return;
    }

    if(beginWindow({to_f(background.w), to_f(background.h)})){
        const auto pos = ImGui::GetWindowPos();
        auto *drawList = ImGui::GetWindowDrawList();
        drawList->AddImage(background, pos, {pos.x + background.w, pos.y + background.h});

        const auto title = plainLayoutText(m_title);
        drawCenteredGameText(drawList, {pos.x + background.w * 0.5f, pos.y + 120.0f}, title.c_str(), 1, 12);

        const ImVec2 inputPos {pos.x + 22.0f, pos.y + 225.0f};
        constexpr ImVec2 inputSize {315.0f, 23.0f};
        if(m_inputActive){
            drawList->AddRectFilled(inputPos, {inputPos.x + inputSize.x, inputPos.y + inputSize.y}, IM_COL32(255, 255, 255, 32));
        }

        ImGui::SetCursorScreenPos(inputPos);
        ImGui::SetNextItemWidth(inputSize.x);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, {0, 0, 0, 0});
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, {0, 0, 0, 0});
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, {0, 0, 0, 0});
        ImGui::PushStyleColor(ImGuiCol_Text, {0, 0, 0, 0});
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, {0, 0, 0, 0});
        if(m_requestFocus){
            ImGui::SetKeyboardFocusHere();
            m_requestFocus = false;
        }
        ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue;
        if(m_security){
            inputFlags |= ImGuiInputTextFlags_Password;
        }
        const bool entered = ImGui::InputText("##input-string", m_input.data(), m_input.size(), inputFlags);
        m_inputActive = ImGui::IsItemActive();
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);

        std::string displayText;
        if(m_security){
            displayText.assign(std::char_traits<char>::length(m_input.data()), '*');
        }
        else{
            displayText = m_input.data();
        }
        if(const auto text = gameTextTexture(displayText.c_str(), 1, 14); text){
            drawList->PushClipRect(inputPos, {inputPos.x + inputSize.x, inputPos.y + inputSize.y}, true);
            drawList->AddImage(text, inputPos, {inputPos.x + text.w, inputPos.y + text.h});
            drawList->PopClipRect();
        }

        if(entered || overlayButton("##input-yes", 0X07000001, 0X07000002, 0X07000003, {pos.x + 66, pos.y + 190})){
            inputLineDone();
            setShow(false);
        }
        if(overlayButton("##input-no", 0X07000004, 0X07000005, 0X07000006, {pos.x + 212, pos.y + 190})){
            setShow(false);
            clear();
        }
    }
    endWindow();
}

bool ImInputStringBoard::processEvent(const MirEvent &event) const
{
    if(show() && event.type == MIR_EVENT_KEY_DOWN && event.key.key == MIRK_ESCAPE){
        setShow(false);
        clear();
        return true;
    }
    return ImBoard::processEvent(event);
}

void ImInputStringBoard::clear() const
{
    m_input.fill('\0');
    m_inputActive = false;
}

void ImInputStringBoard::inputLineDone() const
{
    std::string input = m_input.data();
    const auto begin = input.find_first_not_of(" \n\r\t");
    input = begin == std::string::npos ? "" : input.substr(begin);
    clear();

    if(m_onDone){
        m_onDone(to_u8rawstr(input));
    }
}

void ImInputStringBoard::waitInput(std::u8string title, bool security, std::function<void(std::u8string)> onDone)
{
    m_title = std::move(title);
    m_security = security;
    m_onDone = std::move(onDone);
    clear();
    setShow(true);
    m_requestFocus = true;
}
