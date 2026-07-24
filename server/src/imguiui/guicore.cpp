#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <stdexcept>

#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "log.hpp"
#include "server.hpp"
#include "guicore.hpp"
#include "guimainwindow.hpp"

extern Server *g_server;

static void fnGLFWErrorCallback(int error, const char *description)
{
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

GUICore::GUICore()
{
    glfwSetErrorCallback(fnGLFWErrorCallback);
    if(!glfwInit()){
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    m_window = glfwCreateWindow(1280, 800, "mir2x server", nullptr, nullptr);
    if(!m_window){
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "mir2x-server.imgui.ini";

    ImGui::StyleColorsDark();
    setupFonts();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    m_mainWindow = std::make_unique<GUIMainWindow>(this);
}

GUICore::~GUICore()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if(m_window){
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

void GUICore::setupFonts()
{
    // CJK-capable font for the server GUI: --gui-font lands in Stage 2, until
    // then probe well-known system paths. Falls back to the ImGui embedded
    // ASCII font with a console warning.
    static const char *const cjkFontPathList[] =
    {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJKsc-Regular.otf",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    };

    ImGuiIO &io = ImGui::GetIO();
    ImFontConfig fontCfg;
    fontCfg.SizePixels = 16.0f;

    for(const auto *fontPath: cjkFontPathList){
        if(std::FILE *fp = std::fopen(fontPath, "rb"); fp){
            std::fclose(fp);
            io.Fonts->AddFontFromFileTTF(fontPath, 16.0f, nullptr, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
            return;
        }
    }

    io.Fonts->AddFontDefault(&fontCfg);
    m_logEntries.push_back({Log::LOGTYPEV_WARNING, "No CJK font found, server GUI falls back to ASCII-only text"});
}

void GUICore::wake()
{
    // documented thread-safe: wakes the main thread's glfwWaitEventsTimeout()
    glfwPostEmptyEvent();
}

void GUICore::run()
{
    while(!m_quit && !glfwWindowShouldClose(m_window)){
        // replaces Fl::wait(): blocks up to 100ms, returns instantly on any
        // event including the empty events posted by wake()
        glfwWaitEventsTimeout(0.100);

        runPendingActions();

        // old Fl::thread_message() case 1: drain the GUI request queue
        g_server->parseNotifyGUIQ();

        // old Fl::thread_message() case 2: exception propagation
        try{
            g_server->checkException();
        }
        catch(const std::exception &e){
            std::string firstExceptStr;
            g_server->logException(e, &firstExceptStr);
            g_server->restart(firstExceptStr); // enqueues "Restart", handled by parseNotifyGUIQ -> fatalAlert()
        }

        drawFrame();
    }
}

void GUICore::drawFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    m_mainWindow->drawMenuBar();
    m_mainWindow->drawConsole();

    if(m_showActorMonitor){
        m_monitorWindow.drawActorMonitor();
    }
    if(m_showPodMonitor){
        m_monitorWindow.drawPodMonitor();
    }
    if(m_showProfiler){
        m_profilerWindow.draw();
    }
    if(m_showConfigure){
        m_configureWindow.draw();
    }
    if(m_showScript){
        m_scriptWindow.draw();
    }

    m_commandWindow.drawAllWindows();

    drawPasswordModal();
    drawFatalModal();

    ImGui::Render();

    int frameW = 0, frameH = 0;
    glfwGetFramebufferSize(m_window, &frameW, &frameH);
    glViewport(0, 0, frameW, frameH);
    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(m_window);
}

void GUICore::requestPassword(std::function<void(const std::string &)> cb)
{
    m_passwordCb = std::move(cb);
    m_passwordInput[0] = '\0';
    m_passwordPopupOpen = false; // OpenPopup issued on the next frame
}

void GUICore::drawPasswordModal()
{
    if(m_passwordCb && !m_passwordPopupOpen){
        ImGui::OpenPopup("Server Password");
        m_passwordPopupOpen = true;
    }

    if(ImGui::BeginPopupModal("Server Password", nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextUnformatted("Please set server password:");
        const bool enterPressed = ImGui::InputText("Password", m_passwordInput, sizeof(m_passwordInput),
                ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);

        if(ImGui::Button("OK", ImVec2(120, 0)) || enterPressed){
            m_password = m_passwordInput;
            auto cb = std::move(m_passwordCb);
            m_passwordCb = nullptr;
            m_passwordPopupOpen = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            if(cb){
                cb(m_password);
            }
            return;
        }

        ImGui::SameLine();
        if(ImGui::Button("Cancel", ImVec2(120, 0))){
            m_passwordCb = nullptr;
            m_passwordPopupOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void GUICore::runPendingActions()
{
    if(!m_pendingLaunch){
        return;
    }
    m_pendingLaunch = false;

    // legacy Launch behavior: recreate the server object, then launch
    try{
        delete g_server;
        g_server = new Server();
        g_server->launch();
        setLaunched(true);
    }
    catch(const std::exception &e){
        std::string firstExceptStr;
        g_server->logException(e, &firstExceptStr);
        fatalAlert(firstExceptStr.empty() ? "Server launch failed" : firstExceptStr);
    }
}

void GUICore::appendLog(int type, const char *line)
{
    // GUI-thread only (fed by Server::FlushBrowser() inside the frame loop)
    char timeBuf[16];
    {
        const auto nowTime = std::time(nullptr);
        std::tm nowTM {};
        localtime_r(&nowTime, &nowTM);
        std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &nowTM);
    }

    m_logEntries.push_back({type, std::string(timeBuf) + "  " + (line ? line : "")});
    if(m_logEntries.size() > LOG_ENTRY_CAP){
        m_logEntries.erase(m_logEntries.begin(), m_logEntries.begin() + (m_logEntries.size() - LOG_ENTRY_CAP));
    }
}

void GUICore::appendCWLog(uint32_t cwID, int type, const char *prompt, const char *log)
{
    // route into the owning command window's log pane, and mirror into the
    // main console (types offset by 100 so the console can color them)
    m_commandWindow.appendCWLog(cwID, type, prompt, log);
    appendLog(100 + type, (std::string("[CW#") + std::to_string(cwID) + "] " + (prompt ? prompt : "") + (log ? log : "")).c_str());
}

void GUICore::fatalAlert(const std::string &msg)
{
    // replaces fl_alert(); the modal is drawn by the frame loop and the
    // process exits once the user closes it (same terminal behavior)
    m_fatalMsg = msg;
    m_fatalPending = true;
}

void GUICore::drawFatalModal()
{
    if(!m_fatalPending){
        return;
    }

    if(!m_fatalPopupOpen){
        ImGui::OpenPopup("Fatal error");
        m_fatalPopupOpen = true;
    }

    bool modalOpen = true;
    if(ImGui::BeginPopupModal("Fatal error", &modalOpen, ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextUnformatted(m_fatalMsg.c_str());
        if(ImGui::Button("OK", ImVec2(120, 0)) || !modalOpen){
            std::exit(0); // same hard-exit policy as the legacy fl_alert path
        }
        ImGui::EndPopup();
    }
    else if(!modalOpen){
        std::exit(0);
    }
}

std::vector<std::string> GUICore::getCWHistory(uint32_t cwID)
{
    return m_commandWindow.getHistory(to_d(cwID));
}

void GUICore::deleteCommandWindow(int cwID)
{
    m_commandWindow.deleteCommandWindow(cwID);
}

void GUICore::appendCWLogToWindow(uint32_t cwID, int type, const char *prompt, const char *log)
{
    m_commandWindow.appendCWLog(cwID, type, prompt, log);
}

bool GUICore::isCommandWindowOpen(int cwID)
{
    if(auto *slot = m_commandWindow.getSlot(cwID)){
        return slot->open;
    }
    return false;
}

void GUICore::setCommandWindowOpen(int cwID, bool open)
{
    if(auto *slot = m_commandWindow.getSlot(cwID)){
        slot->open = open;
    }
}

void GUICore::execScriptInCommandWindow(const std::string &code)
{
    // legacy behavior: scripts run through command window #1
    if(!m_commandWindow.getSlot(1)){
        if(createCommandWindow() != 1){
            appendLog(Log::LOGTYPEV_WARNING, "no free command window for script execution");
            return;
        }
    }
    m_commandWindow.execString(1, code);
}
