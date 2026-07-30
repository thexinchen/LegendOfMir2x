#include "imeboard.hpp"

#include <algorithm>
#include <array>

#include "colorf.hpp"
#include "gldevice.hpp"
#include "gui_font.hpp"
#include "gui_texture.hpp"
#include "strf.hpp"
#include "totype.hpp"

extern PNGTexDB *g_progUseDB;
extern FontexDB *g_fontexDB;
extern GLDevice *g_glDevice;

namespace
{
    constexpr uint8_t fontID = 11;
    constexpr uint8_t fontSize = 15;

    void drawNineSlice(
            ImDrawList *drawList,
            const GLTexID texture,
            const ImVec2 pos,
            const ImVec2 size,
            const float left,
            const float top,
            const float centerW,
            const float centerH)
    {
        if(!texture){
            return;
        }

        const float right = texture.w - left - centerW;
        const float bottom = texture.h - top - centerH;
        const std::array<float, 4> sourceX {{0, left, left + centerW, to_f(texture.w)}};
        const std::array<float, 4> sourceY {{0, top, top + centerH, to_f(texture.h)}};
        const std::array<float, 4> targetX {{pos.x, pos.x + left, pos.x + size.x - right, pos.x + size.x}};
        const std::array<float, 4> targetY {{pos.y, pos.y + top, pos.y + size.y - bottom, pos.y + size.y}};
        for(int y = 0; y < 3; ++y){
            for(int x = 0; x < 3; ++x){
                if(targetX[x + 1] <= targetX[x] || targetY[y + 1] <= targetY[y]){
                    continue;
                }
                drawList->AddImage(
                    texture,
                    {targetX[x], targetY[y]},
                    {targetX[x + 1], targetY[y + 1]},
                    {sourceX[x] / texture.w, sourceY[y] / texture.h},
                    {sourceX[x + 1] / texture.w, sourceY[y + 1] / texture.h});
            }
        }
    }

    void drawText(ImDrawList *drawList, const ImVec2 pos, const std::string &text, const ImU32 color)
    {
        if(const auto texture = g_fontexDB->retrieve(fontID, fontSize, 0, text.c_str()); texture){
            drawList->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h}, {0, 0}, {1, 1}, color);
        }
    }
}

IMEBoard::IMEBoard()
    : ImBoard("##embedded-ime-board")
{
    moveTo(0.0f, 0.0f);
}

int IMEBoard::fontHeight() const
{
    if(const auto texture = g_fontexDB->retrieve(fontID, fontSize, 0, " "); texture){
        return texture.h;
    }
    return fontSize;
}

int IMEBoard::candidateWidth(const size_t index) const
{
    if(index >= m_candidateList.size()){
        return 0;
    }
    const auto label = str_printf("%zu. %s", index + 1 - m_startIndex, m_candidateList[index].c_str());
    if(const auto texture = g_fontexDB->retrieve(fontID, fontSize, 0, label.c_str()); texture){
        return texture.w;
    }
    return 0;
}

int IMEBoard::totalCandidateWidth() const
{
    int width = 0;
    for(size_t i = m_startIndex; i < std::min(m_startIndex + 9, m_candidateList.size()); ++i){
        if(width){
            width += candidateSpace;
        }
        width += candidateWidth(i);
    }
    return width;
}

void IMEBoard::draw() const
{
    if(!show()){
        return;
    }

    const auto background = g_progUseDB->retrieve(0X09000100);
    const auto upperLeft = g_progUseDB->retrieve(0X09000006);
    const auto lowerRight = g_progUseDB->retrieve(0X09000009);
    const int tokenHeight = fontHeight();
    m_boardSize =
    {
        to_f(std::max<int>(background ? background.w : 338, startX * 2 + totalCandidateWidth())),
        to_f(std::max<int>(background ? background.h : 53, startY * 2 + tokenHeight * 2 + separatorSpace)),
    };

    if(beginWindow(m_boardSize)){
        const auto pos = ImGui::GetWindowPos();
        auto drawList = ImGui::GetWindowDrawList();

        const float left = upperLeft ? upperLeft.w : 12.0f;
        const float right = lowerRight ? lowerRight.w : 12.0f;
        drawNineSlice(
            drawList,
            background,
            pos,
            m_boardSize,
            left,
            10,
            std::max(1.0f, background ? background.w - left - right : 314.0f),
            std::max(1.0f, background ? background.h - 20.0f : 33.0f));
        if(upperLeft){
            drawList->AddImage(upperLeft, pos, {pos.x + upperLeft.w, pos.y + upperLeft.h});
        }
        if(lowerRight){
            drawList->AddImage(
                lowerRight,
                {pos.x + m_boardSize.x - lowerRight.w, pos.y + m_boardSize.y - lowerRight.h},
                {pos.x + m_boardSize.x, pos.y + m_boardSize.y});
        }

        drawList->AddLine(
            {pos.x, pos.y + startY + tokenHeight + separatorSpace * 0.5f},
            {pos.x + m_boardSize.x - 1, pos.y + startY + tokenHeight + separatorSpace * 0.5f},
            IM_COL32(255, 255, 0, 48));
        drawText(drawList, {pos.x + startX, pos.y + startY}, m_ime.result(), IM_COL32(255, 255, 0, 255));

        float candidateX = pos.x + startX;
        const float candidateY = pos.y + startY + tokenHeight + separatorSpace;
        for(size_t i = m_startIndex; i < std::min(m_startIndex + 9, m_candidateList.size()); ++i){
            const auto label = str_printf("%zu. %s", i + 1 - m_startIndex, m_candidateList[i].c_str());
            const int width = candidateWidth(i);
            ImGui::PushID(to_d(i));
            ImGui::SetCursorScreenPos({candidateX, candidateY});
            if(ImGui::InvisibleButton("##ime-candidate", {to_f(width), to_f(tokenHeight)})){
                selectCandidate(i);
            }
            const auto color = ImGui::IsItemActive() ? IM_COL32(0, 0, 255, 255)
                             : ImGui::IsItemHovered() ? IM_COL32(255, 0, 0, 255)
                                                     : IM_COL32(255, 255, 0, 255);
            drawText(drawList, {candidateX, candidateY}, label, color);
            candidateX += width + candidateSpace;
            ImGui::PopID();
        }

        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()){
            m_dragging = true;
        }
        if(!ImGui::IsMouseDown(ImGuiMouseButton_Left)){
            m_dragging = false;
        }
        if(m_dragging){
            const auto delta = ImGui::GetIO().MouseDelta;
            moveTo(
                std::clamp(pos.x + delta.x, 0.0f, std::max(0.0f, to_f(g_glDevice->getRendererWidth()) - m_boardSize.x)),
                std::clamp(pos.y + delta.y, 0.0f, std::max(0.0f, to_f(g_glDevice->getRendererHeight()) - m_boardSize.y)));
        }
    }
    endWindow();
}

void IMEBoard::update(const double)
{
    const auto candidates = m_ime.candidates();
    if(candidates == m_candidateList){
        if(m_ime.empty()){
            dropFocus();
        }
        else if(m_ime.done()){
            if(m_onCommit){
                m_onCommit(m_ime.result());
            }
            dropFocus();
        }
        return;
    }

    m_candidateList = candidates;
    m_startIndex = 0;
}

bool IMEBoard::processEvent(const MirEvent &event) const
{
    if(!show()){
        return false;
    }

    switch(event.type){
        case MIR_EVENT_KEY_DOWN:
            {
                switch(event.key.key){
                    case MIRK_UP:
                    case MIRK_LEFT:
                    case MIRK_PAGEUP:
                        {
                            auto self = const_cast<IMEBoard *>(this);
                            self->m_startIndex = m_startIndex >= 9 ? m_startIndex - 9 : 0;
                            return true;
                        }
                    case MIRK_DOWN:
                    case MIRK_RIGHT:
                    case MIRK_PAGEDOWN:
                        {
                            if(m_startIndex + 9 < m_candidateList.size()){
                                const_cast<IMEBoard *>(this)->m_startIndex += 9;
                            }
                            return true;
                        }
                    case MIRK_RETURN:
                        {
                            if(m_onCommit){
                                m_onCommit(m_ime.result());
                            }
                            const_cast<IMEBoard *>(this)->dropFocus();
                            return true;
                        }
                    case MIRK_BACKSPACE:
                        {
                            m_ime.backspace();
                            return true;
                        }
                    case MIRK_ESCAPE:
                        {
                            const_cast<IMEBoard *>(this)->dropFocus();
                            return true;
                        }
                    case MIRK_SPACE:
                        {
                            selectCandidate(m_startIndex);
                            return true;
                        }
                    default:
                        {
                            const char keyChar = GLDeviceHelper::getKeyChar(event, true);
                            if(keyChar >= 'a' && keyChar <= 'z'){
                                m_ime.feed(keyChar);
                            }
                            else if(keyChar >= '1' && keyChar <= '9'){
                                selectCandidate(m_startIndex + keyChar - '1');
                            }
                            else if(keyChar != '\0'){
                                if(m_onCommit){
                                    m_onCommit(m_ime.result());
                                    m_onCommit(str_printf("%c", keyChar));
                                }
                                const_cast<IMEBoard *>(this)->dropFocus();
                            }
                            return true;
                        }
                }
            }
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
            {
                if(event.button.x < position().x || event.button.x >= position().x + m_boardSize.x
                || event.button.y < position().y || event.button.y >= position().y + m_boardSize.y){
                    const_cast<IMEBoard *>(this)->dropFocus();
                }
                return true;
            }
        case MIR_EVENT_MOUSE_BUTTON_UP:
        case MIR_EVENT_MOUSE_MOTION:
            {
                return true;
            }
        default:
            {
                return true;
            }
    }
}

void IMEBoard::gainFocus(std::string prefix, std::string input, std::function<void(std::string)> onCommit)
{
    m_candidateList.clear();
    m_startIndex = 0;
    m_ime.assign(std::move(prefix), std::move(input));
    m_onCommit = std::move(onCommit);
    setShow(true);
}

void IMEBoard::dropFocus()
{
    m_candidateList.clear();
    m_startIndex = 0;
    m_ime.clear();
    m_onCommit = nullptr;
    setShow(false);
}

void IMEBoard::selectCandidate(const size_t index) const
{
    if(index < m_candidateList.size()){
        m_ime.select(index);
    }
}
