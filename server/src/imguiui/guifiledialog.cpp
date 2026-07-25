#include <algorithm>
#include <system_error>

#include <imgui.h>

#include "guifiledialog.hpp"

void GUIFileDialog::open(const char *title, const char *initialPath, Mode mode, const char *extension)
{
    m_title = title ? title : "Select";
    m_mode = mode;
    m_extension = extension ? extension : "";
    m_selectedEntry = -1;

    std::error_code error;
    auto initial = std::filesystem::absolute(initialPath && initialPath[0] ? initialPath : ".", error);
    if(error){
        initial = ".";
    }

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

void GUIFileDialog::refresh()
{
    m_entries.clear();
    m_selectedEntry = -1;

    std::error_code error;
    for(std::filesystem::directory_iterator it(m_currentDirectory, error), end; !error && it != end; it.increment(error)){
        const bool isDirectory = it->is_directory(error);
        if(error){
            error.clear();
            continue;
        }
        if(!isDirectory && m_mode == Mode::Directory){
            continue;
        }
        if(!isDirectory && !m_extension.empty() && it->path().extension() != m_extension){
            continue;
        }
        m_entries.push_back({it->path(), isDirectory});
    }

    std::sort(m_entries.begin(), m_entries.end(), [](const Entry &lhs, const Entry &rhs)
    {
        if(lhs.isDirectory != rhs.isDirectory){
            return lhs.isDirectory > rhs.isDirectory;
        }
        return lhs.path.filename().string() < rhs.path.filename().string();
    });
}

void GUIFileDialog::enterDirectory(const std::filesystem::path &path)
{
    std::error_code error;
    if(std::filesystem::is_directory(path, error)){
        m_currentDirectory = path;
        refresh();
    }
}

bool GUIFileDialog::draw(std::string &selectedPath)
{
    const auto popupName = m_title + "###" + m_id;
    if(m_openRequested){
        ImGui::OpenPopup(popupName.c_str());
        m_openRequested = false;
    }

    bool open = true;
    bool accepted = false;
    ImGui::SetNextWindowSize(ImVec2(700, 480), ImGuiCond_Appearing);
    if(ImGui::BeginPopupModal(popupName.c_str(), &open)){
        if(ImGui::Button("Up")){
            enterDirectory(m_currentDirectory.parent_path());
        }
        ImGui::SameLine();
        if(ImGui::Button("Refresh")){
            refresh();
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(m_currentDirectory.string().c_str());
        ImGui::Separator();

        const float footerHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        if(ImGui::BeginChild("##entries", ImVec2(0, -footerHeight), ImGuiChildFlags_Borders)){
            for(int index = 0; index < static_cast<int>(m_entries.size()); ++index){
                const auto &entry = m_entries.at(index);
                const auto label = std::string(entry.isDirectory ? "[D] " : "    ") + entry.path.filename().string();
                if(ImGui::Selectable(label.c_str(), m_selectedEntry == index, ImGuiSelectableFlags_AllowDoubleClick)){
                    m_selectedEntry = index;
                    if(entry.isDirectory && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){
                        enterDirectory(entry.path);
                        break;
                    }
                    else if(!entry.isDirectory && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){
                        selectedPath = entry.path.string();
                        accepted = true;
                        break;
                    }
                }
            }
        }
        ImGui::EndChild();

        const bool canSelect =
            m_mode == Mode::Directory ||
            (m_selectedEntry >= 0 && !m_entries.at(m_selectedEntry).isDirectory);
        if(!canSelect){
            ImGui::BeginDisabled();
        }
        if(ImGui::Button(m_mode == Mode::Directory ? "Select current directory" : "Select", ImVec2(180, 0))){
            selectedPath = m_mode == Mode::Directory
                ? m_currentDirectory.string()
                : m_entries.at(m_selectedEntry).path.string();
            accepted = true;
        }
        if(!canSelect){
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if(ImGui::Button("Cancel", ImVec2(100, 0))){
            ImGui::CloseCurrentPopup();
        }

        if(accepted){
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return accepted;
}
