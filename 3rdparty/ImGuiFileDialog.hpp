#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <imgui.h>

class ImGuiFileDialog
{
    public:
        enum class Mode { File, Directory };

    private:
        struct Entry
        {
            std::filesystem::path path;
            bool directory = false;
        };

        std::string m_id;
        std::string m_title;
        std::string m_extension;
        std::filesystem::path m_currentDirectory;
        std::vector<Entry> m_entries;
        int m_selected = -1;
        bool m_openRequested = false;
        Mode m_mode = Mode::File;

    public:
        explicit ImGuiFileDialog(const char *id)
            : m_id(id)
        {}

        void open(const char *title, const char *initialPath, Mode mode, const char *extension = nullptr)
        {
            m_title = title ? title : "Select";
            m_mode = mode;
            m_extension = extension ? extension : "";
            std::transform(m_extension.begin(), m_extension.end(), m_extension.begin(), [](unsigned char ch){ return std::tolower(ch); });
            m_selected = -1;

            std::error_code error;
            auto initial = std::filesystem::absolute(pathFromUTF8(initialPath && initialPath[0] ? initialPath : "."), error);
            if(error){ initial = "."; }

            auto directory = std::filesystem::is_directory(initial, error) ? initial : initial.parent_path();
            while(!directory.empty() && !std::filesystem::is_directory(directory, error)){
                error.clear();
                const auto parent = directory.parent_path();
                if(parent == directory){
                    directory.clear();
                    break;
                }
                directory = parent;
            }
            m_currentDirectory = directory.empty() ? std::filesystem::current_path(error) : directory;
            refresh();
            m_openRequested = true;
        }

        bool draw(std::string &selectedPath, bool *cancelled = nullptr)
        {
            if(cancelled){ *cancelled = false; }
            const auto popup = m_title + "###" + m_id;
            if(m_openRequested){ ImGui::OpenPopup(popup.c_str()); m_openRequested = false; }

            bool accepted = false;
            bool open = true;
            const auto *viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
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
                                selectedPath = pathUTF8(entry.path);
                                accepted = true;
                            }
                        }
                    }
                }
                ImGui::EndChild();

                const bool selectable = m_mode == Mode::Directory || (m_selected >= 0 && !m_entries.at(m_selected).directory);
                ImGui::BeginDisabled(!selectable);
                if(ImGui::Button(m_mode == Mode::Directory ? "Select current directory" : "Select", ImVec2(190, 0))){
                    selectedPath = m_mode == Mode::Directory ? pathUTF8(m_currentDirectory) : pathUTF8(m_entries.at(m_selected).path);
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

    private:
        static std::filesystem::path pathFromUTF8(const char *text)
        {
            return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(text)));
        }

        static std::string pathUTF8(const std::filesystem::path &path)
        {
            const auto text = path.generic_u8string();
            return {reinterpret_cast<const char *>(text.data()), text.size()};
        }

        void refresh()
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

        void enterDirectory(const std::filesystem::path &path)
        {
            std::error_code error;
            if(std::filesystem::is_directory(path, error)){
                m_currentDirectory = path;
                refresh();
            }
        }
};
