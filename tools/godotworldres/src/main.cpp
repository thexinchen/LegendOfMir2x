#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "dbcomid.hpp"
#include "fflerror.hpp"
#include "hexstr.hpp"
#include "itemrecord.hpp"
#include "gui/ImSkillBoard.hpp"
#include "mir2xmapdata.hpp"
#include "strf.hpp"
#include "totype.hpp"
#include "zsdb.hpp"

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct MapHeader
{
    char magic[4] {'M', '2', 'G', 'W'};
    uint32_t version = 1;
    uint32_t mapID = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t tileCount = 0;
    uint32_t objectCount = 0;
};

struct TileRecord
{
    uint16_t x = 0;
    uint16_t y = 0;
    uint32_t textureID = 0;
};

struct ObjectRecord
{
    uint16_t x = 0;
    uint16_t y = 0;
    uint32_t textureID = 0;
    uint8_t depth = 0;
    uint8_t flags = 0;
    uint8_t tickType = 0;
    uint8_t frameCount = 0;
};

struct SpriteHeader
{
    char magic[4] {'M', '2', 'S', 'P'};
    uint32_t version = 1;
    uint32_t spriteCount = 0;
};

struct SpriteRecord
{
    uint32_t key = 0;
    int16_t dx = 0;
    int16_t dy = 0;
};

struct MonsterMetaRecord
{
    uint32_t monsterID = 0;
    uint16_t lookID = 0;
    uint8_t shadow = 0;
    uint8_t reserved = 0;
};

struct ItemMetaRecord
{
    uint32_t itemID = 0;
    uint16_t shape = 0;
    uint16_t reserved = 0;
    uint32_t pkgGfxID = 0;
};

struct SkillMetaRecord
{
    uint32_t magicID = 0;
    uint32_t iconID = 0;
    uint8_t page = 0;
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t flags = 0;
};

struct BuffMetaRecord
{
    uint32_t buffID = 0;
    uint32_t iconID = 0;
    int8_t favor = 0;
    uint8_t reserved[3] {};
};

struct MagicEffectMetaRecord
{
    uint32_t magicID = 0;
    uint32_t gfxID = 0;
    uint32_t modColor = 0;
    uint16_t frameCount = 0;
    uint16_t gfxIDCount = 0;
    uint16_t speed = 0;
    uint8_t stage = 0;
    uint8_t type = 0;
    uint8_t gfxDirType = 0;
    uint8_t flags = 0;
};
#pragma pack(pop)

static_assert(sizeof(MapHeader) == 28);
static_assert(sizeof(TileRecord) == 8);
static_assert(sizeof(ObjectRecord) == 12);
static_assert(sizeof(SpriteHeader) == 12);
static_assert(sizeof(SpriteRecord) == 8);
static_assert(sizeof(MonsterMetaRecord) == 8);
static_assert(sizeof(ItemMetaRecord) == 12);
static_assert(sizeof(SkillMetaRecord) == 12);
static_assert(sizeof(BuffMetaRecord) == 12);
static_assert(sizeof(MagicEffectMetaRecord) == 22);

static bool animatedTextureSet(uint32_t textureID)
{
    switch(textureID >> 16){
        case 11:
        case 26:
        case 41:
        case 56:
        case 71:
            return true;
        default:
            return false;
    }
}

template<typename T> static void writeVector(std::ofstream &ofs, const std::vector<T> &data)
{
    if(!data.empty()){
        ofs.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size() * sizeof(T)));
    }
}

static std::string mapBinName(uint32_t mapID)
{
    const auto &record = DBCOM_MAPRECORD(mapID);
    if(!record){
        return {};
    }
    const std::string mapName = to_cstr(record.name);
    if(const auto pos = mapName.find('_'); pos != std::string::npos && pos + 1 < mapName.size()){
        return mapName.substr(pos + 1) + ".bin";
    }
    return {};
}

static void extractTexture(ZSDB &textureDB, uint32_t textureID, const fs::path &textureDir)
{
    char key[16] {};
    const auto keyString = hexstr::to_string<uint32_t, 4>(textureID, key, true);
    const auto outputPath = textureDir / (std::string(keyString, 8) + ".png");
    if(fs::exists(outputPath)){
        return;
    }

    std::vector<uint8_t> pngData;
    if(!textureDB.decomp(keyString, 8, &pngData)){
        throw fflpanic("missing map texture: 0x{:08X}", textureID);
    }
    std::ofstream ofs(outputPath, std::ios::binary);
    writeVector(ofs, pngData);
    if(!ofs){
        throw fflpanic("failed to write texture: {}", outputPath.string());
    }
}

static size_t convertMap(uint32_t mapID, ZSDB &mapDB, ZSDB &textureDB, const fs::path &outputDir)
{
    const auto binName = mapBinName(mapID);
    if(binName.empty()){
        return 0;
    }

    std::vector<uint8_t> mapDataBuf;
    if(!mapDB.decomp(binName.c_str(), 0, &mapDataBuf)){
        std::fprintf(stderr, "skip map %u: missing %s\n", mapID, binName.c_str());
        return 0;
    }
    const Mir2xMapData mapData(mapDataBuf.data(), mapDataBuf.size());

    std::vector<uint8_t> landList(mapData.w() * mapData.h());
    std::vector<TileRecord> tileList;
    std::vector<ObjectRecord> objectList;
    std::unordered_set<uint32_t> textureSet;

    for(size_t y = 0; y < mapData.h(); ++y){
        for(size_t x = 0; x < mapData.w(); ++x){
            const auto &cell = mapData.cell(to_d(x), to_d(y));
            landList[x + y * mapData.w()] = to_u8(cell.land.type | (cell.land.canFly << 6) | (cell.land.canWalk << 7));

            if(!(x % 2) && !(y % 2)){
                const auto &tile = mapData.tile(to_d(x), to_d(y));
                if(tile.valid){
                    tileList.push_back({to_u16(x), to_u16(y), tile.texID});
                    textureSet.insert(tile.texID);
                }
            }

            for(const auto &object: cell.obj){
                if(!object.valid){
                    continue;
                }
                objectList.push_back({
                    to_u16(x),
                    to_u16(y),
                    object.texID,
                    to_u8(object.depth),
                    to_u8((object.animated ? 1 : 0) | (object.alpha ? 2 : 0)),
                    to_u8(object.tickType),
                    to_u8(object.frameCount),
                });
                textureSet.insert(object.texID);
                if(object.animated && animatedTextureSet(object.texID)){
                    for(uint32_t frame = 1; frame < object.frameCount; ++frame){
                        textureSet.insert(object.texID + frame);
                    }
                }
            }
        }
    }

    const auto mapDir = outputDir / "maps";
    const auto textureDir = outputDir / "textures";
    fs::create_directories(mapDir);
    fs::create_directories(textureDir);

    const MapHeader header {
        .mapID = mapID,
        .width = to_u32(mapData.w()),
        .height = to_u32(mapData.h()),
        .tileCount = to_u32(tileList.size()),
        .objectCount = to_u32(objectList.size()),
    };
    const auto mapPath = mapDir / str_printf("%08X.m2xmap", mapID);
    std::ofstream ofs(mapPath, std::ios::binary);
    ofs.write(reinterpret_cast<const char *>(&header), sizeof(header));
    writeVector(ofs, landList);
    writeVector(ofs, tileList);
    writeVector(ofs, objectList);
    if(!ofs){
        throw fflpanic("failed to write map: {}", mapPath.string());
    }

    const auto metaPath = mapDir / str_printf("%08X.m2xmeta", mapID);
    std::ofstream metaFile(metaPath, std::ios::binary);
    const uint32_t miniMapID = DBCOM_MAPRECORD(mapID).miniMapID.value_or(UINT32_MAX);
    metaFile.write(reinterpret_cast<const char *>(&miniMapID), sizeof(miniMapID));
    if(!metaFile){
        throw fflpanic("failed to write map metadata: {}", metaPath.string());
    }

    for(const auto textureID: textureSet){
        extractTexture(textureDB, textureID, textureDir);
    }

    std::printf("map %u: %zux%zu, %zu tiles, %zu objects, %zu textures\n", mapID, mapData.w(), mapData.h(), tileList.size(), objectList.size(), textureSet.size());
    return textureSet.size();
}

static size_t convertSprites(const char *family, const char *dbPath, const fs::path &outputDir)
{
    ZSDB db(dbPath);
    const auto spriteDir = outputDir / "sprites" / family;
    fs::create_directories(spriteDir);

    std::vector<SpriteRecord> spriteList;
    std::vector<uint8_t> pngData;
    for(const auto &entry: db.getEntryList()){
        if(!(entry.fileName && std::strlen(entry.fileName) >= 8)){
            continue;
        }
        const auto key = hexstr::to_hex<uint32_t, 4>(entry.fileName);
        const bool hasOffset = std::strlen(entry.fileName) >= 18;
        const int dx = hasOffset ? (entry.fileName[8] != '0' ? 1 : -1) * to_d(hexstr::to_hex<uint32_t, 2>(entry.fileName + 10)) : 0;
        const int dy = hasOffset ? (entry.fileName[9] != '0' ? 1 : -1) * to_d(hexstr::to_hex<uint32_t, 2>(entry.fileName + 14)) : 0;
        spriteList.push_back({key, check_cast<int16_t>(dx), check_cast<int16_t>(dy)});

        const auto outputPath = spriteDir / str_printf("%08X.png", key);
        if(!fs::exists(outputPath)){
            if(!db.decomp(entry.fileName, 0, &pngData)){
                throw fflpanic("failed to extract sprite: {}", entry.fileName);
            }
            std::ofstream ofs(outputPath, std::ios::binary);
            writeVector(ofs, pngData);
        }
    }

    const auto indexPath = outputDir / "sprites" / (std::string(family) + ".m2xindex");
    std::ofstream ofs(indexPath, std::ios::binary);
    const SpriteHeader header {.spriteCount = to_u32(spriteList.size())};
    ofs.write(reinterpret_cast<const char *>(&header), sizeof(header));
    writeVector(ofs, spriteList);
    if(!ofs){
        throw fflpanic("failed to write sprite index: {}", indexPath.string());
    }

    if(std::strcmp(family, "monster") == 0){
        std::vector<MonsterMetaRecord> metaList;
        for(uint32_t monsterID = 1; monsterID < DBCOM_MONSTERENDID(); ++monsterID){
            const auto &record = DBCOM_MONSTERRECORD(monsterID);
            if(record.name){
                metaList.push_back({monsterID, check_cast<uint16_t>(record.lookID), to_u8(record.shadow)});
            }
        }
        std::ofstream metaFile(outputDir / "sprites" / "monster.m2xmeta", std::ios::binary);
        const SpriteHeader metaHeader {.spriteCount = to_u32(metaList.size())};
        metaFile.write(reinterpret_cast<const char *>(&metaHeader), sizeof(metaHeader));
        writeVector(metaFile, metaList);
    }
    if(std::strcmp(family, "hero") == 0){
        std::vector<ItemMetaRecord> metaList;
        for(uint32_t itemID = 1; itemID < DBCOM_ITEMENDID(); ++itemID){
            const auto &record = DBCOM_ITEMRECORD(itemID);
            if(record.name){
                metaList.push_back({itemID, check_cast<uint16_t>(record.shape), to_u16(record.packable()), check_cast<uint32_t>(record.pkgGfxID)});
            }
        }
        std::ofstream metaFile(outputDir / "sprites" / "item.m2xmeta", std::ios::binary);
        const SpriteHeader metaHeader {.spriteCount = to_u32(metaList.size())};
        metaFile.write(reinterpret_cast<const char *>(&metaHeader), sizeof(metaHeader));
        writeVector(metaFile, metaList);

        std::ofstream nameFile(outputDir / "sprites" / "item_name.m2xmeta", std::ios::binary);
        nameFile.write(reinterpret_cast<const char *>(&metaHeader), sizeof(metaHeader));
        for(const auto &meta: metaList){
            const std::string name(to_cstr(DBCOM_ITEMRECORD(meta.itemID).name));
            const auto length = check_cast<uint16_t>(name.size());
            nameFile.write(reinterpret_cast<const char *>(&meta.itemID), sizeof(meta.itemID));
            nameFile.write(reinterpret_cast<const char *>(&length), sizeof(length));
            nameFile.write(name.data(), length);
        }
    }
    if(std::strcmp(family, "proguse") == 0){
        std::vector<SkillMetaRecord> metaList;
        for(const auto &gfx: SkillBoardData::m_iconGfxList){
            const auto &record = DBCOM_MAGICRECORD(gfx.magicID);
            if(!record){
                continue;
            }
            const auto elemID = magicElemID(record.elem);
            const int page = elemID == MET_NONE
                           ? 7
                           : (elemID >= MET_BEGIN && elemID < MET_END ? elemID - MET_BEGIN : -1);
            if(page < 0){
                continue;
            }
            metaList.push_back({
                gfx.magicID,
                gfx.magicIcon,
                check_cast<uint8_t>(page),
                check_cast<uint8_t>(gfx.x),
                check_cast<uint8_t>(gfx.y),
                to_u8(gfx.passive),
            });
        }
        std::ofstream metaFile(outputDir / "sprites" / "skill.m2xmeta", std::ios::binary);
        const SpriteHeader metaHeader {.spriteCount = to_u32(metaList.size())};
        metaFile.write(reinterpret_cast<const char *>(&metaHeader), sizeof(metaHeader));
        writeVector(metaFile, metaList);

        std::vector<BuffMetaRecord> buffList;
        for(uint32_t buffID = 1; buffID < DBCOM_BUFFENDID(); ++buffID){
            const auto &record = DBCOM_BUFFRECORD(buffID);
            if(record.name && record.icon.show && record.icon.gfxID != SYS_U32NIL){
                buffList.push_back({buffID, record.icon.gfxID, check_cast<int8_t>(record.favor)});
            }
        }
        std::ofstream buffFile(outputDir / "sprites" / "buff.m2xmeta", std::ios::binary);
        const SpriteHeader buffHeader {.spriteCount = to_u32(buffList.size())};
        buffFile.write(reinterpret_cast<const char *>(&buffHeader), sizeof(buffHeader));
        writeVector(buffFile, buffList);
    }
    if(std::strcmp(family, "magic") == 0){
        std::vector<MagicEffectMetaRecord> metaList;
        for(uint32_t magicID = 1; magicID < DBCOM_MAGICENDID(); ++magicID){
            const auto &record = DBCOM_MAGICRECORD(magicID);
            if(!record){
                continue;
            }
            for(int stage = MST_BEGIN; stage < MST_END; ++stage){
                const auto [gfxEntry, gfxRef] = DBCOM_MAGICGFXENTRY(magicID, magicStageName(stage));
                if(!(gfxEntry && gfxEntry->gfxID != SYS_U32NIL && gfxEntry->frameCount > 0)){
                    continue;
                }
                metaList.push_back({
                    magicID,
                    gfxEntry->gfxID,
                    gfxRef ? gfxRef->modColor : gfxEntry->modColor,
                    check_cast<uint16_t>(gfxEntry->frameCount),
                    check_cast<uint16_t>(gfxEntry->gfxIDCount),
                    check_cast<uint16_t>(gfxEntry->speed),
                    check_cast<uint8_t>(stage),
                    check_cast<uint8_t>(magicGfxEntryID(gfxEntry->type)),
                    check_cast<uint8_t>(gfxEntry->gfxDirType),
                    to_u8((gfxEntry->loop ? 1 : 0) | (gfxEntry->onGround ? 2 : 0)),
                });
            }
        }
        std::ofstream metaFile(outputDir / "sprites" / "magic.m2xmeta", std::ios::binary);
        const SpriteHeader metaHeader {.spriteCount = to_u32(metaList.size())};
        metaFile.write(reinterpret_cast<const char *>(&metaHeader), sizeof(metaHeader));
        writeVector(metaFile, metaList);

        std::ofstream nameFile(outputDir / "sprites" / "magic_name.m2xmeta", std::ios::binary);
        uint32_t nameCount = 0;
        for(uint32_t magicID = 1; magicID < DBCOM_MAGICENDID(); ++magicID){
            nameCount += DBCOM_MAGICRECORD(magicID) ? 1 : 0;
        }
        const SpriteHeader nameHeader {.spriteCount = nameCount};
        nameFile.write(reinterpret_cast<const char *>(&nameHeader), sizeof(nameHeader));
        for(uint32_t magicID = 1; magicID < DBCOM_MAGICENDID(); ++magicID){
            if(!DBCOM_MAGICRECORD(magicID)){
                continue;
            }
            const std::string name(to_cstr(DBCOM_MAGICRECORD(magicID).name));
            const auto length = check_cast<uint16_t>(name.size());
            nameFile.write(reinterpret_cast<const char *>(&magicID), sizeof(magicID));
            nameFile.write(reinterpret_cast<const char *>(&length), sizeof(length));
            nameFile.write(name.data(), length);
        }
    }
    std::printf("sprite family %s: %zu frames\n", family, spriteList.size());
    return spriteList.size();
}

int main(int argc, char *argv[])
{
    if(argc >= 5 && std::strcmp(argv[1], "--sprites") == 0 && (argc % 2 == 1)){
        const fs::path outputDir(argv[2]);
        size_t totalCount = 0;
        for(int index = 3; index + 1 < argc; index += 2){
            totalCount += convertSprites(argv[index], argv[index + 1], outputDir);
        }
        return totalCount ? 0 : 1;
    }
    if(argc != 4 && argc != 5){
		std::fprintf(stderr, "Usage: godotworldres mapbin.zsdb map.zsdb output-dir [map-id]\n");
		std::fprintf(stderr, "       godotworldres --sprites output-dir family sprite.zsdb [family sprite.zsdb ...]\n");
        return 2;
    }

    ZSDB mapDB(argv[1]);
    ZSDB textureDB(argv[2]);
    const fs::path outputDir(argv[3]);
    if(argc == 5){
        const auto mapID = check_cast<uint32_t>(std::stoul(argv[4]));
        return convertMap(mapID, mapDB, textureDB, outputDir) ? 0 : 1;
    }

    size_t mapCount = 0;
    for(uint32_t mapID = 1; mapID < DBCOM_MAPENDID(); ++mapID){
        if(convertMap(mapID, mapDB, textureDB, outputDir)){
            ++mapCount;
        }
    }
    std::printf("converted %zu maps\n", mapCount);
    return mapCount ? 0 : 1;
}
