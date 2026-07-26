#pragma once

#include <memory>
#include <tuple>
#include <unordered_map>

#include <GLTexture.hpp>
#include "zsdb.hpp"

class MagicFrameDB final
{
    private:
        struct CachedFrame
        {
            int dx = 0;
            int dy = 0;
            GLTexture image;
        };

        std::unique_ptr<ZSDB> m_zsdbPtr;
        std::unordered_map<uint32_t, CachedFrame> m_cachedFrameList;

    public:
        explicit MagicFrameDB(const char *zsdbPath)
            : m_zsdbPtr(std::make_unique<ZSDB>(zsdbPath))
        {}

        std::tuple<GLTexture *, int, int> retrieve(uint32_t);
};
