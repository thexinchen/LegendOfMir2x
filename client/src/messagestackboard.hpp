#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include "colorf.hpp"
#include "gui_textengine.hpp"
#include "raiitimer.hpp"

class MessageStackBoard
{
    public:
        struct Margin final
        {
            int top = 0;
            int bottom = 0;
            int left = 0;
            int right = 0;
        };

        struct FontConfig final
        {
            uint8_t id = 11;
            uint8_t size = 15;
            uint8_t style = 0;
            uint32_t color = colorf::WHITE_A255;
        };

        struct InitArgs final
        {
            int width = 0;
            int corner = 0;
            FontConfig font {};
            uint64_t showTime = 0;
            size_t entryLimit = 0;
            int itemSpace = 0;
            Margin margin {};
            uint32_t bgColor = 0;
            uint32_t borderColor = 0;
        };

    private:
        struct Message final
        {
            hres_timer timer;
            std::unique_ptr<XMLTypeset> typeset;
        };

    private:
        int m_width = 0;
        int m_corner = 0;
        FontConfig m_font {};
        uint64_t m_showTime = 0;
        size_t m_entryLimit = 0;
        int m_itemSpace = 0;
        Margin m_margin {};
        uint32_t m_bgColor = 0;
        uint32_t m_borderColor = 0;
        std::deque<std::unique_ptr<Message>> m_messageList;

    public:
        explicit MessageStackBoard(InitArgs);

    public:
        void addMessage(const std::u8string &);
        void addXMLMessage(const std::u8string &);
        void clear();
        void update(double);

    public:
        bool empty() const;
        int w() const;
        int h() const;
        void draw(int, int) const;
};
