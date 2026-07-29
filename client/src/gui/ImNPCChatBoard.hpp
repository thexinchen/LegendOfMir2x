#pragma once

#include <cstdint>
#include <string>

#include "ImBoard.hpp"
#include "layoutboard.hpp"

class ProcessRun;
class ImNPCChatBoard final: public ImBoard
{
    private:
        static constexpr int margin = 35;

    private:
        uint64_t m_npcUID = 0;
        std::string m_eventPath;
        ProcessRun *m_processRun;
        mutable LayoutBoard m_chatBoard;

    public:
        explicit ImNPCChatBoard(ProcessRun *);

    public:
        void draw() const override;
        bool processEvent(const MirEvent &) const override;

    public:
        void loadXML(uint64_t, const char *, const char *);

    public:
        ImVec2 boardSize() const;
        float height() const { return boardSize().y; }

    private:
        void onClickEvent(const char *, const char *, const char *, bool);
        uint32_t getNPCFaceKey() const;
};

