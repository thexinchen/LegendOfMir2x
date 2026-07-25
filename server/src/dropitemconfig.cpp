#include <cstdint>
#include <cstdlib>
#include <cinttypes>

#include "mathf.hpp"
#include "dbcomid.hpp"
#include "server.hpp"
#include "peerconfig.hpp"
#include "dropitemconfig.hpp"
#include "serverargparser.hpp"
#include "imguiui/guicore.hpp"
#include "dropitemdata.hpp"

extern PeerConfig *g_peerConfig;
extern ServerArgParser *g_serverArgParser;
extern GUICore *g_guiCore;

static bool validDropItemConfig(const InnDropItemConfig &node)
{
    return true
        && DBCOM_MONSTERID(node.monsterName)
        && DBCOM_ITEMID(node.itemName)

        && node.group     >= 0
        && node.probRecip >= 1

        && node.repeat >= 1
        && node.count  >= 1;
}

const std::map<int, std::vector<DropItemConfig>> &getMonsterDropItemConfigList(uint32_t monsterID)
{
    const static auto s_monsterDropitemConfigList = []()
    {
        // data split into separate TUs to avoid MSVC code generator crash (ICE 0xC0000005)
        std::vector<InnDropItemConfig> dropItemConfigNodeList;
        for(const auto &fn: {
            getDropItemChunk000,
            getDropItemChunk001,
            getDropItemChunk002,
            getDropItemChunk003,
            getDropItemChunk004,
            getDropItemChunk005,
            getDropItemChunk006,
            getDropItemChunk007,
            getDropItemChunk008,
            getDropItemChunk009,
            getDropItemChunk010,
            getDropItemChunk011,
            getDropItemChunk012,
            getDropItemChunk013,
            getDropItemChunk014,
            getDropItemChunk015,
            getDropItemChunk016,
            getDropItemChunk017,
            getDropItemChunk018,
            getDropItemChunk019,
            getDropItemChunk020,
            getDropItemChunk021,
            getDropItemChunk022,
            getDropItemChunk023,
            getDropItemChunk024,
            getDropItemChunk025,
            getDropItemChunk026,
            getDropItemChunk027,
            getDropItemChunk028,
            getDropItemChunk029,
            getDropItemChunk030,
            getDropItemChunk031,
            getDropItemChunk032,
            getDropItemChunk033,
            getDropItemChunk034,
            getDropItemChunk035,
            getDropItemChunk036,
            getDropItemChunk037,
            getDropItemChunk038,
            getDropItemChunk039,
            getDropItemChunk040,
            getDropItemChunk041,
            getDropItemChunk042,
            getDropItemChunk043,
            getDropItemChunk044,
            getDropItemChunk045,
            getDropItemChunk046,
            getDropItemChunk047,
            getDropItemChunk048,
            getDropItemChunk049,
            getDropItemChunk050,
            getDropItemChunk051,
            getDropItemChunk052,
        }){
            auto chunk = fn();
            dropItemConfigNodeList.insert(dropItemConfigNodeList.end(), chunk.begin(), chunk.end());
        }

        std::unordered_map<uint32_t, std::map<int, std::vector<DropItemConfig>>> monsterDropItemList;
        for(const auto &node: dropItemConfigNodeList){
            if(!validDropItemConfig(node)){
                continue;
            }

            const auto monsterID = DBCOM_MONSTERID(node.monsterName);
            if(!monsterID){
                continue;
            }

            const auto itemID = DBCOM_ITEMID(node.itemName);
            if(!itemID){
                continue;
            }

            for(int i = 0; i < node.repeat; ++i){
                monsterDropItemList[monsterID][node.group].push_back(DropItemConfig
                {
                    .itemID    = itemID,
                    .probRecip = node.probRecip,
                    .count     = node.count,
                });
            }
        }
        return monsterDropItemList;
    }();

    const static std::map<int, std::vector<DropItemConfig>> s_emptyConfigList;
    if(!DBCOM_MONSTERRECORD(monsterID)){
        return s_emptyConfigList;
    }

    if(const auto p = s_monsterDropitemConfigList.find(monsterID); p != s_monsterDropitemConfigList.end()){
        return p->second;
    }
    return s_emptyConfigList;
}

std::vector<SDItem> getMonsterDropItemList(uint32_t monsterID)
{
    std::vector<SDItem> itemList;
    const auto [dropRate, goldRate] = []() -> std::tuple<double, double>
    {
        if(g_serverArgParser->slave){
            const auto sdPC = g_peerConfig->getConfig();
            return
            {
                sdPC.dropRate,
                sdPC.goldRate,
            };
        }
        else{
            const auto confg = g_guiCore->getConfig();
            return
            {
                confg.dropRate,
                confg.goldRate,
            };
        }
    }();

    fflassert(dropRate >= 0.0);
    fflassert(goldRate >= 0.0);

    for(const auto &[group, dropItemList]: getMonsterDropItemConfigList(monsterID)){
        for(const auto &dropItem: dropItemList){
            const auto adjProbRecip = [&dropItem, dropRate]() -> int
            {
                if(dropItem.probRecip <= 0){
                    return 0;
                }
                return std::max<int>(1, std::lround(dropItem.probRecip / dropRate));
            }();

            if((dropItem.probRecip > 0) && ((mathf::rand() % adjProbRecip) == 0)){
                const auto &ir = DBCOM_ITEMRECORD(dropItem.itemID);
                fflassert(ir);

                if(ir.isGold()){
                    const auto adjGold = std::max<size_t>(1, std::lround(dropItem.count * goldRate));
                    for(const auto &goldItem: SDItem::buildGoldItem(adjGold)){
                        itemList.push_back(SDItem
                        {
                            .itemID = goldItem.itemID,
                            .seqID  = 1,
                            .count  = std::max<size_t>(1, std::lround(goldItem.count * mathf::rand<float>(0.9, 1.1))),
                        });
                    }
                }
                else if(ir.packable()){
                    itemList.push_back(SDItem
                    {
                        .itemID = dropItem.itemID,
                        .seqID  = 1,
                        .count  = to_uz(dropItem.count),
                    });
                }
                else{
                    for(int i = 0; i < dropItem.count; ++i){
                        itemList.push_back(SDItem
                        {
                            .itemID = dropItem.itemID,
                            .seqID  = 1,
                            .count  = 1,
                        });
                    }
                }

                if(group > 0){
                    break;  // can only drop one item if not in zero group
                }
            }
        }
    }
    return itemList;
}
