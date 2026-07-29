#pragma once

#include "ImBoard.hpp"

class ProcessRun;

class ImAcutionBoard final: public ImBoard
{
    private:
        ProcessRun *m_processRun = nullptr;

    public:
        explicit ImAcutionBoard(ProcessRun *);

    public:
        void draw() const override;
};
