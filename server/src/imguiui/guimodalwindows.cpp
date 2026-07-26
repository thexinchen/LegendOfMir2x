#include <algorithm>
#include <cstring>

#include <imgui.h>

#include "log.hpp"
#include "strf.hpp"
#include "totype.hpp"
#include "filesys.hpp"
#include "message.hpp"
#include "uidf.hpp"
#include "uidsf.hpp"
#include "serdesmsg.hpp"
#include "dispatcher.hpp"
#include "serverargparser.hpp"
#include "guicore.hpp"
#include "guimodalwindows.hpp"

extern Log *g_mir2xLog;
extern ServerArgParser *g_serverArgParser;

GUIConfigureWindow::GUIConfigureWindow(GUICore *core)
    : m_core(core)
{
    const auto config = m_core->getConfig();
    std::strncpy(m_mapPath,        config.mapPath.c_str(), sizeof(m_mapPath) - 1);
    std::strncpy(m_scriptPath,     config.scriptPath.c_str(), sizeof(m_scriptPath) - 1);
    std::strncpy(m_maxPlayerCount, std::to_string(config.maxPlayerCount).c_str(), sizeof(m_maxPlayerCount) - 1);
    std::strncpy(m_experienceRate, str_printf("%.2f", config.experienceRate).c_str(), sizeof(m_experienceRate) - 1);
    std::strncpy(m_dropRate,       str_printf("%.2f", config.dropRate).c_str(), sizeof(m_dropRate) - 1);
    std::strncpy(m_goldRate,       str_printf("%.2f", config.goldRate).c_str(), sizeof(m_goldRate) - 1);

    // command-line ports take precedence; not editable unless explicitly given
    std::strncpy(m_clientPort, std::to_string(g_serverArgParser->masterConfig().clientPort.first).c_str(), sizeof(m_clientPort) - 1);
    m_clientPortEditable = g_serverArgParser->masterConfig().clientPort.second;

    std::strncpy(m_slavePort, std::to_string(g_serverArgParser->peerPort.first).c_str(), sizeof(m_slavePort) - 1);
    m_slavePortEditable = g_serverArgParser->peerPort.second;
}

void GUIConfigureWindow::draw()
{
    bool open = true;
    ImGui::SetNextWindowSizeConstraints(ImVec2(570, 0), ImVec2(570, FLT_MAX));
    if(ImGui::Begin("Server Configure", &open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)){
        const float rowHeight = ImGui::GetFrameHeightWithSpacing();
        const float labelWidth = ImGui::CalcTextSize("Experience Rate:").x + ImGui::GetStyle().ItemSpacing.x;
        const float browseWidth = ImGui::GetFrameHeight();

        if(ImGui::BeginTable("ConfigureFields", 3, ImGuiTableFlags_SizingStretchProp)){
            ImGui::TableSetupColumn("Label",  ImGuiTableColumnFlags_WidthFixed, labelWidth);
            ImGui::TableSetupColumn("Value",  ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Browse", ImGuiTableColumnFlags_WidthFixed, browseWidth);

            const auto drawLabel = [rowHeight](const char *label)
            {
                ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(label);
                ImGui::TableSetColumnIndex(1);
            };

            drawLabel("Map Path:");
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##mapPath", m_mapPath, sizeof(m_mapPath));
            ImGui::TableSetColumnIndex(2);
            if(ImGui::Button("...##map", ImVec2(browseWidth, 0))){
                m_selectingScriptDirectory = false;
                m_fileDialog.open("Select map package", m_mapPath, ImGuiFileDialog::Mode::File, ".zsdb");
            }

            drawLabel("Script Path:");
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##scriptPath", m_scriptPath, sizeof(m_scriptPath));
            ImGui::TableSetColumnIndex(2);
            if(ImGui::Button("...##script", ImVec2(browseWidth, 0))){
                m_selectingScriptDirectory = true;
                m_fileDialog.open("Select script directory", m_scriptPath, ImGuiFileDialog::Mode::Directory);
            }

            const auto drawField = [&drawLabel](const char *label, const char *id, char *buffer, size_t bufferSize)
            {
                drawLabel(label);
                ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
                ImGui::InputText(id, buffer, bufferSize);
            };

            drawField("Maximal Player:",  "##maxPlayer",      m_maxPlayerCount, sizeof(m_maxPlayerCount));
            drawField("Experience Rate:", "##experienceRate", m_experienceRate, sizeof(m_experienceRate));
            drawField("Drop Rate:",       "##dropRate",       m_dropRate,       sizeof(m_dropRate));
            drawField("Gold Rate:",       "##goldRate",       m_goldRate,       sizeof(m_goldRate));

            if(!m_clientPortEditable){
                ImGui::BeginDisabled();
            }
            drawField("Client Port:", "##clientPort", m_clientPort, sizeof(m_clientPort));
            if(!m_clientPortEditable){
                ImGui::EndDisabled();
            }

            if(!m_slavePortEditable){
                ImGui::BeginDisabled();
            }
            drawField("SlavePort:", "##slavePort", m_slavePort, sizeof(m_slavePort));
            if(!m_slavePortEditable){
                ImGui::EndDisabled();
            }
            ImGui::EndTable();
        }

        ImGui::Separator();
        const float buttonWidth = std::max(70.0f, ImGui::CalcTextSize("Cancel").x + ImGui::GetStyle().FramePadding.x * 2.0f);
        const float buttonGroupWidth = buttonWidth * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
        if(const float availableWidth = ImGui::GetContentRegionAvail().x; availableWidth > buttonGroupWidth){
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availableWidth - buttonGroupWidth);
        }

        if(ImGui::Button("Cancel", ImVec2(buttonWidth, 0))){
            open = false;
        }
        ImGui::SameLine();
        if(ImGui::Button("Apply", ImVec2(buttonWidth, 0))){
            applyConfig();
        }
        ImGui::SameLine();
        if(ImGui::Button("OK", ImVec2(buttonWidth, 0))){
            applyConfig();
            open = false;
        }

        std::string selectedPath;
        if(m_fileDialog.draw(selectedPath)){
            auto *destination = m_selectingScriptDirectory ? m_scriptPath : m_mapPath;
            const auto destinationSize = m_selectingScriptDirectory ? sizeof(m_scriptPath) : sizeof(m_mapPath);
            std::strncpy(destination, selectedPath.c_str(), destinationSize - 1);
            destination[destinationSize - 1] = '\0';
        }
    }
    ImGui::End();

    if(!open){
        m_core->setConfigureOpen(false);
    }
}

void GUIConfigureWindow::applyConfig()
{
    // validation rules mirror the legacy applyConfig(); invalid values are
    // reported in the log console and keep the old config entry
    auto config = m_core->getConfig();

    const auto fnAssignPath = [](const char *val, const char *usage, std::string &dst)
    {
        if(str_haschar(val)){
            if(filesys::hasFile(val)){
                dst = val;
            }
            else{
                g_mir2xLog->addLog(LOGTYPE_WARNING, "invalid %s: %s", to_cstr(usage), to_cstr(val));
            }
        }
        else{
            dst.clear();
        }
    };

    fnAssignPath(m_mapPath,    "map path",    config.mapPath);
    fnAssignPath(m_scriptPath, "script path", config.scriptPath);

    const auto fnAssignPositiveInteger = [](const char *value, const char *usage, int &dst)
    {
        try{
            if(const int val = std::stoi(value); val > 0){
                dst = val;
                return;
            }
            g_mir2xLog->addLog(LOGTYPE_WARNING, "Invalid %s: %s", usage, to_cstr(value));
        }
        catch(...){
            g_mir2xLog->addLog(LOGTYPE_WARNING, "Invalid %s: %s", usage, to_cstr(value));
        }
    };

    fnAssignPositiveInteger(m_maxPlayerCount, "maximal player", config.maxPlayerCount);
    if(m_clientPortEditable){
        fnAssignPositiveInteger(m_clientPort, "client port", config.clientPort);
    }
    if(m_slavePortEditable){
        fnAssignPositiveInteger(m_slavePort, "slave port", config.slavePort);
    }

    const auto fnAssignPositiveDouble = [](const char *value, const char *usage, double &dst)
    {
        try{
            if(const double val = std::stod(value); val >= 0){
                dst = val;
                return;
            }
            g_mir2xLog->addLog(LOGTYPE_WARNING, "Invalid %s: %s", usage, to_cstr(value));
        }
        catch(...){
            g_mir2xLog->addLog(LOGTYPE_WARNING, "Invalid %s: %s", usage, to_cstr(value));
        }
    };

    fnAssignPositiveDouble(m_experienceRate, "experience rate", config.experienceRate);
    fnAssignPositiveDouble(m_dropRate,       "drop rate",       config.dropRate);
    fnAssignPositiveDouble(m_goldRate,       "gold rate",       config.goldRate);

    m_core->setConfig(config);

    // notify peers (unchanged from the legacy applyConfig)
    for(size_t i = 1; i <= uidsf::peerCount(); ++i){
        Dispatcher().post(uidf::getPeerCoreUID(i), {AM_PEERCONFIG, cerealf::serialize(SDPeerConfig
        {
            .dropRate = config.dropRate,
            .goldRate = config.goldRate,
        })});
    }

    g_mir2xLog->addLog(LOGTYPE_INFO, "Server configuration applied");
}
