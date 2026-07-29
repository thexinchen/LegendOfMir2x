#include "ImInventoryBoard.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "cblog.hpp"
#include "dbcomid.hpp"
#include "fflerror.hpp"
#include "gldevice.hpp"
#include "gui_font.hpp"
#include "gui_texture.hpp"
#include "invpack.hpp"
#include "processrun.hpp"
#include "strf.hpp"
#include "sysconst.hpp"
#include "totype.hpp"

extern GLDevice *g_glDevice;
extern PNGTexDB *g_progUseDB;
extern PNGTexDB *g_itemDB;
extern FontexDB *g_fontexDB;

namespace
{
    constexpr float gridX = 18.0f;
    constexpr float gridY = 59.0f;
    constexpr float gridW = SYS_INVGRIDGW * SYS_INVGRIDPW;
    constexpr float gridH = SYS_INVGRIDGH * SYS_INVGRIDPH;

    void drawGameText(ImDrawList *list, ImVec2 pos, const std::string &text, uint8_t size, ImU32 color, bool centered = false)
    {
        if(const auto texture = g_fontexDB->retrieve(1, size, 0, text.c_str()); texture){
            if(centered){
                pos.x -= texture.w * 0.5f;
            }
            list->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h}, {0, 0}, {1, 1}, color);
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

    bool textureButton(const char *id, uint32_t offID, uint32_t downID, ImVec2 pos)
    {
        const auto off = g_progUseDB->retrieve(offID);
        if(!off){
            return false;
        }
        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {to_f(off.w), to_f(off.h)});
        const auto down = g_progUseDB->retrieve(downID);
        const auto shown = ImGui::IsItemActive() && down ? down : off;
        ImGui::GetWindowDrawList()->AddImage(shown, pos, {pos.x + shown.w, pos.y + shown.h});
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

ImInventoryBoard::ImInventoryBoard(ProcessRun *processRun)
    : ImBoard("##inventory-board")
    , m_processRun(fflcheck(processRun))
{}

void ImInventoryBoard::update(double delta)
{
    m_accuTime += delta;
}

size_t ImInventoryBoard::getRowCount() const
{
    const auto hero = m_processRun->getMyHero();
    if(!hero){
        return 0;
    }
    size_t rows = 0;
    for(const auto &bin: hero->getInvPack().getPackBinList()){
        rows = std::max(rows, to_uz(bin.y + bin.h));
    }
    return rows;
}

size_t ImInventoryBoard::getStartRow() const
{
    const auto rows = getRowCount();
    return rows > SYS_INVGRIDGH ? to_uz(std::lround((rows - SYS_INVGRIDGH) * m_scrollValue)) : 0;
}

int ImInventoryBoard::getPackBinIndex(int localX, int localY) const
{
    if(localX < gridX || localY < gridY || localX >= gridX + gridW || localY >= gridY + gridH){
        return -1;
    }
    const int gx = (localX - to_d(gridX)) / SYS_INVGRIDPW;
    const int gy = (localY - to_d(gridY)) / SYS_INVGRIDPH + to_d(getStartRow());
    const auto &bins = m_processRun->getMyHero()->getInvPack().getPackBinList();
    for(int i = 0; i < std::ssize(bins); ++i){
        const auto &bin = bins.at(i);
        if(gx >= bin.x && gx < bin.x + bin.w && gy >= bin.y && gy < bin.y + bin.h){
            return i;
        }
    }
    return -1;
}

void ImInventoryBoard::draw() const
{
    if(!show()){
        return;
    }
    const auto background = g_progUseDB->retrieve(0X0000001B);
    const auto hero = m_processRun->getMyHero();
    if(!(background && hero)){
        return;
    }

    if(beginWindow({to_f(background.w), to_f(background.h)})){
        const auto pos = ImGui::GetWindowPos();
        auto *drawList = ImGui::GetWindowDrawList();
        drawList->AddImage(background, pos, {pos.x + background.w, pos.y + background.h});

        const int frame = to_d(m_accuTime * 8.0) % 10;
        if(const auto wmd = g_progUseDB->retrieve(0X04000010 + frame); wmd){
            drawList->AddImage(wmd, {pos.x + 23, pos.y + 14}, {pos.x + 23 + wmd.w, pos.y + 14 + wmd.h});
        }

        const char *title = "【背包】";
        switch(m_sdInvOp.invOp){
            case INVOP_TRADE : title = "【请选择出售物品】"; break;
            case INVOP_SECURE: title = "【请选择存储物品】"; break;
            case INVOP_REPAIR: title = "【请选择修理物品】"; break;
            default: break;
        }
        drawGameText(drawList, {pos.x + 238, pos.y + 25}, title, 12, IM_COL32_WHITE, true);
        drawGameText(drawList, {pos.x + 132, pos.y + 486}, str_ksep(hero->getGold(), ','), 12, IM_COL32(255, 255, 0, 255), true);

        if(overlayButton("##inventory-close", 0X0000001C, 0X0000001D, {pos.x + 394, pos.y + 498})){
            setShow(false);
            const_cast<ImInventoryBoard *>(this)->clearInvOp();
        }
        if(m_sdInvOp.invOp == INVOP_NONE){
            if(overlayButton("##inventory-sort", 0X000000C0, 0X000000C1, {pos.x + 374, pos.y + 12})){
                hero->getInvPack().repack();
            }
        }

        constexpr float barX = 410.0f;
        constexpr float barY = 63.0f;
        constexpr float barH = 369.0f;
        ImGui::SetCursorScreenPos({pos.x + barX - 7, pos.y + barY - 7});
        ImGui::InvisibleButton("##inventory-slider", {22, barH + 14});
        if(ImGui::IsItemActive()){
            m_scrollValue = std::clamp((ImGui::GetIO().MousePos.y - pos.y - barY) / (barH - 1.0f), 0.0f, 1.0f);
        }
        if(const auto slider = g_progUseDB->retrieve(0X00000080); slider){
            const ImVec2 sliderPos {pos.x + barX - 7, pos.y + barY + m_scrollValue * (barH - 1.0f) - 7};
            drawList->AddImage(slider, sliderPos, {sliderPos.x + slider.w, sliderPos.y + slider.h}, {0, 0}, {1, 1},
                    ImGui::IsItemActive() ? IM_COL32_WHITE : IM_COL32(128, 128, 128, 255));
        }

        ImGui::SetCursorScreenPos({pos.x + gridX, pos.y + gridY});
        ImGui::InvisibleButton("##inventory-grid", {gridW, gridH});
        const bool gridHovered = ImGui::IsItemHovered();
        if(gridHovered && ImGui::GetIO().MouseWheel != 0.0f){
            const auto rows = getRowCount();
            if(rows > SYS_INVGRIDGH){
                m_scrollValue = std::clamp(m_scrollValue - ImGui::GetIO().MouseWheel / to_f(rows - SYS_INVGRIDGH), 0.0f, 1.0f);
            }
        }

        const auto mouse = ImGui::GetIO().MousePos;
        const int localX = to_d(mouse.x - pos.x);
        const int localY = to_d(mouse.y - pos.y);
        const int hoveredIndex = gridHovered ? getPackBinIndex(localX, localY) : -1;
        const auto startRow = getStartRow();
        drawList->PushClipRect({pos.x + gridX, pos.y + gridY}, {pos.x + gridX + gridW, pos.y + gridY + gridH}, true);
        const auto &bins = hero->getInvPack().getPackBinList();
        for(int i = 0; i < std::ssize(bins); ++i){
            const auto &bin = bins.at(i);
            if(!bin){
                continue;
            }
            const auto &record = DBCOM_ITEMRECORD(bin.item.itemID);
            if(!record){
                continue;
            }
            const ImVec2 cellPos
            {
                pos.x + gridX + bin.x * SYS_INVGRIDPW,
                pos.y + gridY + (bin.y - to_d(startRow)) * SYS_INVGRIDPH,
            };
            if(const auto itemTex = g_itemDB->retrieve(record.pkgGfxID | 0X01000000); itemTex){
                const ImVec2 itemPos
                {
                    cellPos.x + (bin.w * SYS_INVGRIDPW - itemTex.w) * 0.5f,
                    cellPos.y + (bin.h * SYS_INVGRIDPH - itemTex.h) * 0.5f,
                };
                drawList->AddImage(itemTex, itemPos, {itemPos.x + itemTex.w, itemPos.y + itemTex.h});
            }
            if(bin.item.count > 1){
                drawGameText(
                        drawList,
                        {cellPos.x + bin.w * SYS_INVGRIDPW, cellPos.y - 2},
                        std::to_string(bin.item.count),
                        10,
                        IM_COL32(255, 255, 0, 255),
                        true);
            }
            if(i == hoveredIndex){
                drawList->AddRectFilled(cellPos, {cellPos.x + bin.w * SYS_INVGRIDPW, cellPos.y + bin.h * SYS_INVGRIDPH}, IM_COL32(255, 255, 255, 48));
            }
            else if(m_sdInvOp.invOp != INVOP_NONE && i == m_selectedIndex){
                drawList->AddRectFilled(cellPos, {cellPos.x + bin.w * SYS_INVGRIDPW, cellPos.y + bin.h * SYS_INVGRIDPH}, IM_COL32(0, 0, 255, 48));
            }
        }
        drawList->PopClipRect();

        if(gridHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
            auto &pack = hero->getInvPack();
            const auto grabbed = pack.getGrabbedItem();
            if(m_sdInvOp.invOp == INVOP_NONE){
                if(hoveredIndex >= 0){
                    const auto selected = pack.getPackBinList().at(hoveredIndex);
                    pack.setGrabbedItem(selected.item);
                    pack.remove(selected.item);
                    if(grabbed){
                        pack.add(grabbed, selected.x, selected.y);
                    }
                }
                else if(grabbed){
                    const int gx = (localX - to_d(gridX)) / SYS_INVGRIDPW;
                    const int gy = (localY - to_d(gridY)) / SYS_INVGRIDPH + to_d(startRow);
                    const auto [w, h] = InvPack::getPackBinSize(grabbed.itemID);
                    pack.add(grabbed, gx - w / 2, gy - h / 2);
                    pack.setGrabbedItem({});
                }
            }
            else if(hoveredIndex >= 0){
                m_selectedIndex = hoveredIndex;
                const auto &selected = pack.getPackBinList().at(hoveredIndex);
                if(m_sdInvOp.hasType(DBCOM_ITEMRECORD(selected.item.itemID).type)){
                    m_processRun->sendNPCEvent(m_sdInvOp.uid, {}, m_sdInvOp.queryTag.c_str(),
                            str_printf("%d:%d", to_d(selected.item.itemID), to_d(selected.item.seqID)));
                }
                else{
                    m_processRun->addCBLog(CBLOG_ERR, u8"该物品类型不能用于当前操作");
                    m_selectedIndex = -1;
                    m_invOpCost = -1;
                }
            }
        }
        if(gridHovered && hoveredIndex >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Right)){
            packBinConsume(bins.at(hoveredIndex));
        }

        if(m_sdInvOp.invOp != INVOP_NONE && m_selectedIndex >= 0){
            if(const auto opBg = g_progUseDB->retrieve(0X000000B0); opBg){
                drawList->AddImage(opBg, {pos.x + 295, pos.y + 475}, {pos.x + 295 + opBg.w, pos.y + 475 + opBg.h});
            }
            uint32_t offID = 0X000000B3;
            uint32_t downID = 0X000000B4;
            if(m_sdInvOp.invOp == INVOP_SECURE){ offID = 0X000000B5; downID = 0X000000B6; }
            if(m_sdInvOp.invOp == INVOP_REPAIR){ offID = 0X000000B1; downID = 0X000000B2; }
            if(textureButton("##inventory-operation", offID, downID, {pos.x + 298, pos.y + 478})){
                commitInvOp();
            }
            if(m_invOpCost >= 0){
                drawGameText(drawList, {pos.x + 132, pos.y + 503}, str_ksep(m_invOpCost, ','), 12, IM_COL32(255, 255, 0, 255), true);
            }
        }

        if(hoveredIndex >= 0){
            const auto lines = plainLayoutLines(bins.at(hoveredIndex).item.getXMLLayout());
            const float tooltipH = std::max(40.0f, 20.0f + lines.size() * 15.0f);
            const ImVec2 tooltipPos
            {
                std::clamp(mouse.x, 0.0f, to_f(g_glDevice->getRendererWidth()) - 220.0f),
                std::clamp(mouse.y, 0.0f, to_f(g_glDevice->getRendererHeight()) - tooltipH),
            };
            auto *foreground = ImGui::GetForegroundDrawList();
            foreground->AddRectFilled(tooltipPos, {tooltipPos.x + 220, tooltipPos.y + tooltipH}, IM_COL32(0, 0, 0, 200), 5);
            foreground->AddRect(tooltipPos, {tooltipPos.x + 220, tooltipPos.y + tooltipH}, IM_COL32(231, 231, 189, 200), 5);
            for(size_t i = 0; i < lines.size(); ++i){
                drawGameText(foreground, {tooltipPos.x + 10, tooltipPos.y + 10 + i * 15}, lines.at(i), 12, IM_COL32_WHITE);
            }
        }
    }
    endWindow();
}

bool ImInventoryBoard::processEvent(const MirEvent &event) const
{
    if(show() && event.type == MIR_EVENT_KEY_DOWN && event.key.key == MIRK_ESCAPE){
        setShow(false);
        return true;
    }
    return ImBoard::processEvent(event);
}

void ImInventoryBoard::packBinConsume(const PackBin &bin) const
{
    const auto &record = DBCOM_ITEMRECORD(bin.item.itemID);
    fflassert(record);
    const auto equip = [this, &bin](int wl)
    {
        InvPack::playItemSoundEffect(bin.item.itemID, true);
        m_processRun->requestEquipWear(bin.item.itemID, bin.item.seqID, wl);
    };
    if(to_u8sv(record.type) == u8"恢复药水" || to_u8sv(record.type) == u8"强化药水" || to_u8sv(record.type) == u8"技能书"){
        InvPack::playItemSoundEffect(bin.item.itemID, true);
        m_processRun->requestConsumeItem(bin.item.itemID, bin.item.seqID, 1);
    }
    else if(to_u8sv(record.type) == u8"头盔"){ equip(WLG_HELMET); }
    else if(to_u8sv(record.type) == u8"武器"){ equip(WLG_WEAPON); }
    else if(to_u8sv(record.type) == u8"衣服"){ equip(WLG_DRESS); }
    else if(to_u8sv(record.type) == u8"鞋"){ equip(WLG_SHOES); }
    else if(to_u8sv(record.type) == u8"项链"){ equip(WLG_NECKLACE); }
    else if(to_u8sv(record.type) == u8"手镯"){ equip(WLG_ARMRING0); }
    else if(to_u8sv(record.type) == u8"戒指"){ equip(WLG_RING0); }
}

void ImInventoryBoard::clearInvOp()
{
    m_sdInvOp.clear();
    m_selectedIndex = -1;
    m_invOpCost = -1;
}

void ImInventoryBoard::startInvOp(SDStartInvOp operation)
{
    m_sdInvOp = std::move(operation);
}

void ImInventoryBoard::setInvOpCost(int mode, uint32_t itemID, uint32_t seqID, size_t cost)
{
    if(m_sdInvOp.invOp == mode && m_selectedIndex >= 0){
        const auto &bins = m_processRun->getMyHero()->getInvPack().getPackBinList();
        if(m_selectedIndex < std::ssize(bins)){
            const auto &item = bins.at(m_selectedIndex).item;
            if(item.itemID == itemID && item.seqID == seqID){
                m_invOpCost = to_d(cost);
            }
        }
    }
}

void ImInventoryBoard::commitInvOp() const
{
    if(m_sdInvOp.invOp == INVOP_NONE || m_selectedIndex < 0){
        return;
    }
    const auto &bin = m_processRun->getMyHero()->getInvPack().getPackBinList().at(m_selectedIndex);
    if(bin && m_sdInvOp.hasType(DBCOM_ITEMRECORD(bin.item.itemID).type)){
        m_processRun->sendNPCEvent(m_sdInvOp.uid, {}, m_sdInvOp.commitTag.c_str(),
                str_printf("%d:%d", to_d(bin.item.itemID), to_d(bin.item.seqID)));
    }
}

void ImInventoryBoard::removeItem(uint32_t itemID, uint32_t seqID, size_t count)
{
    auto &pack = m_processRun->getMyHero()->getInvPack();
    auto grabbed = pack.getGrabbedItem();
    if(itemID == grabbed.itemID && seqID == grabbed.seqID){
        if(count < grabbed.count){
            grabbed.count -= count;
            pack.setGrabbedItem(grabbed);
        }
        else{
            pack.setGrabbedItem({});
        }
    }
    else{
        if(m_selectedIndex >= 0){
            const auto &item = pack.getPackBinList().at(m_selectedIndex).item;
            if(item.itemID == itemID && item.seqID == seqID){
                m_selectedIndex = -1;
            }
        }
        pack.remove(itemID, seqID, count);
    }
}

void ImInventoryBoard::moveToUpperRight() const
{
    const auto background = g_progUseDB->retrieve(0X0000001B);
    moveTo(background ? to_f(g_glDevice->getRendererWidth() - background.w) : 0.0f, 0.0f);
}
