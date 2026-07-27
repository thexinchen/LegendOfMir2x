#include "quickaccessboard.hpp"

// ===== merged from quickaccessboard/quickaccessgrid.cpp =====
#include "gldevice.hpp"
#include "pngtexdb.hpp"
#include "processrun.hpp"

extern PNGTexDB *g_itemDB;
extern GLDevice *g_glDevice;

QuickAccessGrid::QuickAccessGrid(
        Widget::VarDir argDir,
        Widget::VarInt argX,
        Widget::VarInt argY,

        Widget::VarSizeOpt argW,
        Widget::VarSizeOpt argH,

        int argSlot,
        ProcessRun *argProc,

        Widget *argParent,
        bool    argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),
          .w = std::move(argW),
          .h = std::move(argH),

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , slot(argSlot)
    , proc(argProc)

    , bg
      {{
          .w = [this]{ return w(); },
          .h = [this]{ return h(); },

          .drawFunc = [this](int drawDstX, int drawDstY)
          {
              if(Widget::ROIMap{.x{drawDstX}, .y{drawDstY}, .ro{roi()}}.in(GLDeviceHelper::getMousePLoc())){
                   g_glDevice->fillRectangle(colorf::WHITE + colorf::A_SHF(64), drawDstX, drawDstY, w(), h());
               }
          },

          .parent{this},
      }}

    , item
      {{
          .dir = DIR_NONE,

          .x = [this]{ return w() / 2; },
          .y = [this]{ return h() / 2; },

          .texLoadFunc = [this] -> GLTexID
          {
              if(const auto &item = proc->getMyHero()->getBelt(slot)){
                  return g_itemDB->retrieve(DBCOM_ITEMRECORD(item.itemID).pkgGfxID | 0X01000000);
              }
              return nullptr;
          },

          .parent{this},
      }}

    , count
      {{
          .dir = DIR_UPRIGHT,
          .x = [this]{ return w() - 1; },
          .y = 0,

          .textFunc = [this] -> std::string
          {
              if(const auto &item = proc->getMyHero()->getBelt(slot); item && (item.count > 1)){
                  return std::to_string(item.count);
              }
              return {};
          },

          .font
          {
              .id = 1,
              .size = 10,
          },

          .parent{this},
      }}
{}

// ===== merged from quickaccessboard/quickaccessboard.cpp =====
#include <tuple>
#include "totype.hpp"
#include "invpack.hpp"
#include "pngtexdb.hpp"
#include "sysconst.hpp"
#include "gldevice.hpp"
#include "processrun.hpp"
#include "widget.hpp"

extern PNGTexDB *g_itemDB;
extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

QuickAccessBoard::QuickAccessBoard(
        dir8_t argDir,

        int argX,
        int argY,

        ProcessRun *argProc,

        Widget *argParent,
        bool    argAutoDelete)

    : Widget
      {{
          .dir = argDir,

          .x = argX,
          .y = argY,

          .w = std::nullopt,
          .h = std::nullopt,

          .attrs
          {
              .inst
              {
                  .show = false, // hide by default
              },
          },

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , m_processRun(argProc)
    , m_bg
      {{
          .texLoadFunc = [this]{ return g_progUseDB->retrieve(m_texID); },
          .parent{this},
      }}

    , m_buttonClose
      {{
          .x = 263,
          .y = 32,

          .texIDList
          {
              .off  = 0X00000061,
              .on   = 0X00000061,
              .down = 0X00000062,
          },

          .onTrigger = [this](Widget *, int)
          {
              setShow(false);
          },

          .parent{this},
      }}
{
    for(int slot = 0; slot < 6; ++slot){
        addChild(new QuickAccessGrid
        {
            DIR_UPLEFT,

            getGridLoc(slot).x,
            getGridLoc(slot).y,
            getGridLoc(slot).w,
            getGridLoc(slot).h,

            slot,
            m_processRun,
        },

        true);
    }
}

bool QuickAccessBoard::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    if(!valid){
        return consumeFocus(false);
    }

    if(m_buttonClose.processEventParent(event, valid, m)){
        return true;
    }

    switch(event.type){
        case MIR_EVENT_MOUSE_MOTION:
            {
                if((event.motion.state & MIR_BUTTON_LMASK) && (m.in(to_d(event.motion.x), to_d(event.motion.y)) || focus())){
                    moveBy(to_d(event.motion.xrel), to_d(event.motion.yrel), Widget::makeROI(0, 0, g_glDevice->getRendererSize()));
                    return consumeFocus(true);
                }
                return consumeFocus(false);
            }
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
            {
                switch(event.button.button){
                    case MIR_BUTTON_LEFT:
                        {
                            for(int slot = 0; slot < 6; ++slot){
                                const auto [gridX, gridY, gridW, gridH] = getGridLoc(slot);
                                if(mathf::pointInRectangle(to_d(event.button.x), to_d(event.button.y), m.x + gridX, m.y + gridY, gridW, gridH)){
                                    if(const auto grabbedItem = m_processRun->getMyHero()->getInvPack().getGrabbedItem()){
                                        const auto &ir = DBCOM_ITEMRECORD(grabbedItem.itemID);
                                        if(ir.beltable()){
                                            m_processRun->requestEquipBelt(grabbedItem.itemID, grabbedItem.seqID, slot);
                                        }
                                        else{
                                            m_processRun->getMyHero()->getInvPack().add(grabbedItem);
                                            m_processRun->getMyHero()->getInvPack().setGrabbedItem({});
                                        }
                                    }
                                    else if(m_processRun->getMyHero()->getBelt(slot)){
                                        m_processRun->requestGrabBelt(slot);
                                    }
                                    break;
                                }
                            }

                            if(m.in(to_d(event.button.x), to_d(event.button.y))){
                                return consumeFocus(true);
                            }
                            else{
                                return consumeFocus(false);
                            }
                        }
                    case MIR_BUTTON_RIGHT:
                        {
                            for(int slot = 0; slot < 6; ++slot){
                                const auto [gridX, gridY, gridW, gridH] = getGridLoc(slot);
                                if(mathf::pointInRectangle(to_d(event.button.x), to_d(event.button.y), m.x + gridX, m.y + gridY, gridW, gridH)){
                                    gridConsume(slot);
                                    break;
                                }
                            }

                            if(m.in(to_d(event.button.x), to_d(event.button.y))){
                                return consumeFocus(true);
                            }
                            else{
                                return consumeFocus(false);
                            }
                        }
                    default:
                        {
                            return consumeFocus(false);
                        }
                }
            }
        case MIR_EVENT_KEY_DOWN:
            {
                if(focus()){
                    if(const auto ch = GLDeviceHelper::getKeyChar(event, false); ch >= '1' && ch <= '6'){
                        gridConsume(ch - '1');
                    }
                    return consumeFocus(true);
                }
                return consumeFocus(false);
            }
        default:
            {
                return false;
            }
    }
}

void QuickAccessBoard::gridConsume(int slot)
{
    fflassert(slot >= 0, slot);
    fflassert(slot <  6, slot);

    if(const auto &item = m_processRun->getMyHero()->getBelt(slot)){
        InvPack::playItemSoundEffect(item.itemID, true);
        m_processRun->requestConsumeItem(item.itemID, item.seqID, 1);
    }
}
