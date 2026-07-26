#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "imguihelper.hpp"
#include "wilimagepackage.hpp"

class PreviewWindow;

class MainWindow final: public ImGuiApp
{
    private:
        struct ImageEntry
        {
            uint32_t index = 0;
            std::string text;
        };

        std::unique_ptr<WilImagePackage> m_package;
        std::unique_ptr<PreviewWindow> m_preview;
        std::vector<ImageEntry> m_entries;
        std::string m_fileFullName;
        std::string m_status = "Version 0.0.1";
        int m_selectedEntry = -1;

        bool m_showAbout = false;
        bool m_busy = false;
        int m_progress = 0;
        size_t m_scanIndex = 0;
        bool m_exportAll = false;
        size_t m_exportIndex = 0;
        std::string m_exportPath;

        bool m_showOffsetCross = true;
        bool m_showLayer[3] = {true, true, true};
        bool m_saveIndex = false;
        bool m_saveLayers = false;
        bool m_saveOffset = false;
        bool m_autoAlpha = false;
        bool m_removeShadowMosaic = false;
        bool m_offsetDraw = true;
        bool m_clearBackground = false;

        ImGuiFileDialog m_openDialog {"PkgOpen"};
        ImGuiFileDialog m_exportDialog {"PkgExport"};
        ImGuiFileDialog m_exportAllDialog {"PkgExportAll"};

    public:
        MainWindow();
        ~MainWindow();

        WilImagePackage *package() const { return m_package.get(); }
        uint32_t selectedImageIndex() const;
        bool autoAlphaEnabled() const { return m_autoAlpha; }
        bool removeShadowMosaicEnabled() const { return m_removeShadowMosaic; }
        bool showOffsetCrossEnabled() const { return m_showOffsetCross; }
        bool offsetDrawEnabled() const { return m_offsetDraw; }
        bool clearBackgroundEnabled() const { return m_clearBackground; }
        bool layerIndexEnabled(int index) const { return index >= 0 && index < 3 && m_showLayer[index]; }

    protected:
        void draw() override;

    private:
        void drawMenu();
        void drawMainWindow();
        void drawDialogs();
        void drawProgress();
        void processJobs();
        void openPackage(const std::string &);
        void selectEntry(int);
        void saveImage(uint32_t, const std::string &);
        std::string imageIndexString(int) const;
};
