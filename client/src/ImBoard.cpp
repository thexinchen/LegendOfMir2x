#include "ImBoard.hpp"

#include <algorithm>

#include "gldevice.hpp"
#include "totype.hpp"

extern GLDevice *g_glDevice;

ImBoard::ImBoard(std::string windowID, bool initialShow)
    : m_windowID(std::move(windowID))
    , m_show(initialShow)
{}

bool ImBoard::beginWindow(ImVec2 windowSize) const
{
    if(!m_positioned){
        m_position =
        {
            std::max(0.0f, (to_f(g_glDevice->getRendererWidth()) - windowSize.x) * 0.5f),
            std::max(0.0f, (to_f(g_glDevice->getRendererHeight()) - windowSize.y) * 0.5f),
        };
        m_positioned = true;
    }

    m_position.x = std::clamp(m_position.x, 0.0f, std::max(0.0f, to_f(g_glDevice->getRendererWidth()) - windowSize.x));
    m_position.y = std::clamp(m_position.y, 0.0f, std::max(0.0f, to_f(g_glDevice->getRendererHeight()) - windowSize.y));

    ImGui::SetNextWindowPos(m_position, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    const bool open = ImGui::Begin(
        m_windowID.c_str(),
        nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBackground);
    m_position = ImGui::GetWindowPos();
    m_size = ImGui::GetWindowSize();
    return open;
}

void ImBoard::endWindow() const
{
    ImGui::End();
    ImGui::PopStyleVar();
}

bool ImBoard::processEvent(const MirEvent &event) const
{
    if(!show()){
        return false;
    }

    switch(event.type){
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
        case MIR_EVENT_MOUSE_BUTTON_UP:
            {
                return event.button.x >= m_position.x &&
                       event.button.x <  m_position.x + m_size.x &&
                       event.button.y >= m_position.y &&
                       event.button.y <  m_position.y + m_size.y;
            }
        case MIR_EVENT_MOUSE_MOTION:
            {
                return event.motion.x >= m_position.x &&
                       event.motion.x <  m_position.x + m_size.x &&
                       event.motion.y >= m_position.y &&
                       event.motion.y <  m_position.y + m_size.y;
            }
        default:
            {
                return false;
            }
    }
}
