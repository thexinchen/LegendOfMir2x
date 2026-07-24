#pragma once
#include <cstring>
#include <memory>

#include "zsdb.hpp"
#include "inndb.hpp"
#include "audioclip.hpp"

struct BGMusicElement
{
    std::shared_ptr<AudioClip> music = nullptr; // miniaudio decodes at play time
    std::vector<uint8_t> musicFileData;
};

class BGMusicDB: public innDB<uint32_t, BGMusicElement>
{
    private:
        std::unique_ptr<ZSDB> m_zsdbPtr;

    public:
        BGMusicDB(size_t resMax)
            : innDB<uint32_t, BGMusicElement>(resMax)
        {}

    public:
        virtual ~BGMusicDB() = default;

    public:
        void load(const char *bgmDBName)
        {
            m_zsdbPtr = std::make_unique<ZSDB>(bgmDBName);
        }

    public:
        std::shared_ptr<AudioClip> retrieve(uint32_t key)
        {
            if(auto p = innLoad(key)){
                return p->music;
            }
            return nullptr;
        }

    public:
        std::optional<std::tuple<BGMusicElement, size_t>> loadResource(uint32_t) override;
        void freeResource(BGMusicElement &element) override;
};
