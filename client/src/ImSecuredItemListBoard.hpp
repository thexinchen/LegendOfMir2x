#pragma once

#include <optional>
#include <vector>

#include "ImBoard.hpp"
#include "serdesmsg.hpp"

class ProcessRun;
class ImSecuredItemListBoard final: public ImBoard
{
    private:
        ProcessRun *m_processRun;
        mutable size_t m_page = 0;
        mutable std::optional<size_t> m_selectedItem;
        std::vector<SDItem> m_itemList;

    public:
        explicit ImSecuredItemListBoard(ProcessRun *);

    public:
        void draw() const override;
        bool processEvent(const MirEvent &) const override;

    public:
        void setItemList(std::vector<SDItem>);
        void removeItem(uint32_t, uint32_t);

    private:
        size_t pageCount() const;
        void selectItem() const;
};
