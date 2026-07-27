#pragma once

// ===== merged from acutionboard/acutionboard.hpp =====
#include "gui_core.hpp"
#include "gui_widgets.hpp"

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
