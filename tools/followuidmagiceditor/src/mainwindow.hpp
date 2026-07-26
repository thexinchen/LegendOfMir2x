#pragma once

#include <cstdint>

#include "imguihelper.hpp"
#include "magicdrawarea.hpp"

class MainWindow final: public ImGuiApp
{
    private:
        MagicDrawArea m_drawArea;
        double m_frameTime = 0.0;
        bool m_showAbout = false;
        bool m_confirmQuit = false;

    public:
        MainWindow(uint32_t, const char *);

    protected:
        void draw() override;
        void update(double) override;
        bool onCloseRequested() override;
};
