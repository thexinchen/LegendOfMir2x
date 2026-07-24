#pragma once
//
// Stage 2: ImGui replacement for commandwindow.fl + commandinput.cpp.
// Up to 16 command windows (slots 1..16); each owns a CommandLuaModule and a
// single-thread worker (the legacy threadPool(1)). Eval modes AUTO/LOCAL/ASYNC
// keep the legacy semantics (AUTO: asyncEval through the actor pool when
// running, else raw; LOCAL: raw; ASYNC: must be running).
//
// Worker threads never touch ImGui state: they report through
// Server::addCWLogString (drained on the GUI thread) and flip an atomic busy
// flag; input re-focus happens on the GUI frame after completion.

#include <array>
#include <mutex>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <cstdint>

class GUICore;
class threadPool;
class CommandLuaModule;

struct CWLogEntry
{
    int         type = 0; // 0/1 output, 2 error
    std::string prompt;
    std::string text;
};

class GUICommandWindow
{
    public:
        struct Slot
        {
            uint32_t cwid = 0;
            bool     open = true;

            std::unique_ptr<CommandLuaModule> luaModule;
            std::unique_ptr<threadPool>       worker;

            std::vector<CWLogEntry> logList;   // GUI-thread only
            char inputBuf[1024] {};
            int  evalMode = 0;                 // 0 AUTO, 1 LOCAL, 2 ASYNC
            std::atomic<bool> busy {false};
            bool justFinished = false;         // re-focus input next frame

            std::mutex historyLock;
            std::vector<std::string> history;

            Slot() = default;
            ~Slot(); // defined in the .cpp where the worker/module types are complete
        };

    private:
        GUICore *m_core = nullptr;
        std::array<std::unique_ptr<Slot>, 17> m_slotList; // indices 1..16

    public:
        explicit GUICommandWindow(GUICore *core)
            : m_core(core)
        {}

        // defined in the .cpp where CommandLuaModule/threadPool are complete
        ~GUICommandWindow();

    public:
        int  createCommandWindow();            // 0 when all slots busy
        void deleteCommandWindow(int cwid);
        void execString(int cwid, const std::string &code);

    public:
        Slot *getSlot(int cwid);
        std::vector<std::string> getHistory(int cwid); // thread-safe copy (Lua binding)

    public:
        void appendCWLog(uint32_t cwid, int type, const char *prompt, const char *text);
        void drawAllWindows();

    private:
        void drawSlot(int cwid, Slot &);
};
