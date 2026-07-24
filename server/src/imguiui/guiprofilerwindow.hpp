#pragma once
//
// Stage 2: ImGui replacement for profilerwindow.fl. Pulls logProfiling()
// output on the legacy timer cadence and renders it monospace.

#include <string>

class GUICore;

class GUIProfilerWindow
{
    private:
        GUICore    *m_core = nullptr;
        std::string m_text;
        double      m_lastRefresh = -1.0;

    public:
        explicit GUIProfilerWindow(GUICore *core)
            : m_core(core)
        {}

    public:
        void draw();
};
