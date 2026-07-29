#include "fflerror.hpp"
#include "gldevice.hpp"
#include "imeboard.hpp"
#include "processrun.hpp"
#include "guimanager.hpp"
#include "clientargparser.hpp"

extern IMEBoard *g_imeBoard;
extern GLDevice *g_glDevice;
extern ClientArgParser *g_clientArgParser;

GUIManager::GUIManager(ProcessRun *argProc)
    : Widget
      {{
          .w = []{ return g_glDevice->getRendererWidth();  },
          .h = []{ return g_glDevice->getRendererHeight(); },
      }}

    , m_processRun(argProc)
    , m_NPCChatBoard
      {
          argProc,
      }

    , m_mainUI(argProc)

    , m_friendChatBoard
      {
          g_glDevice->getRendererWidth()  / 2 - 250,
          g_glDevice->getRendererHeight() / 2 - 250,
          argProc,
      }

    , m_horseBoard
      {
          argProc,
      }

    , m_skillBoard
      {
          g_glDevice->getRendererWidth()  / 2 - 180,
          g_glDevice->getRendererHeight() / 2 - 224,
          argProc,
      }

    , m_guildBoard
      {
          argProc,
      }

    , m_miniMapBoard
      {
          argProc,
      }

    , m_acutionBoard
      {
          argProc,
      }

    , m_purchaseBoard
      {
          argProc,
      }

    , m_teamStateBoard
      {
          argProc,
      }

    , m_inventoryBoard
      {
          argProc,
      }

    , m_questStateBoard
      {
          argProc,
      }

    , m_playerStateBoard
      {
          argProc,
      }

    , m_inputStringBoard {}

    , m_runtimeConfigBoard
      {
          g_glDevice->getRendererWidth()  / 2 - 255,
          g_glDevice->getRendererHeight() / 2 - 234,

          600,
          480,

          argProc,
          this,
      }

    , m_securedItemListBoard
      {
          m_processRun,
      }
{
    fflassert(m_processRun);
    g_imeBoard->dropFocus();
}

void GUIManager::drawDefault(Widget::ROIMap m) const
{
    if(!m.calibrate(this)){
        return;
    }

    m_miniMapBoard.draw();
    m_NPCChatBoard.draw();
    m_friendChatBoard.draw();
    m_skillBoard.draw();
    m_mainUI.draw();
    m_acutionBoard.draw();
    m_horseBoard.draw();
    m_guildBoard.draw();
    m_inputStringBoard.draw();
    m_questStateBoard.draw();
    m_teamStateBoard.draw();
    m_securedItemListBoard.draw();
    m_inventoryBoard.draw();
    m_playerStateBoard.draw();

    m_purchaseBoard.draw();

    Widget::drawDefault(m);

    if(g_imeBoard->show()){
        g_imeBoard->drawRoot({});
    }
}

void GUIManager::updateDefault(double fUpdateTime)
{
    Widget::updateDefault(fUpdateTime);
    m_purchaseBoard.update(fUpdateTime);
    m_mainUI.update(fUpdateTime);
    m_acutionBoard.update(fUpdateTime);
    m_horseBoard.update(fUpdateTime);
    m_guildBoard.update(fUpdateTime);
    m_inputStringBoard.update(fUpdateTime);
    m_questStateBoard.update(fUpdateTime);
    m_teamStateBoard.update(fUpdateTime);
    m_securedItemListBoard.update(fUpdateTime);
    m_inventoryBoard.update(fUpdateTime);
    m_playerStateBoard.update(fUpdateTime);
    m_miniMapBoard.update(fUpdateTime);
    m_NPCChatBoard.update(fUpdateTime);
    m_friendChatBoard.update(fUpdateTime);
    m_skillBoard.update(fUpdateTime);
    g_imeBoard->update(fUpdateTime);
}

bool GUIManager::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    switch(event.type){
        case MIR_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case MIR_EVENT_WINDOW_RESIZED:
            {
                afterResize();
                return true;
            }
        default:
            {
                break;
            }
    }

    bool tookEvent = false;
    const auto fnProcEventRoot = [&event, valid, &tookEvent](Widget *widget)
    {
        if(widget->show()){
            tookEvent |= widget->processEventRoot(event, valid && !tookEvent, {});
        }
    };

    fnProcEventRoot(g_imeBoard);

    tookEvent |= Widget::processEventDefault(event, valid && !tookEvent, m);

    tookEvent |= valid && !tookEvent && m_purchaseBoard.processEvent(event);
    tookEvent |= valid && !tookEvent && m_mainUI.processEvent(event);
    tookEvent |= valid && !tookEvent && m_acutionBoard.processEvent(event);
    tookEvent |= valid && !tookEvent && m_horseBoard.processEvent(event);
    tookEvent |= valid && !tookEvent && m_guildBoard.processEvent(event);
    tookEvent |= valid && !tookEvent && m_inputStringBoard.processEvent(event);
    tookEvent |= valid && !tookEvent && m_questStateBoard.processEvent(event);
    tookEvent |= valid && !tookEvent && m_teamStateBoard.processEvent(event);
    tookEvent |= valid && !tookEvent && m_securedItemListBoard.processEvent(event);
    tookEvent |= valid && !tookEvent && m_inventoryBoard.processEvent(event);
    tookEvent |= valid && !tookEvent && m_playerStateBoard.processEvent(event);
    tookEvent |= valid && !tookEvent && m_NPCChatBoard.processEvent(event);
    tookEvent |= valid && !tookEvent && m_friendChatBoard.processEvent(event);
    tookEvent |= valid && !tookEvent && m_skillBoard.processEvent(event);
    tookEvent |= valid && !tookEvent && m_miniMapBoard.processEvent(event);

    return tookEvent;
}

Widget *GUIManager::getWidget(const std::string_view &name)
{
    if(name == "RuntimeConfigBoard"){
        return &m_runtimeConfigBoard;
    }

    else{
        throw fflvalue(name);
    }
}

void GUIManager::flipBoard(std::string_view name)
{
    if(name == "HorseBoard"){
        m_horseBoard.flipShow();
        return;
    }
    if(name == "GuildBoard"){
        m_guildBoard.flipShow();
        return;
    }
    if(name == "QuestStateBoard"){
        m_questStateBoard.flipShow();
        return;
    }
    if(name == "TeamStateBoard"){
        m_teamStateBoard.flipShow();
        return;
    }
    if(name == "InventoryBoard"){
        m_inventoryBoard.flipShow();
        return;
    }
    if(name == "PlayerStateBoard"){
        m_playerStateBoard.flipShow();
        return;
    }
    if(name == "FriendChatBoard"){
        m_friendChatBoard.flipShow();
        return;
    }
    if(name == "SkillBoard"){
        m_skillBoard.flipShow();
        return;
    }
    getWidget(name)->flipShow();
}

void GUIManager::afterResizeDefault()
{
    m_runtimeConfigBoard.updateWindowSize({w(), h()}, true);

    const auto fnSetWidgetPLoc = [this](Widget *widgetPtr)
    {
        const auto moveDX = std::max<int>(widgetPtr->dx() - (w() - widgetPtr->w()), 0);
        const auto moveDY = std::max<int>(widgetPtr->dy() - (h() - widgetPtr->h()), 0);

        // move upper-left
        //
        widgetPtr->moveBy(-moveDX, -moveDY);
    };

    fnSetWidgetPLoc(g_imeBoard);
}
