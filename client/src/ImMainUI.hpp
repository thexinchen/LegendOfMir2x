#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>

#include "cblog.hpp"
#include "mirevent.hpp"

class ProcessRun;

class ImMainUI final
{
    private:
        struct LogLine
        {
            std::string text;
            uint32_t color = 0XFFFFFFFF;
        };

        struct BlinkState
        {
            double elapsedMS = 0.0;
            double remainingMS = 0.0;
            bool active = false;
        };

    private:
        ProcessRun *m_processRun = nullptr;

        mutable bool m_expand = false;
        mutable bool m_minimize = false;
        mutable bool m_quickAccessShown = false;
        mutable bool m_focusCommand = false;
        mutable bool m_quickDragging = false;
        mutable bool m_acMagic = false;
        mutable bool m_dcMagic = false;

        mutable float m_quickX = 0.0f;
        mutable float m_quickY = -1.0f;
        mutable std::array<char, 512> m_command {};

        double m_accuTimeMS = 0.0;
        std::deque<LogLine> m_logList;
        mutable std::unordered_map<std::string, BlinkState> m_buttonBlink;

    public:
        explicit ImMainUI(ProcessRun *);

    public:
        void draw() const;
        void update(double);
        bool processEvent(const MirEvent &);

    public:
        void addXMLLog(const char *);
        void addParLog(const char *);
        void addLog(int, const char *);
        void startButtonBlink(std::string_view, double = 0.0);

    public:
        int shiftHeight() const;

    private:
        void drawHUD() const;
        void drawQuickAccess() const;
        void submitCommand() const;
        void consumeQuickSlot(int) const;
        bool blinkVisible(std::string_view) const;
        void stopButtonBlink(std::string_view) const;
};
