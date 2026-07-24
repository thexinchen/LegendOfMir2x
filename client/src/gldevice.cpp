#include <cmath>
#include <cstring>
#include <numbers>

#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "mirevent.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "log.hpp"
#include "colorf.hpp"
#include "sysconst.hpp"
#include "gldevice.hpp"
#include "clientargparser.hpp"

extern Log *g_mir2xLog;
extern ClientArgParser *g_clientArgParser;

static GLDevice *g_glDeviceSelf = nullptr; // for static GLFW callbacks

static_assert(sizeof(ImU32) == sizeof(uint32_t));

// colorf packs 0xAABBGGRR, identical to ImU32 -- colors pass through unchanged
static constexpr ImU32 fnColor32(uint32_t color)
{
    return static_cast<ImU32>(color);
}

static void fnGLFWErrorCallback(int error, const char *description)
{
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

// ---------------------------------------------------------------------------
// event bridge: GLFW callbacks -> synthesized MirEvents
// ---------------------------------------------------------------------------

static MirKeycode fnGLFWKeyToMir(int key)
{
    switch(key){
        case GLFW_KEY_ESCAPE   : return MIRK_ESCAPE;
        case GLFW_KEY_ENTER    : return MIRK_RETURN;
        case GLFW_KEY_TAB      : return MIRK_TAB;
        case GLFW_KEY_BACKSPACE: return MIRK_BACKSPACE;
        case GLFW_KEY_SPACE    : return MIRK_SPACE;
        case GLFW_KEY_UP       : return MIRK_UP;
        case GLFW_KEY_DOWN     : return MIRK_DOWN;
        case GLFW_KEY_LEFT     : return MIRK_LEFT;
        case GLFW_KEY_RIGHT    : return MIRK_RIGHT;
        case GLFW_KEY_HOME     : return MIRK_HOME;
        case GLFW_KEY_END      : return MIRK_END;
        case GLFW_KEY_DELETE   : return MIRK_DELETE;
        case GLFW_KEY_LEFT_SHIFT: return MIRK_LSHIFT;
        case GLFW_KEY_RIGHT_SHIFT: return MIRK_RSHIFT;
        case GLFW_KEY_LEFT_CONTROL: return MIRK_LCTRL;
        case GLFW_KEY_RIGHT_CONTROL: return MIRK_RCTRL;
        case GLFW_KEY_F1: case GLFW_KEY_F2: case GLFW_KEY_F3: case GLFW_KEY_F4:
        case GLFW_KEY_F5: case GLFW_KEY_F6: case GLFW_KEY_F7: case GLFW_KEY_F8:
        case GLFW_KEY_F9: case GLFW_KEY_F10: case GLFW_KEY_F11: case GLFW_KEY_F12:
            return (MirKeycode)(MIRK_F1 + (key - GLFW_KEY_F1));
        default:
            if(key >= GLFW_KEY_A && key <= GLFW_KEY_Z){
                return (MirKeycode)(MIRK_A + (key - GLFW_KEY_A));
            }
            if(key >= GLFW_KEY_0 && key <= GLFW_KEY_9){
                return (MirKeycode)(MIRK_0 + (key - GLFW_KEY_0));
            }
            if(key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9){
                return (MirKeycode)(MIRK_KP_0 + (key - GLFW_KEY_KP_0));
            }
            if(key > 0 && key < 128){
                return (MirKeycode)key; // punctuation etc. mostly ASCII-aligned
            }
            return MIRK_UNKNOWN;
    }
}

static uint16_t fnGLFWModsToMir(int mods)
{
    uint16_t sdlMods = 0;
    if(mods & GLFW_MOD_SHIFT) sdlMods |= MIRKMOD_SHIFT;
    if(mods & GLFW_MOD_CONTROL) sdlMods |= MIRKMOD_CTRL;
    if(mods & GLFW_MOD_ALT) sdlMods |= MIRKMOD_ALT;
    if(mods & GLFW_MOD_CAPS_LOCK) sdlMods |= MIRKMOD_CAPS;
    if(mods & GLFW_MOD_NUM_LOCK) sdlMods |= MIRKMOD_NUM;
    return sdlMods;
}

void GLDevice::fnKeyEvent(GLFWwindow *window, int key, int, int action, int mods)
{
    if(action != GLFW_PRESS && action != GLFW_RELEASE){
        return; // ignore repeats for key events; text comes via char callback
    }

    MirEvent event {};
    event.type = (action == GLFW_PRESS) ? MIR_EVENT_KEY_DOWN : MIR_EVENT_KEY_UP;
    event.key.key = fnGLFWKeyToMir(key);
    event.key.mod = fnGLFWModsToMir(mods);
    event.key.down = (action == GLFW_PRESS);

    auto *self = static_cast<GLDevice *>(glfwGetWindowUserPointer(window));
    if(self){
        std::lock_guard<std::mutex> lockGuard(self->m_eventLock);
        self->m_eventQ.emplace_back(event, std::string{});
    }
}

void GLDevice::fnCharEvent(GLFWwindow *window, unsigned int codepoint)
{
    // UTF-8 encode; the bytes travel with the queued event (see pollEvent)
    std::string text;
    if(codepoint < 0x80){
        text += (char)codepoint;
    }
    else if(codepoint < 0x800){
        text += (char)(0xC0 | (codepoint >> 6));
        text += (char)(0x80 | (codepoint & 0x3F));
    }
    else if(codepoint < 0x10000){
        text += (char)(0xE0 | (codepoint >> 12));
        text += (char)(0x80 | ((codepoint >> 6) & 0x3F));
        text += (char)(0x80 | (codepoint & 0x3F));
    }
    else{
        text += (char)(0xF0 | (codepoint >> 18));
        text += (char)(0x80 | ((codepoint >> 12) & 0x3F));
        text += (char)(0x80 | ((codepoint >> 6) & 0x3F));
        text += (char)(0x80 | (codepoint & 0x3F));
    }

    MirEvent event {};
    event.type = MIR_EVENT_TEXT_INPUT;

    auto *self = static_cast<GLDevice *>(glfwGetWindowUserPointer(window));
    if(self){
        std::lock_guard<std::mutex> lockGuard(self->m_eventLock);
        self->m_eventQ.emplace_back(event, std::move(text));
    }
}

void GLDevice::fnMouseButtonEvent(GLFWwindow *window, int button, int action, int)
{
    if(action != GLFW_PRESS && action != GLFW_RELEASE){
        return;
    }

    MirEvent event {};
    event.type = (action == GLFW_PRESS) ? MIR_EVENT_MOUSE_BUTTON_DOWN : MIR_EVENT_MOUSE_BUTTON_UP;
    event.button.button = [button]() -> uint8_t
    {
        switch(button){
            case GLFW_MOUSE_BUTTON_LEFT  : return MIR_BUTTON_LEFT;
            case GLFW_MOUSE_BUTTON_RIGHT : return MIR_BUTTON_RIGHT;
            case GLFW_MOUSE_BUTTON_MIDDLE: return MIR_BUTTON_MIDDLE;
            default                      : return (uint8_t)(button + 1);
        }
    }();
    event.button.down = (action == GLFW_PRESS);

    double x = 0, y = 0;
    glfwGetCursorPos(window, &x, &y);
    event.button.x = to_f(x);
    event.button.y = to_f(y);

    auto *self = static_cast<GLDevice *>(glfwGetWindowUserPointer(window));
    if(self){
        std::lock_guard<std::mutex> lockGuard(self->m_eventLock);
        self->m_eventQ.emplace_back(event, std::string{});
    }
}

void GLDevice::fnCursorPosEvent(GLFWwindow *window, double x, double y)
{
    MirEvent event {};
    event.type = MIR_EVENT_MOUSE_MOTION;
    event.motion.x = to_f(x);
    event.motion.y = to_f(y);

    static double s_lastX = 0, s_lastY = 0;
    event.motion.xrel = to_f(x - s_lastX);
    event.motion.yrel = to_f(y - s_lastY);
    s_lastX = x;
    s_lastY = y;

    auto *self = static_cast<GLDevice *>(glfwGetWindowUserPointer(window));
    if(self){
        std::lock_guard<std::mutex> lockGuard(self->m_eventLock);
        self->m_eventQ.emplace_back(event, std::string{});
    }
}

void GLDevice::fnScrollEvent(GLFWwindow *window, double, double yoffset)
{
    MirEvent event {};
    event.type = MIR_EVENT_MOUSE_WHEEL;
    event.wheel.y = to_f(yoffset);
    event.wheel.x = 0.0f;

    double x = 0, y = 0;
    glfwGetCursorPos(window, &x, &y);
    event.wheel.mouse_x = to_f(x);
    event.wheel.mouse_y = to_f(y);

    auto *self = static_cast<GLDevice *>(glfwGetWindowUserPointer(window));
    if(self){
        std::lock_guard<std::mutex> lockGuard(self->m_eventLock);
        self->m_eventQ.emplace_back(event, std::string{});
    }
}

bool GLDevice::pollEvent(MirEvent *event)
{
    glfwPollEvents();

    std::lock_guard<std::mutex> lockGuard(m_eventLock);
    if(m_eventQ.empty()){
        return false;
    }

    auto queued = std::move(m_eventQ.front());
    m_eventQ.pop_front();

    if(queued.first.type == MIR_EVENT_TEXT_INPUT && !queued.second.empty()){
        // keep the bytes in a ring so the pointer
        // stays valid across the next 256 polls
        m_textRing[m_textRingIdx] = std::move(queued.second);
        queued.first.text.text = m_textRing[m_textRingIdx].c_str();
        m_textRingIdx = (m_textRingIdx + 1) % m_textRing.size();
    }

    *event = queued.first;
    return true;
}

void GLDevice::flushEvent(MirEventType type)
{
    std::lock_guard<std::mutex> lock(m_eventLock);
    m_eventQ.erase(
        std::remove_if(m_eventQ.begin(), m_eventQ.end(),
            [type](const std::pair<MirEvent, std::string> &p) {
                return p.first.type == type;
            }),
        m_eventQ.end());
}



// ---------------------------------------------------------------------------
// window / context / ImGui bootstrap
// ---------------------------------------------------------------------------

GLDevice::GLDevice()
{
    g_glDeviceSelf = this;

    glfwSetErrorCallback(fnGLFWErrorCallback);
    if(!glfwInit()){
        throw fflpanic("failed to initialize GLFW");
    }
    // window creation happens in createInitViewWindow()/createMainWindow(),
    // mirroring the old lifecycle (InitView drives the former)
}

GLDevice::~GLDevice()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    for(auto &[key, texID]: m_cover){
        if(texID){
            glDeleteTextures(1, &texID.id);
        }
    }

    if(m_window){
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

void GLDevice::createWindow(bool initViewWindow)
{
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    if(initViewWindow){
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

        int windowW = 800, windowH = 600;
        if(GLFWmonitor *primary = glfwGetPrimaryMonitor()){
            const GLFWvidmode *mode = glfwGetVideoMode(primary);
            if(mode){
                windowW = std::min<int>(windowW, mode->width);
                windowH = std::min<int>(windowH, mode->height);
            }
        }

        if(m_window){
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
        m_window = glfwCreateWindow(windowW, windowH, "MIR2X-V0.1-LOADING", nullptr, nullptr);
    }

    else{
        const bool fullscreen = g_clientArgParser && (g_clientArgParser->screenMode == 1 || g_clientArgParser->screenMode == 2);

        if(m_window){
            // recreate to switch title/flags, mirroring the reset behavior
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }

        if(fullscreen){
            GLFWmonitor *primary = glfwGetPrimaryMonitor();
            const GLFWvidmode *mode = primary ? glfwGetVideoMode(primary) : nullptr;
            m_window = glfwCreateWindow(mode ? mode->width : SYS_WINDOW_MIN_W, mode ? mode->height : SYS_WINDOW_MIN_H, "MIR2X-V0.1", primary, nullptr);
        }
        else{
            m_window = glfwCreateWindow(SYS_WINDOW_MIN_W, SYS_WINDOW_MIN_H, "MIR2X-V0.1", nullptr, nullptr);
            if(m_window){
                glfwSetWindowSizeLimits(m_window, SYS_WINDOW_MIN_W, SYS_WINDOW_MIN_H, GLFW_DONT_CARE, GLFW_DONT_CARE);
            }
        }
    }

    if(!m_window){
        throw fflpanic("failed to create GLFW window");
    }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback        (m_window, fnKeyEvent);
    glfwSetCharCallback       (m_window, fnCharEvent);
    glfwSetMouseButtonCallback(m_window, fnMouseButtonEvent);
    glfwSetCursorPosCallback  (m_window, fnCursorPosEvent);
    glfwSetScrollCallback     (m_window, fnScrollEvent);
    glfwSetWindowCloseCallback(m_window, [](GLFWwindow *win)
    {
        MirEvent event {};
        event.type = MIR_EVENT_QUIT;
        auto *self = static_cast<GLDevice *>(glfwGetWindowUserPointer(win));
        if(self){
            std::lock_guard<std::mutex> lockGuard(self->m_eventLock);
            self->m_eventQ.emplace_back(event, std::string{});
        }
    });

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);
    glfwShowWindow(m_window);

    setWindowIcon();
}

void GLDevice::createInitViewWindow()
{
    createWindow(true);
    if(!ImGui::GetCurrentContext()){
        initImGui();
    }
    else{
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
    }
}

void GLDevice::createMainWindow()
{
    createWindow(false);
    // ImGui context survives window recreation; rebind the backend to the new window/context
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void GLDevice::initImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = "mir2x-client.imgui.ini";

    ImGui::StyleColorsDark();

    // default UI font with CJK ranges; game text uses FontexDB, not this atlas
    constexpr static uint8_t ttfData[]
    {
        #embed "monaco.ttf"
    };

    static const char *const cjkFontPathList[] =
    {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJKsc-Regular.otf",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    };

    io.Fonts->AddFontFromMemoryTTF(const_cast<uint8_t *>(ttfData), sizeof(ttfData), 15.0f);

    for(const auto *fontPath: cjkFontPathList){
        if(std::FILE *fp = std::fopen(fontPath, "rb"); fp){
            std::fclose(fp);
            ImFontConfig mergeCfg;
            mergeCfg.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(fontPath, 15.0f, &mergeCfg, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
            break;
        }
    }

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void GLDevice::setWindowIcon()
{
    // GLFW image icon from winicon.png: cosmetic, deferred
}

void GLDevice::showCursor(bool show)
{
    if(m_window){
        glfwSetInputMode(m_window, GLFW_CURSOR, show ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }
}

// ---------------------------------------------------------------------------
// frame boundary
// ---------------------------------------------------------------------------

GLDeviceHelper::RenderNewFrame::RenderNewFrame(GLDevice *devPtr)
    : m_device(devPtr ? devPtr : g_glDeviceSelf)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

GLDeviceHelper::RenderNewFrame::~RenderNewFrame()
{
    ImGui::Render();

    int frameW = 0, frameH = 0;
    glfwGetFramebufferSize(m_device->m_window, &frameW, &frameH);
    glViewport(0, 0, frameW, frameH);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(m_device->m_window);
    m_device->updateFPS();
}

void GLDevice::present()
{
    // presentation happens in RenderNewFrame's dtor
}

void GLDevice::clearScreen()
{
    // the framebuffer is cleared per frame in RenderNewFrame's dtor
}

// ---------------------------------------------------------------------------
// draw state helpers
// ---------------------------------------------------------------------------

uint32_t GLDevice::swapColor(uint32_t color)
{
    const auto old = m_color;
    m_color = color;
    return old;
}

uint32_t GLDevice::texModColor(GLTexID tex) const
{
    if(auto p = m_texModColor.find(tex.id); p != m_texModColor.end()){
        return p->second;
    }
    return 0XFFFFFFFF;
}

uint32_t GLDevice::setTexModColor(GLTexID tex, uint32_t color)
{
    const auto old = texModColor(tex);
    m_texModColor[tex.id] = color;
    return old;
}

void GLDevice::clearTexModColor(GLTexID tex, uint32_t saved)
{
    if(saved == 0XFFFFFFFF){
        m_texModColor.erase(tex.id);
    }
    else{
        m_texModColor[tex.id] = saved;
    }
}

void GLDevice::pushClipRect(int x, int y, int w, int h)
{
    if(auto *drawList = ImGui::GetBackgroundDrawList()){
        drawList->PushClipRect(ImVec2(to_f(x), to_f(y)), ImVec2(to_f(x + w), to_f(y + h)), true);
    }
}

void GLDevice::popClipRect()
{
    if(auto *drawList = ImGui::GetBackgroundDrawList()){
        drawList->PopClipRect();
    }
}

GLDeviceHelper::EnableRenderColor::EnableRenderColor(uint32_t color, GLDevice *devPtr)
    : m_device(devPtr ? devPtr : g_glDeviceSelf)
    , m_savedColor(m_device->swapColor(color))
{}

GLDeviceHelper::EnableRenderColor::~EnableRenderColor()
{
    m_device->swapColor(m_savedColor);
}

GLDeviceHelper::EnableRenderCropRectangle::EnableRenderCropRectangle(int x, int y, int w, int h, GLDevice *devPtr)
    : m_device(devPtr ? devPtr : g_glDeviceSelf)
{
    m_device->pushClipRect(x, y, w, h);
}

GLDeviceHelper::EnableRenderCropRectangle::~EnableRenderCropRectangle()
{
    m_device->popClipRect();
}

GLDeviceHelper::EnableTextureModColor::EnableTextureModColor(GLTexID tex, uint32_t color)
    : m_device(g_glDeviceSelf)
    , m_tex(tex)
    , m_savedColor(m_device->setTexModColor(tex, color))
{}

GLDeviceHelper::EnableTextureModColor::~EnableTextureModColor()
{
    m_device->clearTexModColor(m_tex, m_savedColor);
}

// ---------------------------------------------------------------------------
// input helpers
// ---------------------------------------------------------------------------

char GLDeviceHelper::getKeyChar(const MirEvent &event, bool checkShiftKey)
{
    if(event.type != MIR_EVENT_KEY_DOWN){
        return '\0';
    }

    const bool shift = checkShiftKey && (event.key.mod & MIRKMOD_SHIFT);
    switch(event.key.key){
        case MIRK_SPACE: return ' ';
        case MIRK_0: return shift ? ')' : '0';
        case MIRK_1: return shift ? '!' : '1';
        case MIRK_2: return shift ? '@' : '2';
        case MIRK_3: return shift ? '#' : '3';
        case MIRK_4: return shift ? '$' : '4';
        case MIRK_5: return shift ? '%' : '5';
        case MIRK_6: return shift ? '^' : '6';
        case MIRK_7: return shift ? '&' : '7';
        case MIRK_8: return shift ? '*' : '8';
        case MIRK_9: return shift ? '(' : '9';
        default:
            if(event.key.key >= MIRK_A && event.key.key <= MIRK_Z){
                const auto ch = (char)('a' + (event.key.key - MIRK_A));
                return shift ? (char)(ch - 'a' + 'A') : ch;
            }
            return '\0';
    }
}

GLDeviceHelper::MirEventPLoc GLDeviceHelper::getMousePLoc()
{
    double x = 0, y = 0;
    if(g_glDeviceSelf){
        // window pointer accessed via device; cursor pos is in screen coords
    }
    auto *window = glfwGetCurrentContext();
    if(window){
        glfwGetCursorPos(window, &x, &y);
    }
    return MirEventPLoc {to_d(x), to_d(y)};
}

std::tuple<int, int, uint32_t> GLDeviceHelper::getMouseState()
{
    auto *window = glfwGetCurrentContext();
    double x = 0, y = 0;
    if(window){
        glfwGetCursorPos(window, &x, &y);
    }

    uint32_t state = 0;
    if(window){
        if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT  ) == GLFW_PRESS) state |= MIR_BUTTON_MASK(MIR_BUTTON_LEFT);
        if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) state |= MIR_BUTTON_MASK(MIR_BUTTON_MIDDLE);
        if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT ) == GLFW_PRESS) state |= MIR_BUTTON_MASK(MIR_BUTTON_RIGHT);
    }
    return {to_d(x), to_d(y), state};
}

std::optional<GLDeviceHelper::MirEventPLoc> GLDeviceHelper::getEventPLoc(const MirEvent &event)
{
    switch(event.type){
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
        case MIR_EVENT_MOUSE_BUTTON_UP:
            return MirEventPLoc {to_d(event.button.x), to_d(event.button.y)};
        case MIR_EVENT_MOUSE_MOTION:
            return MirEventPLoc {to_d(event.motion.x), to_d(event.motion.y)};
        case MIR_EVENT_MOUSE_WHEEL:
            return MirEventPLoc {to_d(event.wheel.mouse_x), to_d(event.wheel.mouse_y)};
        case MIR_EVENT_KEY_DOWN:
        case MIR_EVENT_KEY_UP:
            return getMousePLoc();
        default:
            return std::nullopt;
    }
}

std::tuple<int, int> GLDeviceHelper::getTextureSize(GLTexID tex)
{
    return {tex.w, tex.h};
}

int GLDeviceHelper::getTextureWidth(GLTexID tex, std::optional<int> optW)
{
    return optW.value_or(tex.w);
}

int GLDeviceHelper::getTextureHeight(GLTexID tex, std::optional<int> optH)
{
    return optH.value_or(tex.h);
}

// ---------------------------------------------------------------------------
// window queries
// ---------------------------------------------------------------------------

std::tuple<int, int> GLDevice::getWindowSize()
{
    int w = -1, h = -1;
    glfwGetWindowSize(m_window, &w, &h);
    return {w, h};
}

int GLDevice::getWindowWidth () { return std::get<0>(getWindowSize()); }
int GLDevice::getWindowHeight() { return std::get<1>(getWindowSize()); }

std::pair<int, int> GLDevice::getRendererSize()
{
    int w = -1, h = -1;
    glfwGetFramebufferSize(m_window, &w, &h);
    return {w, h};
}

int GLDevice::getRendererWidth () { return getRendererSize().first;  }
int GLDevice::getRendererHeight() { return getRendererSize().second; }

void GLDevice::setWindowTitle(const char *title)
{
    glfwSetWindowTitle(m_window, title ? title : "");
}

void GLDevice::setWindowSize(int w, int h)
{
    glfwSetWindowSize(m_window, w, h);
}

void GLDevice::setWindowResizable(bool resizable)
{
    int winW = 0, winH = 0;
    glfwGetWindowSize(m_window, &winW, &winH);
    if(resizable){
        glfwSetWindowSizeLimits(m_window, SYS_WINDOW_MIN_W, SYS_WINDOW_MIN_H, GLFW_DONT_CARE, GLFW_DONT_CARE);
    }
    else{
        glfwSetWindowSizeLimits(m_window, winW, winH, winW, winH);
    }
}

void GLDevice::toggleWindowFullscreen()
{
    if(glfwGetWindowMonitor(m_window)){
        // windowed: restore
        glfwSetWindowMonitor(m_window, nullptr, 100, 100, SYS_WINDOW_MIN_W, SYS_WINDOW_MIN_H, GLFW_DONT_CARE);
        glfwSetWindowSizeLimits(m_window, SYS_WINDOW_MIN_W, SYS_WINDOW_MIN_H, GLFW_DONT_CARE, GLFW_DONT_CARE);
    }
    else if(GLFWmonitor *primary = glfwGetPrimaryMonitor()){
        const GLFWvidmode *mode = glfwGetVideoMode(primary);
        if(mode){
            glfwSetWindowMonitor(m_window, primary, 0, 0, mode->width, mode->height, mode->refreshRate);
        }
    }
}

void GLDevice::enableSystemIME(uint64_t id)
{
    // Stage 1b: text input moves to ImGui widgets (which drive the OS IME
    // themselves) in Stage 2/3; until then this is a tracked no-op
    m_imeEnableList.insert(id);
}

void GLDevice::disableSystemIME(uint64_t id)
{
    m_imeEnableList.erase(id);
}

void GLDevice::updateFPS()
{
    m_fpsMonitor.update();
}

size_t GLDevice::getFPS() const
{
    return m_fpsMonitor.fps();
}

// ---------------------------------------------------------------------------
// textures
// ---------------------------------------------------------------------------

GLTexID GLDevice::createRGBATexture(const uint32_t *data, size_t w, size_t h)
{
    fflassert(data);
    fflassert(w > 0);
    fflassert(h > 0);

    GLuint texName = 0;
    glGenTextures(1, &texName);
    if(!texName){
        return {};
    }

    glBindTexture(GL_TEXTURE_2D, texName);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // colorf packs 0xAABBGGRR: little-endian byte order is R,G,B,A = GL_RGBA
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, to_d(w), to_d(h), 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    return GLTexID {texName, to_d(w), to_d(h)};
}

GLTexID GLDevice::createTextureFromSurface(const GLSurface &surf)
{
    if(!(surf.w > 0 && surf.h > 0)){
        return {};
    }

    // GLSurface holds ARGB8888 (0xAARRGGBB); createRGBATexture expects the
    // colorf RGBA layout (0xAABBGGRR) -- swap the R and B channels
    std::vector<uint32_t> buf(surf.pixels.size());
    for(size_t i = 0; i < surf.pixels.size(); ++i){
        const auto px = surf.pixels[i];
        buf[i] = (px & 0XFF00FF00) | ((px & 0X000000FF) << 16) | ((px & 0X00FF0000) >> 16);
    }
    return createRGBATexture(buf.data(), to_uz(surf.w), to_uz(surf.h));
}

GLTexID GLDevice::loadPNGTexture(const void *data, size_t size)
{
    if(!(data && size > 0)){
        return {};
    }

    int w = 0, h = 0, comp = 0;
    GLTexID result {};
    if(uint8_t *pixBuf = stbi_load_from_memory(static_cast<const stbi_uc *>(data), to_d(size), &w, &h, &comp, 4)){
        result = createRGBATexture(reinterpret_cast<const uint32_t *>(pixBuf), to_uz(w), to_uz(h));
        stbi_image_free(pixBuf);
    }
    return result;
}

void GLDevice::destroyTexture(GLTexID tex)
{
    if(tex){
        glDeleteTextures(1, &tex.id);
    }
}

GLTexID GLDevice::getCover(int r, int angle)
{
    fflassert(r > 0);
    fflassert(angle >= 0 && angle <= 360);

    const int key = r * 360 + angle;
    if(auto p = m_cover.find(key); p != m_cover.end()){
        if(p->second){
            return p->second;
        }
        throw fflpanic("invalid registered cover: r = {}, angle = {}", r, angle);
    }

    const int w = r * 2 - 1;
    const int h = r * 2 - 1;

    std::vector<uint32_t> buf(w * h);
    for(int y = 0; y < h; ++y){
        for(int x = 0; x < w; ++x){
            const int dx =  1 * (x - r + 1);
            const int dy = -1 * (y - r + 1);
            const int curr_r2 = dx * dx + dy * dy;
            const uint8_t alpha = [curr_r2, r]() -> uint8_t
            {
                if(g_clientArgParser->debugAlphaCover){
                    return 255;
                }
                return 255 - std::min<uint8_t>(255, to_dround(255.0 * curr_r2 / (r * r)));
            }();

            const auto curr_angle = [dx, dy]() -> int
            {
                if(dx == 0){
                    return (dy >= 0) ? 0 : 180;
                }
                return ((dx > 0) ? 0 : 180) + to_d(std::lround((1.0 - 2.0 * std::atan(to_df(dy) / dx) / 3.14159265358979323846) * 90.0));
            }();

            if(curr_r2 < r * r && std::clamp<int>(curr_angle, 0, 360) <= angle){
                buf[x + y * w] = colorf::RGBA(0XFF, 0XFF, 0XFF, alpha);
            }
            else{
                buf[x + y * w] = colorf::RGBA(0, 0, 0, 0);
            }
        }
    }

    if(auto tex = createRGBATexture(buf.data(), w, h)){
        return m_cover[key] = tex;
    }
    throw fflpanic("failed to create texture: r = {}, angle = {}", r, angle);
}

// ---------------------------------------------------------------------------
// fonts (stb_truetype bridge for FontexDB)
// ---------------------------------------------------------------------------
// draw primitives (all through the background draw list)
// ---------------------------------------------------------------------------

void GLDevice::setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    m_color = colorf::RGBA(r, g, b, a);
}

void GLDevice::drawTexture(GLTexID tex, int dstX, int dstY, int dstW, int dstH, int srcX, int srcY, int srcW, int srcH)
{
    if(!(tex && srcW > 0 && srcH > 0)){
        return;
    }

    auto *drawList = ImGui::GetBackgroundDrawList();
    const ImVec2 uv0(to_f(srcX) / tex.w, to_f(srcY) / tex.h);
    const ImVec2 uv1(to_f(srcX + srcW) / tex.w, to_f(srcY + srcH) / tex.h);
    drawList->AddImage(tex, ImVec2(to_f(dstX), to_f(dstY)), ImVec2(to_f(dstX + dstW), to_f(dstY + dstH)), uv0, uv1, fnColor32(texModColor(tex)));

    if(g_clientArgParser->debugDrawTexture){
        drawRectangle(colorf::BLUE + colorf::A_SHF(128), dstX, dstY, dstW, dstH);
    }
}

void GLDevice::drawTexture(GLTexID tex, int dstX, int dstY, int srcX, int srcY, int srcW, int srcH)
{
    drawTexture(tex, dstX, dstY, srcW, srcH, srcX, srcY, srcW, srcH);
}

void GLDevice::drawTexture(GLTexID tex, int dstX, int dstY)
{
    if(tex){
        drawTexture(tex, dstX, dstY, 0, 0, tex.w, tex.h);
    }
}

void GLDevice::drawTexture(GLTexID tex, dir8_t dir, int anchorX, int anchorY)
{
    if(tex){
        const auto [dstX, dstY] = [tex, dir, anchorX, anchorY]() -> std::tuple<int, int>
        {
            switch(dir){
                case DIR_UPLEFT   : return {anchorX             , anchorY             };
                case DIR_UP       : return {anchorX - tex.w / 2 , anchorY             };
                case DIR_UPRIGHT  : return {anchorX - tex.w - 1 , anchorY             };
                case DIR_RIGHT    : return {anchorX - tex.w - 1 , anchorY - tex.h / 2 };
                case DIR_DOWNRIGHT: return {anchorX - tex.w - 1 , anchorY - tex.h - 1 };
                case DIR_DOWN     : return {anchorX - tex.w / 2 , anchorY - tex.h - 1 };
                case DIR_DOWNLEFT : return {anchorX             , anchorY - tex.h - 1 };
                case DIR_LEFT     : return {anchorX             , anchorY - tex.h / 2 };
                default           : return {anchorX - tex.w / 2 , anchorY - tex.h / 2 };
            }
        }();
        drawTexture(tex, dstX, dstY);
    }
}

void GLDevice::drawTextureEx(GLTexID tex,
        int srcX, int srcY, int srcW, int srcH,
        int dstX, int dstY, int dstW, int dstH,
        int centerDstOffX, int centerDstOffY,
        int rotateDegree, MirFlipMode flip)
{
    if(!(tex && srcW > 0 && srcH > 0)){
        return;
    }

    // rotate the dst quad around
    // (dstX + centerDstOffX, dstY + centerDstOffY), positive angle = CCW
    const double radian = -1.0 * (rotateDegree % 360) / 180.0 * std::numbers::pi;
    const auto fnRotatePoint = [x0 = dstX + centerDstOffX, y0 = dstY + centerDstOffY, radian](double x, double y) -> std::pair<double, double>
    {
        const double dx =  x - x0;
        const double dy = -y + y0;

        return
        {
            x0 + (std::cos(radian) * dx - std::sin(radian) * dy),
            y0 - (std::sin(radian) * dx + std::cos(radian) * dy),
        };
    };

    const auto rp0 = fnRotatePoint(dstX            + 0.5, dstY            + 0.5);
    const auto rp1 = fnRotatePoint(dstX + dstW - 1 + 0.5, dstY            + 0.5);
    const auto rp2 = fnRotatePoint(dstX + dstW - 1 + 0.5, dstY + dstH - 1 + 0.5);
    const auto rp3 = fnRotatePoint(dstX            + 0.5, dstY + dstH - 1 + 0.5);

    auto uv0 = ImVec2(to_f(srcX) / tex.w, to_f(srcY) / tex.h);
    auto uv1 = ImVec2(to_f(srcX + srcW) / tex.w, to_f(srcY) / tex.h);
    auto uv2 = ImVec2(to_f(srcX + srcW) / tex.w, to_f(srcY + srcH) / tex.h);
    auto uv3 = ImVec2(to_f(srcX) / tex.w, to_f(srcY + srcH) / tex.h);

    if(flip & MIR_FLIP_HORIZONTAL){
        std::swap(uv0.x, uv1.x);
        std::swap(uv3.x, uv2.x);
    }
    if(flip & MIR_FLIP_VERTICAL){
        std::swap(uv0.y, uv3.y);
        std::swap(uv1.y, uv2.y);
    }

    ImGui::GetBackgroundDrawList()->AddImageQuad(tex,
            ImVec2(to_f(rp0.first), to_f(rp0.second)),
            ImVec2(to_f(rp1.first), to_f(rp1.second)),
            ImVec2(to_f(rp2.first), to_f(rp2.second)),
            ImVec2(to_f(rp3.first), to_f(rp3.second)),
            uv0, uv1, uv2, uv3,
            fnColor32(texModColor(tex)));
}

void GLDevice::drawLine(int x0, int y0, int x1, int y1)
{
    ImGui::GetBackgroundDrawList()->AddLine(ImVec2(to_f(x0), to_f(y0)), ImVec2(to_f(x1), to_f(y1)), fnColor32(m_color));
}

void GLDevice::drawLine(uint32_t color, int x0, int y0, int x1, int y1)
{
    GLDeviceHelper::EnableRenderColor enableColor(color, this);
    drawLine(x0, y0, x1, y1);
}

void GLDevice::drawCross(uint32_t color, int x, int y, size_t r)
{
    GLDeviceHelper::EnableRenderColor enableColor(color, this);
    drawLine(x - to_d(r), y, x + to_d(r), y);
    drawLine(x, y - to_d(r), x, y + to_d(r));
}

void GLDevice::drawLinef(float x0, float y0, float x1, float y1)
{
    ImGui::GetBackgroundDrawList()->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), fnColor32(m_color));
}

void GLDevice::drawLinef(uint32_t color, float x0, float y0, float x1, float y1)
{
    GLDeviceHelper::EnableRenderColor enableColor(color, this);
    drawLinef(x0, y0, x1, y1);
}

void GLDevice::drawCrossf(uint32_t color, float x, float y, float r)
{
    GLDeviceHelper::EnableRenderColor enableColor(color, this);
    drawLinef(x - r, y, x + r, y);
    drawLinef(x, y - r, x, y + r);
}

void GLDevice::drawPixel(int x, int y)
{
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(to_f(x), to_f(y)), ImVec2(to_f(x) + 1.0f, to_f(y) + 1.0f), fnColor32(m_color));
}

void GLDevice::drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2)
{
    ImGui::GetBackgroundDrawList()->AddTriangle(
            ImVec2(to_f(x0), to_f(y0)), ImVec2(to_f(x1), to_f(y1)), ImVec2(to_f(x2), to_f(y2)), fnColor32(m_color));
}

void GLDevice::drawTriangle(uint32_t color, int x0, int y0, int x1, int y1, int x2, int y2)
{
    GLDeviceHelper::EnableRenderColor enableColor(color, this);
    drawTriangle(x0, y0, x1, y1, x2, y2);
}

void GLDevice::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2)
{
    ImGui::GetBackgroundDrawList()->AddTriangleFilled(
            ImVec2(to_f(x0), to_f(y0)), ImVec2(to_f(x1), to_f(y1)), ImVec2(to_f(x2), to_f(y2)), fnColor32(m_color));
}

void GLDevice::fillTriangle(uint32_t color, int x0, int y0, int x1, int y1, int x2, int y2)
{
    GLDeviceHelper::EnableRenderColor enableColor(color, this);
    fillTriangle(x0, y0, x1, y1, x2, y2);
}

void GLDevice::drawRectangle(int x, int y, int w, int h, int rad)
{
    if(!(w > 0 && h > 0)){
        return;
    }
    ImGui::GetBackgroundDrawList()->AddRect(
            ImVec2(to_f(x), to_f(y)), ImVec2(to_f(x + w), to_f(y + h)), fnColor32(m_color), to_f(rad));
}

void GLDevice::drawRectangle(uint32_t color, int x, int y, int w, int h, int rad)
{
    GLDeviceHelper::EnableRenderColor enableColor(color, this);
    drawRectangle(x, y, w, h, rad);
}

void GLDevice::fillRectangle(int x, int y, int w, int h, int rad)
{
    if(!(w > 0 && h > 0)){
        return;
    }
    ImGui::GetBackgroundDrawList()->AddRectFilled(
            ImVec2(to_f(x), to_f(y)), ImVec2(to_f(x + w), to_f(y + h)), fnColor32(m_color), to_f(rad));
}

void GLDevice::fillRectangle(uint32_t color, int x, int y, int w, int h, int rad)
{
    GLDeviceHelper::EnableRenderColor enableColor(color, this);
    fillRectangle(x, y, w, h, rad);
}

void GLDevice::drawCircle(uint32_t color, int x, int y, int r)
{
    GLDeviceHelper::EnableRenderColor enableColor(color, this);
    if(r > 0){
        ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(to_f(x), to_f(y)), to_f(r), fnColor32(m_color));
    }
}

void GLDevice::fillCircle(uint32_t color, int x, int y, int r)
{
    GLDeviceHelper::EnableRenderColor enableColor(color, this);
    if(r > 0){
        ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(to_f(x), to_f(y)), to_f(r), fnColor32(m_color));
    }
}

void GLDevice::drawWidthRectangle(size_t width, int x, int y, int w, int h)
{
    if(!(w > 0 && h > 0 && width > 0)){
        return;
    }
    ImGui::GetBackgroundDrawList()->AddRect(
            ImVec2(to_f(x), to_f(y)), ImVec2(to_f(x + w), to_f(y + h)), fnColor32(m_color), 0.0f, 0, to_f(width));
}

void GLDevice::drawWidthRectangle(uint32_t color, size_t width, int x, int y, int w, int h)
{
    GLDeviceHelper::EnableRenderColor enableColor(color, this);
    drawWidthRectangle(width, x, y, w, h);
}

void GLDevice::drawHLineFading(uint32_t startColor, uint32_t endColor, int x, int y, int length)
{
    if(!length){
        return;
    }

    const auto f = to_f(length > 0 ? x + length : x);
    const auto t = to_f(length > 0 ? x : x + length);
    const auto c0 = length > 0 ? startColor : endColor;
    const auto c1 = length > 0 ? endColor : startColor;

    ImGui::GetBackgroundDrawList()->AddRectFilledMultiColor(
            ImVec2(f, to_f(y)), ImVec2(t, to_f(y) + 1.0f),
            fnColor32(c0), fnColor32(c1), fnColor32(c1), fnColor32(c0));
}

void GLDevice::drawVLineFading(uint32_t startColor, uint32_t endColor, int x, int y, int length)
{
    if(!length){
        return;
    }

    const auto f = to_f(length > 0 ? y + length : y);
    const auto t = to_f(length > 0 ? y : y + length);
    const auto c0 = length > 0 ? startColor : endColor;
    const auto c1 = length > 0 ? endColor : startColor;

    ImGui::GetBackgroundDrawList()->AddRectFilledMultiColor(
            ImVec2(to_f(x), f), ImVec2(to_f(x) + 1.0f, t),
            fnColor32(c0), fnColor32(c0), fnColor32(c1), fnColor32(c1));
}

void GLDevice::drawBoxFading(uint32_t startColor, uint32_t endColor, int x, int y, int w, int h, int fadeW, int fadeH)
{
    // approximation of per-pixel edge fade: solid center tinted by
    // endColor plus gradient strips on the four edges
    if(!(w > 0 && h > 0)){
        return;
    }

    fillRectangle(endColor, x, y, w, h);

    fadeW = std::clamp<int>(fadeW, 0, w / 2);
    fadeH = std::clamp<int>(fadeH, 0, h / 2);

    auto *drawList = ImGui::GetBackgroundDrawList();
    const auto c0 = fnColor32(startColor);
    const auto c1 = fnColor32(endColor);

    if(fadeW > 0){
        drawList->AddRectFilledMultiColor(ImVec2(to_f(x), to_f(y)), ImVec2(to_f(x + fadeW), to_f(y + h)), c0, c1, c1, c0);
        drawList->AddRectFilledMultiColor(ImVec2(to_f(x + w - fadeW), to_f(y)), ImVec2(to_f(x + w), to_f(y + h)), c1, c0, c0, c1);
    }
    if(fadeH > 0){
        drawList->AddRectFilledMultiColor(ImVec2(to_f(x), to_f(y)), ImVec2(to_f(x + w), to_f(y + fadeH)), c0, c0, c1, c1);
        drawList->AddRectFilledMultiColor(ImVec2(to_f(x), to_f(y + h - fadeH)), ImVec2(to_f(x + w), to_f(y + h)), c1, c1, c0, c0);
    }
}

void GLDevice::drawString(uint32_t, int, int, const char *)
{
    // the 8x8 bitmap debug font had no callers; dropped in
    // the ImGui migration (ImGui text replaces it where needed)
}
