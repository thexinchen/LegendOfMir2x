#pragma once
//
// Stage 1a of the FLTK -> ImGui migration.
//
// The plain ServerConfig struct used to live inside the fluid-generated
// serverconfigurewindow.fl; the GUI-thread config store lived in the
// ServerConfigureWindow class. Both are extracted here so the server core
// (which reads the config from actor/DB threads) no longer depends on FLTK.
// The ImGui configure dialog re-uses this store in Stage 2.

#include <mutex>
#include <string>

struct ServerConfig
{
    std::string mapPath    = "map/mapbin.zsdb";
    std::string scriptPath = "script";

    int    maxPlayerCount = 5000;
    double experienceRate = 1.0;
    double dropRate       = 1.0;
    double goldRate       = 1.0;

    int clientPort = 5000;
    int slavePort  = 6000;
};

class ServerConfigStore
{
    private:
        mutable std::mutex m_lock;
        ServerConfig       m_config;

    public:
        ServerConfig getConfig() const
        {
            std::lock_guard<std::mutex> lockGuard(m_lock);
            return m_config;
        }

        void setConfig(const ServerConfig &config)
        {
            std::lock_guard<std::mutex> lockGuard(m_lock);
            m_config = config;
        }
};
