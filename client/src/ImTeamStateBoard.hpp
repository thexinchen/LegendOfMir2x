#pragma once

#include <deque>
#include <utility>

#include "ImBoard.hpp"
#include "protocoldef.hpp"
#include "serdesmsg.hpp"
#include "raiitimer.hpp"

class ProcessRun;
class ImTeamStateBoard final: public ImBoard
{
    private:
        static constexpr int uidRegionX = 13;
        static constexpr int uidRegionY = 80;
        static constexpr int uidRegionW = 231;
        static constexpr int uidRegionH = 98;
        static constexpr int texRepeatH = 70;
        static constexpr int lineH = 16;
        static constexpr int uidMinCount = 5;
        static constexpr int uidMaxCount = 10;

    private:
        ProcessRun *m_processRun;
        mutable bool m_showCandidateList = false;
        mutable int m_startIndex[2] {0, 0};
        mutable int m_selectedIndex[2] {-1, -1};
        std::deque<std::pair<SDTeamCandidate, hres_timer>> m_teamCandidateList;
        SDTeamMemberList m_teamMemberList;

    public:
        explicit ImTeamStateBoard(ProcessRun *);

    public:
        void draw() const override;
        bool processEvent(const MirEvent &) const override;

    public:
        void refresh() const;
        void addTeamCandidate(SDTeamCandidate);
        void setTeamMemberList(SDTeamMemberList);

    public:
        const auto &getTeamMemberList() const
        {
            return m_teamMemberList;
        }

    private:
        size_t lineCount() const;
        size_t lineShowCount() const;
        const SDTeamPlayer &getSDTeamPlayer(int) const;
};
