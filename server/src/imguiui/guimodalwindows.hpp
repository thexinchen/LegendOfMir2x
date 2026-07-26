#pragma once
//
// Stage 2: ImGui replacement for serverconfigurewindow.fl. Same field set
// and validation rules; Apply keeps the AM_PEERCONFIG broadcast to peers.
// File pickers use a cross-platform ImGui file dialog.

#include <ImGuiFileDialog.hpp>

class GUICore;

class GUIConfigureWindow
{
    private:
        GUICore *m_core = nullptr;

    private:
        char m_mapPath[512]    {};
        char m_scriptPath[512] {};
        char m_maxPlayerCount[32] {};
        char m_experienceRate[32] {};
        char m_dropRate[32]       {};
        char m_goldRate[32]       {};
        char m_clientPort[32]     {};
        char m_slavePort[32]      {};

    private:
        bool m_clientPortEditable = true;
        bool m_slavePortEditable  = true;
        bool m_selectingScriptDirectory = false;
        ImGuiFileDialog m_fileDialog {"ConfigureFileDialog"};

    public:
        explicit GUIConfigureWindow(GUICore *core);

    public:
        void draw();

    private:
        void applyConfig();
};
