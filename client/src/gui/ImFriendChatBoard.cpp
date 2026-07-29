#include "ImFriendChatBoard.hpp"

#include "totype.hpp"

ImFriendChatBoard::ImFriendChatBoard(const int x, const int y, ProcessRun *processRun)
    : ImBoard("##friend-chat-board")
    , m_content(x, y, processRun)
{
    moveTo(to_f(x), to_f(y));
}

void ImFriendChatBoard::draw() const
{
    if(!show()){
        return;
    }

    const ImVec2 boardSize {to_f(m_content.w()), to_f(m_content.h())};
    if(beginWindow(boardSize)){
        const auto pos = ImGui::GetWindowPos();
        m_content.moveTo(to_dround(pos.x), to_dround(pos.y));
        m_content.drawRoot({});
    }
    endWindow();
}

void ImFriendChatBoard::update(const double elapsedMS)
{
    m_content.update(elapsedMS);
}

bool ImFriendChatBoard::processEvent(const MirEvent &event) const
{
    if(!show()){
        return false;
    }

    const bool tookEvent = m_content.processEventRoot(event, true, {});
    moveTo(to_f(m_content.dx()), to_f(m_content.dy()));
    if(!m_content.show()){
        ImBoard::setShow(false);
    }
    return tookEvent || ImBoard::processEvent(event);
}

void ImFriendChatBoard::setShow(const bool value) const
{
    ImBoard::setShow(value);
    m_content.setShow(value);
}

void ImFriendChatBoard::flipShow() const
{
    setShow(!show());
}
