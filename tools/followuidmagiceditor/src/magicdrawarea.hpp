#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <imgui.h>

#include "dbcomid.hpp"
#include "magicframedb.hpp"

class MagicDrawArea
{
    private:
        bool m_adjustR = false;
        int m_adjustTargetOff = -1;
        uint32_t m_r = 160;
        uint32_t m_frame = 0;
        uint32_t m_magicID = 0;
        uint32_t m_gfxID = 0;
        int m_frameCount = 0;
        int m_gfxIDCount = 0;
        int m_gfxDirType = 0;
        std::unique_ptr<MagicFrameDB> m_frameDBPtr;
        std::vector<std::tuple<int, int>> m_offList;

    public:
        void draw(const ImVec2 &, const ImVec2 &);
        void load(uint32_t, const char *);
        void reset();
        std::string output() const;
        void updateFrame();

    private:
        int magicDirCount() const;
        std::tuple<int, int> getGfxDirPLoc(int, int, int) const;
        std::tuple<ImGuiTexture *, int, int> getFrameImage(int);
};
