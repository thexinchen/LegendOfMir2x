#pragma once

#include <cstdint>
#include <tuple>
#include <vector>

#include "ImBoard.hpp"
#include "gui_texture.hpp"
#include "serdesmsg.hpp"

class ProcessRun;
class ImPurchaseBoard final: public ImBoard
{
    private:
        static constexpr int startX = 19;
        static constexpr int startY = 15;
        static constexpr int boxW = 38;
        static constexpr int boxH = 38;
        static constexpr int lineW = 233;
        static constexpr int lineH = 42;

    private:
        ProcessRun *m_processRun;
        uint64_t m_npcUID = 0;
        mutable int m_ext1Page = 0;
        mutable int m_ext1PageGridSelected = -1;
        mutable SDSellItemList m_sdSellItemList;
        mutable int m_selected = 0;
        std::vector<uint32_t> m_itemList;
        mutable float m_scroll = 0.0f;

    public:
        explicit ImPurchaseBoard(ProcessRun *);

    public:
        void draw() const override;
        bool processEvent(const MirEvent &) const override;

    public:
        void loadSell(uint64_t, std::vector<uint32_t>);
        uint32_t selectedItemID() const;
        void onBuySucceed(uint64_t, uint32_t, uint32_t);
        void setSellItemList(SDSellItemList);

    private:
        size_t getStartIndex() const;
        void setExtendedItemID(uint32_t) const;
        int extendedBoardGfxID() const;
        int extendedPageCount() const;
        std::tuple<uint32_t, uint32_t> getExtSelectedItemSeqID() const;
        size_t getItemPrice(int) const;
        void requestExt2Buy(uint32_t) const;
        static GLTexID getItemTexture(const char8_t *, uint32_t);
};
