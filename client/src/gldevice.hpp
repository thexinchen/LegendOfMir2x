#pragma once
//
// Stage 1b of the SDL3 -> GLFW/OpenGL/Dear ImGui migration.
//
// GLDevice replaces the video half of SDLDevice: GLFW window + GL 3.3 core
// context + Dear ImGui. ALL drawing (game world and, later, UI) goes through
// a fullscreen ImGui "World" draw list; the SDLDevice drawing API names are
// kept so world/UI code compiles unchanged with GLTexID handles instead of
// SDL_Texture*.
//
// Audio moved to AudioDevice (miniaudio). SDL3/SDL_ttf headers stay in this
// file only as a temporary bridge: SDL_Event/SDL_Point/SDL_Rect POD types
// keep the event plumbing intact until a native event struct replaces them,
// and SDL_ttf rasterizes FontexDB glyphs until a FreeType port lands. Both
// are removed in the dependency-removal stage.

#include <array>
#include <deque>
#include <mutex>
#include <memory>
#include <vector>
#include <optional>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include <SDL3/SDL.h> // event/POD types only (bridge), removed later

#include "gltex.hpp"
#include "glfont.hpp"
#include "totype.hpp"
#include "protocoldef.hpp"
#include "fflerror.hpp"
#include "fpsmonitor.hpp"

struct GLFWwindow;
class GLDevice;

namespace GLDeviceHelper
{
    class EnableRenderColor final
    {
        private:
            GLDevice *m_device;
            uint32_t  m_savedColor;

        public:
            /* ctor */  EnableRenderColor(uint32_t, GLDevice * = nullptr);
            /* dtor */ ~EnableRenderColor();
    };

    struct EnableRenderBlendMode
    {
        public:
            // ImGui draw lists are alpha-blended; no per-primitive blend modes
            /* ctor */  EnableRenderBlendMode(SDL_BlendMode, GLDevice * = nullptr) {}
            /* dtor */ ~EnableRenderBlendMode() {}
    };

    struct EnableRenderCropRectangle
    {
        private:
            GLDevice *m_device;

        public:
            /* ctor */  EnableRenderCropRectangle(int, int, int, int, GLDevice * = nullptr);
            /* dtor */ ~EnableRenderCropRectangle();
    };

    class RenderNewFrame final
    {
        private:
            GLDevice *m_device;

        public:
            /* ctor */  RenderNewFrame(GLDevice * = nullptr);
            /* dtor */ ~RenderNewFrame();
    };

    class EnableTextureBlendMode final
    {
        public:
            /* ctor */  EnableTextureBlendMode(GLTexID, SDL_BlendMode) {}
            /* dtor */ ~EnableTextureBlendMode() {}
    };

    class EnableTextureModColor final
    {
        private:
            GLDevice *m_device;
            GLTexID   m_tex;
            uint32_t  m_savedColor;

        public:
            /* ctor */  EnableTextureModColor(GLTexID, uint32_t);
            /* dtor */ ~EnableTextureModColor();
    };

    struct SDLEventPLoc final
    {
        const int x = 0;
        const int y = 0;
    };

    char getKeyChar(const SDL_Event &, bool);

    SDLEventPLoc getMousePLoc();
    std::tuple<int, int, Uint32> getMouseState();

    std::optional<SDLEventPLoc> getEventPLoc(const SDL_Event &);

    std::tuple<int, int> getTextureSize  (GLTexID);
    int                  getTextureWidth (GLTexID, std::optional<int> = std::nullopt);
    int                  getTextureHeight(GLTexID, std::optional<int> = std::nullopt);
}

class GLDevice final
{
    private:
        friend class GLDeviceHelper::RenderNewFrame;
        friend class GLDeviceHelper::EnableRenderCropRectangle;

    private:
        GLFWwindow *m_window = nullptr;

    private:
        FPSMonitor m_fpsMonitor;

    private:
        std::unordered_map<int, GLTexID> m_cover;

    private:
        std::unordered_set<uint64_t> m_imeEnableList;

    private:
        // draw state consumed by the draw-list primitives
        uint32_t m_color = 0XFFFFFFFF;                          // current RGBA (colorf layout)
        std::unordered_map<uint32_t, uint32_t> m_texModColor;   // GLTexID.id -> mod color

    private:
        // GLFW callbacks enqueue synthesized SDL_Events; Client drains them.
        // TEXT_INPUT events carry their UTF-8 bytes in the pair's string;
        // pollEvent() copies them into m_textRing so the const char* handed
        // out via event.text.text stays valid across subsequent polls.
        std::mutex m_eventLock;
        std::deque<std::pair<SDL_Event, std::string>> m_eventQ;
        std::array<std::string, 256> m_textRing;
        size_t m_textRingIdx = 0;

    private:
        static void fnKeyEvent(GLFWwindow *, int key, int scancode, int action, int mods);
        static void fnCharEvent(GLFWwindow *, unsigned int codepoint);
        static void fnMouseButtonEvent(GLFWwindow *, int button, int action, int mods);
        static void fnCursorPosEvent(GLFWwindow *, double x, double y);
        static void fnScrollEvent(GLFWwindow *, double xoffset, double yoffset);

    public:
        /* ctor */  GLDevice();
        /* dtor */ ~GLDevice();

    public:
        // event bridge, replaces SDL_PollEvent in Client::processEvent
        bool pollEvent(SDL_Event *);

    public:
        GLTexID loadPNGTexture(const void *, size_t);

    public:
        void setWindowIcon();
        void toggleWindowFullscreen();
        void showCursor(bool);

    public:
        void drawTexture(GLTexID, dir8_t, int, int);

    public:
        void drawTexture(GLTexID, int, int);
        void drawTexture(GLTexID, int, int, int, int, int, int);
        void drawTexture(GLTexID, int, int, int, int, int, int, int, int);

    public:
        void drawTextureEx(GLTexID,
                int, int,
                int, int, // src region

                int, int,
                int, int, // dst region

                int, // center x on dst
                int, // center y on dst

                int, // rotate in 360-degree on dst
                SDL_FlipMode = SDL_FLIP_NONE);

    public:
        void present();      // no-op: presentation happens in RenderNewFrame dtor
        void setWindowTitle(const char *);
        void clearScreen();

        void drawLine(int, int, int, int);
        void drawLine(uint32_t, int, int, int, int);

        void drawCross(uint32_t, int, int, size_t);

        void drawLinef(float, float, float, float);
        void drawLinef(uint32_t, float, float, float, float);

        void drawCrossf(uint32_t, float, float, float);

        void setColor(uint8_t, uint8_t, uint8_t, uint8_t);
        void drawPixel(int, int);

    public:
        void drawTriangle(          int, int, int, int, int, int);
        void drawTriangle(uint32_t, int, int, int, int, int, int);

        void fillTriangle(          int, int, int, int, int, int);
        void fillTriangle(uint32_t, int, int, int, int, int, int);

    public:
        void drawRectangle(          int, int, int, int, int = 0);
        void drawRectangle(uint32_t, int, int, int, int, int = 0);

        void fillRectangle(          int, int, int, int, int = 0);
        void fillRectangle(uint32_t, int, int, int, int, int = 0);

    public:
        void drawCircle(uint32_t, int, int, int);
        void fillCircle(uint32_t, int, int, int);

    public:
        void drawWidthRectangle(          size_t, int, int, int, int);
        void drawWidthRectangle(uint32_t, size_t, int, int, int, int);

    public:
        void drawHLineFading(uint32_t, uint32_t, int, int, int);
        void drawVLineFading(uint32_t, uint32_t, int, int, int);

        void drawBoxFading(uint32_t, uint32_t, int, int, int, int, int, int);

    public:
        GLTexID createTextureFromSurface(const GLSurface &);

        std::tuple<int, int> getWindowSize();
        int getWindowWidth();
        int getWindowHeight();

        std::pair<int, int> getRendererSize();
        int getRendererWidth();
        int getRendererHeight();

    public:
        void createMainWindow();
        void createInitViewWindow();

    public:
        void  enableSystemIME(uint64_t);
        void disableSystemIME(uint64_t);

    public:
        GLTexID createRGBATexture(const uint32_t *, size_t, size_t);
        void    destroyTexture(GLTexID);

    public:
        void   updateFPS();
        size_t getFPS() const;

    public:
        void setWindowResizable(bool);

    public:
        GLTexID getCover(int, int); // diameter = 2 * r - 1, r >= 1

    public:
        void drawString(uint32_t, int, int, const char *);

    public:
        void setWindowSize(int, int);

    private:
        void createWindow(bool initViewWindow);
        void initImGui();

    private:
        uint32_t currColor() const { return m_color; }
        uint32_t texModColor(GLTexID) const;

    public:
        void pushClipRect(int, int, int, int);
        void popClipRect();

    public:
        // internal for GLDeviceHelper state classes
        uint32_t swapColor(uint32_t);
        uint32_t setTexModColor(GLTexID, uint32_t);
        void     clearTexModColor(GLTexID, uint32_t saved);
};
