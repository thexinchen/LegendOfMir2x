#include "ImPurchaseBoard.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "client.hpp"
#include "dbcomid.hpp"
#include "fflerror.hpp"
#include "gldevice.hpp"
#include "gui_font.hpp"
#include "ImInputStringBoard.hpp"
#include "processrun.hpp"
#include "strf.hpp"
#include "totype.hpp"

extern Client *g_client;
extern PNGTexDB *g_itemDB;
extern PNGTexDB *g_progUseDB;
extern FontexDB *g_fontexDB;

namespace
{
    void drawTexture(ImDrawList *list, const GLTexID texture, const ImVec2 pos, const ImU32 tint = IM_COL32_WHITE)
    {
        if(texture){
            list->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h}, {0, 0}, {1, 1}, tint);
        }
    }

    void drawText(ImDrawList *list, const ImVec2 pos, const std::string &text, const uint8_t font, const uint8_t size, const ImU32 color)
    {
        if(const auto texture = g_fontexDB->retrieve(font, size, 0, text.c_str()); texture){
            list->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h}, {0, 0}, {1, 1}, color);
        }
    }

    bool textureButton(const char *id, const uint32_t hoverID, const uint32_t downID, const ImVec2 pos)
    {
        const auto hover = g_progUseDB->retrieve(hoverID);
        const auto down = g_progUseDB->retrieve(downID);
        const auto sizeTexture = hover ? hover : down;
        if(!sizeTexture){
            return false;
        }
        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {to_f(sizeTexture.w), to_f(sizeTexture.h)});
        if(ImGui::IsItemHovered() || ImGui::IsItemActive()){
            drawTexture(ImGui::GetWindowDrawList(), ImGui::IsItemActive() && down ? down : hover, pos);
        }
        if(clicked){ playButtonClickSound(); }
        return clicked;
    }

    std::vector<std::string> plainLayoutLines(const std::u8string &layout)
    {
        const std::string source = to_cstr(layout);
        std::vector<std::string> lines;
        std::string line;
        bool inTag = false;
        for(const char ch: source){
            if(ch == '<'){
                inTag = true;
            }
            else if(ch == '>'){
                inTag = false;
            }
            else if(!inTag){
                if(ch == '\n'){
                    if(!line.empty()){
                        lines.push_back(std::move(line));
                        line.clear();
                    }
                }
                else{
                    line.push_back(ch);
                }
            }
        }
        if(!line.empty()){
            lines.push_back(std::move(line));
        }
        return lines;
    }
}

ImPurchaseBoard::ImPurchaseBoard(ProcessRun *processRun)
    : ImBoard("##purchase-board")
    , m_processRun(fflcheck(processRun))
{
    moveTo(0.0f, 0.0f);
}

void ImPurchaseBoard::draw() const
{
    if(!show()){
        return;
    }

    const auto background = g_progUseDB->retrieve(0X08000000 + to_u32(extendedBoardGfxID()));
    if(!background){
        return;
    }

    int hoveredExtIndex = -1;
    if(beginWindow({to_f(background.w), to_f(background.h)})){
        const auto pos = ImGui::GetWindowPos();
        auto *drawList = ImGui::GetWindowDrawList();
        drawTexture(drawList, background, pos);

        if(textureButton("##purchase-close", 0X0000001C, 0X0000001D, {pos.x + 257, pos.y + 183})){
            setShow(false);
            setExtendedItemID(0);
        }
        if(textureButton("##purchase-select", 0X08000003, 0X08000004, {pos.x + 105, pos.y + 185})){
            setExtendedItemID(selectedItemID());
        }

        const size_t first = getStartIndex();
        for(size_t i = first; i < std::min(m_itemList.size(), first + 4); ++i){
            const auto &record = DBCOM_ITEMRECORD(m_itemList[i]);
            if(!record){
                continue;
            }
            const int row = to_d(i - first);
            const ImVec2 rowPos {pos.x + startX, pos.y + startY + lineH * row};
            if(const auto item = getItemTexture(record.type, record.pkgGfxID); item){
                const float ratio = std::max({to_f(item.w) / boxW, to_f(item.h) / boxH, 1.0f});
                const ImVec2 size {item.w / ratio, item.h / ratio};
                drawList->AddImage(item,
                    {rowPos.x + (boxW - size.x) * 0.5f, rowPos.y + (boxH - size.y) * 0.5f},
                    {rowPos.x + (boxW + size.x) * 0.5f, rowPos.y + (boxH + size.y) * 0.5f});
            }
            if(const auto label = g_fontexDB->retrieve(1, 12, 0, to_cstr(record.name)); label){
                drawTexture(drawList, label, {rowPos.x + boxW + 10, rowPos.y + (boxH - label.h) * 0.5f}, IM_COL32(255, 255, 0, 255));
            }

            ImGui::PushID(to_d(i));
            ImGui::SetCursorScreenPos(rowPos);
            ImGui::InvisibleButton("##purchase-row", {to_f(lineW), to_f(boxH)});
            if(ImGui::IsItemClicked()){
                m_selected = to_d(i);
                if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){
                    setExtendedItemID(selectedItemID());
                }
            }
            ImGui::PopID();
            if(m_selected == to_d(i)){
                drawList->AddRectFilled(rowPos, {rowPos.x + lineW, rowPos.y + boxH}, IM_COL32(255, 255, 255, 64));
            }
        }

        const ImVec2 listMin {pos.x + startX, pos.y + startY};
        const ImVec2 listMax {listMin.x + lineW, listMin.y + lineH * 3 + boxH};
        if(ImGui::IsMouseHoveringRect(listMin, listMax) && ImGui::GetIO().MouseWheel != 0.0f && m_itemList.size() > 4){
            m_scroll = std::clamp(
                m_scroll + (ImGui::GetIO().MouseWheel > 0.0f ? -1.0f : 1.0f) / to_f(m_itemList.size() - 4),
                0.0f,
                1.0f);
        }

        const ImVec2 sliderTop {pos.x + 266, pos.y + 25};
        ImGui::SetCursorScreenPos({sliderTop.x - 7, sliderTop.y - 7});
        ImGui::InvisibleButton("##purchase-slider", {21, 139});
        if(ImGui::IsItemActive()){
            m_scroll = std::clamp((ImGui::GetIO().MousePos.y - sliderTop.y) / 125.0f, 0.0f, 1.0f);
        }
        if(const auto thumb = g_progUseDB->retrieve(0X00000080); thumb){
            drawTexture(drawList, thumb, {sliderTop.x - 7, sliderTop.y + m_scroll * 125.0f - 7});
        }

        if(extendedBoardGfxID() == 1){
            if(textureButton("##purchase-ext1-close", 0X0000001C, 0X0000001D, {pos.x + 448, pos.y + 159})){
                setExtendedItemID(0);
            }
            if(textureButton("##purchase-ext1-left", 0X08000007, 0X08000008, {pos.x + 315, pos.y + 163})){
                m_ext1Page = std::max(0, m_ext1Page - 1);
            }
            if(textureButton("##purchase-ext1-buy", 0X08000005, 0X08000006, {pos.x + 357, pos.y + 163})){
                const auto [itemID, seqID] = getExtSelectedItemSeqID();
                if(itemID){
                    m_processRun->requestBuy(m_npcUID, itemID, seqID, 1);
                }
            }
            if(textureButton("##purchase-ext1-right", 0X08000009, 0X0800000A, {pos.x + 405, pos.y + 163})){
                m_ext1Page = std::max(0, std::min(extendedPageCount() - 1, m_ext1Page + 1));
            }

            const int pageCount = extendedPageCount();
            if(pageCount > 0){
                for(int grid = 0; grid < 12; ++grid){
                    const int itemIndex = m_ext1Page * 12 + grid;
                    if(itemIndex >= to_d(m_sdSellItemList.list.size())){
                        break;
                    }
                    const auto &sellItem = m_sdSellItemList.list[itemIndex];
                    const auto &record = DBCOM_ITEMRECORD(sellItem.item.itemID);
                    if(!record){
                        throw fflpanic("bad item in sell list: itemID = {}, seqID = {}", sellItem.item.itemID, sellItem.item.seqID);
                    }
                    const int c = grid % 4;
                    const int r = grid / 4;
                    const ImVec2 gridPos {pos.x + 313 + boxW * c, pos.y + 41 + boxH * r};
                    if(const auto item = getItemTexture(record.type, record.pkgGfxID); item){
                        const float ratio = std::max({to_f(item.w) / boxW, to_f(item.h) / boxH, 1.0f});
                        const ImVec2 size {item.w / ratio, item.h / ratio};
                        drawList->AddImage(item,
                            {gridPos.x + (boxW - size.x) * 0.5f, gridPos.y + (boxH - size.y) * 0.5f},
                            {gridPos.x + (boxW + size.x) * 0.5f, gridPos.y + (boxH + size.y) * 0.5f});
                    }
                    drawText(drawList, gridPos, str_ksep(getItemPrice(itemIndex)), 1, 10, IM_COL32(255, 255, 0, 255));
                    ImGui::PushID(itemIndex);
                    ImGui::SetCursorScreenPos(gridPos);
                    ImGui::InvisibleButton("##purchase-ext1-grid", {to_f(boxW), to_f(boxH)});
                    const bool hovered = ImGui::IsItemHovered();
                    if(ImGui::IsItemClicked()){
                        m_ext1PageGridSelected = itemIndex;
                    }
                    ImGui::PopID();
                    if(m_ext1PageGridSelected == itemIndex || hovered){
                        drawList->AddRectFilled(
                            gridPos,
                            {gridPos.x + boxW, gridPos.y + boxH},
                            m_ext1PageGridSelected == itemIndex ? IM_COL32(0, 0, 255, 96) : IM_COL32(255, 255, 255, 96));
                    }
                    if(hovered){
                        hoveredExtIndex = itemIndex;
                    }
                }
                drawText(drawList, {pos.x + 389, pos.y + 18}, str_printf("第%d/%d页", m_ext1Page + 1, pageCount), 1, 12, IM_COL32(255, 255, 0, 255));
                const ImVec2 gridMin {pos.x + 313, pos.y + 41};
                const ImVec2 gridMax {gridMin.x + 152, gridMin.y + 114};
                if(ImGui::IsMouseHoveringRect(gridMin, gridMax) && ImGui::GetIO().MouseWheel != 0.0f){
                    m_ext1Page = std::clamp(m_ext1Page + (ImGui::GetIO().MouseWheel > 0.0f ? -1 : 1), 0, pageCount - 1);
                }
            }
        }
        else if(extendedBoardGfxID() == 2){
            if(textureButton("##purchase-ext2-close", 0X0000001C, 0X0000001D, {pos.x + 474, pos.y + 56})){
                setExtendedItemID(0);
            }
            const auto [itemID, seqID] = getExtSelectedItemSeqID();
            if(seqID){
                throw fflpanic("unexpected extSeqID: {}", seqID);
            }
            if(textureButton("##purchase-ext2-buy", 0X0800000B, 0X0800000C, {pos.x + 366, pos.y + 60}) && itemID){
                requestExt2Buy(itemID);
            }
            if(itemID){
                const auto &record = DBCOM_ITEMRECORD(itemID);
                const ImVec2 gridPos {pos.x + 303, pos.y + 16};
                if(const auto item = getItemTexture(record.type, record.pkgGfxID); item){
                    const float ratio = std::max({to_f(item.w) / boxW, to_f(item.h) / boxH, 1.0f});
                    const ImVec2 size {item.w / ratio, item.h / ratio};
                    drawList->AddImage(item,
                        {gridPos.x + (boxW - size.x) * 0.5f, gridPos.y + (boxH - size.y) * 0.5f},
                        {gridPos.x + (boxW + size.x) * 0.5f, gridPos.y + (boxH + size.y) * 0.5f});
                }
                const auto priceText = str_ksep(getItemPrice(0)) + " 金币";
                if(const auto label = g_fontexDB->retrieve(1, 13, 0, priceText.c_str()); label){
                    drawTexture(drawList, label, {pos.x + 353, pos.y + 16 + (boxH - label.h) * 0.5f}, IM_COL32(255, 255, 0, 255));
                }
            }
        }
    }
    endWindow();

    if(hoveredExtIndex >= 0){
        const auto goldPrice = getItemPrice(hoveredExtIndex);
        const auto lines = plainLayoutLines(m_sdSellItemList.list.at(hoveredExtIndex).item.getXMLLayout(
        {
            {SDItem::XML_PRICE, std::to_string(goldPrice)},
            {SDItem::XML_PRICECOLOR, goldPrice > 100 ? "red" : "green"},
        }));
        const auto mouse = ImGui::GetIO().MousePos;
        const float tooltipHeight = std::max(40.0f, 20.0f + lines.size() * 15.0f);
        auto *foreground = ImGui::GetForegroundDrawList();
        foreground->AddRectFilled(
            mouse,
            {mouse.x + 220.0f, mouse.y + tooltipHeight},
            IM_COL32(0, 0, 0, 200));
        for(size_t i = 0; i < lines.size(); ++i){
            drawText(foreground, {mouse.x + 10, mouse.y + 10 + i * 15.0f}, lines[i], 1, 12, IM_COL32_WHITE);
        }
    }
}

bool ImPurchaseBoard::processEvent(const MirEvent &) const
{
    return show();
}

void ImPurchaseBoard::loadSell(const uint64_t npcUID, std::vector<uint32_t> itemList)
{
    m_npcUID = npcUID;
    m_itemList = std::move(itemList);
}

size_t ImPurchaseBoard::getStartIndex() const
{
    return m_itemList.size() <= 4 ? 0 : std::lround((m_itemList.size() - 4) * m_scroll);
}

uint32_t ImPurchaseBoard::selectedItemID() const
{
    return m_selected >= 0 && m_selected < to_d(m_itemList.size()) ? m_itemList.at(m_selected) : 0;
}

void ImPurchaseBoard::setExtendedItemID(const uint32_t itemID) const
{
    m_ext1PageGridSelected = -1;
    if(itemID){
        CMQuerySellItemList message {};
        message.npcUID = m_npcUID;
        message.itemID = itemID;
        g_client->send({CM_QUERYSELLITEMLIST, message});
    }
    else{
        m_sdSellItemList.clear();
    }
}

void ImPurchaseBoard::setSellItemList(SDSellItemList list)
{
    if(m_npcUID == list.npcUID){
        m_sdSellItemList = std::move(list);
    }
    else{
        m_sdSellItemList.clear();
    }
    m_ext1Page = 0;
}

int ImPurchaseBoard::extendedBoardGfxID() const
{
    if(m_sdSellItemList.npcUID && !m_sdSellItemList.list.empty()){
        return DBCOM_ITEMRECORD(m_sdSellItemList.list.front().item.itemID).packable() ? 2 : 1;
    }
    return 0;
}

int ImPurchaseBoard::extendedPageCount() const
{
    if(extendedBoardGfxID() != 1){
        return -1;
    }
    return m_sdSellItemList.list.empty() ? 0 : to_d(m_sdSellItemList.list.size() + 11) / 12;
}

std::tuple<uint32_t, uint32_t> ImPurchaseBoard::getExtSelectedItemSeqID() const
{
    if(extendedBoardGfxID() == 1){
        if(m_ext1PageGridSelected >= 0 && m_ext1PageGridSelected < to_d(m_sdSellItemList.list.size())){
            const auto &item = m_sdSellItemList.list.at(m_ext1PageGridSelected).item;
            return {item.itemID, item.seqID};
        }
        return {0, 0};
    }
    if(extendedBoardGfxID() == 2){
        return m_sdSellItemList.list.empty()
             ? std::tuple<uint32_t, uint32_t> {0, 0}
             : std::tuple<uint32_t, uint32_t> {m_sdSellItemList.list.front().item.itemID, 0};
    }
    throw fflreach();
}

size_t ImPurchaseBoard::getItemPrice(const int itemIndex) const
{
    if(itemIndex < 0 || itemIndex >= to_d(m_sdSellItemList.list.size())){
        throw fflpanic("invalid argument: itemIndex = {}", itemIndex);
    }
    for(const auto &cost: m_sdSellItemList.list.at(itemIndex).costList){
        if(cost.isGold()){
            return cost.count;
        }
    }
    return 0;
}

void ImPurchaseBoard::requestExt2Buy(const uint32_t itemID) const
{
    const auto header = str_printf(u8"<layout><par>请输入你要购买<t color=\"red\">%s</t>的数量</par></layout>", to_cstr(DBCOM_ITEMRECORD(itemID).name));
    m_processRun->getInputStringBoard()->waitInput(header, false, [itemID, npcUID = m_npcUID, processRun = m_processRun](std::u8string input)
    {
        int count = 0;
        try{
            count = std::stoi(to_cstr(input));
        }
        catch(...){
            processRun->addCBLog(CBLOG_ERR, u8"无效输入:%s", to_cstr(input));
        }
        if(DBCOM_ITEMRECORD(itemID) && count > 0){
            processRun->requestBuy(npcUID, itemID, 0, count);
        }
    });
}

void ImPurchaseBoard::onBuySucceed(const uint64_t npcUID, const uint32_t itemID, const uint32_t seqID)
{
    if(npcUID != m_npcUID || npcUID != m_sdSellItemList.npcUID || DBCOM_ITEMRECORD(itemID).packable()){
        return;
    }
    for(auto p = m_sdSellItemList.list.begin(); p != m_sdSellItemList.list.end(); ++p){
        if(p->item.itemID == itemID && p->item.seqID == seqID){
            m_sdSellItemList.list.erase(p);
            return;
        }
    }
}

GLTexID ImPurchaseBoard::getItemTexture(const char8_t *itemType, const uint32_t pkgGfxID)
{
    if(auto texture = g_itemDB->retrieve(pkgGfxID | 0X02000000)){
        return texture;
    }
    if(str_haschar(itemType)){
        const auto type = to_u8sv(itemType);
        if(type == u8"项链" || type == u8"戒指" || type == u8"手镯" || type == u8"鞋" || type == u8"道具"
                || type == u8"火把" || type == u8"头盔" || type == u8"武器" || type == u8"衣服"){
            return g_itemDB->retrieve(pkgGfxID | 0X01000000);
        }
    }
    return nullptr;
}
