#pragma once

#include <map>
#include <string>
#include <vector>

#include "ImBoard.hpp"
#include "serdesmsg.hpp"

class ProcessRun;
class ImQuestStateBoard final: public ImBoard
{
    private:
        struct QuestDespState
        {
            bool folded = true;
            std::map<std::string, std::string> desp {};
        };

    private:
        ProcessRun *m_processRun;
        mutable float m_scrollValue = 0.0f;
        mutable std::map<std::string, QuestDespState> m_questDesp;

    public:
        explicit ImQuestStateBoard(ProcessRun *);

    public:
        void draw() const override;
        bool processEvent(const MirEvent &) const override;

    public:
        void setQuestDesp(SDQuestDespList);
        void updateQuestDesp(SDQuestDespUpdate);
};
