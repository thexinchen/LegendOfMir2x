#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include <imgui.h>

#include "editormap.hpp"
#include "animationdb.hpp"
#include "imagemapdb.hpp"
#include "imguihelper.hpp"
#include "layerbrowserwindow.hpp"
#include "landtype.hpp"
#include "wilanitimer.hpp"

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
            ImGuiTexture texture;
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
