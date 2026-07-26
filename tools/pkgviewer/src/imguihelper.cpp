#include <cstdio>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "imguihelper.hpp"
namespace
{
    void glfwErrorCallback(int error, const char *description)
    {
        std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
    }
}
ImGuiApp::ImGuiApp(const char *title, int width, int height, const char *iniFile)
    : m_iniFile(iniFile ? iniFile : "mir2x-tool.imgui.ini")
{
    glfwSetErrorCallback(glfwErrorCallback);
    if(!glfwInit()){
        throw std::runtime_error("Failed to initialize GLFW");
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if(!m_window){
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = m_iniFile.c_str();
    ImGui::StyleColorsDark();
    setupFonts();
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}
ImGuiApp::~ImGuiApp()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if(m_window){
        glfwDestroyWindow(m_window);
    }
    glfwTerminate();
}
void ImGuiApp::setupFonts()
{
    static const char *fontList[] = {
#ifdef _WIN32
        "C:/Windows/Fonts/msyh.ttc", "C:/Windows/Fonts/simhei.ttf",
#else
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
#endif
    };
    auto &io = ImGui::GetIO();
    for(const auto *fontPath: fontList){
        if(auto *fp = std::fopen(fontPath, "rb")){
            std::fclose(fp);
            if(io.Fonts->AddFontFromFileTTF(fontPath, 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull())){
                return;
            }
        }
    }
    io.Fonts->AddFontDefault();
}
int ImGuiApp::run()
{
    double lastTime = glfwGetTime();
    while(true){
        if(glfwWindowShouldClose(m_window)){
            if(m_forceClose || onCloseRequested()){
                break;
            }
            glfwSetWindowShouldClose(m_window, GLFW_FALSE);
        }
        glfwPollEvents();
        const double now = glfwGetTime();
        update(now - lastTime);
        lastTime = now;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        draw();
        drawAlert();
        ImGui::Render();
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(m_window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
    }
    return 0;
}
void ImGuiApp::close()
{
    m_forceClose = true;
    glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}
void ImGuiApp::alert(const std::string &text, const std::string &title)
{
    m_alertTitle = title;
    m_alertText = text;
    m_alertRequested = true;
}
void ImGuiApp::drawAlert()
{
    const std::string popup = m_alertTitle + "###ToolAlert";
    if(m_alertRequested){
        ImGui::OpenPopup(popup.c_str());
        m_alertRequested = false;
    }
    if(ImGui::BeginPopupModal(popup.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextWrapped("%s", m_alertText.c_str());
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 100.0f) * 0.5f);
        if(ImGui::Button("OK", ImVec2(100, 0))){
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

#include <algorithm>
#include <cctype>
#include <system_error>

namespace
{
    std::string pathUTF8(const std::filesystem::path &path)
    {
        const auto text = path.u8string();
        return {reinterpret_cast<const char *>(text.data()), text.size()};
    }
}

void ImGuiFileDialog::open(const char *title, const char *initialPath, Mode mode, const char *extension)
{
    m_title = title ? title : "Select";
    m_mode = mode;
    m_extension = extension ? extension : "";
    std::transform(m_extension.begin(), m_extension.end(), m_extension.begin(), [](unsigned char ch){ return std::tolower(ch); });
    m_selected = -1;
    std::error_code error;
    auto initial = std::filesystem::absolute(initialPath && initialPath[0] ? initialPath : ".", error);
    if(error){ initial = "."; }
    m_currentDirectory = std::filesystem::is_directory(initial, error) ? initial : initial.parent_path();
    if(m_currentDirectory.empty()){ m_currentDirectory = std::filesystem::current_path(error); }
    refresh();
    m_openRequested = true;
}

void ImGuiFileDialog::refresh()
{
    m_entries.clear();
    m_selected = -1;
    std::error_code error;
    for(std::filesystem::directory_iterator it(m_currentDirectory, error), end; !error && it != end; it.increment(error)){
        const bool directory = it->is_directory(error);
        if(error){ error.clear(); continue; }
        if(!directory && m_mode == Mode::Directory){ continue; }
        auto extension = it->path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch){ return std::tolower(ch); });
        if(!directory && !m_extension.empty() && extension != m_extension){ continue; }
        m_entries.push_back({it->path(), directory});
    }
    std::sort(m_entries.begin(), m_entries.end(), [](const Entry &lhs, const Entry &rhs)
    {
        if(lhs.directory != rhs.directory){ return lhs.directory > rhs.directory; }
        return pathUTF8(lhs.path.filename()) < pathUTF8(rhs.path.filename());
    });
}

void ImGuiFileDialog::enterDirectory(const std::filesystem::path &path)
{
    std::error_code error;
    if(std::filesystem::is_directory(path, error)){ m_currentDirectory = path; refresh(); }
}

bool ImGuiFileDialog::draw(std::string &selectedPath, bool *cancelled)
{
    if(cancelled){ *cancelled = false; }
    const auto popup = m_title + "###" + m_id;
    if(m_openRequested){ ImGui::OpenPopup(popup.c_str()); m_openRequested = false; }
    bool accepted = false;
    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(700, 480), ImGuiCond_Appearing);
    if(ImGui::BeginPopupModal(popup.c_str(), &open)){
        if(ImGui::Button("Up")){ enterDirectory(m_currentDirectory.parent_path()); }
        ImGui::SameLine();
        if(ImGui::Button("Refresh")){ refresh(); }
        ImGui::SameLine();
        const auto currentDirectory = pathUTF8(m_currentDirectory);
        ImGui::TextUnformatted(currentDirectory.c_str());
        ImGui::Separator();
        const float footerHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        if(ImGui::BeginChild("##entries", ImVec2(0, -footerHeight), ImGuiChildFlags_Borders)){
            for(int i = 0; i < static_cast<int>(m_entries.size()); ++i){
                const auto &entry = m_entries.at(i);
                const auto label = std::string(entry.directory ? "[D] " : "    ") + pathUTF8(entry.path.filename());
                if(ImGui::Selectable(label.c_str(), i == m_selected, ImGuiSelectableFlags_AllowDoubleClick)){
                    m_selected = i;
                    if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){
                        if(entry.directory){ enterDirectory(entry.path); break; }
                        selectedPath = entry.path.string();
                        accepted = true;
                    }
                }
            }
        }
        ImGui::EndChild();
        const bool selectable = m_mode == Mode::Directory || (m_selected >= 0 && !m_entries.at(m_selected).directory);
        ImGui::BeginDisabled(!selectable);
        if(ImGui::Button(m_mode == Mode::Directory ? "Select current directory" : "Select", ImVec2(190, 0))){
            selectedPath = m_mode == Mode::Directory ? m_currentDirectory.string() : m_entries.at(m_selected).path.string();
            accepted = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", ImVec2(100, 0))){
            if(cancelled){ *cancelled = true; }
            ImGui::CloseCurrentPopup();
        }
        if(accepted){ ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if(!open && cancelled){ *cancelled = true; }
    return accepted;
}

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

ImGuiTexture::~ImGuiTexture() { clear(); }

ImGuiTexture::ImGuiTexture(ImGuiTexture &&other) noexcept
    : m_texture(other.m_texture)
    , m_width(other.m_width)
    , m_height(other.m_height)
{
    other.m_texture = 0;
    other.m_width = other.m_height = 0;
}

ImGuiTexture &ImGuiTexture::operator=(ImGuiTexture &&other) noexcept
{
    if(this != &other){
        clear();
        m_texture = other.m_texture;
        m_width = other.m_width;
        m_height = other.m_height;
        other.m_texture = 0;
        other.m_width = other.m_height = 0;
    }
    return *this;
}

void ImGuiTexture::clear()
{
    if(m_texture){ glDeleteTextures(1, &m_texture); m_texture = 0; }
    m_width = m_height = 0;
}

bool ImGuiTexture::loadRGBA(const void *data, int width, int height)
{
    if(!data || width <= 0 || height <= 0){ clear(); return false; }
    if(!m_texture){ glGenTextures(1, &m_texture); }
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

bool ImGuiTexture::loadPNG(const void *data, size_t size)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    auto *pixels = stbi_load_from_memory(static_cast<const stbi_uc *>(data), static_cast<int>(size), &width, &height, &channels, 4);
    if(!pixels){ clear(); return false; }
    const bool result = loadRGBA(pixels, width, height);
    stbi_image_free(pixels);
    return result;
}
