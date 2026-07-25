#include <sstream>

#include <GLFW/glfw3.h>
#include <imgui.h>

#include "logprof.hpp"
#include "guicore.hpp"
#include "guiprofilerwindow.hpp"

void GUIProfilerWindow::draw()
{
    const auto now = glfwGetTime();
    if(now - m_lastRefresh >= 1.370){
        m_lastRefresh = now;
        m_rows.clear();
        m_summary.clear();
        logProfiling([this](const std::string &s)
        {
            if(!s.starts_with("--- ")){
                return;
            }

            auto content = s.substr(4);
            if(content.starts_with("Profiler ran ")){
                while(!content.empty() && (content.back() == '\n' || content.back() == '\r')){
                    content.pop_back();
                }
                m_summary = std::move(content);
                return;
            }

            Row row;
            std::string longestUnit;
            std::string totalUnit;
            std::istringstream input(content);
            if(input >> row.name >> row.calls >> row.longestTime >> longestUnit >> row.totalTime >> totalUnit;
                    longestUnit == "secs" && totalUnit == "secs"){
                m_rows.push_back(std::move(row));
            }
        });
    }

    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(830, 505), ImGuiCond_Appearing);
    if(ImGui::Begin("Profiler", &open)){
        const float footerHeight = m_summary.empty() ? 0.0f : ImGui::GetFrameHeightWithSpacing();
        if(ImGui::BeginTable("ProfilerTable", 4,
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_ScrollY |
                    ImGuiTableFlags_SizingStretchProp,
                    ImVec2(0, -footerHeight))){
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Command Name", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Calls",        ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn("Longest Time", ImGuiTableColumnFlags_WidthFixed, 160.0f);
            ImGui::TableSetupColumn("Total Time",   ImGuiTableColumnFlags_WidthFixed, 160.0f);
            ImGui::TableHeadersRow();

            for(const auto &row: m_rows){
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row.name.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%lld", row.calls);
                ImGui::TableNextColumn();
                ImGui::Text("%.3f secs", row.longestTime);
                ImGui::TableNextColumn();
                ImGui::Text("%.3f secs", row.totalTime);
            }
            ImGui::EndTable();
        }

        if(!m_summary.empty()){
            ImGui::TextDisabled("%s", m_summary.c_str());
        }
    }
    ImGui::End();

    if(!open){
        m_core->setProfilerOpen(false);
    }
}
