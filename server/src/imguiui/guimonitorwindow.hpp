#pragma once
//
// Stage 2: ImGui replacements for actormonitorwindow.fl/podmonitorwindow.fl.
// Data sources are the same atomic snapshots the FLTK tables polled
// (ActorPool::getActorMonitor()/getPodMonitor()); refresh throttled to the
// legacy 1.37s cadence. Double-clicking an actor row opens its pod monitor.

#include <vector>
#include <cstdint>

#include "actormonitor.hpp"

class GUICore;

class GUIMonitorWindow
{
    private:
        GUICore *m_core = nullptr;

    private:
        std::vector<ActorMonitor> m_actorList;
        ActorPodMonitor           m_podMonitor;
        uint64_t                  m_podUID = 0;
        double                    m_lastRefresh = -1.0;

    public:
        explicit GUIMonitorWindow(GUICore *core)
            : m_core(core)
        {}

    public:
        void     openPodMonitor(uint64_t uid);
        uint64_t getPodUID() const { return m_podUID; }

    public:
        void drawActorMonitor();
        void drawPodMonitor();

    private:
        void refreshSnapshots();
};
