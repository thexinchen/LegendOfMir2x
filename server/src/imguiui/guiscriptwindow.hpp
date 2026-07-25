#pragma once
//
// Stage 2: ImGui replacement for scriptwindow.fl. Loads a Lua file, shows it,
// and executes it through command window #1 (created on demand), matching the
// legacy Script -> Execute -> Run flow.

#include <string>

#include "guifiledialog.hpp"

class GUICore;

class GUIScriptWindow
{
    private:
        GUICore    *m_core = nullptr;
        std::string m_text;
        std::string m_fileName;
        GUIFileDialog m_fileDialog {"ScriptFileDialog"};
        bool m_loadRequestedFromMainMenu = false;

    public:
        explicit GUIScriptWindow(GUICore *core)
            : m_core(core)
        {}

    public:
        void draw();
        void drawFileDialog();
        void openFileDialog(bool fromMainMenu);

    private:
        bool loadFile(const char *);
        void runFile();
};
