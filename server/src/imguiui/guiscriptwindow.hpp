#pragma once
//
// Stage 2: ImGui replacement for scriptwindow.fl. Loads a Lua file, shows it,
// and executes it through command window #1 (created on demand), matching the
// legacy Script -> Execute -> Run flow.

#include <string>

class GUICore;

class GUIScriptWindow
{
    private:
        GUICore    *m_core = nullptr;
        std::string m_text;
        std::string m_fileName;

    public:
        explicit GUIScriptWindow(GUICore *core)
            : m_core(core)
        {}

    public:
        void draw();
};
