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
          DIR_UPLEFT,
          0,
          0,
          argProc,
      }

    , m_mainUI(argProc)

    , m_friendChatBoard
      {
          g_glDevice->getRendererWidth()  / 2 - 250,
          g_glDevice->getRendererHeight() / 2 - 250,
          argProc,
          this,
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
          this,
      }

    , m_guildBoard
      {
          argProc,
      }

    , m_miniMapBoard
      {{
          .proc = argProc,
      }}

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
          g_glDevice->getRendererWidth()  / 2 - 129,
          g_glDevice->getRendererHeight() / 2 - 122,
          argProc,
          this,
      }

    , m_inventoryBoard
      {{
          .x = g_glDevice->getRendererWidth()  / 2 - 141,
          .y = g_glDevice->getRendererHeight() / 2 - 233,

          .runProc = argProc,
          .parent{this},
      }}

    , m_questStateBoard
      {
          argProc,
      }

    , m_playerStateBoard
      {
          g_glDevice->getRendererWidth()  / 2 - 164,
          g_glDevice->getRendererHeight() / 2 - 233,
          argProc,
          this,
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
          0,
          0,
          m_processRun,
          this,
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

    m_miniMapBoard.drawRoot({});
    m_NPCChatBoard.drawRoot({});
    m_mainUI.draw();
    m_acutionBoard.draw();
    m_horseBoard.draw();
    m_guildBoard.draw();
    m_inputStringBoard.draw();
    m_questStateBoard.draw();

    if(m_purchaseBoard.show()){
        m_purchaseBoard.drawRoot({});
    }

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
    m_NPCChatBoard.update(fUpdateTime);
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

    fnProcEventRoot(&m_purchaseBoard);
    tookEvent |= valid && !tookEvent && m_mainUI.processEvent(event);
    tookEvent |= valid && !tookEvent && m_acutionBoard.processEvent(event);
    tookEvent |= valid && !tookEvent && m_horseBoard.processEvent(event);
    tookEvent |= valid && !tookEvent && m_guildBoard.processEvent(event);
    tookEvent |= valid && !tookEvent && m_inputStringBoard.processEvent(event);
    tookEvent |= valid && !tookEvent && m_questStateBoard.processEvent(event);
    fnProcEventRoot(&m_NPCChatBoard);
    fnProcEventRoot(&m_miniMapBoard);

    return tookEvent;
}

Widget *GUIManager::getWidget(const std::string_view &name)
{
    if(name == "InventoryBoard"){
        return &m_inventoryBoard;
    }

    else if(name == "NPCChatBoard"){
        return &m_NPCChatBoard;
    }

    else if(name == "FriendChatBoard"){
        return &m_friendChatBoard;
    }

    else if(name == "SkillBoard"){
        return &m_skillBoard;
    }

    else if(name == "MiniMapBoard"){
        return &m_miniMapBoard;
    }

    else if(name == "TeamStateBoard"){
        return &m_teamStateBoard;
    }

    else if(name == "PlayerStateBoard"){
        return &m_playerStateBoard;
    }

    else if(name == "PurchaseBoard"){
        return &m_purchaseBoard;
    }

    else if(name == "RuntimeConfigBoard"){
        return &m_runtimeConfigBoard;
    }

    else if(name == "SecuredItemListBoard"){
        return &m_securedItemListBoard;
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
    fnSetWidgetPLoc(&m_skillBoard);
    fnSetWidgetPLoc(&m_inventoryBoard);
    fnSetWidgetPLoc(&m_playerStateBoard);
    fnSetWidgetPLoc(&m_friendChatBoard);
}
