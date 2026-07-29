#pragma once

#include "ImBoard.hpp"

class ProcessRun;

class ImHorseBoard final: public ImBoard
{
    private:
        ProcessRun *m_processRun = nullptr;

    public:
        explicit ImHorseBoard(ProcessRun *);

    public:
        void draw() const override;
        bool processEvent(const MirEvent &) const override;
};
