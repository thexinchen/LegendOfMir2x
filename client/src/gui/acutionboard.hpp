#pragma once

// ===== merged from acutionboard/acutionboard.hpp =====
#include "widget.hpp"
#include "imageboard.hpp"

class ProcessRun;
class AcutionBoard: public Widget
{
    private:
        ProcessRun *m_runProc;

    private:
        ImageBoard m_background;

    public:
        AcutionBoard(ProcessRun *, Widget * = nullptr, bool = false);
};
