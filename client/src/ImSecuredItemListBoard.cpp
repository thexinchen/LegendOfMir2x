#include "ImSecuredItemListBoard.hpp"

#include <algorithm>
#include <string>

#include "dbcomid.hpp"
#include "fflerror.hpp"
#include "gui_font.hpp"
#include "gui_texture.hpp"
#include "processrun.hpp"
#include "strf.hpp"
#include "totype.hpp"

extern PNGTexDB *g_itemDB;
extern PNGTexDB *g_progUseDB;
extern FontexDB *g_fontexDB;

namespace
{
    constexpr float boardW = 198.0f;
    constexpr float boardH = 204.0f;
    constexpr float boxW = 38.0f;
    constexpr float boxH = 38.0f;
    constexpr float startX = 23.0f;
    constexpr float startY = 41.0f;

    void drawGameText(ImDrawList *drawList, ImVec2 pos, const std::string &text, uint8_t size, ImU32 color)
    {
        if(const auto texture = g_fontexDB->retrieve(1, size, 0, text.c_str()); texture){
            drawList->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h}, {0, 0}, {1, 1}, color);
        }
    }

    void drawCenteredGameText(ImDrawList *drawList, ImVec2 center, const std::string &text)
    {
        if(const auto texture = g_fontexDB->retrieve(1, 12, 0, text.c_str()); texture){
            const ImVec2 pos {center.x - texture.w * 0.5f, center.y};
            drawList->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h}, {0, 0}, {1, 1}, IM_COL32(255, 255, 0, 255));
        }
    }

    bool overlayButton(const char *id, uint32_t hoverID, uint32_t downID, ImVec2 pos)
    {
        const auto hover = g_progUseDB->retrieve(hoverID);
        if(!hover){
            return false;
        }
        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {to_f(hover.w), to_f(hover.h)});
        if(ImGui::IsItemHovered() || ImGui::IsItemActive()){
            const auto down = g_progUseDB->retrieve(downID);
            const auto shown = ImGui::IsItemActive() && down ? down : hover;
            ImGui::GetWindowDrawList()->AddImage(shown, pos, {pos.x + shown.w, pos.y + shown.h});
        }
        return clicked;
    }
}

ImSecuredItemListBoard::ImSecuredItemListBoard(ProcessRun *processRun)
    : ImBoard("##secured-item-list-board")
    , m_processRun(fflcheck(processRun))
{
    moveTo(0, 0);
}

size_t ImSecuredItemListBoard::pageCount() const
{
    return (m_itemList.size() + 11) / 12;
}

void ImSecuredItemListBoard::draw() const
{
    if(!show()){
        return;
    }

    const auto atlas = g_progUseDB->retrieve(0X08000001);
    if(!atlas){
        return;
    }

    if(beginWindow({boardW, boardH})){
        const auto pos = ImGui::GetWindowPos();
        auto *drawList = ImGui::GetWindowDrawList();
        drawList->AddImage(
                atlas,
                pos,
                {pos.x + boardW, pos.y + boardH},
                {290.0f / atlas.w, 0},
                {(290.0f + boardW) / atlas.w, boardH / atlas.h});

        const auto pages = pageCount();
        if(pages > 0){
            m_page = std::min(m_page, pages - 1);
            drawCenteredGameText(drawList, {pos.x + 99.0f, pos.y + 18.0f}, str_printf("第%zu/%zu页", m_page + 1, pages));
        }
        else{
            m_page = 0;
            drawCenteredGameText(drawList, {pos.x + 99.0f, pos.y + 18.0f}, "（空）");
        }

        if(overlayButton("##secured-left", 0X08000007, 0X08000008, {pos.x + 25, pos.y + 163}) && m_page > 0){
            --m_page;
        }
        if(overlayButton("##secured-select", 0X08000005, 0X08000006, {pos.x + 67, pos.y + 163})){
            selectItem();
        }
        if(overlayButton("##secured-right", 0X08000009, 0X0800000A, {pos.x + 115, pos.y + 163}) && m_page + 1 < pages){
            ++m_page;
        }
        if(overlayButton("##secured-close", 0X0000001C, 0X0000001D, {pos.x + 158, pos.y + 159})){
            setShow(false);
        }

        std::optional<size_t> hoveredItem;
        for(int r = 0; r < 3; ++r){
            for(int c = 0; c < 4; ++c){
                const size_t index = m_page * 12 + r * 4 + c;
                if(index >= m_itemList.size()){
                    continue;
                }
                const auto &item = m_itemList.at(index);
                const auto &record = DBCOM_ITEMRECORD(item.itemID);
                fflassert(record);

                const ImVec2 gridPos {pos.x + startX + c * boxW, pos.y + startY + r * boxH};
                ImGui::SetCursorScreenPos(gridPos);
                ImGui::PushID(to_d(index));
                if(ImGui::InvisibleButton("##secured-grid", {boxW, boxH})){
                    m_selectedItem = index;
                }
                const bool hovered = ImGui::IsItemHovered();
                if(hovered){
                    hoveredItem = index;
                    if(ImGui::GetIO().MouseWheel > 0.0f && m_page > 0){
                        --m_page;
                    }
                    else if(ImGui::GetIO().MouseWheel < 0.0f && m_page + 1 < pages){
                        ++m_page;
                    }
                }
                ImGui::PopID();

                if(const auto itemTexture = g_itemDB->retrieve(record.pkgGfxID | 0X02000000); itemTexture){
                    const ImVec2 itemPos
                    {
                        gridPos.x + (boxW - itemTexture.w) * 0.5f,
                        gridPos.y + (boxH - itemTexture.h) * 0.5f,
                    };
                    drawList->AddImage(itemTexture, itemPos, {itemPos.x + itemTexture.w, itemPos.y + itemTexture.h});
                }
                if(record.packable() && item.count > 0){
                    drawGameText(drawList, gridPos, str_ksep(item.count), 10, IM_COL32(255, 255, 0, 255));
                }
                if(m_selectedItem == index){
                    drawList->AddRectFilled(gridPos, {gridPos.x + boxW, gridPos.y + boxH}, IM_COL32(0, 0, 255, 96));
                }
                else if(hovered){
                    drawList->AddRectFilled(gridPos, {gridPos.x + boxW, gridPos.y + boxH}, IM_COL32(255, 255, 255, 96));
                }
            }
        }

        if(hoveredItem.has_value()){
            const auto &record = DBCOM_ITEMRECORD(m_itemList.at(hoveredItem.value()).itemID);
            const auto mouse = ImGui::GetIO().MousePos;
            constexpr ImVec2 tooltipSize {240, 60};
            auto *tooltipDrawList = ImGui::GetForegroundDrawList();
            tooltipDrawList->AddRectFilled(mouse, {mouse.x + tooltipSize.x, mouse.y + tooltipSize.y}, IM_COL32(0, 0, 0, 200));
            drawGameText(tooltipDrawList, {mouse.x + 20, mouse.y + 12}, to_cstr(record.name), 12, IM_COL32_WHITE);
            drawGameText(
                    tooltipDrawList,
                    {mouse.x + 20, mouse.y + 31},
                    str_haschar(record.description) ? to_cstr(record.description) : "暂无描述",
                    12,
                    IM_COL32_WHITE);
        }
    }
    endWindow();
}

bool ImSecuredItemListBoard::processEvent(const MirEvent &event) const
{
    if(show() && event.type == MIR_EVENT_KEY_DOWN && event.key.key == MIRK_ESCAPE){
        setShow(false);
        return true;
    }
    return ImBoard::processEvent(event);
}

void ImSecuredItemListBoard::selectItem() const
{
    if(m_selectedItem.has_value() && m_selectedItem.value() < m_itemList.size()){
        const auto &item = m_itemList.at(m_selectedItem.value());
        m_processRun->requestRemoveSecuredItem(item.itemID, item.seqID);
    }
}

void ImSecuredItemListBoard::setItemList(std::vector<SDItem> itemList)
{
    m_page = 0;
    m_selectedItem.reset();
    m_itemList = std::move(itemList);
}

void ImSecuredItemListBoard::removeItem(uint32_t itemID, uint32_t seqID)
{
    std::erase_if(m_itemList, [itemID, seqID](const auto &item)
    {
        return item.itemID == itemID && item.seqID == seqID;
    });
    const auto pages = pageCount();
    m_page = pages > 0 ? std::min(m_page, pages - 1) : 0;
    if(m_selectedItem.has_value() && m_selectedItem.value() >= m_itemList.size()){
        m_selectedItem.reset();
    }
}
