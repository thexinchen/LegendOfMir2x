#pragma once

#include "ImBoard.hpp"

class ProcessRun;

class ImGuildBoard final: public ImBoard
{
    private:
        ProcessRun *m_processRun = nullptr;
        mutable float m_scrollValue = 0.0f;

    public:
        explicit ImGuildBoard(ProcessRun *);

    public:
        void draw() const override;
        bool processEvent(const MirEvent &) const override;
};
