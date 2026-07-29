#include "ImPlayerStateBoard.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "dbcomid.hpp"
#include "fflerror.hpp"
#include "gui_font.hpp"
#include "gui_texture.hpp"
#include "invpack.hpp"
#include "processrun.hpp"
#include "strf.hpp"
#include "totype.hpp"

extern PNGTexDB *g_itemDB;
extern PNGTexDB *g_progUseDB;
extern PNGTexOffDB *g_equipDB;
extern FontexDB *g_fontexDB;

namespace
{
    constexpr int charX = 90;
    constexpr int charY = 200;

    void drawText(ImDrawList *list, ImVec2 pos, const std::string &text, uint8_t font = 1, uint8_t size = 12, ImU32 color = IM_COL32_WHITE, bool center = false)
    {
        if(const auto texture = g_fontexDB->retrieve(font, size, 0, text.c_str()); texture){
            if(center){ pos.x -= texture.w * 0.5f; }
            list->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h}, {0, 0}, {1, 1}, color);
        }
    }

    bool closeButton(ImVec2 pos)
    {
        const auto hover = g_progUseDB->retrieve(0X0000001C);
        if(!hover){ return false; }
        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton("##state-close", {to_f(hover.w), to_f(hover.h)});
        if(ImGui::IsItemHovered() || ImGui::IsItemActive()){
            const auto down = g_progUseDB->retrieve(0X0000001D);
            const auto shown = ImGui::IsItemActive() && down ? down : hover;
            ImGui::GetWindowDrawList()->AddImage(shown, pos, {pos.x + shown.w, pos.y + shown.h});
        }
        return clicked;
    }

    std::vector<std::string> plainLines(const std::u8string &layout)
    {
        const std::string source = to_cstr(layout);
        std::vector<std::string> result;
        std::string line;
        bool tag = false;
        for(const char ch: source){
            if(ch == '<'){ tag = true; }
            else if(ch == '>'){ tag = false; }
            else if(!tag && ch == '\n'){
                if(!line.empty()){ result.push_back(std::move(line)); line.clear(); }
            }
            else if(!tag){ line.push_back(ch); }
        }
        if(!line.empty()){ result.push_back(std::move(line)); }
        return result;
    }
}

ImPlayerStateBoard::ImPlayerStateBoard(ProcessRun *processRun)
    : ImBoard("##player-state-board")
    , m_processRun(fflcheck(processRun))
    , m_gridList([]
      {
          std::array<WearGrid, WLG_END> grids {};
          grids[WLG_DRESS]    = {charX,      charY - 100, 60, 110};
          grids[WLG_HELMET]   = {charX + 10, charY - 135, 30, 25};
          grids[WLG_WEAPON]   = {charX - 50, charY - 120, 45, 90};
          grids[WLG_SHOES]    = {10,  240, SYS_INVGRIDPW, 56};
          grids[WLG_NECKLACE] = {168,  88};
          grids[WLG_ARMRING0] = {10,  155};
          grids[WLG_ARMRING1] = {168, 155};
          grids[WLG_RING0]    = {10,  195};
          grids[WLG_RING1]    = {168, 195};
          grids[WLG_TORCH]    = {88,  265};
          grids[WLG_CHARM]    = {128, 265};
          return grids;
      }())
{}

void ImPlayerStateBoard::draw() const
{
    if(!show()){ return; }
    const auto background = g_progUseDB->retrieve(0X06000000);
    const auto hero = m_processRun->getMyHero();
    if(!(background && hero)){ return; }

    if(beginWindow({to_f(background.w), to_f(background.h)})){
        const auto pos = ImGui::GetWindowPos();
        auto *list = ImGui::GetWindowDrawList();
        list->AddImage(background, pos, {pos.x + background.w, pos.y + background.h});
        if(closeButton({pos.x + 288, pos.y + 13})){ setShow(false); }

        drawText(list, {pos.x + 164, pos.y + 38}, hero->getName(), 1, 12, hero->getNameColor() | 0XFF, true);

        const auto drawEquipLayer = [list, pos](GLTexID texture, int dx, int dy)
        {
            list->AddImage(texture, {pos.x + charX + dx, pos.y + charY + dy}, {pos.x + charX + dx + texture.w, pos.y + charY + dy + texture.h});
        };
        if(auto [texture, dx, dy] = g_equipDB->retrieve(hero->gender() ? 0X00000000 : 0X00000001); texture){
            drawEquipLayer(texture, dx, dy);
        }
        if(const auto itemID = hero->getWLItem(WLG_DRESS).itemID){
            const auto &record = DBCOM_ITEMRECORD(itemID);
            if(record.pkgGfxID >= 0){
                if(auto [texture, dx, dy] = g_equipDB->retrieve(to_u32(record.pkgGfxID) | 0X01000000); texture){
                    drawEquipLayer(texture, dx, dy);
                }
            }
        }
        if(const auto itemID = hero->getWLItem(WLG_WEAPON).itemID){
            const auto &record = DBCOM_ITEMRECORD(itemID);
            if(record.shape > 0){
                if(auto [texture, dx, dy] = g_equipDB->retrieve(0X01000000 + record.pkgGfxID); texture){ drawEquipLayer(texture, dx, dy); }
            }
        }
        if(const auto itemID = hero->getWLItem(WLG_HELMET).itemID){
            const auto &record = DBCOM_ITEMRECORD(itemID);
            if(record.shape > 0){
                if(auto [texture, dx, dy] = g_equipDB->retrieve(0X01000000 + record.pkgGfxID); texture){ drawEquipLayer(texture, dx, dy); }
            }
        }
        else if(hero->getWLDesp().hair >= HAIR_BEGIN){
            if(auto [texture, dx, dy] = g_equipDB->retrieve((hero->gender() ? 0X0000003C : 0X00000046) + hero->getWLDesp().hair - HAIR_BEGIN); texture){
                drawEquipLayer(texture, dx, dy);
            }
        }

        for(int i = WLG_W_BEGIN; i < WLG_W_END; ++i){
            const auto &item = hero->getWLItem(i);
            if(!item){ continue; }
            const auto &record = DBCOM_ITEMRECORD(item.itemID);
            if(const auto texture = g_itemDB->retrieve(record.pkgGfxID | 0X01000000); texture){
                const auto &grid = m_gridList.at(i);
                const ImVec2 itemPos
                {
                    pos.x + grid.x + (grid.w - texture.w) * 0.5f,
                    pos.y + grid.y + (grid.h - texture.h) / to_f(i == WLG_SHOES ? 1 : 2),
                };
                list->AddImage(texture, itemPos, {itemPos.x + texture.w, itemPos.y + texture.h});
            }
        }

        const auto combat = hero->getCombatNode();
        int bodyLoad = 0;
        for(int i = WLG_BEGIN; i < WLG_END; ++i){
            if(i != WLG_WEAPON && hero->getWLItem(i)){ bodyLoad += DBCOM_ITEMRECORD(hero->getWLItem(i).itemID).weight; }
        }
        const int weaponLoad = hero->getWLItem(WLG_WEAPON) ? DBCOM_ITEMRECORD(hero->getWLItem(WLG_WEAPON).itemID).weight : 0;
        const int invLoad = hero->getInvPack().getWeight();
        const bool health = hero->getSDHealth().has_value();
        const std::array<std::pair<std::string, bool>, 9> values
        {{
            {str_printf("%d", to_d(hero->getLevel())), false},
            {str_printf("%.2f%%", hero->getLevelRatio() * 100.0), false},
            {str_printf("%d/%d", health ? hero->getSDHealth()->hp : 0, health ? hero->getSDHealth()->getMaxHP() : 0), false},
            {str_printf("%d/%d", health ? hero->getSDHealth()->mp : 0, health ? hero->getSDHealth()->getMaxMP() : 0), false},
            {str_printf("%d/%d", invLoad, combat.load.inventory), invLoad > combat.load.inventory},
            {str_printf("%d/%d", bodyLoad, combat.load.body), bodyLoad > combat.load.body},
            {str_printf("%d/%d", weaponLoad, combat.load.weapon), weaponLoad > combat.load.weapon},
            {str_printf("%d", combat.dcHit), false},
            {str_printf("%d", combat.dcDodge), false},
        }};
        for(size_t i = 0; i < values.size(); ++i){
            drawText(list, {pos.x + 279, pos.y + 97 + i * 24}, values[i].first, 1, 12, values[i].second ? IM_COL32(255, 0, 0, 255) : IM_COL32_WHITE, true);
        }

        drawText(list, {pos.x + 21,  pos.y + 317}, str_printf("攻击 %d - %d", combat.dc[0], combat.dc[1]), 9, 15);
        drawText(list, {pos.x + 130, pos.y + 317}, str_printf("防御 %d - %d", combat.ac[0], combat.ac[1]), 9, 15);
        drawText(list, {pos.x + 21,  pos.y + 345}, str_printf("魔法 %d - %d", combat.mc[0], combat.mc[1]), 9, 15);
        drawText(list, {pos.x + 130, pos.y + 345}, str_printf("魔防 %d - %d", combat.mac[0], combat.mac[1]), 9, 15);
        drawText(list, {pos.x + 233, pos.y + 345}, str_printf("道术 %d - %d", combat.sc[0], combat.sc[1]), 9, 15);
        drawText(list, {pos.x + 10, pos.y + 376}, "攻击元素", 9, 15);
        drawText(list, {pos.x + 10, pos.y + 406}, "防御元素", 9, 15);
        drawText(list, {pos.x + 10, pos.y + 436}, "弱点元素", 9, 15);

        for(int i = MET_BEGIN; i < MET_END; ++i){
            std::array<int, 2> element {};
            switch(i){
                case MET_FIRE   : element = {combat.dcElem.fire,    combat.acElem.fire};    break;
                case MET_ICE    : element = {combat.dcElem.ice,     combat.acElem.ice};     break;
                case MET_LIGHT  : element = {combat.dcElem.light,   combat.acElem.light};   break;
                case MET_WIND   : element = {combat.dcElem.wind,    combat.acElem.wind};    break;
                case MET_HOLY   : element = {combat.dcElem.holy,    combat.acElem.holy};    break;
                case MET_DARK   : element = {combat.dcElem.dark,    combat.acElem.dark};    break;
                case MET_PHANTOM: element = {combat.dcElem.phantom, combat.acElem.phantom}; break;
                default: break;
            }
            const float x = pos.x + 62 + (i - MET_BEGIN) * 37;
            const auto drawElement = [list, x, i](float y, uint32_t baseID, int value, ImU32 color)
            {
                if(const auto icon = g_progUseDB->retrieve(baseID + to_u32(i - MET_BEGIN)); icon){
                    list->AddImage(icon, {x, y}, {x + icon.w, y + icon.h});
                }
                drawText(list, {x + 20, y + 1}, str_printf("%+d", value), 1, 12, color);
            };
            if(element[0] > 0){ drawElement(pos.y + 374, 0X06000010, element[0], IM_COL32(0, 255, 0, 255)); }
            if(element[1] > 0){ drawElement(pos.y + 404, 0X06000010, element[1], IM_COL32(0, 255, 0, 255)); }
            if(element[1] < 0){ drawElement(pos.y + 434, 0X06000020, element[1], IM_COL32(255, 0, 0, 255)); }
        }

        const auto mouse = ImGui::GetIO().MousePos;
        int hovered = -1;
        for(int i = WLG_BEGIN; i < WLG_END; ++i){
            const auto &grid = m_gridList.at(i);
            if(mouse.x >= pos.x + grid.x && mouse.x < pos.x + grid.x + grid.w && mouse.y >= pos.y + grid.y && mouse.y < pos.y + grid.y + grid.h){
                hovered = i;
                if(i >= WLG_W_BEGIN && i < WLG_W_END){
                    const uint32_t texID = i == WLG_SHOES ? 0X06000002 : 0X06000001;
                    if(const auto cover = g_progUseDB->retrieve(texID); cover){
                        const ImVec2 coverPos {pos.x + grid.x - 1, pos.y + grid.y + (i == WLG_SHOES ? -6 : -3)};
                        list->AddImage(cover, coverPos, {coverPos.x + cover.w, coverPos.y + cover.h}, {0, 0}, {1, 1}, IM_COL32(255, 255, 255, 128));
                    }
                }
                break;
            }
        }
        if(hovered >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
            auto &pack = hero->getInvPack();
            if(const auto grabbed = pack.getGrabbedItem()){
                if(hero->canWear(grabbed.itemID, hovered)){ m_processRun->requestEquipWear(grabbed.itemID, grabbed.seqID, hovered); }
                else{ pack.add(grabbed); pack.setGrabbedItem({}); }
            }
            else if(hero->getWLItem(hovered)){ m_processRun->requestGrabWear(hovered); }
        }
        if(hovered >= 0 && hero->getWLItem(hovered)){
            const auto lines = plainLines(hero->getWLItem(hovered).getXMLLayout());
            const float h = std::max(40.0f, 20.0f + lines.size() * 15.0f);
            auto *fg = ImGui::GetForegroundDrawList();
            fg->AddRectFilled(mouse, {mouse.x + 220, mouse.y + h}, IM_COL32(0, 0, 0, 200), 5);
            fg->AddRect(mouse, {mouse.x + 220, mouse.y + h}, IM_COL32(231, 231, 189, 200), 5);
            for(size_t i = 0; i < lines.size(); ++i){ drawText(fg, {mouse.x + 10, mouse.y + 10 + i * 15}, lines[i]); }
        }
    }
    endWindow();
}

bool ImPlayerStateBoard::processEvent(const MirEvent &event) const
{
    if(show() && event.type == MIR_EVENT_KEY_DOWN && event.key.key == MIRK_ESCAPE){ setShow(false); return true; }
    return ImBoard::processEvent(event);
}
