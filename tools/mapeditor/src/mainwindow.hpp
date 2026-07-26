#pragma once

#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuiFileDialog.hpp>
#include <GLTexture.hpp>

#include "editormap.hpp"
#include "animationdb.hpp"
#include "imagemapdb.hpp"
#include "layerbrowserwindow.hpp"
#include "landtype.hpp"
#include "wilanitimer.hpp"

class ImGuiApp
{
    private:
        GLFWwindow *m_window = nullptr;
        std::string m_iniFile;
        std::string m_alertTitle;
        std::string m_alertText;
        bool m_alertRequested = false;
        bool m_forceClose = false;

    protected:
        explicit ImGuiApp(const char *title, int width, int height, const char *iniFile)
            : m_iniFile(iniFile ? iniFile : "mir2x-tool.imgui.ini")
        {
            glfwSetErrorCallback(glfwErrorCallback);
            if(!glfwInit()){
                throw std::runtime_error("Failed to initialize GLFW");
            }
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
            m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
            if(!m_window){
                glfwTerminate();
                throw std::runtime_error("Failed to create GLFW window");
            }
            glfwMakeContextCurrent(m_window);
            glfwSwapInterval(1);
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGui::GetIO().IniFilename = m_iniFile.c_str();
            ImGui::StyleColorsDark();
            setupFonts();
            ImGui_ImplGlfw_InitForOpenGL(m_window, true);
            ImGui_ImplOpenGL3_Init("#version 330");
        }

        virtual void draw() = 0;
        virtual void update(double) {}
        virtual bool onCloseRequested() { return true; }

    public:
        virtual ~ImGuiApp()
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            if(m_window){
                glfwDestroyWindow(m_window);
            }
            glfwTerminate();
        }

        ImGuiApp(const ImGuiApp &) = delete;
        ImGuiApp &operator=(const ImGuiApp &) = delete;

        int run()
        {
            double lastTime = glfwGetTime();
            while(true){
                if(glfwWindowShouldClose(m_window)){
                    if(m_forceClose || onCloseRequested()){
                        break;
                    }
                    glfwSetWindowShouldClose(m_window, GLFW_FALSE);
                }
                glfwPollEvents();
                const double now = glfwGetTime();
                update(now - lastTime);
                lastTime = now;
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();
                draw();
                drawAlert();
                ImGui::Render();
                int frameWidth = 0;
                int frameHeight = 0;
                glfwGetFramebufferSize(m_window, &frameWidth, &frameHeight);
                glViewport(0, 0, frameWidth, frameHeight);
                glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                glfwSwapBuffers(m_window);
            }
            return 0;
        }

        void close()
        {
            m_forceClose = true;
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }

        void alert(const std::string &text, const std::string &title = "Error")
        {
            m_alertTitle = title;
            m_alertText = text;
            m_alertRequested = true;
        }

        GLFWwindow *window() const { return m_window; }

    private:
        static void glfwErrorCallback(int error, const char *description)
        {
            std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
        }

        void setupFonts()
        {
            static const char *fontList[] = {
#ifdef _WIN32
                "C:/Windows/Fonts/msyh.ttc", "C:/Windows/Fonts/simhei.ttf",
#else
                "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
                "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
#endif
            };

			const float fFontPixelSize = 26.0f;

            auto &io = ImGui::GetIO();
            for(const auto *fontPath: fontList){
                if(auto *fp = std::fopen(fontPath, "rb")){
                    std::fclose(fp);
                    if(io.Fonts->AddFontFromFileTTF(fontPath, fFontPixelSize, nullptr, io.Fonts->GetGlyphRangesChineseFull())){
                        return;
                    }
                }
            }
            io.Fonts->AddFontDefault();
        }

        void drawAlert()
        {
            const std::string popup = m_alertTitle + "###ToolAlert";
            if(m_alertRequested){
                ImGui::OpenPopup(popup.c_str());
                m_alertRequested = false;
            }
            if(ImGui::BeginPopupModal(popup.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
                ImGui::TextWrapped("%s", m_alertText.c_str());
                ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 100.0f) * 0.5f);
                if(ImGui::Button("OK", ImVec2(100, 0))){
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
};

class AttributeSelector
{
    public:
        bool open = false;
        bool walkable = false;
        bool flyable = false;
        bool grass = false;
        bool stone = false;
        bool pond = false;
        bool sand = false;
        bool ocean = false;

    public:
        void draw(const char *);
        bool testLand(const Mir2xMapData::LAND &) const;
};

class MainWindow final: public ImGuiApp
{
    private:
        struct CachedImage
        {
            GLTexture texture;
            bool attempted = false;
        };

        enum class PendingLoad
        {
            None,
            Layer,
            Mir2Map,
            Mir2xMapData,
        };

        std::unordered_map<uint32_t, CachedImage> m_imageCache;
        std::unique_ptr<ImageMapDB> m_imageMapDB;
        LayerBrowserWindow m_layerBrowser;
        AttributeSelector m_attributeSelect;
        AttributeSelector m_attributeGrid;
        WilAniTimer m_aniTimer;
        std::unique_ptr<AnimationDB> m_animationDB;

        std::string m_wilPath;
        std::string m_workingPath;
        std::string m_status = "no map loaded";
        PendingLoad m_pendingLoad = PendingLoad::None;
        bool m_continuePendingLoad = false;

        float m_scrollX = 0.0f;
        float m_scrollY = 0.0f;
        bool m_showAbout = false;
        bool m_confirmQuit = false;

        bool m_gridLine = false;
        bool m_attributeLine = false;
        bool m_lightLine = false;
        bool m_tileLine = false;
        bool m_objectLine[4] = {};
        bool m_objectIndexLine[2] = {};

        bool m_showLight = true;
        bool m_showTile = true;
        bool m_showObject[4] = {};
        bool m_showObjectIndex[2] = {true, true};
        bool m_removeShadowMosaic = true;
        bool m_clearBackground = true;

        bool m_enableEdit = false;
        bool m_editGround = false;
        bool m_enableSelect = false;
        int m_selectMode = 1;
        bool m_reversed = false;
        bool m_deselect = false;
        bool m_enableTest = false;
        int m_animationIndex = 0;
        int m_layerMode = 0;

        ImGuiFileDialog m_wilDialog {"MapWilPath"};
        ImGuiFileDialog m_workingDialog {"MapWorkingPath"};
        ImGuiFileDialog m_mapDialog {"MapLoad"};
        ImGuiFileDialog m_layerDialog {"LayerLoad"};
        ImGuiFileDialog m_mapDataDialog {"MapDataLoad"};

    public:
        MainWindow();
        ~MainWindow();

        int getAnimationIndex() const { return m_enableTest ? m_animationIndex : -1; }
        bool removeShadowMosaicEnabled() const { return m_removeShadowMosaic; }
        void drawLayerCanvas(const Mir2xMapData &, float &, float &);

    protected:
        void draw() override;
        void update(double) override;
        bool onCloseRequested() override;

    private:
        void drawMenu();
        void drawEditor();
        void drawDialogs();
        void drawModals();
        void renderEditorCanvas(const ImVec2 &, const ImVec2 &);
        void handleEditorInput(const ImVec2 &, const ImVec2 &, float, float);
        void addSelection(int, int, float, float, float, float);
        CachedImage *retrieveImage(uint32_t);
        void clearImageCache() { m_imageCache.clear(); }

        void requestLoad(PendingLoad);
        void beginPendingLoad();
        void loadMir2Map(const std::string &);
        void loadLayer(const std::string &);
        void loadMir2xMapData(const std::string &);
        void afterLoadMap(const std::string &);
        void makeWorkingFolder();
        void saveMir2xMapData();
        void extractOverview(int);
        bool testAttribute(const Mir2xMapData::LAND &) const;

        friend class LayerBrowserWindow;
};

extern MainWindow *g_mainWindow;
extern ImageMapDB *g_imageMapDB;
extern EditorMap g_editorMap;
extern LayerBrowserWindow *g_layerBrowserWindow;
