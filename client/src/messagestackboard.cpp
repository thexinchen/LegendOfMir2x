#include <algorithm>

#include <imgui.h>

#include "gldevice.hpp"
#include "messagestackboard.hpp"
#include "xmlf.hpp"

extern GLDevice *g_glDevice;

MessageStackBoard::MessageStackBoard(MessageStackBoard::InitArgs args)
    : m_width(args.width)
    , m_corner(args.corner)
    , m_font(args.font)
    , m_showTime(args.showTime)
    , m_entryLimit(args.entryLimit)
    , m_itemSpace(args.itemSpace)
    , m_margin(args.margin)
    , m_bgColor(args.bgColor)
    , m_borderColor(args.borderColor)
{}

void MessageStackBoard::addMessage(const std::u8string &text)
{
    const auto xml = xmlf::toParString("%s", text.empty() ? "" : reinterpret_cast<const char *>(text.c_str()));
    addXMLMessage(to_u8rawstr(xml));
}

void MessageStackBoard::addXMLMessage(const std::u8string &xml)
{
    while((m_entryLimit > 0) && (m_messageList.size() >= m_entryLimit)){
        m_messageList.pop_front();
    }

    auto message = std::make_unique<Message>();
    message->typeset = std::make_unique<XMLTypeset>(
        m_width,
        LALIGN_LEFT,
        false,
        false,
        m_font.id,
        m_font.size,
        m_font.style,
        m_font.color);
    message->typeset->loadXML(reinterpret_cast<const char *>(xml.c_str()));
    m_messageList.push_back(std::move(message));
}

void MessageStackBoard::clear()
{
    m_messageList.clear();
}

void MessageStackBoard::update(double)
{
    if(m_showTime > 0){
        while(!m_messageList.empty() && m_messageList.front()->timer.diff_msec() >= m_showTime){
            m_messageList.pop_front();
        }
    }
    for(const auto &message: m_messageList){
        message->typeset->update(0.0);
    }
}

bool MessageStackBoard::empty() const
{
    return m_messageList.empty();
}

int MessageStackBoard::w() const
{
    int width = 0;
    for(const auto &message: m_messageList){
        width = std::max(width, message->typeset->pw() + m_margin.left + m_margin.right);
    }
    return width;
}

int MessageStackBoard::h() const
{
    int height = 0;
    for(const auto &message: m_messageList){
        height += message->typeset->ph() + m_margin.top + m_margin.bottom;
    }
    if(!m_messageList.empty()){
        height += (to_d(m_messageList.size()) - 1) * m_itemSpace;
    }
    return height;
}

void MessageStackBoard::draw(int x, int y) const
{
    auto *drawList = ImGui::GetBackgroundDrawList();
    int drawY = y;
    for(const auto &message: m_messageList){
        const int boxW = message->typeset->pw() + m_margin.left + m_margin.right;
        const int boxH = message->typeset->ph() + m_margin.top + m_margin.bottom;
        const int corner = std::min({m_corner, boxW / 2, boxH / 2});
        if(colorf::A(m_bgColor)){
            g_glDevice->fillRectangle(m_bgColor, x, drawY, boxW, boxH, corner);
        }
        message->typeset->drawImGui(drawList, x + m_margin.left, drawY + m_margin.top);
        if(colorf::A(m_borderColor)){
            g_glDevice->drawRectangle(m_borderColor, x, drawY, boxW, boxH, corner);
        }
        drawY += boxH + m_itemSpace;
    }
}
