#include <cstring>

#include <imgui.h>
#include <tinyfiledialogs/tinyfiledialogs.h>

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
    ImGui::SetNextWindowSize(ImVec2(460, 0), ImGuiCond_FirstUseEver);
    if(ImGui::Begin("Server Configure", &open)){
        if(ImGui::Button("Browse...##map")){
            if(const char *picked = tinyfd_openFileDialog("Map package", m_mapPath, 1, (const char * const[]){"*.zsdb"}, "map package (*.zsdb)", 0)){
                std::strncpy(m_mapPath, picked, sizeof(m_mapPath) - 1);
            }
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("Map path", m_mapPath, sizeof(m_mapPath));

        if(ImGui::Button("Browse...##script")){
            if(const char *picked = tinyfd_selectFolderDialog("Script directory", m_scriptPath)){
                std::strncpy(m_scriptPath, picked, sizeof(m_scriptPath) - 1);
            }
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("Script path", m_scriptPath, sizeof(m_scriptPath));

        ImGui::InputText("Max player", m_maxPlayerCount, sizeof(m_maxPlayerCount));
        ImGui::InputText("Experience rate", m_experienceRate, sizeof(m_experienceRate));
        ImGui::InputText("Drop rate", m_dropRate, sizeof(m_dropRate));
        ImGui::InputText("Gold rate", m_goldRate, sizeof(m_goldRate));

        if(!m_clientPortEditable){
            ImGui::BeginDisabled();
        }
        ImGui::InputText("Client port", m_clientPort, sizeof(m_clientPort));
        if(!m_clientPortEditable){
            ImGui::EndDisabled();
        }

        if(!m_slavePortEditable){
            ImGui::BeginDisabled();
        }
        ImGui::InputText("Slave port", m_slavePort, sizeof(m_slavePort));
        if(!m_slavePortEditable){
            ImGui::EndDisabled();
        }

        ImGui::Separator();
        if(ImGui::Button("Apply", ImVec2(100, 0))){
            applyConfig();
        }
        ImGui::SameLine();
        if(ImGui::Button("Close", ImVec2(100, 0))){
            open = false;
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
