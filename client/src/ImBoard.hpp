#pragma once

#include <string>

#include <imgui.h>

#include "mirevent.hpp"

class ImBoard
{
    private:
        std::string m_windowID;

        mutable bool m_show = false;
        mutable bool m_positioned = false;
        mutable ImVec2 m_position {};
        mutable ImVec2 m_size {};

    protected:
        explicit ImBoard(std::string, bool = false);

    protected:
        bool beginWindow(ImVec2) const;
        void endWindow() const;

    public:
        virtual ~ImBoard() = default;

    public:
        virtual void draw() const = 0;
        virtual void update(double) {}
        virtual bool processEvent(const MirEvent &) const;

    public:
        bool show() const { return m_show; }
        void setShow(bool value) const { m_show = value; }
        void flipShow() const { m_show = !m_show; }

    public:
        ImVec2 position() const { return m_position; }
        ImVec2 size() const { return m_size; }
};
