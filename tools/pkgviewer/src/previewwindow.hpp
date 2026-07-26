#pragma once
#include <vector>
#include <cstdint>
#include "imguihelper.hpp"

class MainWindow;

class PreviewWindow
{
    MainWindow *m_owner = nullptr;
    std::vector<uint32_t> m_imageBuf;
    ImGuiTexture m_texture;
    uint32_t m_imageIndex = 0;
    int m_imageOffX = 0;
    int m_imageOffY = 0;
    bool m_open = false;
    bool m_resizeRequested = false;
    public:
        explicit PreviewWindow(MainWindow *owner): m_owner(owner) {}
        void draw();
        bool loadImage();
        void show() { m_open = true; }
        void hide() { m_open = false; }
};
