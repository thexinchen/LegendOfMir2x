#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ImBoard.hpp"
#include "ime.hpp"
#include "protocoldef.hpp"

class Widget;
class IMEBoard final: public ImBoard
{
    private:
        mutable IME m_ime;
        bool m_active = true;

        size_t m_startIndex = 0;
        std::vector<std::string> m_candidateList;

        Widget *m_inputWidget = nullptr;
        std::function<void(std::string)> m_onCommit;

        mutable bool m_dragging = false;
        mutable ImVec2 m_boardSize {};

    private:
        static constexpr int startX = 12;
        static constexpr int startY = 10;
        static constexpr int separatorSpace = 4;
        static constexpr int candidateSpace = 5;

    public:
        IMEBoard();

    public:
        void draw() const override;
        void update(double) override;
        bool processEvent(const MirEvent &) const override;

    public:
        bool active() const noexcept { return m_active; }
        void gainFocus(std::string, std::string, Widget *, std::function<void(std::string)>);
        void dropFocus();

    private:
        int fontHeight() const;
        int candidateWidth(size_t) const;
        int totalCandidateWidth() const;
        void selectCandidate(size_t) const;
};
