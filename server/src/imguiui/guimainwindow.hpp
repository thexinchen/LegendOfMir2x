#pragma once
//
// ImGui replacement for the fluid-generated MainWindow (mainwindow.fl):
// main menu bar + the server log console. Monitor/command/configure/script
// entries are disabled placeholders until Stage 2.

class GUICore;

class GUIMainWindow
{
    private:
        GUICore *m_core = nullptr;
        bool     m_autoScroll = true;

    public:
        explicit GUIMainWindow(GUICore *core)
            : m_core(core)
        {}

    public:
        void drawMenuBar();
        void drawConsole();
};
