#pragma once

#include <memory>

#include "ImBoard.hpp"

class ProcessRun;
class SkillBoardConfig;

class ImSkillBoard final: public ImBoard
{
    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;

    public:
        ImSkillBoard(int, int, ProcessRun *);
        ~ImSkillBoard() override;

    public:
        void draw() const override;
        void update(double) override;
        bool processEvent(const MirEvent &) const override;

    public:
        SkillBoardConfig &getConfig();
        const SkillBoardConfig &getConfig() const;
};
