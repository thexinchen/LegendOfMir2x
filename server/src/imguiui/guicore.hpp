#pragma once
//
// Stage 1a of the FLTK -> ImGui migration.
//
// GUICore owns the GLFW window, the OpenGL context and the ImGui context for
// the server monitor GUI (master mode only; slave mode stays headless). It
// replaces the FLTK event pump in Server::mainLoop():
//
//   FLTK                           ImGui/Glfw equivalent
//   ----                           -------------------
//   Fl::wait() loop                GUICore::run() frame loop
//   Fl::awake((void*)1/2)          GUICore::wake()  (glfwPostEmptyEvent, thread-safe)
//   Fl::thread_message() switch    parseNotifyGUIQ()/checkException() drained per frame
//   fl_alert(...) + exit           fatalAlert() modal, exit on close
//   Fl_Native_File_Chooser         tinyfiledialogs (Stage 2)
//
// The Server public API (addLog/addCWLogString/notifyGUI/parseNotifyGUIQ/
// getConfig call sites, Lua bindings) is unchanged; only the presentation
// side of the queue drain lands here.
//
// This header is GLFW/ImGui-free so it can be included from server core code.

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

#include "serverconfig.hpp"

struct GLFWwindow;
class GUIMainWindow;
extern class GUICore *g_guiCore; // master mode only, nullptr in slave mode

class GUICore
{
    public:
        struct GUILogEntry
        {
            int         type = 0; // Log::LOGTYPEV_* for server logs, 100+ for CW logs
            std::string text;
        };

    private:
        GLFWwindow *m_window = nullptr;
        bool        m_quit   = false;

    private:
        std::vector<GUILogEntry> m_logEntries; // GUI-thread only
        static constexpr size_t LOG_ENTRY_CAP = 5000;

    private:
        bool        m_fatalPending = false;
        bool        m_fatalPopupOpen = false;
        std::string m_fatalMsg;

    private:
        ServerConfigStore m_configStore;
        std::string       m_password;  // set by the password dialog (Stage 2)
        bool              m_launched = false;

    private:
        std::unique_ptr<GUIMainWindow> m_mainWindow;

    public:
        GUICore();
        ~GUICore();

    public:
        // main-thread frame loop, returns on window close or quit()
        void run();

        // thread-safe wakeup, replaces Fl::awake(): wakes glfwWaitEventsTimeout()
        void wake();

        void quit() { m_quit = true; wake(); }

    public:
        // GUI-thread sinks fed by Server::FlushBrowser()/FlushCWBrowser()
        void appendLog(int type, const char *line);
        void appendCWLog(uint32_t cwID, int type, const char *prompt, const char *log);
        const std::vector<GUILogEntry> &getLogEntries() const { return m_logEntries; }

    public:
        // replaces fl_alert() on the Restart notify path; exits on modal close
        void fatalAlert(const std::string &msg);

    public:
        ServerConfig getConfig() const { return m_configStore.getConfig(); }
        void         setConfig(const ServerConfig &config) { m_configStore.setConfig(config); }

    public:
        // NPC dialogue AES key material (npchar.cpp); empty until the
        // password dialog is implemented (Stage 2) -- identical to the
        // legacy --auto-launch behavior
        const std::string &getServerPassword() const { return m_password; }

    public:
        bool hasLaunched() const { return m_launched; }
        void setLaunched(bool launched = true) { m_launched = launched; }

    public:
        // Stage 1a stubs, replaced when the command windows land in Stage 2
        std::vector<std::string> getCWHistory(uint32_t cwID) const;
        void                     deleteCommandWindow(int cwID);

    private:
        void setupFonts();
        void drawFrame();
        void drawFatalModal();
};
