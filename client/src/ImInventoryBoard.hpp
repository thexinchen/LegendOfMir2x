#pragma once

#include "ImBoard.hpp"
#include "serdesmsg.hpp"

class PackBin;
class ProcessRun;
class ImInventoryBoard final: public ImBoard
{
    private:
        ProcessRun *m_processRun;
        mutable double m_accuTime = 0.0;
        mutable float m_scrollValue = 0.0f;
        mutable int m_invOpCost = -1;
        mutable int m_selectedIndex = -1;
        SDStartInvOp m_sdInvOp;

    public:
        explicit ImInventoryBoard(ProcessRun *);

    public:
        void draw() const override;
        void update(double) override;
        bool processEvent(const MirEvent &) const override;

    public:
        void clearInvOp();
        void startInvOp(SDStartInvOp);
        void setInvOpCost(int, uint32_t, uint32_t, size_t);
        void removeItem(uint32_t, uint32_t, size_t);
        void moveToUpperRight() const;

    private:
        size_t getRowCount() const;
        size_t getStartRow() const;
        int getPackBinIndex(int, int) const;
        void packBinConsume(const PackBin &) const;
        void commitInvOp() const;
};
