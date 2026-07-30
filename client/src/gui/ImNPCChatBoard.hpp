#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "ImBoard.hpp"

class ProcessRun;
class ImNPCChatBoard final: public ImBoard
{
    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;

    private:
        static constexpr int margin = 35;

    private:
        uint64_t m_npcUID = 0;
        std::string m_eventPath;
        ProcessRun *m_processRun;

    public:
        explicit ImNPCChatBoard(ProcessRun *);
        ~ImNPCChatBoard() override;

    public:
        void draw() const override;
        void update(double) override;
        bool processEvent(const MirEvent &) const override;

    public:
        void loadXML(uint64_t, const char *, const char *);

    public:
        ImVec2 boardSize() const;
        float height() const { return boardSize().y; }

    private:
        void onClickEvent(const char *, const char *, const char *, bool) const;
        uint32_t getNPCFaceKey() const;
};
