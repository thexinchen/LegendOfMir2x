#pragma once

#include <array>
#include <utility>

#include "ImBoard.hpp"
#include "imeboard.hpp"
#include "sdruntimeconfig.hpp"

class ProcessRun;
class ImRuntimeConfigBoard final: public ImBoard
{
    private:
        mutable SDRuntimeConfig m_config;
        ProcessRun *m_processRun;
        ImVec2 m_boardSize;

        mutable int m_mainPage = 0;
        mutable int m_systemTab = 0;
        mutable int m_socialTab = 0;
        mutable int m_gameTab = 0;
        mutable int m_previewWidget = 0;
        mutable int m_previewFont = 1;
        mutable int m_previewFontSize = 12;
        mutable int m_unusedWaitSeconds = 0;
        mutable bool m_dragging = false;
        mutable std::pair<int, int> m_displayWindowSize {800, 600};
        mutable int m_displayIME = IME_DISABLE;
        mutable std::array<char, 128> m_englishPreview {};
        mutable std::array<char, 128> m_chinesePreview {};

    public:
        ImRuntimeConfigBoard(int, int, int, int, ProcessRun *);

    public:
        void draw() const override;
        bool processEvent(const MirEvent &) const override;

    public:
        void setConfig(const SDRuntimeConfig &);
        const SDRuntimeConfig &getConfig() const { return m_config; }
        void updateWindowSize(std::pair<int, int>, bool);
        void updateIME(int, bool);

    private:
        void reportRuntimeConfig(int) const;
        void applyAudioConfig() const;
};
