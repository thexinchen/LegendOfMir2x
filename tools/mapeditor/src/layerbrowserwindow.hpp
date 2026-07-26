#pragma once

#include <vector>

#include "mir2xmapdata.hpp"
#include "sysconst.hpp"

class MainWindow;

class LayerBrowserWindow
{
    friend class MainWindow;

    private:
        std::vector<Mir2xMapData> m_layers;
        int m_selected = -1;
        bool m_importAttribute = true;
        bool m_importLight = true;
        bool m_importTile = true;
        bool m_importObject[4] = {true, true, true, true};
        bool m_viewDrawAttribute = false;
        bool m_viewDrawLight = true;
        bool m_viewDrawTile = true;
        bool m_viewDrawObject[4] = {true, true, true, true};
        bool m_viewClearBackground = true;
        bool m_viewGridLine = false;
        bool m_viewAttributeLine = false;
        bool m_viewLightLine = false;
        bool m_viewTileLine = false;
        bool m_viewObjectLine[4] = {};

    public:
        bool browserOpen = false;
        bool viewOpen = false;
        float scrollX = 0.0f;
        float scrollY = 0.0f;

    public:
        void addEntry(Mir2xMapData data);
        void drawBrowser(MainWindow *);
        void drawView(MainWindow *);

        bool importAttribute() const { return m_importAttribute; }
        bool importLight() const { return m_importLight; }
        bool importTile() const { return m_importTile; }
        bool importObject(int depthType) const;
        const Mir2xMapData *getLayer() const;
};
