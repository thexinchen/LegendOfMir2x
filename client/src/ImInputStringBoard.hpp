#pragma once

#include <array>
#include <functional>
#include <string>

#include "ImBoard.hpp"

class ImInputStringBoard final: public ImBoard
{
    private:
        mutable std::array<char, 256> m_input {};
        mutable bool m_security = false;
        mutable bool m_requestFocus = false;
        mutable bool m_inputActive = false;
        mutable std::u8string m_title;
        mutable std::function<void(std::u8string)> m_onDone;

    public:
        ImInputStringBoard();

    public:
        void draw() const override;
        bool processEvent(const MirEvent &) const override;

    public:
        void clear() const;
        bool focus() const { return show() && m_inputActive; }
        void waitInput(std::u8string, bool, std::function<void(std::u8string)>);

    private:
        void inputLineDone() const;
};
