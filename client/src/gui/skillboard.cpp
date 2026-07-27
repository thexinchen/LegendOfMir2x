#include "skillboard.hpp"

// ===== merged from skillboard/skillboardconfig.cpp =====
#include "fflerror.hpp"
#include "dbcomid.hpp"

std::optional<char> SkillBoardConfig::getMagicKey(uint32_t magicID) const
{
    if(auto p = m_learnedMagicList.find(magicID); p != m_learnedMagicList.end()){
        return p->second.key;
    }
    return {};
}

std::optional<int> SkillBoardConfig::getMagicLevel(uint32_t magicID) const
{
    if(auto p = m_learnedMagicList.find(magicID); p != m_learnedMagicList.end()){
        return p->second.level;
    }
    return {};
}

void SkillBoardConfig::setMagicLevel(uint32_t magicID, int level)
{
    fflassert(DBCOM_MAGICRECORD(magicID));
    fflassert(SkillBoard::getMagicIconGfx(magicID));

    fflassert(level >= 1);
    fflassert(level <= 3);

    if(auto p = m_learnedMagicList.find(magicID); p != m_learnedMagicList.end()){
        fflassert(level >= p->second.level);
        p->second.level = level;
    }
    else{
        m_learnedMagicList[magicID].level = level;
    }
}

void SkillBoardConfig::setMagicKey(uint32_t magicID, std::optional<char> key)
{
    fflassert(DBCOM_MAGICRECORD(magicID));
    fflassert(SkillBoard::getMagicIconGfx(magicID));

    fflassert(hasMagicID(magicID));
    fflassert(!SkillBoard::getMagicIconGfx(magicID)->passive);
    fflassert(!key.has_value() || (key.value() >= 'a' && key.value() <= 'z') || (key.value() >= '0' && key.value() <= '9'));

    m_learnedMagicList[magicID].key = key;
    if(key.has_value()){
        for(auto &p: m_learnedMagicList){
            if((p.first != magicID) && p.second.key == key){
                p.second.key.reset();
            }
        }
    }
}

// ===== merged from skillboard/skillboard.cpp =====
#include "dbcomid.hpp"
#include "gui_texture.hpp"
#include "gldevice.hpp"
#include "processrun.hpp"
#include "gui_widgets.hpp"

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

SkillBoard::SkillBoard(int argX, int argY, ProcessRun *runPtr, Widget *argParent, bool argAutoDelete)
    : Widget
      {{
          .x = argX,
          .y = argY,

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , m_processRun(fflcheck(runPtr))
    , m_bg
      {{
          .texLoadFunc = []{ return g_progUseDB->retrieve(0X05000000); },
          .parent{this},
      }}

    , m_pageCanvas
      {{
          .x = SkillBoard::getPageRectange().x,
          .y = SkillBoard::getPageRectange().y,
          .w = SkillBoard::getPageRectange().w,
          .h = SkillBoard::getPageRectange().h,

          .parent{this},
      }}

    , m_skillPageList([this]() -> std::vector<SkillPage *>
      {
          std::vector<SkillPage *> pageList;
          pageList.reserve(8);

          for(int i = 0; i < 8; ++i){
              auto pagePtr = new SkillPage
              {{
                  .pageTexID = to_u32(0X05000010 + i),

                  .config = std::addressof(m_config),
                  .proc = m_processRun,

                  .parent
                  {
                      .widget = std::addressof(m_pageCanvas),
                      .autoDelete = true,
                  },
              }};

              for(const auto &iconGfx: m_iconGfxList){
                  if(i == getSkillPageIndex(iconGfx.magicID)){
                      pagePtr->addIcon(iconGfx.magicID);
                  }
              }

              pagePtr->setShow([i, this]
              {
                  return m_selectedTabIndex == i;
              });

              pageList.push_back(pagePtr);
          }
          return pageList;
      }())

    , m_tabButtonList([this]() -> std::vector<TritexButton *>
      {
          std::vector<TritexButton *> tabButtonList;
          tabButtonList.reserve(8);

          const int tabX = 45;
          const int tabY = 10;
          const int tabW = 34;

          // don't know WTF
          // can't compile if use std::vector<TritexButton>
          // looks for complicated parameter list gcc can't forward correctly ???

          for(int i = 0; i < 8; ++i){
              tabButtonList.push_back(new TritexButton
              {{
                  .x = tabX + tabW * i,
                  .y = tabY,

                  .texIDList
                  {
                      .on   = 0X05000020 + to_u32(i),
                      .down = 0X05000030 + to_u32(i),
                  },

                  .onOverIn = [i, this]
                  {
                      m_cursorOnTabIndex = i;
                  },

                  .onOverOut = [i, this]
                  {
                      if(i != m_cursorOnTabIndex){
                          return;
                      }
                      m_cursorOnTabIndex = -1;
                  },

                  .onTrigger = [i, this](int)
                  {
                      if(m_selectedTabIndex == i){
                          return;
                      }

                      m_cursorOnTabIndex = -1, // radio mode button won't call onOverOut when pressed
                      m_tabButtonList.at(m_selectedTabIndex)->setOff();

                      m_selectedTabIndex = i;
                      m_slider.setValue(0, false);

                      for(auto pagePtr: m_skillPageList){
                          pagePtr->moveTo(0, 0);
                      }
                  },

                  .onClickDone = false,
                  .radioMode = true,
                  .modColor = colorf::RGBA(255, 200, 255, 255),

                  .parent{this},
              }});

              if(i == m_selectedTabIndex){
                  m_tabButtonList.at(i)->setDown();
              }
          }
          return tabButtonList;
      }())

    , m_slider
      {{
          .bar
          {
              .x = 326,
              .y = 74,
              .w = 6,
              .h = 266,
          },

          .index = 0,
          .onChange = [this](float value)
          {
              auto canvasH = SkillBoard::getPageRectange().h;
              auto pagePtr = m_skillPageList.at(m_selectedTabIndex);

              if(pagePtr->h() > canvasH){
                  pagePtr->moveTo(0, to_d((canvasH - pagePtr->h()) * value)); // coordinate in canvas
              }
          },

          .parent{this},
      }}

    , m_closeButton
      {{
          .x = 317,
          .y = 402,

          .texIDList
          {
              .on   = 0X0000001C,
              .down = 0X0000001D,
          },

          .onTrigger = [this](int)
          {
              setShow(false);
          },

          .parent{this},
      }}
{
    setShow(false);
    setSize([this]{ return m_bg.w(); },
            [this]{ return m_bg.h(); });
}

void SkillBoard::drawDefault(Widget::ROIMap m) const
{
    if(!m.calibrate(this)){
        return;
    }

    drawChild(&m_bg, m);

    drawTabName(m);

    drawChild(&m_slider, m);
    drawChild(&m_closeButton, m);

    for(auto buttonPtr: m_tabButtonList){
        drawChild(buttonPtr, m);
    }

    drawChild(&m_pageCanvas, m);
}

bool SkillBoard::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    if(!valid){
        return consumeFocus(false);
    }

    if(m_closeButton.processEventParent(event, valid, m)){
        consumeFocus(false);
        return true;
    }

    bool tabConsumed = false;
    for(auto buttonPtr: m_tabButtonList){
        tabConsumed |= buttonPtr->processEventParent(event, valid && !tabConsumed, m);
    }

    if(tabConsumed){
        return consumeFocus(true);
    }

    if(m_slider.processEventParent(event, valid, m)){
        return true;
    }

    if(m_pageCanvas.processEventParent(event, valid, m)){
        return consumeFocus(true);
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
                            return consumeFocus(m.in(to_d(event.button.x), to_d(event.button.y)));
                        }
                    default:
                        {
                            return consumeFocus(false);
                        }
                }
            }
        case MIR_EVENT_MOUSE_WHEEL:
            {
                if(m.create(m_pageCanvas.roi(this)).in(to_d(event.wheel.mouse_x), to_d(event.wheel.mouse_y))){
                    m_slider.addValue(to_d(event.wheel.y) * -0.1f, true);
                }
                return consumeFocus(true);
            }
        default:
            {
                return consumeFocus(false);
            }
    }
}

void SkillBoard::drawTabName(Widget::ROIMap m) const
{
    const LabelBoard tabName
    {{
        .label = to_u8cstr([this]() -> std::u8string
        {
            if(m_cursorOnTabIndex >= 0){
                return str_printf(u8"元素【%s】", to_cstr(magicElemName(cursorOnElem())));
            }

            if(m_selectedTabIndex >= 0){
                for(const auto magicIconPtr: m_skillPageList.at(m_selectedTabIndex)->getMagicIconButtonList()){
                    if(magicIconPtr->cursorOn()){
                        if(const auto &mr = DBCOM_MAGICRECORD(magicIconPtr->magicID())){
                            return str_printf(u8"元素【%s】%s", str_haschar(mr.elem) ? to_cstr(mr.elem) : "无", to_cstr(mr.name));
                        }
                        else{
                            return str_printf(u8"元素【无】");
                        }
                    }
                }
                return str_printf(u8"元素【%s】", to_cstr(magicElemName(selectedElem())));
            }

            // fallback
            // shouldn't reach here
            return str_printf(u8"元素【无】");
        }()),

        .font
        {
            .id = 1,
            .size = 12,
        },
    }};

    drawAsChild(&tabName, DIR_UPLEFT, 30, 400, m);
}

// ===== merged from skillboard/magiciconbutton.cpp =====
#include "processrun.hpp"

MagicIconButton::MagicIconButton(MagicIconButton::InitArgs args)
    : Widget
      {{
          .parent = std::move(args.parent),
      }}

    , m_magicID(fflcheck(args.magicID, true
                && DBCOM_MAGICRECORD(args.magicID)
                && SkillBoard::getMagicIconGfx(args.magicID)))

    , m_config    (fflcheck(args.config))
    , m_processRun(fflcheck(args.proc  ))

    , m_icon
      {{
          .texIDList
          {
              .off  = SkillBoard::getMagicIconGfx(m_magicID)->magicIcon,
              .on   = SkillBoard::getMagicIconGfx(m_magicID)->magicIcon,
              .down = SkillBoard::getMagicIconGfx(m_magicID)->magicIcon,
          },

          .onClickDone = false,
          .parent{this},
      }}
{
    moveTo(SkillBoard::getMagicIconGfx(m_magicID)->x * 60 + 12,
           SkillBoard::getMagicIconGfx(m_magicID)->y * 65 + 13);

    // leave some pixels to draw level label
    // since level can change during run, can't get the exact size here

    setSize(m_icon.w() + 8,
            m_icon.h() + 8);
}

void MagicIconButton::drawDefault(Widget::ROIMap m) const
{
    if(!m.calibrate(this)){
        return;
    }

    if(const auto levelOpt = m_config->getMagicLevel(magicID()); levelOpt.has_value()){
        Widget::drawDefault(m);

        const TextBoard magicLevel
        {{
            .textFunc = std::to_string(levelOpt.value()),
            .font
            {
                .id = 3,
                .size = 12,
                .color = colorf::YELLOW_A255,
            },
        }};
        drawAsChild(&magicLevel, DIR_UPLEFT, m_icon.w() - 2, m_icon.h() - 1, m);

        if(const auto keyOpt = m_config->getMagicKey(magicID()); keyOpt.has_value()){
            const TextShadowBoard magicKey
            {{
                .shadowX = 2,
                .shadowY = 2,

                .textFunc = [keyOpt]{ return str_printf("%c", std::toupper(keyOpt.value())); },
                .font
                {
                    .id = 3,
                    .size = 20,
                    .color = colorf::RGBA(0XFF, 0X80, 0X00, 0XE0),
                },
                .shadowColor = colorf::RGBA(0X00, 0X00, 0X00, 0XE0),
            }};
            drawAsChild(&magicKey, DIR_UPLEFT, 2, 2, m);
        }
    }
}

bool MagicIconButton::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    const auto result = m_icon.processEventParent(event, valid, m);
    if(event.type == MIR_EVENT_KEY_DOWN && cursorOn()){
        if(const auto key = GLDeviceHelper::getKeyChar(event, false); (key >= '0' && key <= '9') || (key >= 'a' && key <= 'z')){
            if(m_config->hasMagicID(magicID())){
                if(SkillBoard::getMagicIconGfx(magicID())->passive){
                    m_processRun->addCBLog(CBLOG_SYS, u8"无法为被动技能设置快捷键：%s", to_cstr(DBCOM_MAGICRECORD(magicID()).name));
                }
                else{
                    m_config->setMagicKey(magicID(), key);
                    m_processRun->requestSetMagicKey(magicID(), key);
                }
            }
            return consumeFocus(true);
        }
    }
    return result;
}

// ===== merged from skillboard/skillpage.cpp =====
#include "gldevice.hpp"
#include "gui_texture.hpp"

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

SkillPage::SkillPage(SkillPage::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),
          .w = SkillBoard::getPageRectange().w,

          .parent = std::move(args.parent),
      }}

    , m_config    (fflcheck(args.config))
    , m_processRun(fflcheck(args.proc  ))

    , m_bg
      {{
          .texLoadFunc = [texID = args.pageTexID]{ return g_progUseDB->retrieve(texID); },
          .parent{this},
      }}
{
    setH([this]
    {
        const auto low  = std::min<int>(m_bg.h(), SkillBoard::getPageRectange().h);
        const auto high = std::max<int>(m_bg.h(), SkillBoard::getPageRectange().h);

        int maxButtonReachY = 0;
        for(const auto button: m_magicIconButtonList){
            maxButtonReachY = std::max<int>(maxButtonReachY, button->dy() + button->h());
        }

        return std::clamp<int>(maxButtonReachY + 10, low, high); // give 10 pixels of bottom margin
    });
}

void SkillPage::addIcon(uint32_t argMagicID)
{
    for(auto button: m_magicIconButtonList){
        if(button->magicID() == argMagicID){
            return;
        }
    }

    fflassert(DBCOM_MAGICRECORD(argMagicID));
    m_magicIconButtonList.push_back(new MagicIconButton
    {{
        .magicID = argMagicID,

        .config = m_config,
        .proc   = m_processRun,

        .parent
        {
            .widget = this,
            .autoDelete = true,
        },
    }});
}
