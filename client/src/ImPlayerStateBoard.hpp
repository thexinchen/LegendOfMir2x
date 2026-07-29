#pragma once

#include <array>

#include "ImBoard.hpp"
#include "protocoldef.hpp"
#include "sysconst.hpp"

class ProcessRun;
class ImPlayerStateBoard final: public ImBoard
{
    private:
        struct WearGrid
        {
            int x = 0;
            int y = 0;
            int w = SYS_INVGRIDPW;
            int h = SYS_INVGRIDPH;
        };

    private:
        ProcessRun *m_processRun;
        const std::array<WearGrid, WLG_END> m_gridList;

    public:
        explicit ImPlayerStateBoard(ProcessRun *);

    public:
        void draw() const override;
        bool processEvent(const MirEvent &) const override;
};
