#pragma once
#include <vector>
#include <cstdint>
#include "gui_core.hpp"
#include "gui_widgets.hpp"
#include "layoutboard.hpp"

class InputStringBoard: public Widget
{
    private:
        std::function<void(std::u8string)> m_onDone;

    private:
        ImageBoard m_bg;

    private:
        LayoutBoard m_textInfo;

    private:
        GfxShapeBoard m_inputBg;
        PasswordBox    m_input;

    private:
        TritexButton m_yesButton;
        TritexButton m_nopButton;

    public:
        InputStringBoard(
                Widget::VarDir,
                Widget::VarInt,
                Widget::VarInt,

                bool,

                Widget * = nullptr,
                bool     = false);

    private:
        void inputLineDone();

    public:
        void clear()
        {
            m_input.clear();
        }

    public:
        void waitInput(std::u8string, bool, std::function<void(std::u8string)>);
};
