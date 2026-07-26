#pragma once
//
// Stage 2: ImGui replacement for profilerwindow.fl. Pulls logProfiling()
// output on the legacy timer cadence and renders it as an ImGui table.

#include <string>
#include <vector>

class GUICore;

class GUIProfilerWindow
{
    private:
        struct Row
        {
            std::string name;
            long long calls = 0;
            double longestTime = 0.0;
            double totalTime = 0.0;
        };

    private:
        GUICore    *m_core = nullptr;
        std::vector<Row> m_rows;
        std::string m_summary;
        double      m_lastRefresh = -1.0;

    public:
        explicit GUIProfilerWindow(GUICore *core)
            : m_core(core)
        {}

    public:
        void draw();
};
