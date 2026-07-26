#pragma once
#include <vector>
#include <cstdint>
#include <GLTexture.hpp>

class MainWindow;

class PreviewWindow
{
    MainWindow *m_owner = nullptr;
    std::vector<uint32_t> m_imageBuf;
    GLTexture m_texture;
    int m_imageOffX = 0;
    int m_imageOffY = 0;
    public:
        explicit PreviewWindow(MainWindow *owner): m_owner(owner) {}
        void draw();
        bool loadImage();
        void clear();
};
