#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>
#include <imgui.h>

struct GLFWwindow;

class ImGuiApp
{
    GLFWwindow *m_window = nullptr;
    std::string m_iniFile;
    std::string m_alertTitle;
    std::string m_alertText;
    bool m_alertRequested = false;
    bool m_forceClose = false;
    protected:
        explicit ImGuiApp(const char *, int, int, const char *);
        virtual void draw() = 0;
        virtual void update(double) {}
        virtual bool onCloseRequested() { return true; }
    public:
        virtual ~ImGuiApp();
        ImGuiApp(const ImGuiApp &) = delete;
        ImGuiApp &operator=(const ImGuiApp &) = delete;
        int run();
        void close();
        void alert(const std::string &, const std::string & = "Error");
        GLFWwindow *window() const { return m_window; }
    private:
        void setupFonts();
        void drawAlert();
};

class ImGuiFileDialog
{
    public:
        enum class Mode { File, Directory };
    private:
        struct Entry { std::filesystem::path path; bool directory = false; };
        std::string m_id;
        std::string m_title;
        std::string m_extension;
        std::filesystem::path m_currentDirectory;
        std::vector<Entry> m_entries;
        int m_selected = -1;
        bool m_openRequested = false;
        Mode m_mode = Mode::File;
    public:
        explicit ImGuiFileDialog(const char *id): m_id(id) {}
        void open(const char *, const char *, Mode, const char * = nullptr);
        bool draw(std::string &, bool * = nullptr);
    private:
        void refresh();
        void enterDirectory(const std::filesystem::path &);
};

class ImGuiTexture
{
    unsigned int m_texture = 0;
    int m_width = 0;
    int m_height = 0;
    public:
        ImGuiTexture() = default;
        ~ImGuiTexture();
        ImGuiTexture(const ImGuiTexture &) = delete;
        ImGuiTexture &operator=(const ImGuiTexture &) = delete;
        ImGuiTexture(ImGuiTexture &&) noexcept;
        ImGuiTexture &operator=(ImGuiTexture &&) noexcept;
        bool loadRGBA(const void *, int, int);
        bool loadPNG(const void *, size_t);
        void clear();
        int width() const { return m_width; }
        int height() const { return m_height; }
        bool valid() const { return m_texture != 0; }
        ImTextureID id() const { return static_cast<ImTextureID>(m_texture); }
};
