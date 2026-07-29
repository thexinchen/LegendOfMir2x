#pragma once

#include <tuple>

#include "ImBoard.hpp"
#include "gui_texture.hpp"

class ProcessRun;
class ImMiniMapBoard final: public ImBoard
{
    private:
        ProcessRun *m_processRun;

    private:
        mutable bool m_alphaOn = false;
        mutable bool m_extended = false;
        mutable bool m_autoCenter = true;
        mutable bool m_configActive = false;
        mutable bool m_dragStarted = false;

    private:
        mutable double m_zoomFactor = 1.0;
        mutable int m_mapImageDX = 0;
        mutable int m_mapImageDY = 0;

    public:
        explicit ImMiniMapBoard(ProcessRun *);

    public:
        void draw() const override;
        bool processEvent(const MirEvent &) const override;

    public:
        void flipAlpha() const;
        void flipExtended() const;
        void flipAutoCenter() const;

    public:
        GLTexID getMiniMapTexture() const;

    private:
        std::tuple<int, int> imageSize() const;
        std::tuple<int, int> imageOffset(int, int) const;
        std::tuple<int, int> mapLocationFromCanvas(int, int, int, int) const;
        std::tuple<int, int> canvasLocationFromMap(int, int, int, int) const;

    private:
        void fixMapImageOffset(int, int) const;
        void zoomOnCanvasAt(int, int, int, int, double) const;
};
