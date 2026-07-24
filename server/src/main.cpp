#include <ctime>
#include <asio.hpp>
#include "log.hpp"
#include "argf.hpp"
#include "dbpod.hpp"
#include "server.hpp"
#include "mapbindb.hpp"
#include "actorpool.hpp"
#include "peerconfig.hpp"
#include "serverargparser.hpp"
#include "imguiui/guicore.hpp"

ServerArgParser          *g_serverArgParser;
PeerConfig               *g_peerConfig;
Log                      *g_mir2xLog;
ActorPool                *g_actorPool;
DBPod                    *g_dbPod;

MapBinDB                 *g_mapBinDB;
Server                   *g_server;
GUICore                  *g_guiCore;

int main(int argc, char *argv[])
{
    std::srand((unsigned int)std::time(nullptr));
    try{
        argf::parser cmdParser(argc, argv);
        g_serverArgParser = new ServerArgParser(cmdParser);

        if(g_serverArgParser->disableProfiler){
            logDisableProfiler();
        }

        std::atexit(+[]()
        {
            logProfiling([](const std::string &s)
            {
                std::printf("%s", s.c_str());
            });
        });

        g_mir2xLog       = new Log("mir2x-server-v0.1");
        g_server    = new Server();
        g_mapBinDB  = new MapBinDB();
        g_actorPool = new ActorPool(g_serverArgParser->actorPoolThread);

        if(g_serverArgParser->slave){
            g_peerConfig = new PeerConfig();
        }

        if(!g_serverArgParser->slave){
            g_dbPod = new DBPod();
        }

        if(!g_serverArgParser->slave){
            g_guiCore = new GUICore();

            if(g_serverArgParser->masterConfig().autoLaunch){
                g_server->launch();
                g_guiCore->setLaunched(true);
            }
        }

        g_server->mainLoop();
    }
    catch(const std::exception &e){
        // use raw log directly
        // no gui available because we are out of gui event loop
        g_mir2xLog->addLog(LOGTYPE_WARNING, "Exception in main thread: %s", e.what());
    }
    catch(...){
        g_mir2xLog->addLog(LOGTYPE_WARNING, "Unknown exception caught in main thread");
    }
    return 0;
}
