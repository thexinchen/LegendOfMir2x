#pragma once

// ===== merged from runtimeconfigboard/labelsliderbar.hpp =====
#include "gui_core.hpp"
#include "gui_widgets.hpp"

class LabelSliderBar: public Widget
{
    private:
        LabelBoard   m_label;
        GfxCropBoard m_labelCrop;

    private:
        TexSliderBar m_slider;

    public:
        LabelSliderBar(dir8_t,
                int,
                int,

                const char8_t *,
                int, // label width

                int, // slider index
                int, // slider width
                std::function<void(float)>,

                Widget * = nullptr,
                bool     = false);

    public:
        TexSliderBar *getSlider()
        {
            return &m_slider;
        }
};

// ===== merged from runtimeconfigboard/tabheader.hpp =====
#include <functional>
#include <any>
#include "gui_core.hpp"

class TabHeader: public Widget
{
    private:
        LabelBoard m_label;
        TrigfxButton m_button;

    public:
        TabHeader(dir8_t,
                int,
                int,

                const char8_t *,
                std::function<void(Widget *, int)>,

                std::any,

                Widget * = nullptr,
                bool     = false);
};

// ===== merged from runtimeconfigboard/menupage.hpp =====
#include <tuple>
#include <initializer_list>
#include "gui_core.hpp"

class MenuPage: public Widget
{
    private:
        GfxShapeBoard m_tabHeaderBg;

    private:
        Widget *m_selectedHeader = nullptr;

    public:
        MenuPage(dir8_t,
                int,
                int,

                Widget::VarSizeOpt,
                int,

                std::initializer_list<std::tuple<const char8_t *, Widget *, bool>>,

                Widget * = nullptr,
                bool     = false);
};

// ===== merged from runtimeconfigboard/runtimeconfigboard.hpp =====
#include <tuple>
#include <string>
#include "mathf.hpp"
#include "sdruntimeconfig.hpp"
#include "layoutboard.hpp"
#include "gui_core.hpp"
#include "gui_widgets.hpp"

class ProcessRun;
class RuntimeConfigBoard: public Widget
{
    private:
        friend class LabelSliderBar;
        friend class MenuPage;

    private:
        SDRuntimeConfig m_sdRuntimeConfig;

    private:
        BaseFrameBoard m_frameBoard;

    private:
        GfxShapeBoard m_leftMenuBackground;
        LayoutBoard    m_leftMenu;

    private:
        PullMenu       m_pageSystem_resolution;
        PullMenu       m_pageSystem_ime;
        LabelSliderBar m_pageSystem_musicSlider;
        LabelSliderBar m_pageSystem_soundEffectSlider;
        MenuPage       m_pageSystem;

    private:
        MenuPage m_pageSocial;

    private:
        MenuPage m_pageGameConfig;

    private:
        ProcessRun *m_processRun;

    public:
        RuntimeConfigBoard(int, int, int, int, ProcessRun *, Widget * = nullptr, bool = false);

    public:
        void drawDefault(Widget::ROIMap) const override;

    public:
        bool processEventDefault(const MirEvent &, bool, Widget::ROIMap) override;

    private:
        void reportRuntimeConfig(int);
        void applyAudioConfig();

    public:
        void setConfig(const SDRuntimeConfig &);

    public:
        const SDRuntimeConfig &getConfig() const
        {
            return m_sdRuntimeConfig;
        }

    public:
        void updateWindowSize(std::pair<int, int>, bool);
        void updateIME(int, bool);
};
