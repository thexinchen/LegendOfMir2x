#include "controlboard.hpp"

// ===== merged from controlboard/cbface.cpp =====
#include <cinttypes>
#include "pngtexdb.hpp"
#include "gldevice.hpp"
#include "processrun.hpp"
#include "clientmonster.hpp"

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

CBFace::CBFace(
        Widget::VarDir argDir,
        Widget::VarInt argX,
        Widget::VarInt argY,

        ProcessRun *argProc,

        Widget *argParent,
        bool    argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),

          .w = 82, // this is the window to show face + hp bar in controlboard
          .h = 97,

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , m_processRun(argProc)
    , m_faceFull
      {{
          // size: 84 x 94
          // width has 2 empty pixels at right side
          .texLoadFunc = [this]
          {
              if(auto texPtr = g_progUseDB->retrieve(getFaceTexID())){
                  return texPtr;
              }
              return g_progUseDB->retrieve(0X010007CF);
          },
      }}

    , m_face
      {{
          .y = CBFace::BAR_HEIGHT, // 3

          .getter = &m_faceFull,
          .vr
          {
              [this]{ return m_faceFull.w() - 2; }, // 82
              [this]{ return m_faceFull.h()    ; }, // 94
          },

          .parent{this},
      }}

    , m_hpBar
      {{
          .w = [this]{ return to_dround(getHPRatio() * m_face.w()); },
          .h = CBFace::BAR_HEIGHT,

          .drawFunc = [](const Widget *self, int drawDstX, int drawDstY)
          {
              g_glDevice->fillRectangle(colorf::RED_A255, drawDstX, drawDstY, self->w(), self->h());
          },

          .parent{this},
      }}

    , m_drawBuffIDList
      {{
          .w = [this]{ return w(); },
          .h = [this]{ return h(); },

          .drawFunc = [this](const Widget *self, int drawDstX, int drawDstY)
          {
              drawBuffIDList(drawDstX, drawDstY, self->w(), self->h());
          },
      }}
{}

double CBFace::getHPRatio() const
{
    if(const auto coPtr = m_processRun->findUID(m_processRun->getFocusUID(FOCUS_MOUSE))){
        switch(coPtr->type()){
            case UID_PLY:
            case UID_MON:
                {
                    return coPtr->getHealthRatio().at(0);
                }
            default:
                {
                    break;
                }
        }
    }

    return m_processRun->getMyHero()->getHealthRatio().at(0);
}

uint32_t CBFace::getFaceTexID() const
{
    if(const auto coPtr = m_processRun->findUID(m_processRun->getFocusUID(FOCUS_MOUSE))){
        switch(coPtr->type()){
            case UID_PLY:
                {
                    return dynamic_cast<Hero *>(coPtr)->faceGfxID();
                }
            case UID_MON:
                {
                    if(const auto lookID = dynamic_cast<ClientMonster *>(coPtr)->lookID(); lookID >= 0){
                        return UINT32_C(0X01000000) + (lookID - LID_BEGIN);
                    }
                    return SYS_U32NIL;
                }
            default:
                {
                    break;
                }
        }
    }
    return m_processRun->getMyHero()->faceGfxID();
}

const std::optional<SDBuffIDList> &CBFace::getSDBuffIDListOpt() const
{
    if(const auto coPtr = m_processRun->findUID(m_processRun->getFocusUID(FOCUS_MOUSE))){
        switch(coPtr->type()){
            case UID_PLY:
            case UID_MON:
                {
                    return coPtr->getSDBuffIDListOpt();
                }
            default:
                {
                    break;
                }
        }
    }
    return m_processRun->getMyHero()->getSDBuffIDListOpt();
}

void CBFace::drawBuffIDList(int drawDstX, int drawDstY, int, int) const
{
    const auto sdBuffIDListOpt = getSDBuffIDListOpt();
    if(!sdBuffIDListOpt.has_value()){
        return;
    }

    const auto &sdBuffIDList = sdBuffIDListOpt.value();

    constexpr int buffIconDrawW = 16;
    constexpr int buffIconDrawH = 16;

    // +--16--+
    // |      |
    // |      16
    // |      |
    // *------+
    // ^
    // |
    // +--- (buffIconOffStartX, buffIconOffStartY)

    const int buffIconOffStartX = drawDstX + 20;
    const int buffIconOffStartY = drawDstY + 79;

    for(int drawIconCount = 0; const auto id: sdBuffIDList.idList){
        const auto &br = DBCOM_BUFFRECORD(id);
        fflassert(br);

        if(br.icon.gfxID != SYS_U32NIL){
            if(auto iconTexPtr = g_progUseDB->retrieve(br.icon.gfxID)){
                const int buffIconOffX = buffIconOffStartX + (drawIconCount % 5) * buffIconDrawW;
                const int buffIconOffY = buffIconOffStartY - (drawIconCount / 5) * buffIconDrawH;

                const auto [texW, texH] = GLDeviceHelper::getTextureSize(iconTexPtr);
                g_glDevice->drawTexture(iconTexPtr, buffIconOffX, buffIconOffY, buffIconDrawW, buffIconDrawH, 0, 0, texW, texH);

                const auto baseColor = [&br]() -> uint32_t
                {
                    if(br.favor > 0){
                        return colorf::GREEN;
                    }
                    else if(br.favor == 0){
                        return colorf::YELLOW;
                    }
                    else{
                        return colorf::RED;
                    }
                }();

                const auto startColor = baseColor | colorf::A_SHF(255);
                const auto   endColor = baseColor | colorf::A_SHF( 64);

                const auto edgeGridCount = (buffIconDrawW + buffIconDrawH) * 2 - 4;
                const auto startLoc = std::lround(edgeGridCount * std::fmod(m_accuTime, 1500.0) / 1500.0);

                g_glDevice->drawBoxFading(startColor, endColor, buffIconOffX, buffIconOffY, buffIconDrawW, buffIconDrawH, startLoc, buffIconDrawW + buffIconDrawH);
                drawIconCount++;
            }
        }
    }
}

// ===== merged from controlboard/cbleft.cpp =====
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <functional>

#include "colorf.hpp"
#include "totype.hpp"
#include "pngtexdb.hpp"
#include "gldevice.hpp"
#include "processrun.hpp"

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

CBLeft::CBLeft(
        Widget::VarDir argDir,
        Widget::VarInt argX,
        Widget::VarInt argY,

        ProcessRun *argProc,
        Widget *argParent,
        bool argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),
          .w = 178,
          .h = 133,

          .attrs
          {
              .inst
              {
                  .show = std::function<bool(const Widget *)>([](const Widget *self)
                  {
                      return self->hasParent<ControlBoard>()->m_minimize == false;
                  }),
              },
          },

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , m_processRun(argProc)

    , m_bgFull
      {{
          .texLoadFunc = [](const Widget *)
          {
              return g_progUseDB->retrieve(0X00000012);
          },
      }}

    , m_bg
      {{
          .getter = &m_bgFull,
          .vr
          {
              0,
              0,
              [this]{ return w(); },
              [this]{ return h(); },
          },
          .parent{this},
      }}

    , m_hpFull
      {{
          .texLoadFunc = [](const Widget *)
          {
              return g_progUseDB->retrieve(0X00000018);
          },
      }}

    , m_hp
      {{
          .dir = DIR_DOWNLEFT,
          .x = 33,
          .y = 95,

          .getter = &m_hpFull,
          .vr
          {
              0,
              [this]
              {
                  return m_hpFull.h() - m_hp.gfxCropH();
              },

              m_hpFull.w(),
              [this] -> int
              {
                  if(auto myHero = m_processRun->getMyHero()){
                      return to_dround(m_hpFull.h() * myHero->getHealthRatio().at(0));
                  }
                  return 0;
              },
          },

          .parent{this},
      }}

    , m_mpFull
      {{
          .texLoadFunc = []
          {
              return g_progUseDB->retrieve(0X00000019);
          },
      }}

    , m_mp
      {{
          .dir = DIR_DOWNLEFT,
          .x = 73,
          .y = 95,

          .getter = &m_mpFull,
          .vr
          {
              0,
              [this]
              {
                  return m_mpFull.h() - m_mp.gfxCropH();
              },

              m_mpFull.w(),
              [this]
              {
                  if(auto myHero = m_processRun->getMyHero()){
                      return to_dround(m_mpFull.h() * myHero->getHealthRatio().at(1));
                  }
                  return 0;
              },
          },

          .parent{this},
      }}

    , m_levelBarFull
      {{
          .texLoadFunc = []
          {
              return g_progUseDB->retrieve(0X000000A0);
          },
      }}

    , m_levelBar
      {{
          .dir = DIR_DOWN,
          .x = 153,
          .y = 115,

          .getter = &m_levelBarFull,
          .vr
          {
              0,
              [this]
              {
                  return m_levelBarFull.h() - m_levelBar.gfxCropH();
              },

              m_levelBarFull.w(),
              [this]
              {
                  if(auto myHero = m_processRun->getMyHero()){
                      return to_dround(m_levelBarFull.h() * myHero->getLevelRatio());
                  }
                  return 0;
              },
          },

          .parent{this},
      }}

    , m_inventoryBarFull
      {{
          .texLoadFunc = [](const Widget *)
          {
              return g_progUseDB->retrieve(0X000000A0);
          },
      }}

    , m_inventoryBar
      {{
          .dir = DIR_DOWN,
          .x = 166,
          .y = 115,

          .getter = &m_inventoryBarFull,
          .vr
          {
              0,
              [this]
              {
                  return m_inventoryBarFull.h() - m_inventoryBar.gfxCropH();
              },

              m_inventoryBarFull.w(),
              [this] -> int
              {
                  if(auto myHero = m_processRun->getMyHero()){
                      return to_dround(m_inventoryBarFull.h() * myHero->getInventoryRatio());
                  }
                  return 0;
              },
          },

          .parent{this},
      }}

    , m_buttonQuickAccess
      {{
          .x = 148,
          .y = 2,

          .texIDList
          {
              .on   = 0X0B000000,
              .down = 0X0B000001,
          },

          .onTrigger = [this](Widget *, int)
          {
              if(auto p = m_processRun->getWidget("QuickAccessBoard")){
                  p->flipShow();
              }
          },

          .parent{this},
      }}

    , m_buttonClose
      {{
          .x = 8,
          .y = 72,

          .texIDList
          {
              .on   = 0X0000001E,
              .down = 0X0000001F,
          },

          .onTrigger = [](Widget *, int)
          {
              std::exit(0);
          },

          .parent{this},
      }}

    , m_buttonMinize
      {{
          .x = 109,
          .y = 72,

          .texIDList
          {
              .on   = 0X00000020,
              .down = 0X00000021,
          },

          .parent{this},
      }}

    , m_mapGLocFull
      {{
          .textFunc = [this]
          {
              return getMapGLocStr();
          },

          .font
          {
              .id = 10,
              .size = 15,
          },
      }}

    , m_mapGLoc
      {{
          .dir = DIR_NONE,
          .x = 73,
          .y = 117,

          .getter = &m_mapGLocFull,
          .vr
          {
              [this]
              {
                  if(m_mapGLocFull.w() < m_mapGLocMaxWidth){
                      return 0;
                  }
                  return to_d(m_mapGLocPixelSpeed * m_mapGLocAccuTime / 1000.0) % (m_mapGLocFull.w() - m_mapGLocMaxWidth);
              },
              0,

              [this]
              {
                  return std::min<int>(m_mapGLocFull.w(), m_mapGLocMaxWidth);
              },

              [this](const Widget *)
              {
                  return m_mapGLocFull.h();
              },
          },

          .parent{this},
      }}
{}

std::string CBLeft::getMapGLocStr() const
{
    if(uidf::isMap(m_processRun->mapUID())){
        if(const auto &mr = DBCOM_MAPRECORD(m_processRun->mapID())){
            const auto mapNameFull = std::string(to_cstr(mr.name));
            const auto mapNameBase = mapNameFull.substr(0, mapNameFull.find('_'));

            if(auto myHero = m_processRun->getMyHero()){
                return str_printf("%s: %d %d", mapNameBase.c_str(), myHero->x(), myHero->y());
            }
            else{
                return str_printf("%s", mapNameBase.c_str());
            }
        }
    }
    return {};
}

// ===== merged from controlboard/cblevel.cpp =====
#include "bevent.hpp"
#include "gldevice.hpp"
#include "processrun.hpp"
#include "textboard.hpp"
#include "imageboard.hpp"

extern GLDevice *g_glDevice;

CBLevel::CBLevel(
        Widget::VarDir argDir,
        Widget::VarInt argX,
        Widget::VarInt argY,

        ProcessRun *argProc,
        Button::TriggerCBFunc argOnClick,

        Widget *argParent,
        bool    argAutoDelete)

    : TrigfxButton
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),

          .onTrigger = std::move(argOnClick),

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , m_processRun(argProc)
    , m_canvas
      {{
          .w = 16,
          .h = 16,

          .childList
          {
              {
                  .widget = new ImageBoard
                  {{
                      .texLoadFunc = []
                      {
                          return g_glDevice->getCover(8, 360);
                      },

                      .modColor = [this] -> uint32_t
                      {
                          switch(getState()){
                              case BEVENT_ON  : return colorf::BLUE + colorf::A_SHF(0XFF);
                              case BEVENT_DOWN: return colorf::RED  + colorf::A_SHF(0XFF);
                              default         : return 0;
                          }
                      },
                  }},

                  .dir = DIR_NONE,

                  .x = [this]{ return m_canvas.w() / 2; },
                  .y = [this]{ return m_canvas.h() / 2; },

                  .autoDelete = true,
              },

              {
                  .widget = new TextBoard
                  {{
                      .textFunc = [this] -> std::string
                      {
                          return std::to_string(m_processRun->getMyHero()->getLevel());
                      },

                      .font
                      {
                          .id = 0,
                          .size = 12,
                          .color = colorf::YELLOW + colorf::A_SHF(255),
                      },
                  }},

                  .dir = DIR_NONE,

                  .x = [this]{ return m_canvas.w() / 2; },
                  .y = [this]{ return m_canvas.h() / 2; },

                  .autoDelete = true,
              },
          },
      }}
{
    setGfxFunc([this]{ return &m_canvas; });
}

// ===== merged from controlboard/cbmiddle.cpp =====
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <functional>

#include "log.hpp"
#include "colorf.hpp"
#include "totype.hpp"
#include "pngtexdb.hpp"
#include "gldevice.hpp"
#include "imageboard.hpp"
#include "processrun.hpp"
#include "clientmonster.hpp"
#include "teamstateboard.hpp"

extern Log *g_mir2xLog;
extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

CBMiddle::CBMiddle(
        Widget::VarDir argDir,
        Widget::VarInt argX,
        Widget::VarInt argY,

        Widget::VarSizeOpt argW,

        ProcessRun *argProc,

        Widget *argParent,
        bool    argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),
          .w = std::move(argW),

          .attrs
          {
              .inst
              {
                  .show = std::function<bool(const Widget *)>([](const Widget *self)
                  {
                      if(const auto cb = self->hasParent<ControlBoard>(); !cb->m_minimize && !cb->m_expand){
                          return true;
                      }
                      return false;
                  }),

                  .moveOnFocus = false,
                  .afterResize = [](Widget *self)
                  {
                      if(auto middle = dynamic_cast<CBMiddle *>(self)){
                          middle->m_logBoard.setLineWidth(middle->getLogWindowWidth());
                          middle->m_cmdBoard.setLineWidth(0);
                      }
                  },
              },
          },

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , m_processRun(argProc)
    , m_logBoard(hasParent<ControlBoard>()->m_logBoard)
    , m_cmdBoard(hasParent<ControlBoard>()->m_cmdBoard)

    , m_bg
      {{
          .w = [this]{ return w(); },
          .h = [this]{ return h(); },

          .drawFunc = [this](const Widget *self, int drawDstX, int drawDstY)
          {
              g_glDevice->fillRectangle(colorf::A_SHF(0XFF), drawDstX, drawDstY, self->w(), self->h());
          },

          .parent{this},
      }}

    , m_face
      {
          DIR_DOWNRIGHT,
          [this]{ return w() - 19; },
          [this]{ return h() - 20; },

          argProc,

          this,
          false,
      }

    , m_bgImgFull
      {{
          .texLoadFunc = [](const Widget *)
          {
              return g_progUseDB->retrieve(0X00000013);
          },
      }}

    , m_bgImg
      {{
          .getter = std::addressof(m_bgImgFull),
          .vr
          {
              50,
              0,
              287,
              m_bgImgFull.h(),
          },

          .resize
          {
              [this]{ return             w() - (m_bgImgFull.w() - 287); },
              [this]{ return m_bgImgFull.h()                          ; },
          },

          .parent{this},
      }}

    , m_switchMode
      {{
          .x = [this]{ return w() - 15; },
          .y = 3,

          .texIDList
          {
              .on   = 0X00000028,
              .down = 0X00000029, // invalid texID
          },

          .onTrigger = [this](Widget *, int clickCount)
          {
              hasParent<ControlBoard>()->onClickSwitchModeButton(clickCount);
          },

          .onClickDone{false},
          .parent{this},
      }}

    , m_slider
      {{
          .bar
          {
              .x = [this]{ return w() - 10; },
              .y = 40,
              .w = 5,
              .h = 60,
          },

          .index = 2,
          .parent{this},
      }}

    , m_logView
      {{
          .x = LOG_WINDOW_X,
          .y = LOG_WINDOW_Y,

          .getter = std::addressof(m_logBoard),
          .vr
          {
              0,
              [this]{ return std::max<int>(0, to_dround((m_logBoard.h() - LOG_WINDOW_HEIGHT) * m_slider.getValue())); },
              [this]{ return getLogWindowWidth(); },
              LOG_WINDOW_HEIGHT,
          },

          .parent{this},
      }}

    , m_cmdView
      {{
          .x = CMD_WINDOW_X,
          .y = CMD_WINDOW_Y,

          .getter = std::addressof(m_cmdBoard),
          .vr
          {
              [this]{ return m_cmdBoardCropX ; },
              0,

              [this]{ return getCmdWindowWidth(); },
              CMD_WINDOW_HEIGHT,
          },

          .parent{this},
      }}
{
    setH([this]{ return m_bgImgFull.h(); });

    moveFront(&m_cmdView);
    moveFront(&m_logView);
    moveFront(&m_face);
    moveFront(&m_bg);
}

bool CBMiddle::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    if(Widget::processEventDefault(event, valid, m)){
        return true;
    }

    switch(event.type){
        case MIR_EVENT_KEY_DOWN:
            {
                if(!valid){
                    return false;
                }

                switch(event.key.key){
                    case MIRK_RETURN:
                        {
                            return hasParent<ControlBoard>()->m_cmdBoard.consumeFocus(true);
                        }
                    default:
                        {
                            return false;
                        }
                }
            }
        case MIR_EVENT_MOUSE_BUTTON_UP:
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
        case MIR_EVENT_MOUSE_MOTION:
        default:
            {
                return false;
            }
    }
}

void CBMiddle::onCmdCR()
{
    m_cmdBoardCropX = 0;
}

void CBMiddle::onCmdCursorMove()
{
    const auto cmdBoardCropW = getCmdWindowWidth();
    const auto cursorROI = Widget::makeROI(m_cmdBoard.getCursorPLoc());

    if(cursorROI.x < m_cmdBoardCropX){
        m_cmdBoardCropX = cursorROI.x;
    }

    if(cursorROI.x + cursorROI.w > m_cmdBoardCropX + cmdBoardCropW){
        m_cmdBoardCropX = cursorROI.x + cursorROI.w - cmdBoardCropW;
    }
}

// ===== merged from controlboard/cbmiddleexpand.cpp =====
#include "pngtexdb.hpp"
#include "gldevice.hpp"

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

CBMiddleExpand::CBMiddleExpand(
        Widget::VarDir argDir,
        Widget::VarInt argX,
        Widget::VarInt argY,

        Widget::VarSizeOpt argW,

        ProcessRun *argProc,

        Widget *argParent,
        bool    argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),
          .w = std::move(argW),

          .attrs
          {
              .inst
              {
                  .show = std::function<bool(const Widget *)>([](const Widget *self)
                  {
                      if(const auto cb = self->hasParent<ControlBoard>(); !cb->m_minimize && cb->m_expand){
                          return true;
                      }
                      return false;
                  }),

                  .afterResize = [](Widget *self)
                  {
                      if(auto expanded = dynamic_cast<CBMiddleExpand *>(self)){
                          expanded->m_logBoard.setLineWidth(expanded->getLogWindowWidth());
                          expanded->m_cmdBoard.setLineWidth(expanded->getCmdWindowWidth());
                      }
                  },
              },
          },

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , m_processRun(argProc)

    , m_logBoard(hasParent<ControlBoard>()->m_logBoard)
    , m_cmdBoard(hasParent<ControlBoard>()->m_cmdBoard)

    , m_bg
      {{
          .w = [this]{ return w(); },
          .h = [this]{ return h(); },

          .drawFunc = [this](const Widget *self, int drawDstX, int drawDstY)
          {
              g_glDevice->fillRectangle(colorf::A_SHF(0XF0), drawDstX, drawDstY, self->w(), self->h());
          },

          .parent{this},
      }}

    , m_bgImgFull
      {{
          .texLoadFunc = [](const Widget *){ return g_progUseDB->retrieve(0X00000027); },
      }}

    , m_bgImg
      {{
          .getter = std::addressof(m_bgImgFull),
          .vr
          {
              50,
              47,
              287,
              196,
          },

          .resize
          {
              [this]{ return w() - (m_bgImgFull.w() - 287); },
              [this]{ return h() - (m_bgImgFull.h() - 196); },
          },

          .parent{this},
      }}

    , m_switchMode
      {{
          .x = [this]{ return w() - 15; },
          .y = 3,

          .texIDList
          {
              .on   = 0X00000028,
              .down = 0X00000029, // invalid texID
          },

          .onTrigger = [this](Widget *, int clickCount)
          {
              hasParent<ControlBoard>()->onClickSwitchModeButton(clickCount);
          },

          .onClickDone{false},
          .parent{this},
      }}

    , m_buttonEmoji
      {{
          .x = [this]{ return w() - 94; },
          .y = [this]{ return h() - 44; },

          .texIDList
          {
              .on   = 0X00000023,
              .down = 0X00000024,
          },

          .parent{this},
      }}

    , m_buttonMute
      {{
          .x = [this]{ return w() - 54; },
          .y = [this]{ return h() - 44; },

          .texIDList
          {
              .on   = 0X00000025,
              .down = 0X00000026,
          },

          .parent{this},
      }}

    , m_slider
      {{
           .bar
           {
              .x = [this]{ return w() - 10; },
              .y = 40,
              .w = 5,
              .h = [this]{ return h() - m_bgImgFull.h() + 223; }
           },

          .index = 2,
          .parent{this},
      }}

    , m_logView
      {{
          .x = LOG_WINDOW_X,
          .y = LOG_WINDOW_Y,

          .getter = std::addressof(m_logBoard),
          .vr
          {
              0,
              [this]{ return std::max<int>(0, to_dround((m_logBoard.h() - getLogWindowHeight()) * m_slider.getValue())); },
              [this]{ return getLogWindowWidth (); },
              [this]{ return getLogWindowHeight(); },
          },

          .parent{this},
      }}

    , m_cmdView
      {{
          .x = CMD_WINDOW_X,
          .y = [this]{ return CMD_WINDOW_Y + (h() - m_bgImgFull.h()); },

          .getter = std::addressof(m_cmdBoard),
          .vr
          {
              0,
              [this]{ return m_cmdBoardCropY ; },

              [this]{ return getCmdWindowWidth(); },
              CMD_WINDOW_HEIGHT,
          },

          .parent{this},
      }}
{
    setH([this]
    {
        if(const auto cb = hasParent<ControlBoard>(); !cb->m_minimize && cb->m_expand){
            if(cb->m_maximize){
                return g_glDevice->getRendererHeight();
            }
            else{
                return std::min<int>(400, g_glDevice->getRendererHeight());
            }
        }
        return 0;
    });

    moveFront(&m_cmdView);
    moveFront(&m_logView);
    moveFront(&m_bg);
}

bool CBMiddleExpand::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    if(Widget::processEventDefault(event, valid, m)){
        return true;
    }

    switch(event.type){
        case MIR_EVENT_KEY_DOWN:
            {
                switch(event.key.key){
                    case MIRK_RETURN:
                        {
                            return valid && hasParent<ControlBoard>()->m_cmdBoard.consumeFocus(true);
                        }
                    default:
                        {
                            return false;
                        }
                }
            }
        case MIR_EVENT_MOUSE_BUTTON_UP:
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
        case MIR_EVENT_MOUSE_MOTION:
        default:
            {
                return false;
            }
    }
}

void CBMiddleExpand::onCmdCR()
{
    m_cmdBoardCropY = 0;
}

void CBMiddleExpand::onCmdCursorMove()
{
    const auto cmdBoardCropH = CMD_WINDOW_HEIGHT;
    const auto cursorROI = Widget::makeROI(m_cmdBoard.getCursorPLoc());

    if(cursorROI.y < m_cmdBoardCropY){
        m_cmdBoardCropY = cursorROI.y;
    }

    if(cursorROI.y + cursorROI.h > m_cmdBoardCropY + cmdBoardCropH){
        m_cmdBoardCropY = cursorROI.y + cursorROI.h - cmdBoardCropH;
    }
}

// ===== merged from controlboard/cbright.cpp =====
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <functional>

#include "colorf.hpp"
#include "totype.hpp"
#include "pngtexdb.hpp"
#include "gldevice.hpp"
#include "processrun.hpp"

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

CBRight::CBRight(
        Widget::VarDir argDir,
        Widget::VarInt argX,
        Widget::VarInt argY,

        ProcessRun *argProc,
        Widget     *argParent,
        bool        argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),
          .w = 166,
          .h = 133,

          .attrs
          {
              .inst
              {
                  .show = std::function<bool(const Widget *)>([](const Widget *self)
                  {
                      return self->hasParent<ControlBoard>()->m_minimize == false;
                  }),
              },
          },

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , m_processRun(argProc)

    , m_bgFull
      {{
          .texLoadFunc = [](const Widget *)
          {
              return g_progUseDB->retrieve(0X00000012);
          },
      }}

    , m_bg
      {{
          .getter = &m_bgFull,
          .vr
          {
              [this]{ return m_bgFull.w() - w(); },
              0,

              [this]{ return w(); },
              [this]{ return h(); },
          },
          .parent{this},
      }}

    , m_buttonExchange
      {{
          .x = 4,
          .y = 6,

          .onOffX = 1,
          .onOffY = 1,

          .onRadius = 10,

          .modColor  = colorf::WHITE + colorf::A_SHF(80),
          .downTexID = 0X00000042U,

          .onTrigger = [this](Widget *, int)
          {
              if(auto cb = hasParent<ControlBoard>()){
                  cb->addLog(0, "exchange doesn't implemented yet");
              }
          },

          .triggerOnDone = true,
          .parent{this},
      }}

    , m_buttonMiniMap
      {{
          .x = 4,
          .y = 40,

          .onOffX = 1,
          .onOffY = 1,

          .onRadius = 10,

          .modColor  = colorf::WHITE + colorf::A_SHF(80),
          .downTexID = 0X00000043U,

          .onTrigger = [this](Widget *, int)
          {
              if(auto p = dynamic_cast<MiniMapBoard *>(m_processRun->getWidget("MiniMapBoard"))){
                  if(p->getMiniMapTexture()){
                      p->flipShow();
                  }
                  else{
                      if(auto cb = hasParent<ControlBoard>()){
                          cb->addLog(CBLOG_ERR, to_cstr(u8"没有可用的地图"));
                      }
                  }
              }
          },

          .triggerOnDone = true,
          .parent{this},
      }}

    , m_buttonMagicKey
      {{
          .x = 4,
          .y = 75,

          .onOffX = 1,
          .onOffY = 1,
          .onRadius = 10,

          .modColor  = colorf::WHITE + colorf::A_SHF(80),
          .downTexID = 0X00000044U,

          .onTrigger = [this](Widget *, int)
          {
              m_processRun->flipDrawMagicKey();
          },

          .triggerOnDone = true,
          .parent{this},
      }}

    , m_buttonInventory
      {{
          .x = 48,
          .y = 33,

          .texIDList
          {
              .off  = 0X00000030,
              .on   = 0X00000030,
              .down = 0X00000031,
          },

          .onTrigger = [this](Widget *, int)
          {
              if(auto p = m_processRun->getWidget("InventoryBoard")){
                  p->flipShow();
              }
          },

          .parent{this},
      }}

    , m_buttonHeroState
      {{
          .x = 77,
          .y = 31,

          .texIDList
          {
              .off  = 0X00000033,
              .on   = 0X00000033,
              .down = 0X00000032,
          },

          .onTrigger = [this](Widget *, int)
          {
              if(auto p = m_processRun->getWidget("PlayerStateBoard")){
                  p->flipShow();
              }
          },

          .parent{this},
      }}

    , m_buttonHeroMagic
      {{
          .x = 105,
          .y = 33,

          .texIDList
          {
              .off  = 0X00000035,
              .on   = 0X00000035,
              .down = 0X00000034,
          },

          .onTrigger = [this](Widget *, int)
          {
              if(auto p = m_processRun->getWidget("SkillBoard")){
                  p->flipShow();
              }
          },

          .parent{this},
      }}

    , m_buttonGuild
      {{
          .x = 40,
          .y = 11,

          .texIDList
          {
              .off  = 0X00000036,
              .on   = 0X00000036,
              .down = 0X00000037,
          },

          .onTrigger = [this](Widget *, int)
          {
              if(auto p = m_processRun->getWidget("GuildBoard")){
                  p->flipShow();
              }
          },

          .parent{this},
      }}

    , m_buttonTeam
      {{
          .x = 72,
          .y = 8,

          .texIDList
          {
              .off  = 0X00000038,
              .on   = 0X00000038,
              .down = 0X00000039,
          },

          .onTrigger = [this](Widget *, int)
          {
              auto boardPtr = dynamic_cast<TeamStateBoard *>(m_processRun->getWidget("TeamStateBoard"));
              auto  heroPtr = m_processRun->getMyHero();

              if(heroPtr->hasTeam()){
                  boardPtr->flipShow();
                  if(boardPtr->show()){
                      boardPtr->refresh();
                  }
              }
              else{
                  m_processRun->setCursor(ProcessRun::CURSOR_TEAMFLAG);
              }
          },

          .parent{this},
      }}

    , m_buttonQuest
      {{
          .x = 108,
          .y = 11,

          .texIDList
          {
              .off  = 0X0000003A,
              .on   = 0X0000003A,
              .down = 0X0000003B,
          },

          .onTrigger = [this](Widget *, int)
          {
              if(auto p = m_processRun->getWidget("QuestStateBoard")){
                  p->flipShow();
              }

              m_buttonQuest.stopBlink();
          },

          .parent{this},
      }}

    , m_buttonHorse
      {{
          .x = 40,
          .y = 61,

          .texIDList
          {
              .off  = 0X0000003C,
              .on   = 0X0000003C,
              .down = 0X0000003D,
          },

          .onTrigger = [this](Widget *, int)
          {
              if(auto p = m_processRun->getWidget("HorseBoard")){
                  p->flipShow();
              }
          },

          .parent{this},
      }}

    , m_buttonRuntimeConfig
      {{
          .x = 72,
          .y = 72,

          .texIDList
          {
              .off  = 0X0000003E,
              .on   = 0X0000003E,
              .down = 0X0000003F,
          },

          .onTrigger = [this](Widget *, int)
          {
              if(auto p = m_processRun->getWidget("RuntimeConfigBoard")){
                  p->flipShow();
              }
          },

          .parent{this},
      }}

    , m_buttonFriendChat
      {{
          .x = 108,
          .y = 61,

          .texIDList
          {
              .off  = 0X00000040,
              .on   = 0X00000040,
              .down = 0X00000041,
          },

          .onTrigger = [this](Widget *, int)
          {
              if(auto p = m_processRun->getWidget("FriendChatBoard")){
                  p->flipShow();
              }
          },

          .parent{this},
      }}

    , m_buttonAC
      {{
          .x = 1,
          .y = 105,

          .proc = argProc,
          .names
          {
              "AC",
              "MA",
          },

          .parent{this},
      }}

    , m_buttonDC
      {{
          .x = 84,
          .y = 105,

          .proc = argProc,
          .names
          {
              "DC",
              "MC",
          },

          .parent{this},
      }}
{}

// ===== merged from controlboard/cbtitle.cpp =====
#include "gui_core.hpp"
#include "pngtexdb.hpp"

extern PNGTexDB *g_progUseDB;

CBTitle::CBTitle(
        Widget::VarDir argDir,
        Widget::VarInt argX,
        Widget::VarInt argY,

        ProcessRun *argProc,

        Widget *argParent,
        bool    argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , m_processRun(argProc)
    , m_bg
      {{
          .texLoadFunc = [](const Widget *){ return g_progUseDB->retrieve(0X00000022); },
          .parent{this},
      }}

    , m_arcAni
      {
          DIR_UPLEFT,
          46,
          8,

          0X04000000,
          4,
          1,

          true,
          true,

          this,
          false,
      }

    , m_level
      {
          DIR_NONE,
          63,
          25,

          argProc,
          [this](Widget *self, int) // double-click
          {
              auto &hide = self->hasParent<ControlBoard>()->m_minimize; hide = !hide;
          },

          this,
          false,
      }
{
    setSize([this]{ return m_bg.w(); },
            [this]{ return m_bg.h(); });
}

// ===== merged from controlboard/controlboard.cpp =====
#include <algorithm>
#include <cstring>
#include "log.hpp"
#include "client.hpp"
#include "processrun.hpp"

extern Log *g_mir2xLog;
extern Client *g_client;
extern GLDevice *g_glDevice;

ControlBoard::ControlBoard(ProcessRun *argProc, Widget *argParent, bool argAutoDelete)
    : Widget
      {{
          .dir = DIR_DOWNLEFT,

          .y = [this]{ return g_glDevice->getRendererHeight() - 1; },
          .w = [this]{ return g_glDevice->getRendererWidth ()    ; },
          .h = [this]
          {
              if(m_minimize){
                  return CBTitle::UP_HEIGHT + 10;
              }

              else if(m_expand){
                  return m_middleExpand.h() + CBTitle::UP_HEIGHT;
              }

              else{
                  return m_middle.h() + CBTitle::UP_HEIGHT;
              }
          },

          .attrs
          {
              .inst
              {
                  .moveOnFocus = false,
                  .update = [this](Widget *, double ms)
                  {
                      m_logBoard.update(ms);
                      m_cmdBoard.update(ms);
                      Widget::updateDefault(ms);
                  },
              },
          },

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , m_processRun(argProc)
    , m_logBoard
      {{
          .lineAlign = LALIGN_JUSTIFY,
      }}

    , m_cmdBoard
      {{
          .canEdit = true,
          .enableIME = [this]
          {
              return m_processRun->getRuntimeConfig<RTCFG_IME>();
          },

          .lineAlign = LALIGN_JUSTIFY,

          .onCR = [this](bool shiftHold)
          {
              if(m_expand && shiftHold){
                  return false;
              }

              onInputDone();
              return true;
          },

          .onCursorMove = [this]
          {
              if(m_middle.show()){
                  m_middle.onCmdCursorMove();
              }
              else if(m_middleExpand.show()){
                    m_middleExpand.onCmdCursorMove();
              }
          },
      }}

    , m_left
      {
          DIR_DOWNLEFT,
          0,
          [this]{ return h() - 1; },

          argProc,
          this,
          false,
      }

    , m_right
      {
          DIR_DOWNRIGHT,
          [this]{ return w() - 1; },
          [this]{ return h() - 1; },

          argProc,
          this,
          false,
      }

    , m_middle
      {
          DIR_DOWNLEFT,
          [this]
          {
              return m_left.w();
          },

          [this]
          {
              return h() - 1;
          },

          [this]
          {
              return w() - m_left.w() - m_right.w();
          },

          argProc,
          this,
          false,
      }

    , m_middleExpand
      {
          DIR_DOWNLEFT,
          [this]
          {
              return m_left.w();
          },

          [this]
          {
              return h() - 1;
          },

          [this]
          {
              return w() - m_left.w() - m_right.w();
          },

          argProc,
          this,
          false,
      }

    , m_title
      {
          DIR_UP,
          [this]
          {
              const auto  fullW =         w();
              const auto  leftW = m_left .w();
              const auto rightW = m_right.w();

              return leftW + (fullW - leftW - rightW) / 2;
          },

          0,
          argProc,

          this,
          false,
      }
{
    m_logBoard.setLineWidth(m_middle.getLogWindowWidth());
}

void ControlBoard::addXMLLog(const char *log)
{
    fflassert(str_haschar(log));
    m_logBoard.addLayoutXML(m_logBoard.parCount(), {0, 0, 0, 0}, log);

    m_middle      .m_slider.setValue(1.0f, false);
    m_middleExpand.m_slider.setValue(1.0f, false);
}

void ControlBoard::addParLog(const char *log)
{
    fflassert(str_haschar(log));
    m_logBoard.addParXML(m_logBoard.parCount(), {0, 0, 0, 0}, log);

    m_middle      .m_slider.setValue(1.0f, false);
    m_middleExpand.m_slider.setValue(1.0f, false);
}

void ControlBoard::addLog(int logType, const char *log)
{
    if(!log){
        throw fflpanic("null log string");
    }

    switch(logType){
        case CBLOG_ERR:
            {
                g_mir2xLog->addLog(LOGTYPE_WARNING, "%s", log);
                break;
            }
        default:
            {
                g_mir2xLog->addLog(LOGTYPE_INFO, "%s", log);
                break;
            }
    }

    tinyxml2::XMLDocument xmlDoc(true, tinyxml2::PEDANTIC_WHITESPACE);
    const char *xmlString = [logType]() -> const char *
    {
        // use hex to give alpha
        // color::String2Color has no alpha component

        switch(logType){
            case CBLOG_SYS: return "<par bgcolor = \"rgb(0x00, 0x80, 0x00)\"></par>";
            case CBLOG_DBG: return "<par bgcolor = \"rgb(0x00, 0x00, 0xff)\"></par>";
            case CBLOG_ERR: return "<par bgcolor = \"rgb(0xff, 0x00, 0x00)\"></par>";
            case CBLOG_DEF:
            default       : return "<par></par>";
        }
    }();

    if(xmlDoc.Parse(xmlString) != tinyxml2::XML_SUCCESS){
        throw fflpanic("parse xml template failed: {}", xmlString);
    }

    // to support <, >, / in xml string
    // don't directly pass the raw string to addParXML
    xmlDoc.RootElement()->SetText(log);

    tinyxml2::XMLPrinter printer;
    xmlDoc.Print(&printer);
    m_logBoard.addParXML(m_logBoard.parCount(), {0, 0, 0, 0}, printer.CStr());

    m_middle      .m_slider.setValue(1.0f, false);
    m_middleExpand.m_slider.setValue(1.0f, false);
}

TritexButton *ControlBoard::getButton(const std::string_view &buttonName)
{
    if     (buttonName == "Inventory"    ){ return &m_right.m_buttonInventory    ; }
    else if(buttonName == "HeroState"    ){ return &m_right.m_buttonHeroState    ; }
    else if(buttonName == "HeroMagic"    ){ return &m_right.m_buttonHeroMagic    ; }
    else if(buttonName == "Guild"        ){ return &m_right.m_buttonGuild        ; }
    else if(buttonName == "Team"         ){ return &m_right.m_buttonTeam         ; }
    else if(buttonName == "Quest"        ){ return &m_right.m_buttonQuest        ; }
    else if(buttonName == "Horse"        ){ return &m_right.m_buttonHorse        ; }
    else if(buttonName == "RuntimeConfig"){ return &m_right.m_buttonRuntimeConfig; }
    else if(buttonName == "FriendChat"   ){ return &m_right.m_buttonFriendChat   ; }
    else                                  { return nullptr                       ; }
}

void ControlBoard::onClickSwitchModeButton(int)
{
    if(m_expand){
        m_expand = false;
        m_middle.afterResize();
    }
    else{
        m_expand = true;
        m_middleExpand.afterResize();
    }
}

void ControlBoard::onInputDone()
{
    if(!m_cmdBoard.hasToken()){
        return;
    }

    const std::string fullXML = m_cmdBoard.getXML();
    const std::string fullStr = str_trim(m_cmdBoard.getText(), true, false);

    m_cmdBoard.clear();
    m_cmdBoard.setFocus(false);

    if(m_middle.show()){
        m_middle.onCmdCR();
    }
    else if(m_middleExpand.show()){
        m_middleExpand.onCmdCR();
    }

    if(fullStr.empty()){
        return;
    }

    switch(fullStr[0]){
        case '!': // broadcast
            {
                const std::string content = str_trim(fullStr.substr(1), true, false);
                if(!content.empty()){
                    CMPlayerBroadcast cmPB;
                    std::memset(&cmPB, 0, sizeof(cmPB));
                    std::memcpy(cmPB.content, content.data(), std::min<size_t>(content.size(), sizeof(cmPB.content) - 1));
                    g_client->send({CM_PLAYERBROADCAST, cmPB});
                }
                break;
            }
        case '@': // user command
            {
                if(m_processRun){
                    m_processRun->userCommand(fullStr.c_str() + 1);
                }
                break;
            }
        case '$': // lua command for super user
            {
                if(m_processRun){
                    m_processRun->luaCommand(fullStr.c_str() + 1);
                }
                break;
            }
        default: // normal talk
            {
                addXMLLog(fullXML.c_str());
                CMPlayerSay cmPS;
                std::memset(&cmPS, 0, sizeof(cmPS));
                std::memcpy(cmPS.content, fullStr.data(), std::min<size_t>(fullStr.size(), sizeof(cmPS.content) - 1));
                g_client->send({CM_PLAYERSAY, cmPS});
                break;
            }
    }
}

int ControlBoard::shiftHeight() const
{
    return m_minimize ? 0 : CBMiddle::CB_MIDDLE_TEX_HEIGHT;
}
