#pragma once

#include <cstddef>

#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/gl.h>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#endif

#include <stb_image.h>

class GLTexture
{
    private:
        unsigned int m_texture = 0;
        int m_width = 0;
        int m_height = 0;

    public:
        GLTexture() = default;
        ~GLTexture()
        {
            clear();
        }

        GLTexture(const GLTexture &) = delete;
        GLTexture &operator=(const GLTexture &) = delete;

        GLTexture(GLTexture &&other) noexcept
            : m_texture(other.m_texture)
            , m_width(other.m_width)
            , m_height(other.m_height)
        {
            other.m_texture = 0;
            other.m_width = 0;
            other.m_height = 0;
        }

        GLTexture &operator=(GLTexture &&other) noexcept
        {
            if(this != &other){
                clear();
                m_texture = other.m_texture;
                m_width = other.m_width;
                m_height = other.m_height;
                other.m_texture = 0;
                other.m_width = 0;
                other.m_height = 0;
            }
            return *this;
        }

        bool loadRGBA(const void *data, int width, int height)
        {
            if(!data || width <= 0 || height <= 0){
                clear();
                return false;
            }
            if(!m_texture){
                glGenTextures(1, &m_texture);
            }
            glBindTexture(GL_TEXTURE_2D, m_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            m_width = width;
            m_height = height;
            return true;
        }

        bool loadPNG(const void *data, std::size_t size)
        {
            int width = 0;
            int height = 0;
            int channels = 0;
            auto *pixels = stbi_load_from_memory(
                    static_cast<const stbi_uc *>(data),
                    static_cast<int>(size),
                    &width,
                    &height,
                    &channels,
                    4);
            if(!pixels){
                clear();
                return false;
            }
            const bool result = loadRGBA(pixels, width, height);
            stbi_image_free(pixels);
            return result;
        }

        void clear()
        {
            if(m_texture){
                glDeleteTextures(1, &m_texture);
                m_texture = 0;
            }
            m_width = 0;
            m_height = 0;
        }

        int width() const { return m_width; }
        int height() const { return m_height; }
        bool valid() const { return m_texture != 0; }
        unsigned int id() const { return m_texture; }
};
