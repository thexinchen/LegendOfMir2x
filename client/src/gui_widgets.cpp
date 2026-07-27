#include "gui_widgets.hpp"

// ===== button.cpp =====
#include "protocoldef.hpp"

void Button::evalOverCBFunc(const Button::OverCBFunc &func, Widget *widget)
{
    std::visit(VarDispatcher
    {
        [      ](const std::function<void(        )> &f){ if(f){f(      );} },
        [widget](const std::function<void(Widget *)> &f){ if(f){f(widget);} },

        [](auto &){},

    }, func);
}

void Button::evalClickCBFunc(const Button::ClickCBFunc &func, Widget *widget, bool clickDone, int clickCount)
{
    std::visit(VarDispatcher
    {
        [        clickDone, clickCount](const std::function<void(          bool, int)> &f){ if(f){f(        clickDone, clickCount);} },
        [widget, clickDone, clickCount](const std::function<void(Widget *, bool, int)> &f){ if(f){f(widget, clickDone, clickCount);} },

        [](auto &){},

    }, func);
}

void Button::evalTriggerCBFunc(const Button::TriggerCBFunc &func, Widget *widget, int clickCount)
{
    std::visit(VarDispatcher
    {
        [        clickCount](const std::function<void(          int)> &f){ if(f){f(        clickCount);} },
        [widget, clickCount](const std::function<void(Widget *, int)> &f){ if(f){f(widget, clickCount);} },

        [](auto &){},

    }, func);
}

// ===== gfxshapeboard.cpp =====
#include "gldevice.hpp"

GfxShapeBoard::GfxShapeBoard(GfxShapeBoard::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),
          .w = std::move(args.w),
          .h = std::move(args.h),

          .attrs
          {
              .inst = std::move(args.attrs),
          },
          .parent = std::move(args.parent),
      }}

    , m_drawFunc(std::move(args.drawFunc))
{}

void GfxShapeBoard::drawDefault(Widget::ROIMap m) const
{
    if(!m.calibrate(this)){
        return;
    }

    if(Widget::hasDrawFunc(m_drawFunc)){
        const GLDeviceHelper::EnableRenderCropRectangle enableClip(m.x, m.y, m.ro->w, m.ro->h);
        Widget::execDrawFunc(m_drawFunc, this, m.x - m.ro->x, m.y - m.ro->y);
    }
}

// ===== imageboard.cpp =====
#include "colorf.hpp"
#include "totype.hpp"

extern GLDevice *g_glDevice;

ImageBoard::ImageBoard(ImageBoard::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .parent = args.parent,
      }}

    , m_varColor(std::move(args.modColor))
    , m_varBlendMode(std::move(args.blendMode))

    , m_loadFunc(std::move(args.texLoadFunc))
    , m_xformPair(getHFlipRotatePair(args.hflip, args.vflip, args.rotate))
{
    const auto varTexW = args.w.value_or([this]{ return GLDeviceHelper::getTextureWidth (getTexture(), 0); });
    const auto varTexH = args.h.value_or([this]{ return GLDeviceHelper::getTextureHeight(getTexture(), 0); });

    setSize((m_rotate % 2 == 0) ? varTexW : varTexH,
            (m_rotate % 2 == 0) ? varTexH : varTexW);
}

void ImageBoard::drawDefault(Widget::ROIMap m) const
{
    if(!m.calibrate(this)){
        return;
    }

    if(!colorf::A(Widget::evalU32(m_varColor, this))){
        return;
    }

    const auto [
        drawDstX, drawDstY,
        drawSrcX, drawSrcY,
        drawSrcW, drawSrcH,

        centerOffX,
        centerOffY,

        rotateDegree] = [
            dstX = m.x,
            dstY = m.y,

            srcX = m.ro->x,
            srcY = m.ro->y,
            srcW = m.ro->w,
            srcH = m.ro->h,

            this]() -> std::array<int, 9>
    {
        // draw rotate and flip
        // all corners are indexed as 0, 1, 2, 3

        // 0      1
        //  +----+
        //  |    |   empty square means dst region before rotation
        //  +----+
        // 3      2

        // 0      1
        //  +----+
        //  |....|   solid square means dst region after rotation, the real dst region
        //  +----+
        // 3      2

        switch(m_rotate % 4){
            case 0:
                {
                    //  0           1
                    //  +-----------+
                    //  |...........|
                    //  |...........|
                    //  +-----------+
                    //  3           2

                    return
                    {
                        dstX,
                        dstY,

                        srcX,
                        srcY,

                        srcW,
                        srcH,

                        0,
                        0,

                        0,
                    };
                }
            case 1:
                {
                    // 0           1
                    // +-----------+
                    // |           |
                    // |           |
                    // o-----+-----+
                    // |.....|     2
                    // |.....|
                    // |.....|
                    // |.....|
                    // +-----+
                    // 2     1

                    return
                    {
                        dstX,
                        dstY - srcW,

                        srcY,
                        w() - srcX - srcW,

                        srcH,
                        srcW,

                        0,
                        srcW,

                        90,
                    };
                }
            case 2:
                {
                    // 0           1
                    // +-----------+
                    // |           |
                    // |           |           3
                    // +-----------o-----------+
                    // 3           |...........|
                    //             |...........|
                    //             +-----------+
                    //             1           0

                    return
                    {
                        dstX - srcW,
                        dstY - srcH,

                        w() - srcW - srcX,
                        h() - srcH - srcY,

                        srcW,
                        srcH,

                        srcW,
                        srcH,

                        180,
                    };
                }
            default:
                {
                    // 1     2
                    // +-----+
                    // |.....|
                    // |.....|
                    // |.....|           1
                    // |.....+-----------+
                    // |.....|           |
                    // |.....|           |
                    // +-----o-----------+
                    // 0     3           2

                    return
                    {
                        dstX        + srcW,
                        dstY + srcH - srcW,

                        h() - srcH - srcY,
                        srcX,

                        srcH,
                        srcW,

                        0,
                        srcW,

                        270,
                    };
                }
        }
    }();

    const auto texPtr = getTexture();
    if(!texPtr){
        return;
    }

    const auto [texW, texH] = GLDeviceHelper::getTextureSize(texPtr);

    const auto  widthRatio = to_df(texW) / ((m_rotate % 2 == 0) ? w() : h());
    const auto heightRatio = to_df(texH) / ((m_rotate % 2 == 0) ? h() : w());

    // imgSrcX
    // size and position cropped from original image, no resize, well defined

    /**/  int imgSrcX = to_dround( widthRatio * drawSrcX);
    const int imgSrcY = to_dround(heightRatio * drawSrcY);
    const int imgSrcW = to_dround( widthRatio * drawSrcW);
    const int imgSrcH = to_dround(heightRatio * drawSrcH);

    if(m_hflip){
        imgSrcX = texW - imgSrcX - imgSrcW;
    }

    const GLDeviceHelper::EnableTextureModColor enableColor(texPtr, Widget::evalU32(m_varColor, this));
    const GLDeviceHelper::EnableTextureBlendMode enableBlendMode(texPtr, Widget::evalBlendMode(m_varBlendMode, this));

    g_glDevice->drawTextureEx(
            texPtr,

            imgSrcX, imgSrcY,
            imgSrcW, imgSrcH,

            drawDstX, drawDstY,
            drawSrcW, drawSrcH,

            centerOffX,
            centerOffY,

            rotateDegree,
            m_hflip ? MIR_FLIP_HORIZONTAL : MIR_FLIP_NONE);
}

// ===== inputline.cpp =====
#include <cmath>
#include <utf8.h>
#include "mathf.hpp"
#include "imeboard.hpp"
#include "clientargparser.hpp"

extern IMEBoard *g_imeBoard;
extern GLDevice *g_glDevice;
extern ClientArgParser *g_clientArgParser;

InputLine::InputLine(InputLine::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .w = std::move(args.w),
          .h = std::move(args.h),

          .parent = std::move(args.parent),
      }}

    , m_imeEnabled(std::move(args.enableIME))
    , m_tpset
      {
          0,
          LALIGN_LEFT,
          false,
          false,

          args.font.id,
          args.font.size,
          args.font.style,

          std::move(args.font.color),
          std::move(args.font.bgColor),
      }

    , m_cursorArgs(std::move(args.cursor))

    , m_onTab   (std::move(args.onTab))
    , m_onCR    (std::move(args.onCR))
    , m_onChange(std::move(args.onChange))
    , m_validate(std::move(args.validate))
{}

bool InputLine::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    switch(event.type){
        case MIR_EVENT_TEXT_INPUT:
            {
                // unlike SDL3, GLFW has no SDL_StopTextInput equivalent
                // char events are always dispatched (see GLDevice::fnCharEvent)
                // and the OS IME still shows the input candidates no matter what

                // so just filter the MIR_EVENT_TEXT_INPUT event
                // only widgets using the system IME consume it

				//if(const auto ime = Widget::evalInt(m_imeEnabled, this); (ime != IME_SYSTEM) && valid){
                //    throw fflpanic("received valid MIR_EVENT_TEXT_INPUT while system input is not enabled: IME {}", ime);
                //}
                if(const auto ime = Widget::evalInt(m_imeEnabled, this); ime != IME_SYSTEM){
                    return false;
                }

                if(!valid || !focus()){
                    return false;
                }

                if(str_haschar(event.text.text)){
                    m_cursor += m_tpset.insertUTF8String(m_cursor, 0, event.text.text);
                    if(m_onChange){
                        m_onChange(m_tpset.getRawString());
                    }
                }

                m_cursorBlink = 0.0;
                return true;
            }
        case MIR_EVENT_KEY_DOWN:
            {
                // another widget can consume the event
                // and pass the focus to this widget, don't drop focus for keyboard events

                if(!valid){
                    return false;
                }

                if(!focus()){
                    return false;
                }

                switch(event.key.key){
                    case MIRK_TAB:
                        {
                            if(m_onTab){
                                m_onTab();
                            }
                            return true;
                        }
                    case MIRK_RETURN:
                        {
                            if(m_onCR){
                                m_onCR();
                            }
                            return true;
                        }
                    case MIRK_LEFT:
                        {
                            m_cursor = std::max<int>(0, m_cursor - 1);
                            m_cursorBlink = 0.0;
                            return true;
                        }
                    case MIRK_RIGHT:
                        {
                            if(m_tpset.empty()){
                                m_cursor = 0;
                            }
                            else{
                                m_cursor = std::min<int>(m_tpset.lineTokenCount(0), m_cursor + 1);
                            }
                            m_cursorBlink = 0.0;
                            return true;
                        }
                    case MIRK_BACKSPACE:
                        {
                            if(m_cursor > 0){
                                m_tpset.deleteToken(m_cursor - 1, 0, 1);
                                m_cursor--;

                                if(m_onChange){
                                    m_onChange(m_tpset.getRawString());
                                }
                            }
                            m_cursorBlink = 0.0;
                            return true;
                        }
                    case MIRK_ESCAPE:
                        {
                            setFocus(false);
                            return true;
                        }
                    default:
                        {
                            const auto ime = Widget::evalInt(m_imeEnabled, this);
                            const char keyChar = GLDeviceHelper::getKeyChar(event, true);

                            if(ime == IME_SYSTEM){
                                // when System IME is enabled
                                // SDL3 still dispatch MIR_EVENT_KEY_DOWN, need to ignore
                            }

                            else if((ime == IME_EMBEDED) && g_imeBoard->active() && (keyChar >= 'a' && keyChar <= 'z')){
                                g_imeBoard->gainFocus("", str_printf("%c", keyChar), this, [this](std::string s)
                                {
                                    m_tpset.insertUTF8String(m_cursor, 0, s.c_str());
                                    m_cursor += utf8::distance(s.begin(), s.end());
                                    if(m_onChange){
                                        m_onChange(m_tpset.getRawString());
                                    }
                                });
                            }
                            else if(keyChar != '\0'){
                                m_tpset.insertUTF8String(m_cursor++, 0, str_printf("%c", keyChar).c_str());
                                if(m_onChange){
                                    m_onChange(m_tpset.getRawString());
                                }
                            }

                            m_cursorBlink = 0.0;
                            return true;
                        }
                }
            }
        case MIR_EVENT_MOUSE_BUTTON_UP:
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
            {
                if(!valid){
                    return consumeFocus(false);
                }

                if(!m.in(to_d(event.button.x), to_d(event.button.y))){
                    return consumeFocus(false);
                }

                if(event.type == MIR_EVENT_MOUSE_BUTTON_DOWN){
                    const int eventX = to_d(event.button.x) - m.x;
                    const int eventY = to_d(event.button.y) - m.y;

                    const auto [cursorX, cursorY] = m_tpset.locCursor(eventX, eventY);
                    if(cursorY != 0){
                        throw fflpanic("cursor locates at wrong line");
                    }

                    m_cursor = cursorX;
                    m_cursorBlink = 0.0;
                }

                return consumeFocus(true);
            }
        default:
            {
                return false;
            }
    }
}

void InputLine::setFocus(bool argFocus)
{
    Widget::setFocus(argFocus);
    if(focus() && (Widget::evalInt(m_imeEnabled, this) == IME_SYSTEM)){
        g_glDevice->enableSystemIME(id());
    }
    else{
        g_glDevice->disableSystemIME(id());
    }
    m_cursorBlink = 0.0;
}

void InputLine::drawDefault(Widget::ROIMap m) const
{
    if(!m.calibrate(this)){
        return;
    }

    int dstCropX = m.x;
    int dstCropY = m.y;
    int srcCropX = m.ro->x;
    int srcCropY = m.ro->y;
    int srcCropW = m.ro->w;
    int srcCropH = m.ro->h;

    const int tpsetX = 0;
    const int tpsetY = 0 + (h() - (m_tpset.empty() ? m_tpset.getDefaultFontHeight() : m_tpset.ph())) / 2;

    const auto needDraw = mathf::cropROI(
            &srcCropX, &srcCropY,
            &srcCropW, &srcCropH,
            &dstCropX, &dstCropY,

            w(),
            h(),

            tpsetX, tpsetY, m_tpset.pw(), m_tpset.ph());

    if(needDraw){
        m_tpset.draw({.x=dstCropX, .y=dstCropY, .ro{srcCropX - tpsetX, srcCropY - tpsetY, srcCropW, srcCropH}});
    }

    if(std::fmod(m_cursorBlink, 1000.0) > 500.0){
        return;
    }

    if(!focus()){
        return;
    }

    int cursorY = m.y + tpsetY;
    int cursorX = m.x + tpsetX + [this]()
    {
        if(m_tpset.empty() || m_cursor == 0){
            return 0;
        }

        if(m_cursor == m_tpset.lineTokenCount(0)){
            return m_tpset.pw();
        }

        const auto pToken = m_tpset.getToken(m_cursor - 1, 0);
        return pToken->box.state.w1 + pToken->box.state.x + pToken->box.info.w;
    }();

    int cursorW = Widget::evalSizeOpt(m_cursorArgs.w, this, []{ return 2; });
    int cursorH = std::max<int>(m_tpset.ph(), h());

    if(mathf::rectangleOverlapRegion(m.x, m.y, m.ro->w, m.ro->h, cursorX, cursorY, cursorW, cursorH)){
        g_glDevice->fillRectangle(Widget::evalU32(m_cursorArgs.color, this), cursorX, cursorY, cursorW, cursorH);
    }

    if(g_clientArgParser->debugDrawInputLine){
        g_glDevice->drawRectangle(colorf::BLUE + colorf::A_SHF(255), m.x, m.y, w(), h());
    }
}

void InputLine::deleteChar()
{
    m_tpset.deleteToken(m_cursor - 1, 0, 1);
    m_cursor--;
}

void InputLine::insertChar(char ch)
{
    const char rawString[]
    {
        ch, '\0',
    };

    m_tpset.insertUTF8String(m_cursor, 0, rawString);
    m_cursor++;
}

void InputLine::insertUTF8String(const char *utf8Str)
{
    if(str_haschar(utf8Str)){
        m_cursor += m_tpset.insertUTF8String(m_cursor, 0, utf8Str);
    }
}

void InputLine::clear()
{
    m_cursor = 0;
    m_cursorBlink = 0.0;

    if(!m_tpset.empty()){
        m_tpset.clear();

        if(m_onChange){
            m_onChange({});
        }
    }
}

void InputLine::setInput(const char *utf8Str)
{
    m_cursor = 0;
    m_cursorBlink = 0.0;

    m_tpset.clear();
    if(str_haschar(utf8Str)){
        m_cursor = m_tpset.insertUTF8String(m_cursor, 0, utf8Str);
    }

    if(m_onChange){
        m_onChange(m_tpset.getRawString());
    }
}

// ===== labelboard.cpp =====
#include "strf.hpp"
#include "xmlf.hpp"
#include "xmltypeset.hpp"

LabelBoard::LabelBoard(LabelBoard::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .w = 0, // use override w() and h()
          .h = 0,

          .attrs
          {
              .type
              {
                  .setSize = false,
              },
              .inst = std::move(args.attrs),
          },

          .parent = std::move(args.parent),
      }}

    , m_tpset
      {
          0,
          LALIGN_LEFT,
          false,
          false,
          args.font.id,
          args.font.size,
          args.font.style,
          std::move(args.font.color),
      }
{
    setText(u8"%s", args.label ? args.label : u8"");
}

void LabelBoard::setText(const char8_t *format, ...)
{
    std::u8string text;
    str_format(format, text);
    loadXML(xmlf::toParString("%s", text.empty() ? "" : to_cstr(text)).c_str());
}

void LabelBoard::loadXML(const char *xmlString)
{
    // use the fallback values of m_tpset
    // don't need to specify the font/size/style info here

    m_tpset.loadXML(xmlString);
}

void LabelBoard::setFont(uint8_t argFont)
{
    m_tpset.setFont(argFont);
    m_tpset.updateGfx();
}

void LabelBoard::setFontSize(uint8_t argFontSize)
{
    m_tpset.setFontSize(argFontSize);
    m_tpset.updateGfx();
}

void LabelBoard::setFontStyle(uint8_t argFontStyle)
{
    m_tpset.setFontStyle(argFontStyle);
    m_tpset.updateGfx();
}

void LabelBoard::setFontColor(Widget::VarU32 argFontColor)
{
    m_tpset.setFontColor(std::move(argFontColor));
}

void LabelBoard::setImageMaskColor(Widget::VarU32 argColor)
{
    m_tpset.setImageMaskColor(std::move(argColor));
}

void LabelBoard::drawDefault(Widget::ROIMap m) const
{
    if(!m.calibrate(this)){
        return;
    }
    m_tpset.draw(m);
}

// ===== itemflex.cpp =====
ItemFlex::ItemFlex(ItemFlex::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .w = std::nullopt,
          .h = std::nullopt,

          .childList
          {
              {
                  .widget = new Widget
                  {{
                      .attrs
                      {
                          .inst
                          {
                              .moveOnFocus = false,
                          },
                      },
                  }},
                  .autoDelete = true,
              },
          },

          .attrs
          {
              .type
              {
                  .setSize = false,
                  .addChild = false,
                  .removeChild = false,
              },
          },

          .parent = std::move(args.parent),
      }}

    , m_vbox(args.v)
    , m_align(args.align)
    , m_canvas(firstChild())

    , m_headSpace(std::move(args.headSpace))
    , m_itemSpace(std::move(args.itemSpace))
    , m_tailSpace(std::move(args.tailSpace))
    , m_fixedEdgeSize(std::move(args.fixed))
{
    m_canvas->setSize([this]{ return canvasW(); },
                      [this]{ return canvasH(); });

    for(auto [widget, autoDelete]: args.childList){
        addItem(widget, autoDelete);
    }
}

void ItemFlex::addItem(Widget *argWidget, bool argAutoDelete)
{
    if(!argWidget){
        return;
    }

    const auto fnGetOffset = [argWidget, this]
    {
        int offset    = Widget::evalSize(m_headSpace, this);
        int itemSpace = Widget::evalSize(m_itemSpace, this);

        m_canvas->foreachChild([argWidget, &offset, itemSpace, this](const Widget *child, bool)
        {
            if(child == argWidget){
                return true;
            }

            if(!child->localShow()){
                return false;
            }

            offset += (m_vbox ? child->h() : child->w());
            offset += itemSpace;

            return false;
        });

        return offset;
    };

    Widget::VarDir d {};
    Widget::VarInt x {};
    Widget::VarInt y {};

    switch(m_align){
        case ItemAlign::UPLEFT:
            {
                d = DIR_UPLEFT;
                x =  m_vbox ? Widget::VarInt(0) : fnGetOffset;
                y = !m_vbox ? Widget::VarInt(0) : fnGetOffset;

                break;
            }
        case ItemAlign::DOWNRIGHT:
            {
                d =  m_vbox ? DIR_UPRIGHT : DIR_DOWNLEFT;
                x =  m_vbox ? Widget::VarInt{[this]{ return m_canvas->w() - 1; }} : fnGetOffset;
                y = !m_vbox ? Widget::VarInt{[this]{ return m_canvas->h() - 1; }} : fnGetOffset;

                break;
            }
        case ItemAlign::CENTER:
            {
                d =  m_vbox ? DIR_UP : DIR_LEFT;
                x =  m_vbox ? Widget::VarInt{[this]{ return m_canvas->w() / 2; }} : fnGetOffset;
                y = !m_vbox ? Widget::VarInt{[this]{ return m_canvas->h() / 2; }} : fnGetOffset;

                break;
            }
        default:
            {
                std::unreachable();
            }
    }

    m_canvas->addChildAt(argWidget, std::move(d), std::move(x), std::move(y), argAutoDelete);
}

void ItemFlex::removeItem(uint64_t argChildID, bool argTriggerAutoDelete)
{
    if(!argChildID){
        return;
    }

    m_canvas->removeChild(argChildID, argTriggerAutoDelete);
}

bool ItemFlex::hasShowItem() const
{
    return m_canvas->foreachChild([](const Widget *child, bool)
    {
        return child->localShow();
    });
}

void ItemFlex::flipItemShow(uint64_t childID)
{
    if(auto child = m_canvas->hasChild(childID)){
        child->flipShow();
    }
}

void ItemFlex::buildLayout()
{
    // empty function
}

int ItemFlex::canvasW() const
{
    if(m_vbox){
        return Widget::evalSizeOpt(m_fixedEdgeSize, this, [this]
        {
            int maxW = 0;
            m_canvas->foreachChild([&maxW](const Widget *child, bool)
            {
                if(child->localShow()){
                    maxW = std::max<int>(maxW, child->w());
                }
            });
            return maxW;
        });
    }
    else{
        if(const auto lastWidget = lastShowChild()){
            return lastWidget->dx() + lastWidget->w()  + Widget::evalSize(m_tailSpace, this);
        }
        else{
            return Widget::evalSize(m_headSpace, this) + Widget::evalSize(m_tailSpace, this);
        }
    }
}

int ItemFlex::canvasH() const
{
    if(!m_vbox){
        return Widget::evalSizeOpt(m_fixedEdgeSize, this, [this]
        {
            int maxH = 0;
            m_canvas->foreachChild([&maxH](const Widget *child, bool)
            {
                if(child->localShow()){
                    maxH = std::max<int>(maxH, child->h());
                }
            });
            return maxH;
        });
    }
    else{
        if(const auto lastWidget = lastShowChild()){
            return lastWidget->dy() + lastWidget->h()  + Widget::evalSize(m_tailSpace, this);
        }
        else{
            return Widget::evalSize(m_headSpace, this) + Widget::evalSize(m_tailSpace, this);
        }
    }
}

const Widget *ItemFlex::lastShowChild() const
{
    const Widget *lastWidget = nullptr;
    m_canvas->foreachChild(false, [&lastWidget](const Widget *child, bool) -> bool
    {
        if(child->localShow()){
            lastWidget = child;
        }
        return lastWidget;
    });
    return lastWidget;
}

// ===== itempair.cpp =====
#include <utility>
#include "fflerror.hpp"

ItemPair::ItemPair(ItemPair::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .w = [v=args.v, flex=args.flex, first=fflcheck(args.first.widget), second=fflcheck(args.second.widget), this]
          {
              if(v){
                  return std::max<int>(first->w(), second->w());
              }
              else if(flex.has_value()){
                  return Widget::evalSize(flex.value(), this);
              }
              else{
                  return first->w() + second->w();
              }
          },

          .h = [v=args.v, flex=args.flex, first=fflcheck(args.first.widget), second=fflcheck(args.second.widget), this]
          {
              if(!v){
                  return std::max<int>(first->h(), second->h());
              }
              else if(flex.has_value()){
                  return Widget::evalSize(flex.value(), this);
              }
              else{
                  return first->h() + second->h();
              }
          },

          .childList
          {
              {
                  .widget = args.first.widget,
                  .dir = [v = args.v, align = args.align]
                  {
                      switch(align){
                          case ItemAlign::UPLEFT   : return v ? DIR_UPLEFT : DIR_LEFTUP  ;
                          case ItemAlign::CENTER   : return v ? DIR_UP     : DIR_LEFT    ;
                          case ItemAlign::DOWNRIGHT: return v ? DIR_UPRIGHT: DIR_LEFTDOWN;
                      }
                      std::unreachable();
                  }(),

                  .x = [v = args.v, align = args.align, this]
                  {
                      switch(align){
                          case ItemAlign::UPLEFT   : return v ?       0 : 0;
                          case ItemAlign::CENTER   : return v ? w() / 2 : 0;
                          case ItemAlign::DOWNRIGHT: return v ? w() - 1 : 0;
                      }
                      std::unreachable();
                  },

                  .y = [v = args.v, align = args.align, this]
                  {
                      switch(align){
                          case ItemAlign::UPLEFT   : return v ? 0 :       0;
                          case ItemAlign::CENTER   : return v ? 0 : h() / 2;
                          case ItemAlign::DOWNRIGHT: return v ? 0 : h() - 1;
                      }
                      std::unreachable();
                  },

                  .autoDelete = args.first.autoDelete,
              },

              {
                  .widget = args.second.widget,
                  .dir = [v = args.v, align = args.align]
                  {
                      switch(align){
                          case ItemAlign::UPLEFT   : return v ? DIR_DOWNLEFT  : DIR_RIGHTUP  ;
                          case ItemAlign::CENTER   : return v ? DIR_DOWN      : DIR_RIGHT    ;
                          case ItemAlign::DOWNRIGHT: return v ? DIR_DOWNRIGHT : DIR_RIGHTDOWN;
                      }
                      std::unreachable();
                  }(),

                  .x = [v = args.v, align = args.align, this]
                  {
                      switch(align){
                          case ItemAlign::UPLEFT   : return v ?       0 : w() - 1;
                          case ItemAlign::CENTER   : return v ? w() / 2 : w() - 1;
                          case ItemAlign::DOWNRIGHT: return v ? w() - 1 : w() - 1;
                      }
                      std::unreachable();
                  },

                  .y = [v = args.v, align = args.align, this]
                  {
                      switch(align){
                          case ItemAlign::UPLEFT   : return v ? h() - 1 :       0;
                          case ItemAlign::CENTER   : return v ? h() - 1 : h() / 2;
                          case ItemAlign::DOWNRIGHT: return v ? h() - 1 : h() - 1;
                      }
                      std::unreachable();
                  },

                  .autoDelete = args.second.autoDelete,
              }
          },

          .attrs
          {
              .type {.setSize  = false},
              .inst {.moveOnFocus = false},
          },

          .parent = std::move(args.parent),
      }}
{}

// ===== menu.cpp =====
void Menu::evalClickCBFunc(const ClickCBFunc &cbFunc, Widget *widget)
{
    std::visit(VarDispatcher
    {
        [      ](const std::function<void(        )> &f){ if(f){ f(      ); }},
        [widget](const std::function<void(Widget *)> &f){ if(f){ f(widget); }},

        [](const auto &){},
    },

    cbFunc);
}

// ===== buttonbase.cpp =====
#include "audiodevice.hpp"
#include <functional>
#include "pngtexdb.hpp"
#include "soundeffectdb.hpp"

extern GLDevice *g_glDevice;
extern AudioDevice *g_audioDevice;
extern SoundEffectDB *g_seffDB;

ButtonBase::ButtonBase(ButtonBase::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .w = std::move(args.w),
          .h = std::move(args.h),

          .attrs
          {
              .type
              {
                  .setSize  = false,
                  .addChild = false,
              },
              .inst = std::move(args.attrs),
          },
          .parent = std::move(args.parent),
      }}

    , m_onClickDone(args.onClickDone)
    , m_radioMode  (args.radioMode  )

    , m_seff(std::move(args.seff))
    , m_offset
      {
          {0               , 0               },
          {args.offXOnOver , args.offYOnOver },
          {args.offXOnClick, args.offYOnClick},
      }

    , m_onOverIn (std::move(args.onOverIn ))
    , m_onOverOut(std::move(args.onOverOut))
    , m_onClick  (std::move(args.onClick  ))
    , m_onTrigger(std::move(args.onTrigger))
{}

bool ButtonBase::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    if(!valid){
        if(m_radioMode){
            if(getState() == BEVENT_ON){
                setState(BEVENT_OFF);
                onOverOut();
            }
        }
        else{
            if(getState() != BEVENT_OFF){
                setState(BEVENT_OFF);
                onOverOut();
            }
        }
        return consumeFocus(false);
    }

    if(!active()){
        if(m_radioMode){
            if(getState() == BEVENT_ON){
                setState(BEVENT_OFF);
            }
        }
        else{
            if(getState() != BEVENT_OFF){
                setState(BEVENT_OFF);
            }
        }
        return consumeFocus(false);
    }

    switch(event.type){
        case MIR_EVENT_MOUSE_BUTTON_UP:
            {
                if(m.in(to_d(event.button.x), to_d(event.button.y))){
                    switch(getState()){
                        case BEVENT_OFF:
                            {
                                setState(BEVENT_ON);
                                setFocus(true);

                                onBadEvent();
                                break;
                            }
                        case BEVENT_DOWN:
                            {
                                if(m_radioMode){
                                    // keep pressed
                                }
                                else{
                                    setState(BEVENT_ON);
                                    setFocus(true);

                                    onClick(true, event.button.clicks);
                                    if(m_onClickDone){
                                        onTrigger(event.button.clicks);
                                    }
                                }
                                break;
                            }
                        default:
                            {
                                break;
                            }
                    }
                    return true;
                }
                else if(m_radioMode){
                    return consumeFocus(false);
                }
                else{
                    if(getState() != BEVENT_OFF){
                        setState(BEVENT_OFF);
                        onOverOut();
                    }
                    return consumeFocus(false);
                }
            }
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
            {
                if(m.in(to_d(event.button.x), to_d(event.button.y))){
                    switch(getState()){
                        case BEVENT_OFF:
                            {
                                setState(BEVENT_DOWN);
                                setFocus(true);

                                onBadEvent();
                                break;
                            }
                        case BEVENT_ON:
                            {
                                setState(BEVENT_DOWN);
                                setFocus(true);

                                onClick(false, event.button.clicks);
                                if(!m_onClickDone){
                                    onTrigger(event.button.clicks);
                                }
                                break;
                            }
                        default:
                            {
                                break;
                            }
                    }
                    return true;
                }
                else if(m_radioMode){
                    return consumeFocus(false);
                }
                else{
                    if(getState() != BEVENT_OFF){
                        setState(BEVENT_OFF);
                        onOverOut();
                    }
                    return consumeFocus(false);
                }
            }
        case MIR_EVENT_MOUSE_MOTION:
            {
                if(m.in(to_d(event.motion.x), to_d(event.motion.y))){
                    switch(getState()){
                        case BEVENT_OFF:
                            {
                                setState(BEVENT_ON);
                                onOverIn();
                                break;
                            }
                        case BEVENT_DOWN:
                            {
                                if(event.motion.state & MIR_BUTTON_LMASK){
                                    // hold the button and moving
                                    // don't trigger
                                }
                                else if(m_radioMode){
                                    // keep pressed
                                }
                                else{
                                    setState(BEVENT_ON);
                                    onBadEvent();
                                }
                                break;
                            }
                        default:
                            {
                                break;
                            }
                    }
                    return true;
                }
                else if(m_radioMode){
                    if(getState() == BEVENT_ON){
                        setState(BEVENT_OFF);
                        onOverOut();
                    }
                    return consumeFocus(false);
                }
                else{
                    if(getState() != BEVENT_OFF){
                        setState(BEVENT_OFF);
                        onOverOut();
                    }
                    return consumeFocus(false);
                }
            }
        default:
            {
                return consumeFocus(false);
            }
    }
}

void ButtonBase::onOverIn()
{
    Button::evalOverCBFunc(m_onOverIn, this);
    if(m_seff.onOverIn.has_value()){
        g_audioDevice->playSoundEffect(g_seffDB->retrieve((m_seff.onOverIn.value())));
    }
}

void ButtonBase::onOverOut()
{
    Button::evalOverCBFunc(m_onOverOut, this);
    if(m_seff.onOverOut.has_value()){
        g_audioDevice->playSoundEffect(g_seffDB->retrieve((m_seff.onOverOut.value())));
    }
}

void ButtonBase::onClick(bool clickDone, int clickCount)
{
    Button::evalClickCBFunc(m_onClick, this, clickDone, clickCount);
    if(clickDone){
        // pressed button released
    }
    else{
        if(m_seff.onClick.has_value()){
            g_audioDevice->playSoundEffect(g_seffDB->retrieve((m_seff.onClick.value())));
        }
    }
}

void ButtonBase::onTrigger(int clickCount)
{
    Button::evalTriggerCBFunc(m_onTrigger, this, clickCount);
}

void ButtonBase::onBadEvent()
{
}

// ===== trigfxbutton.cpp =====

TrigfxButton::TrigfxButton(TrigfxButton::InitArgs args)
    : ButtonBase
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .w = 0, // use override w() and h()
          .h = 0, // ...

          .onOverIn  = std::move(args.onOverIn),
          .onOverOut = std::move(args.onOverOut),

          .onClick = std::move(args.onClick),
          .onTrigger = std::move(args.onTrigger),

          .seff = args.seff,

          .offXOnOver = args.offXOnOver,
          .offYOnOver = args.offYOnOver,

          .offXOnClick = args.offXOnClick,
          .offYOnClick = args.offYOnClick,

          .onClickDone = args.onClickDone,
          .radioMode   = args.radioMode,

          .attrs  = std::move(args.attrs),
          .parent = std::move(args.parent),
      }}

    , m_gfxFunc(std::move(args.gfxFunc))
    , m_gfxList(args.gfxList)
{}

void TrigfxButton::drawDefault(Widget::ROIMap m) const
{
    if(!m.calibrate(this)){
        return;
    }

    if(auto gfxPtr = evalGfxWidget()){
        drawAsChild(gfxPtr, DIR_UPLEFT, m_offset[getState()][0], m_offset[getState()][1], m);
    }
}

const Widget *TrigfxButton::evalGfxWidget(std::optional<int> stateOpt) const
{
    const auto state = stateOpt.value_or(getState());
    return std::visit(VarDispatcher
    {
        [state, this](const std::function<const Widget *(                   )> &f){ return f ? f(           ) : m_gfxList.at(state); },
        [state, this](const std::function<const Widget *(                int)> &f){ return f ? f(      state) : m_gfxList.at(state); },
        [state, this](const std::function<const Widget *(const Widget *, int)> &f){ return f ? f(this, state) : m_gfxList.at(state); },
        [state, this](const                                               auto & ){ return                      m_gfxList.at(state); },
    },
    m_gfxFunc);
}

const Widget *TrigfxButton::evalGfxWidgetValid() const
{
    for(const auto s = getState() - BEVENT_BEGIN; int i: {0, 1, 2}){
        if(const auto gfxPtr = evalGfxWidget(BEVENT_BEGIN + ((s + i) % 3))){
            return gfxPtr;
        }
    }
    return nullptr;
}

// ===== textboard.cpp =====
#include "fontexdb.hpp"

extern FontexDB *g_fontexDB;
TextBoard::TextBoard(TextBoard::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),
          .w = {},
          .h = {},

          .attrs
          {
              .inst = std::move(args.attrs),
          },
          .parent = std::move(args.parent),
      }}

    , m_font(std::move(args.font))
    , m_textFunc(std::move(args.textFunc))

    , m_image
      {{
          .texLoadFunc = [this]() -> GLTexID
          {
              if(const auto s = getVStr(); s.empty()){
                  return nullptr;
              }
              else{
                  return g_fontexDB->retrieve(m_font.id, m_font.size, m_font.style, s.c_str());
              }
          },

          .modColor = [this]
          {
              return Widget::evalU32(m_font.color, this);
          },

          .blendMode = std::move(args.blendMode),
          .parent{this},
      }}
{
    if(!g_fontexDB->hasFont(m_font.id)){
        throw fflpanic("invalid font: {:d}", m_font.id);
    }
}

std::tuple<std::string, std::string> TextBoard::fontName() const
{
    return g_fontexDB->fontName(m_font.id);
}

// ===== tritexbutton.cpp =====
#include "sysconst.hpp"

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

TritexButton::TritexButton(TritexButton::InitArgs args)
    : ButtonBase
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .w = 0,
          .h = 0,

          .onOverIn  = std::move(args.onOverIn),
          .onOverOut = std::move(args.onOverOut),

          .onClick   = std::move(args.onClick),
          .onTrigger = std::move(args.onTrigger),

          .seff = std::move(args.seff),

          .offXOnOver = args.offXOnOver,
          .offYOnOver = args.offYOnOver,

          .offXOnClick = args.offXOnClick,
          .offYOnClick = args.offYOnClick,

          .onClickDone = args.onClickDone,
          .radioMode   = args.radioMode,

          .attrs  = std::move(args.attrs),
          .parent = std::move(args.parent),
      }}

    , m_texIDFunc(std::move(args.texIDFunc))
    , m_texIDList(std::move(args.texIDList))

    , m_img
      {{
          .texLoadFunc = [this]
          {
              return evalGfxTexture();
          },

          .modColor = [modColor = std::move(args.modColor), this]
          {
              if(!active()){
                  return colorf::RGBA(128, 128, 128, 255);
              }

              if(modColor.has_value()){
                  return Widget::evalU32(modColor.value(), this);
              }

              switch(getState()){
                  case BEVENT_OFF : return colorf::RGBA(255, 255, 255, 255);
                  case BEVENT_ON  : return colorf::RGBA(255, 200, 255, 255);
                  case BEVENT_DOWN: return colorf::RGBA(200, 150, 150, 255);
                  default: std::unreachable();
              }
          },

          .blendMode = [this]
          {
              if(m_blinkTime.has_value()){
                  const auto offTime = std::get<0>(m_blinkTime.value());
                  const auto  onTime = std::get<1>(m_blinkTime.value());

                  if(offTime == 0){
                      return MIR_BLENDMODE_ADD;
                  }
                  else if(onTime == 0){
                      return MIR_BLENDMODE_BLEND;
                  }
                  else{
                      if(std::fmod(m_accuBlinkTime, offTime + onTime) < offTime){
                          return MIR_BLENDMODE_BLEND;
                      }
                      else{
                          return MIR_BLENDMODE_ADD;
                      }
                  }
              }
              else{
                  return MIR_BLENDMODE_BLEND;
              }
          },
      }}
{}

void TritexButton::drawDefault(Widget::ROIMap m) const
{
    if(!m.calibrate(this)){
        return;
    }
    drawAsChild(&m_img, DIR_UPLEFT, m_offset[getState()][0], m_offset[getState()][1], m);
}

GLTexID TritexButton::evalGfxTexture(std::optional<int> stateOpt) const
{
    const auto state = stateOpt.value_or(getState());
    return std::visit(VarDispatcher
    {
        [state, this](const std::function<std::optional<uint32_t>(                   )> &f){ return (f ? f(           ) : m_texIDList[state]).transform([](auto id){ return g_progUseDB->retrieve(id); }).value_or(nullptr); },
        [state, this](const std::function<std::optional<uint32_t>(                int)> &f){ return (f ? f(      state) : m_texIDList[state]).transform([](auto id){ return g_progUseDB->retrieve(id); }).value_or(nullptr); },
        [state, this](const std::function<std::optional<uint32_t>(const Widget *, int)> &f){ return (f ? f(this, state) : m_texIDList[state]).transform([](auto id){ return g_progUseDB->retrieve(id); }).value_or(nullptr); },
        [state, this](const                                                        auto & ){ return (                     m_texIDList[state]).transform([](auto id){ return g_progUseDB->retrieve(id); }).value_or(nullptr); },
    },
    m_texIDFunc);
}

GLTexID TritexButton::evalGfxTextureValid() const
{
    for(const auto s = getState() - BEVENT_BEGIN; int i: {0, 1, 2}){
        if(const auto gfxPtr = evalGfxTexture(BEVENT_BEGIN + ((s + i) % 3))){
            return gfxPtr;
        }
    }
    return nullptr;
}

// ===== acbutton.cpp =====
#include "processrun.hpp"

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

ACButton::ACButton(ACButton::InitArgs args)
    : TrigfxButton
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .onTrigger = [this](int)
          {
              m_buttonIndex = (m_buttonIndex + 1) % m_buttonNameList.size();
          },

          .onClickDone = false,
          .parent = std::move(args.parent),
      }}

    , m_proc(fflcheck(args.proc))
    , m_texMap
      {
          {"AC", 0X00000046},
          {"DC", 0X00000047},
          {"MA", 0X00000048},
          {"MC", 0X00000049},
      }

    , m_buttonNameList(std::move(fflcheck(args.names, !args.names.empty())))
    , m_img
      {{
          .texLoadFunc = [this]{ return g_progUseDB->retrieve(m_texMap.at(buttonName())); },
          .modColor = [this] -> uint32_t
          {
              if(getState() == BEVENT_OFF) return colorf::WHITE_A255;
              else                         return colorf::  RED_A255;
          },
      }}

    , m_text
      {{
          .textFunc = [this]
          {
              const auto [low, high] = m_proc->getACNum(buttonName());
              return str_printf("%d-%d", low, high);
          },

          .font
          {
              .color = [this] -> uint32_t
              {
                  if(getState() == BEVENT_OFF) return colorf::RGBA(0XFF, 0XFF, 0X00, 0XFF);
                  else                         return colorf::RGBA(0XFF, 0X00, 0X00, 0XFF);
              },
          },
      }}

    , m_gfxCanvas
      {{
          .flex = [this]{ return m_img.w() + 5 + m_text.w(); },

          .v = false,
          .align = ItemAlign::CENTER,

          .first{&m_img},
          .second{&m_text},
      }}
{
    setGfxFunc([this]{ return &m_gfxCanvas; });
}

// ===== alphaonbutton.cpp =====
#include "bevent.hpp"

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

AlphaOnButton::AlphaOnButton(AlphaOnButton::InitArgs args)
    : TrigfxButton
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .onOverIn  = std::move(args.onOverIn),
          .onOverOut = std::move(args.onOverOut),

          .onClick   = std::move(args.onClick),
          .onTrigger = std::move(args.onTrigger),

          .onClickDone = args.triggerOnDone,
          .parent = args.parent,
      }}

    , m_onOffX(args.onOffX)
    , m_onOffY(args.onOffY)
    , m_onRadius(args.onRadius)

    , m_down
      {{
          .texLoadFunc = [texID = std::move(args.downTexID), this] -> GLTexID
          {
              return g_progUseDB->retrieve(Widget::evalU32(texID, this));
          },
      }}

    , m_on
      {{
          .w = std::nullopt,
          .h = std::nullopt,

          .childList
          {
              {
                  .widget = new ImageBoard
                  {{

                      .w = [this]{ return m_down.h(); }, // downTexID has blank alpha area on right side, use h() as w()
                      .h = [this]{ return m_down.h(); },

                      .texLoadFunc = [this] -> GLTexID
                      {
                          return g_glDevice->getCover(m_onRadius, 360);
                      },

                      .modColor = std::move(args.modColor),
                  }},

                  .x = m_onOffX,
                  .y = m_onOffY,

                  .autoDelete = true,
              },
          },
      }}

    , m_off
      {{
          .w = [this]{ return m_on.w(); },
          .h = [this]{ return m_on.h(); },
      }}
{
    setGfxList({&m_off, &m_on, &m_down});
}

// ===== menuitem.cpp =====

extern GLDevice *g_glDevice;
MenuItem::MenuItem(MenuItem::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .w = std::nullopt,
          .h = std::nullopt,

          .childList
          {
              {
                  .widget = new TrigfxButton
                  {{
                      .onOverIn = [expandOn = args.expandOnHover, this]
                      {
                          if(m_subWidget && Widget::evalBool(expandOn, this)){
                              m_subWidget->setShow(true);
                          }
                      },

                      .onOverOut = [expandOn = args.expandOnHover, this]
                      {
                          if(m_subWidget && Widget::evalBool(expandOn, this)){
                              m_subWidget->setShow(false);
                          }
                      },

                      .onTrigger = [onClick = std::move(args.onClick), gfxWidget = fflcheck(args.gfxWidget.widget)](int)
                      {
                          Menu::evalClickCBFunc(onClick, gfxWidget);
                      },
                  }},

                  .autoDelete = true,
              },

              {
                  .widget = args.subWidget.widget,
                  .autoDelete = args.subWidget.autoDelete,
              },
          },

          .attrs
          {
              .type
              {
                  .setSize  = false,
                  .addChild = false,
              },

              .inst
              {
                  .moveOnFocus = false,
              },
          },
          .parent = std::move(args.parent),
      }}

    , m_subWidget(args.subWidget.widget)
    , m_gfxButton(fflcheck(dynamic_cast<TrigfxButton *>(firstChild())))

    , m_itemSize(std::move(args.itemSize))
    , m_gfxWidgetCrop
      {{
          .w = [this]
          {
              if(m_itemSize.w.has_value()){
                  return Widget::evalSize(m_itemSize.w.value(), this); // make sure eval with this, not &m_gfxWidgetCrop
              }
              else{
                  return gfxWidget()->w();
              }
          },

          .h = [this]
          {
              if(m_itemSize.h.has_value()){
                  return Widget::evalSize(m_itemSize.h.value(), this); // make sure eval with this, not &m_gfxWidgetCrop
              }
              else{
                  return gfxWidget()->h();
              }
          },

          .childList
          {
              {
                  .widget = args.gfxWidget.widget,
                  .dir = DIR_LEFT,

                  .x = 0,
                  .y = [this]{ return m_gfxWidgetCrop.h() / 2; },

                  .autoDelete = args.gfxWidget.autoDelete,
              },
          },
      }}

    , m_indicator
      {{
           .w = [showInd = args.showIndicator, this]{ return Widget::evalBool(showInd, this) ? MenuItem::INDICATOR_W : 0; },
           .h = [showInd = args.showIndicator, this]{ return Widget::evalBool(showInd, this) ? MenuItem::INDICATOR_H : 0; },

           .drawFunc = [](int dstDrawX, int dstDrawY)
           {
               const int x1 = dstDrawX;
               const int y1 = dstDrawY;

               const int x2 = dstDrawX;
               const int y2 = dstDrawY + MenuItem::INDICATOR_H - 1;

               const int x3 = dstDrawX + MenuItem::INDICATOR_W - 1;
               const int y3 = dstDrawY + MenuItem::INDICATOR_H / 2;

               g_glDevice->fillTriangle(colorf::BLUE_A255, x1, y1, x2, y2, x3, y3);
           },
      }}

    , m_canvas
      {{
          .flex = std::nullopt,

          .v = false,
          .align = ItemAlign::CENTER,

          .first {&m_gfxWidgetCrop},
          .second{&m_indicator    },
      }}

    , m_wrapper
      {{
          .wrapped{&m_canvas},
          .margin = std::move(args.margin),
          .bgDrawFunc = [bgColor = std::move(args.bgColor), showSep = std::move(args.showSeparator), this](int dstDrawX, int dstDrawY)
          {
              std::optional<int> wopt;
              std::optional<int> hopt;

              if(m_gfxButton->getState() != BEVENT_OFF){
                  wopt = w();
                  hopt = h();
                  g_glDevice->fillRectangle(Widget::evalU32(bgColor, this), dstDrawX, dstDrawY, wopt.value(), hopt.value());
              }

              if(Widget::evalBool(showSep, this)){
                  const int  width = wopt.value_or(w());
                  const int dwidth = width >= 4 ? 2 : 0;

                  const int lineX1 = dstDrawX             + dwidth;
                  const int lineX2 = dstDrawX + width - 1 - dwidth;

                  const int lineY = dstDrawY + hopt.value_or(h()) - 1;
                  g_glDevice->drawLine(colorf::GREY + colorf::A_SHF(128), lineX1, lineY, lineX2, lineY);
              }
          },
      }}
{
    m_gfxButton->setGfxList(&m_wrapper);
    if(m_subWidget){
        m_subWidget->setShow(false);
        m_subWidget->moveAt(DIR_UPLEFT,
                [d = args.subWidget.dir, this]{ return m_gfxButton->dx() + Widget::xSizeOff(d, [this]{ return m_gfxButton->w() + 1; }); },
                [d = args.subWidget.dir, this]{ return m_gfxButton->dy() + Widget::ySizeOff(d, [this]{ return m_gfxButton->h() + 1; }); });
    }
}

void MenuItem::drawDefault(Widget::ROIMap m) const
{
    if(!m.calibrate(this)){
        return;
    }

    if(m_gfxButton->show()){
        drawChild(m_gfxButton, m);
    }

    if(m_subWidget && m_subWidget->show()){
        drawChild(m_subWidget, m);
    }
}

bool MenuItem::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    if(m_gfxButton->show()){
        if(m_gfxButton->processEventParent(event, valid, m)){
            return true;
        }
    }

    if(m_subWidget && m_subWidget->show()){
        return m_subWidget->processEventParent(event, valid, m);
    }

    return false;
}

// ===== menubutton.cpp =====
#include "gui_core.hpp"

MenuButton::MenuButton(MenuButton::InitArgs args)
    : MenuItem
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .margin = std::move(args.margin),
          .itemSize = std::move(args.itemSize),

          .gfxWidget = std::move(args.gfxWidget),
          .subWidget
          {
              .dir = DIR_DOWNLEFT,
              .widget = args.subWidget.widget,
              .autoDelete = args.subWidget.autoDelete,
          },

          .expandOnHover = std::move(args.expandOnHover),

          .bgColor = std::move(args.bgColor),
          .onClick = [this]
          {
              if(subWidget()){
                  subWidget()->setFocus(!subWidget()->show());
                  subWidget()->flipShow();
              }
          },

          .parent = std::move(args.parent),
      }}
{}

// ===== gfxdirbutton.cpp =====
#include "pathf.hpp"

extern GLDevice *g_glDevice;

GfxDirButton::GfxDirButton(GfxDirButton::InitArgs args)
    : TrigfxButton
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .onTrigger = std::move(args.onTrigger),
          .parent    = std::move(args.parent),
      }}

    , m_gfxDrawer
      {{
          .w = std::move(args.w),
          .h = std::move(args.h),

          .drawFunc = [triangle = std::move(args.triangle), frame = std::move(args.frame), this](const Widget *self, int dstDrawX, int dstDrawY)
          {
              const auto state = this->getState();

              const auto cw = self->w(); // canvas w
              const auto ch = self->h(); // canvas h

              const auto tw = Widget::evalSizeOpt(triangle.w, self, [cw]{ return cw; });
              const auto th = Widget::evalSizeOpt(triangle.h, self, [ch]{ return ch; });

              int x1 = 0, y1 = 0; //        apex vertex of the isosceles triangle
              int x2 = 0, y2 = 0; // first  base vertex
              int x3 = 0, y3 = 0; // second base vertex

              switch(const auto d = Widget::evalDir(triangle.dir, self)){
                  case DIR_UP   : x1 = tw / 2; y1 =      0; x2 =      0; y2 = th - 1; x3 = tw - 1; y3 = th - 1; break;
                  case DIR_DOWN : x1 = tw / 2; y1 = th - 1; x2 =      0; y2 =      0; x3 = tw - 1; y3 =      0; break;
                  case DIR_LEFT : x1 =      0; y1 = th / 2; x2 = tw - 1; y2 =      0; x3 = tw - 1; y3 = th - 1; break;
                  case DIR_RIGHT: x1 = tw - 1; y1 = th / 2; x2 =      0; y2 =      0; x3 =      0; y3 = th - 1; break;
                  default: throw fflpanic("invalid direction: {}", pathf::dirName(d));
              }

              const auto xoff = (cw - tw) / 2; // move to center
              const auto yoff = (ch - th) / 2;

              g_glDevice->fillTriangle(Widget::evalU32(triangle.color, self),
                      dstDrawX + x1 + xoff, dstDrawY + y1 + yoff,
                      dstDrawX + x2 + xoff, dstDrawY + y2 + yoff,
                      dstDrawX + x3 + xoff, dstDrawY + y3 + yoff);

              switch(state){
                  case BEVENT_OFF : break;
                  case BEVENT_ON  : g_glDevice->fillRectangle(colorf::RED + colorf::A_SHF(32), dstDrawX, dstDrawY, cw, ch); break;
                  case BEVENT_DOWN: g_glDevice->fillRectangle(colorf::RED + colorf::A_SHF(96), dstDrawX, dstDrawY, cw, ch); break;
                  default: std::unreachable();
              }

              if(Widget::evalBool(frame.show, self)){
                  g_glDevice->drawRectangle(Widget::evalU32(frame.color, self), dstDrawX, dstDrawY, cw, ch);
              }
          },
      }}
{
    setGfxFunc([this]{ return &m_gfxDrawer; });
}

// ===== sliderbase.cpp =====
#include "mirevent.hpp"

extern GLDevice *g_glDevice;
extern ClientArgParser *g_clientArgParser;

SliderBase::SliderBase(SliderBase::InitArgs args)
    : Widget
      {{
          .parent = std::move(args.parent),
      }}

    , m_value(fflcheck(args.value, args.value >= 0.0f && args.value <= 1.0f))
    , m_checkFunc(std::move(args.checkFunc))

    , m_barArgs(std::move(args.bar))
    , m_sliderArgs(std::move(args.slider))

    , m_onChange(std::move(args.onChange))
    , m_bar
      {{
          .dir = std::move(args.bar.dir),

          .x = [this]{ return -1 * widgetXFromBar(0); },
          .y = [this]{ return -1 * widgetYFromBar(0); },

          .w = [this]{ return Widget::evalSize(m_barArgs.w, this); },
          .h = [this]{ return Widget::evalSize(m_barArgs.h, this); },

          .contained = std::move(args.barWidget),
          .parent{this},
      }}

    , m_slider
      {{
          .x = [this]{ return sliderXAtValueFromBar(getValue(), m_bar.dx()); },
          .y = [this]{ return sliderYAtValueFromBar(getValue(), m_bar.dy()); },

          .w = [this]{ return Widget::evalSize(m_sliderArgs.w, this); },
          .h = [this]{ return Widget::evalSize(m_sliderArgs.h, this); },

          .contained = std::move(args.sliderWidget),
          .parent{this},
      }}

    , m_debugDraw
      {{
          .w = [this]{ return w(); },
          .h = [this]{ return h(); },

          .drawFunc = [this](const Widget *self, int drawDstX, int drawDstY)
          {
              if(g_clientArgParser->debugSlider){
                  g_glDevice->drawRectangle(colorf::GREEN_A255, drawDstX, drawDstY, self->w(), self->h());
                  g_glDevice->drawRectangle(colorf::BLUE_A255, drawDstX + m_bar.dx(), drawDstY + m_bar.dy(), m_bar.w(), m_bar.h());

                  const auto r = getSliderROI(drawDstX, drawDstY);
                  const auto [cx, cy] = getValueCenter(drawDstX, drawDstY);

                  g_glDevice->drawLine(colorf::YELLOW_A255, r.x, r.y, cx, cy);
                  g_glDevice->drawRectangle(colorf::RED_A255, r.x, r.y, r.w, r.h);
              }
          },

          .parent{this},
      }}
{
    moveTo([this]{ return widgetXFromBar(Widget::evalInt(m_barArgs.x, this)); },
           [this]{ return widgetYFromBar(Widget::evalInt(m_barArgs.y, this)); });

    setSize([this]{ return std::max<int>(Widget::evalSize(m_barArgs.w, this), sliderXAtValueFromBar(1.0f, 0) + m_slider.w()) - widgetXFromBar(0); },
            [this]{ return std::max<int>(Widget::evalSize(m_barArgs.h, this), sliderYAtValueFromBar(1.0f, 0) + m_slider.h()) - widgetYFromBar(0); });

    if(args.bgWidget.widget){
        setBarBgWidget(std::move(args.bgWidget.ox), std::move(args.bgWidget.oy), args.bgWidget.widget, args.bgWidget.autoDelete);
    }
}

bool SliderBase::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return consumeFocus(false);
    }

    if(!valid){
        return consumeFocus(false);
    }

    if(!active()){
        return consumeFocus(false);
    }

    switch(event.type){
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
            {
                if(inSlider(to_d(event.button.x), to_d(event.button.y), m)){
                    m_sliderState = BEVENT_DOWN;
                    return consumeFocus(true);
                }
                else if(const auto mbar = m.create(m_bar.roi(this)); mbar.in(to_d(event.button.x), to_d(event.button.y))){
                    m_sliderState = BEVENT_ON;
                    if(const auto newValue = std::clamp<float>([&event, startDstX = mbar.x - mbar.ro->x, startDstY = mbar.y - mbar.ro->y, this]() -> float
                    {
                        if(vbar()){
                            return ((to_d(event.button.y) - startDstY) * 1.0f) / std::max<int>(1, m_bar.h());
                        }
                        else{
                            return ((to_d(event.button.x) - startDstX) * 1.0f) / std::max<int>(1, m_bar.w());
                        }
                    }(), 0.0f, 1.0f);

                    Widget::execCheckFunc<float>(m_checkFunc, this, newValue)){
                        setValue(newValue, true);
                    }
                    return consumeFocus(true);
                }
                else{
                    m_sliderState = BEVENT_OFF;
                    return consumeFocus(false);
                }
            }
        case MIR_EVENT_MOUSE_BUTTON_UP:
            {
                if(inSlider(to_d(event.button.x), to_d(event.button.y), m)){
                    m_sliderState = BEVENT_ON;
                    return consumeFocus(true);
                }
                else{
                    m_sliderState = BEVENT_OFF;
                    return consumeFocus(false);
                }
            }
        case MIR_EVENT_MOUSE_MOTION:
            {
                if(event.motion.state & MIR_BUTTON_LMASK){
                    if(inSlider(to_d(event.motion.x), to_d(event.motion.y), m) || focus()){
                        m_sliderState = BEVENT_DOWN;
                        if(const auto newValue = std::clamp<float>(getValue() + [&event, this]() -> float
                        {
                            if(vbar()){
                                return pixel2Value(to_d(event.motion.yrel));
                            }
                            else{
                                return pixel2Value(to_d(event.motion.xrel));
                            }
                        }(), 0.0f, 1.0f);

                        Widget::execCheckFunc<float>(m_checkFunc, this, newValue)){
                            setValue(newValue, true);
                        }
                        return consumeFocus(true);
                    }
                    else{
                        m_sliderState = BEVENT_OFF;
                        return consumeFocus(false);
                    }
                }
                else{
                    if(inSlider(to_d(event.motion.x), to_d(event.motion.y), m)){
                        m_sliderState = BEVENT_ON;
                        return consumeFocus(true);
                    }
                    else{
                        m_sliderState = BEVENT_OFF;
                        return consumeFocus(false);
                    }
                }
            }
        default:
            {
                return consumeFocus(false);
            }
    }
}

void SliderBase::setValue(float value, bool triggerCallback)
{
    if(const auto newValue = std::clamp<float>(value, 0.0f, 1.0f); newValue != getValue()){
        m_value = newValue; // can change value to outside of range [min, max]
        if(triggerCallback && Widget::hasUpdateFunc(m_onChange)){
            Widget::execUpdateFunc(m_onChange, this, getValue());
        }
    }
}

void SliderBase::addValue(float diff, bool triggerCallback)
{
    setValue(m_value + diff, triggerCallback);
}

float SliderBase::pixel2Value(int pixel) const
{
    return pixel * 1.0f / std::max<int>(vbar() ? (m_bar.h() - 1) : (m_bar.w() - 1), 1);
}

Widget::ROI SliderBase::getBarROI(int startDstX, int startDstY) const
{
    return Widget::ROI
    {
        .x = startDstX + m_bar.dx(),
        .y = startDstY + m_bar.dy(),
        .w =             m_bar. w(),
        .h =             m_bar. h(),
    };
}

Widget::ROI SliderBase::getSliderROI(int startDstX, int startDstY) const
{
    return Widget::ROI
    {
        .x = startDstX + m_slider.dx(),
        .y = startDstY + m_slider.dy(),
        .w =             m_slider. w(),
        .h =             m_slider. h(),
    };
}

std::tuple<int, int> SliderBase::getValueCenter(int startDstX, int startDstY) const
{
    return
    {
        startDstX + m_slider.dx() + Widget::evalInt(m_sliderArgs.cx.value_or(m_slider.w() / 2), this),
        startDstY + m_slider.dy() + Widget::evalInt(m_sliderArgs.cy.value_or(m_slider.h() / 2), this),
    };
}

bool SliderBase::inSlider(int eventX, int eventY, Widget::ROIMap m) const
{
    if(!m.calibrate(this)){
        return false;
    }
    return m.create(m_slider.roi(this)).in(eventX, eventY);
}

void SliderBase::setBarBgWidget(Widget::VarInt ox, Widget::VarInt oy, Widget *bgWidget, bool autoDelete)
{
    if(m_bgWidgetID){
        if(auto oldBgWidget = hasChild(m_bgWidgetID)){
            removeChild(m_bgWidgetID, oldBgWidget != bgWidget);
        }

        m_bgOff.reset();
        m_bgWidgetID = 0;
    }

    if(bgWidget){
        m_bgOff = std::make_optional(std::make_pair(std::move(ox), std::move(oy)));
        m_bgWidgetID = bgWidget->id();

        addChildAt(bgWidget, DIR_UPLEFT,
                [this]{ return -1 * widgetXFromBar(0) - Widget::evalInt(m_bgOff->first , this); },
                [this]{ return -1 * widgetYFromBar(0) - Widget::evalInt(m_bgOff->second, this); }, autoDelete);
        moveFront(bgWidget);
    }
}

int SliderBase::sliderXAtValueFromBar(float value, int barX) const
{
    fflassert(value >= 0.0f, value);
    fflassert(value <= 1.0f, value);

    const auto barW = Widget::evalSize(m_barArgs.w, this);
    const auto sliderW = Widget::evalSize(m_sliderArgs.w, this);
    const auto sliderCX = Widget::evalInt(m_sliderArgs.cx.value_or(sliderW / 2), this);

    return vbar()
        ? (barX - sliderCX + barW / 2)
        : (barX - sliderCX + to_dround(value * (barW - 1)));
}

int SliderBase::sliderYAtValueFromBar(float value, int barY) const
{
    fflassert(value >= 0.0f, value);
    fflassert(value <= 1.0f, value);

    const auto barH = Widget::evalSize(m_barArgs.h, this);
    const auto sliderH = Widget::evalSize(m_sliderArgs.h, this);
    const auto sliderCY = Widget::evalInt(m_sliderArgs.cy.value_or(sliderH / 2), this);

    return vbar()
        ? (barY - sliderCY + to_dround(value * (barH - 1)))
        : (barY - sliderCY + barH / 2);
}

std::optional<int> SliderBase::bgXFromBar(int barX) const
{
    if(m_bgOff.has_value()){
        return barX - Widget::evalInt(m_bgOff->first, this);
    }
    return std::nullopt;
}

std::optional<int> SliderBase::bgYFromBar(int barY) const
{
    if(m_bgOff.has_value()){
        return barY - Widget::evalInt(m_bgOff->second, this);
    }
    return std::nullopt;
}

// ===== texslider.cpp =====

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;
extern ClientArgParser *g_clientArgParser;

TexSlider::TexSlider(TexSlider::InitArgs args)
    : SliderBase
      {{
          .bar = std::move(args.bar),
          .slider
          {
              .cx = [index = args.index, this]{ return TexSlider::getSliderTexInfo(index)->ox; },
              .cy = [index = args.index, this]{ return TexSlider::getSliderTexInfo(index)->oy; },

              .w = [index = args.index, this]{ return GLDeviceHelper::getTextureWidth (g_progUseDB->retrieve(TexSlider::getSliderTexInfo(index)->texID)); },
              .h = [index = args.index, this]{ return GLDeviceHelper::getTextureHeight(g_progUseDB->retrieve(TexSlider::getSliderTexInfo(index)->texID)); },
          },

          .value = args.value,

          .bgWidget = std::move(args.bgWidget),
          .sliderWidget
          {
              .dir = DIR_UPLEFT,
              .widget = new Widget
              {{
                  .w = {},
                  .h = {},

                  .childList
                  {
                      {new ImageBoard
                      {{
                          .texLoadFunc = [index = args.index, this] -> GLTexID
                          {
                              return g_progUseDB->retrieve(TexSlider::getSliderTexInfo(index)->texID);
                          },

                          .modColor = [this] -> uint32_t
                          {
                              if(active()){ return colorf::WHITE + colorf::A_SHF(0XFF); }
                              else        { return colorf::GREY  + colorf::A_SHF(0XFF); }
                          },

                      }}, DIR_UPLEFT, 0, 0, true},

                      {new ImageBoard
                      {{
                          .texLoadFunc = [index = args.index, this] -> GLTexID
                          {
                              // if ImageBoard::ctor() calls texLoadFunc when constructing, it can trigger UB
                              // because sliderState() requires *this* to be fully constructed

                              switch(sliderState()){
                                  case BEVENT_ON:
                                  case BEVENT_DOWN:
                                      {
                                          return g_glDevice->getCover(TexSlider::getSliderTexInfo(index)->cover, 360);
                                      }
                                  default:
                                      {
                                          return nullptr;
                                      }
                              }
                          },

                          .modColor = [this] -> uint32_t
                          {
                              switch(sliderState()){
                                  case BEVENT_ON:
                                      {
                                          if(active()){ return colorf::BLUE  + colorf::A_SHF(128); }
                                          else        { return colorf::WHITE_A255; }
                                      }
                                  case BEVENT_DOWN:
                                      {
                                          if(active()){ return colorf::RED   + colorf::A_SHF(128); }
                                          else        { return colorf::WHITE_A255; }
                                      }
                                  default:
                                      {
                                          return colorf::WHITE_A255;
                                      }
                              }
                          },
                      }},

                      DIR_NONE,

                      [index = args.index, this]{ return TexSlider::getSliderTexInfo(index)->ox; },
                      [index = args.index, this]{ return TexSlider::getSliderTexInfo(index)->oy; },

                      true},
                  }
              }},
          },

          .onChange = std::move(args.onChange),
          .parent = std::move(args.parent),
      }}
{
    fflassert(w() > 0);
    fflassert(h() > 0);

    const auto sliderInfo = TexSlider::getSliderTexInfo(args.index);

    fflassert(sliderInfo->cover > 0);
    fflassert(g_progUseDB->retrieve(sliderInfo->texID), str_printf("%08X.PNG", sliderInfo->texID));
}

// ===== texinputbackground.cpp =====

extern PNGTexDB *g_progUseDB;

Widget::ROI TexInputBackground::fromInputROI(bool v, Widget::ROI r)
{
    r.x -= ( v ? 2 : 3);
    r.y -= (!v ? 2 : 3);

    r.w += ( v ? 4 : 6);
    r.h += (!v ? 4 : 6);

    return r;
}

Widget::IntSize2D TexInputBackground::borderSize(bool v)
{
    return
    {
         v ? 4 : 6,
        !v ? 4 : 6,
    };
}

TexInputBackground::TexInputBackground(TexInputBackground::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .w = std::move(args.w),
          .h = std::move(args.h),

          .parent = std::move(args.parent),
      }}

    , m_img
      {{
          .texLoadFunc = []{ return g_progUseDB->retrieve(0X00000460); },

          .vflip = args.v,
          .rotate = args.v ? 1 : 0,
      }}

    , m_resize
      {{
          .getter = &m_img,
          .vr
          {
              3,
              3,
              m_img.w() - 6,
              m_img.h() - 6,
          },

          .resize = Widget::VarSize2D{[this] -> Widget::IntSize2D
          {
              return
              {
                  w() - 6,
                  h() - 6,
              };
          }},

          .parent{this},
      }}
{}

Widget::ROI TexInputBackground::getInputROI() const
{
    auto r = m_resize.gfxResizeROI();

    (v() ? r.x : r.y) -= 1;
    (v() ? r.w : r.h) += 2;

    return r;
}

void TexInputBackground::setInputSize(Widget::VarSize2D argSize)
{
    setSize([this, argSize]{ return argSize.w(this) + ( v() ? 4 : 6); },
            [this, argSize]{ return argSize.h(this) + (!v() ? 4 : 6); });
}

void TexInputBackground::setInputSize(Widget::VarSize argW, Widget::VarSize argH)
{
    setSize([this, argW = std::move(argW)]{ return Widget::evalSize(argW, this) + ( v() ? 4 : 6); },
            [this, argH = std::move(argH)]{ return Widget::evalSize(argH, this) + (!v() ? 4 : 6); });
}

// ===== texsliderbar.cpp =====

extern PNGTexDB *g_progUseDB;

TexSliderBar::TexSliderBar(TexSliderBar::InitArgs args)
    : TexSlider
      {{
          .bar = std::move(args.bar),

          .index = args.index,
          .value = args.value,

          .onChange = std::move(args.onChange),
          .parent = std::move(args.parent),
      }}

    , m_bg
      {{
          .v = vbar(),
      }}

    , m_imgBar
      {{
          .texLoadFunc = []{ return g_progUseDB->retrieve(0X00000470); },

          .vflip = vbar(),
          .rotate = vbar() ? 1 : 0,

          .modColor = [this] -> uint32_t
          {
              if(active()){ return colorf::WHITE + colorf::A_SHF(0XFF); }
              else        { return colorf::GREY  + colorf::A_SHF(0XFF); }
          },
      }}

    , m_bar
      {{
          .x = [this]{ return m_bg.getInputROI().x; },
          .y = [this]{ return m_bg.getInputROI().y; },

          .w = [this]{ return to_d(m_bg.getInputROI().w * ( vbar() ? 1.0f : getValue())); },
          .h = [this]{ return to_d(m_bg.getInputROI().h * (!vbar() ? 1.0f : getValue())); },

          .getter = &m_imgBar,
          .parent{&m_bg},
      }}
{
    m_bg.setInputSize(Widget::VarSize2D([this]
    {
        return getBarROI(0, 0).size();
    }));

    setBarBgWidget(m_bar.dx(), m_bar.dy(), &m_bg, false);
}

// ===== checkbox.cpp =====

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

bool CheckBox::evalBoolGetter(const CheckBox::BoolGetter &getter, const Widget *widget)
{
    return std::visit(VarDispatcher
    {
        [widget](const std::function<bool(              )> &func){ return func ? func(      ) : dynamic_cast<const CheckBox *>(widget)->rawGetter(); },
        [widget](const std::function<bool(const Widget *)> &func){ return func ? func(widget) : dynamic_cast<const CheckBox *>(widget)->rawGetter(); },
        [widget](const auto                                &    ){ return                       dynamic_cast<const CheckBox *>(widget)->rawGetter(); },
    },
    getter);
}

void CheckBox::evalBoolSetter(CheckBox::BoolSetter &setter, Widget *widget, bool value)
{
    std::visit(VarDispatcher
    {
        [value, widget](std::function<void(          bool)> &func){ func ? func(        value) : dynamic_cast<CheckBox *>(widget)->rawSetter(value); },
        [value, widget](std::function<void(Widget *, bool)> &func){ func ? func(widget, value) : dynamic_cast<CheckBox *>(widget)->rawSetter(value); },
        [value, widget](auto                                &    ){                              dynamic_cast<CheckBox *>(widget)->rawSetter(value); },
    },
    setter);
}

void CheckBox::evalTriggerFunc(CheckBox::TriggerFunc &trigger, Widget *widget, bool value)
{
    std::visit(VarDispatcher
    {
        [value        ](std::function<void(          bool)> &func){ if(func){ func(        value); }},
        [value, widget](std::function<void(Widget *, bool)> &func){ if(func){ func(widget, value); }},

        [](auto &){},
    },
    trigger);
}

bool CheckBox::hasBoolGetter(const CheckBox::BoolGetter &getter)
{
    return std::visit(VarDispatcher
    {
        [](const std::function<bool(              )> &func){ return !!func;  },
        [](const std::function<bool(const Widget *)> &func){ return !!func;  },
        [](const auto &){ return false; },
    },
    getter);
}

bool CheckBox::hasBoolSetter(const CheckBox::BoolSetter &setter)
{
    return std::visit(VarDispatcher
    {
        [](const std::function<void(          bool)> &func){ return !!func;  },
        [](const std::function<void(Widget *, bool)> &func){ return !!func;  },
        [](const auto &){ return false; },
    },
    setter);
}

bool CheckBox::hasTriggerFunc(const CheckBox::TriggerFunc &trigger)
{
    return std::visit(VarDispatcher
    {
        [](const std::function<void(          bool)> &func){ return !!func;  },
        [](const std::function<void(Widget *, bool)> &func){ return !!func;  },
        [](const auto &){ return false; },
    },
    trigger);
}

CheckBox::CheckBox(CheckBox::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .parent = std::move(args.parent),
      }}

    , m_color(std::move(args.color))

    , m_valGetter  (std::move(args.getter  ))
    , m_valSetter  (std::move(args.setter  ))
    , m_valOnChange(std::move(args.onChange))

    , m_img
      {{
          .dir = DIR_NONE,

          .x = [this]{ return w() / 2; },
          .y = [this]{ return h() / 2; },

          .texLoadFunc = []{ return g_progUseDB->retrieve(0X00000480); },
          .parent{this},
      }}

    , m_box
      {{
          .w = [this]{ return w(); },
          .h = [this]{ return h(); },

          .drawFunc = [this](int drawDstX, int drawDstY)
          {
              // +----1----+
              // |        ||
              // 4        52
              // |        ||
              // |----6---+|
              // +----3----+

              const auto  solidColor = Widget::evalU32(m_color, this);
              const auto shadowColor = colorf::maskRGB(solidColor) + colorf::A_SHF(colorf::A(solidColor) / 2);

              g_glDevice->drawLine( solidColor, drawDstX +       0, drawDstY +       0, drawDstX + w() - 1, drawDstY +       0); // 1
              g_glDevice->drawLine( solidColor, drawDstX + w() - 1, drawDstY +       0, drawDstX + w() - 1, drawDstY + h() - 1); // 2
              g_glDevice->drawLine( solidColor, drawDstX +       0, drawDstY + h() - 1, drawDstX + w() - 1, drawDstY + h() - 1); // 3
              g_glDevice->drawLine( solidColor, drawDstX +       0, drawDstY +       0, drawDstX +       0, drawDstY + h() - 1); // 4
              g_glDevice->drawLine(shadowColor, drawDstX + w() - 2, drawDstY +       0, drawDstX + w() - 2, drawDstY + h() - 2); // 5
              g_glDevice->drawLine(shadowColor, drawDstX +       0, drawDstY + h() - 2, drawDstX + w() - 2, drawDstY + h() - 2); // 6
          },

          .parent{this},
      }}
{
    m_img.setShow([this]{ return getter(); });
    setSize([argW = std::move(args.w), this]{ return Widget::evalSizeOpt(argW, this, [this]{ return std::max<int>({m_img.w(), m_img.h(), 16}); }); },
            [argH = std::move(args.h), this]{ return Widget::evalSizeOpt(argH, this, [this]{ return std::max<int>({m_img.w(), m_img.h(), 16}); }); });
}

bool CheckBox::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return consumeFocus(false);
    }

    if(!valid){
        return consumeFocus(false);
    }

    switch(event.type){
        case MIR_EVENT_MOUSE_BUTTON_UP:
            {
                return consumeFocus(m.in(to_d(event.button.x), to_d(event.button.y)));
            }
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
            {
                if(m.in(to_d(event.button.x), to_d(event.button.y))){
                    setFocus(true);
                    toggle();
                    return true;
                }
                else{
                    return consumeFocus(false);
                }
            }
        case MIR_EVENT_MOUSE_MOTION:
            {
                return consumeFocus(m.in(to_d(event.motion.x), to_d(event.motion.y)));
            }
        case MIR_EVENT_KEY_UP:
            {
                return consumeFocus(focus());
            }
        case MIR_EVENT_KEY_DOWN:
            {
                if(focus()){
                    switch(event.key.key){
                        case MIRK_SPACE:
                        case MIRK_RETURN:
                            {
                                setFocus(true);
                                toggle();
                                return true;
                            }
                        default:
                            {
                                return true;
                            }
                    }
                }
                else{
                    return false;
                }
            }
        default:
            {
                return false;
            }
    }
}

void CheckBox::toggle()
{
    const bool value = !getter();
    setter(value);

    CheckBox::evalTriggerFunc(m_valOnChange, this, value);
}

bool CheckBox::getter() const
{
    return CheckBox::evalBoolGetter(m_valGetter, this);
}

void CheckBox::setter(bool value)
{
    CheckBox::evalBoolSetter(m_valSetter, this, value);
}

bool CheckBox::rawGetter() const
{
    return m_innVal;
}

void CheckBox::rawSetter(bool value)
{
    m_innVal = value;
}

// ===== checklabel.cpp =====

CheckLabel::CheckLabel(CheckLabel::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .parent = std::move(args.parent),
      }}

    , m_box
      {{
          .w = std::move(args.box.w),
          .h = std::move(args.box.h),

          .color = [bcolor = std::move(args.box.color), this] -> uint32_t
          {
              if(const auto color = Widget::evalU32(bcolor, this); m_hoverColor){
                  return colorf::modRGBA(color, colorf::RGBA(0XFF, 0, 0, 0XFF));
              }
              else{
                  return color;
              }
          },

          .getter = [getter = std::move(args.getter), this] -> CheckLabel::BoolGetter
          {
              if(CheckLabel::hasBoolGetter(getter)){
                  return [getter = std::move(getter), this]
                  {
                      return CheckLabel::evalBoolGetter(getter, this);
                  };
              }
              else return nullptr;
          }(),

          .setter = [setter = std::move(args.setter), this] -> CheckLabel::BoolSetter
          {
              if(CheckLabel::hasBoolSetter(setter)){
                  return [setter = std::move(setter), this](bool value) mutable
                  {
                      CheckLabel::evalBoolSetter(setter, this, value);
                  };
                }
                else return nullptr;
          }(),

          .onChange = [trigger = std::move(args.onChange), this] -> CheckLabel::TriggerFunc
          {
              if(CheckLabel::hasTriggerFunc(trigger)){
                  return [trigger = std::move(trigger), this](bool value) mutable
                  {
                      CheckLabel::evalTriggerFunc(trigger, this, value);
                  };
              }
              else return nullptr;
          }(),

          .parent{this},
      }}

    , m_label
      {{
          .label = args.label.text,
          .font
          {
              .id = args.label.font.id,
              .size = args.label.font.size,
              .style = args.label.font.style,

              .color = [fcolor = std::move(args.label.font.color), this] -> uint32_t
              {
                  if(const auto color = Widget::evalU32(fcolor, this); m_hoverColor){
                      return colorf::modRGBA(color, colorf::RGBA(0XFF, 0, 0, 0XFF));
                  }
                  else{
                      return color;
                  }
              },
              .bgColor = std::move(args.label.font.bgColor),
          },
          .parent{this},
      }}
{
    setSize([gap = std::move(args.gap), this]
    {
        return m_box.w() + Widget::evalSizeOpt(gap, this, [this]{ return m_box.w() / 2; }) + m_label.w();
    },

    [this]
    {
        return std::max<int>(m_box.h(), m_label.h());
    });

    if(args.boxFirst){
        m_box  .moveAt(DIR_LEFT ,       0, h() / 2);
        m_label.moveAt(DIR_RIGHT, w() - 1, h() / 2);
    }
    else{
        m_label.moveAt(DIR_LEFT ,       0, h() / 2);
        m_box  .moveAt(DIR_RIGHT, w() - 1, h() / 2);
    }
}

bool CheckLabel::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    if(!valid){
        m_hoverColor = false;
        return consumeFocus(false);
    }


    switch(event.type){
        case MIR_EVENT_MOUSE_MOTION:
        case MIR_EVENT_MOUSE_BUTTON_UP:
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
            {
                m_hoverColor = m.in(GLDeviceHelper::getEventPLoc(event).value());
                break;
            }
        default:
            {
                break;
            }
    }

    if(m_box.processEvent(event, valid, m.create(m_box.roi()))){
        return true;
    }

    switch(event.type){
        case MIR_EVENT_MOUSE_BUTTON_UP:
            {
                if(m.in(to_d(event.button.x), to_d(event.button.y))){
                    return consumeFocus(true, &m_box);
                }
                else{
                    return consumeFocus(false);
                }
            }
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
            {
                if(m.in(to_d(event.button.x), to_d(event.button.y))){
                    m_box.toggle();
                    return consumeFocus(true, &m_box);
                }
                else{
                    return consumeFocus(false);
                }
            }
        case MIR_EVENT_MOUSE_MOTION:
            {
                if(m.in(to_d(event.motion.x), to_d(event.motion.y))){
                    return consumeFocus(true, &m_box);
                }
                else{
                    return consumeFocus(false);
                }
            }
        case MIR_EVENT_KEY_UP:
            {
                return consumeFocus(focus());
            }
        case MIR_EVENT_KEY_DOWN:
            {
                if(focus()){
                    switch(event.key.key){
                        case MIRK_SPACE:
                        case MIRK_RETURN:
                            {
                                m_box.toggle();
                                return consumeFocus(true, &m_box);
                            }
                        default:
                            {
                                return true;
                            }
                    }
                }
                else{
                    return false;
                }
            }
        default:
            {
                return false;
            }
    }
}

// ===== radioselector.cpp =====

extern PNGTexDB *g_progUseDB;

RadioSelector::RadioSelector(Widget::VarDir argDir,

        Widget::VarInt argX,
        Widget::VarInt argY,

        int argGap,
        int argItemSpace,

        std::initializer_list<std::tuple<Widget *, bool>> argWidgetList,

        std::function<const Widget *(const Widget *                )> argValGetter,
        std::function<void          (      Widget *, Widget *      )> argValSetter,
        std::function<void          (      Widget *, Widget *, bool)> argValOnChange,

        Widget * argParent,
        bool     argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),
          .w = std::nullopt,
          .h = std::nullopt,

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , m_gap(std::max<int>(0, argGap))
    , m_itemSpace(std::max<int>(0, argItemSpace))

    , m_valGetter(std::move(argValGetter))
    , m_valSetter(std::move(argValSetter))
    , m_valOnChange(std::move(argValOnChange))

    , m_imgOff  {{.w=16, .h=16, .texLoadFunc=[](const Widget *){ return g_progUseDB->retrieve(0X00000370); }, .modColor=colorf::WHITE_A255}}
    , m_imgOn   {{.w=16, .h=16, .texLoadFunc=[](const Widget *){ return g_progUseDB->retrieve(0X00000370); }, .modColor=colorf::  RED_A255}}
    , m_imgDown {{.w=16, .h=16, .texLoadFunc=[](const Widget *){ return g_progUseDB->retrieve(0X00000371); }, .modColor=colorf::WHITE_A255}}

{
    for(auto [widget, autoDelete]: argWidgetList){
        append(widget, autoDelete);
    }
}

void RadioSelector::append(Widget *widget, bool autoDelete)
{
    auto button = new RadioSelector::InternalRadioButton
    {{
        .gfxList
        {
            &m_imgOff,
            &m_imgOn,
            &m_imgDown,
        },

        .onTrigger = [this](Widget *selfButton, int)
        {
            setter(getRadioWidget(selfButton));
            foreachRadioButton([selfButton, this](Widget *button)
            {
                if(selfButton != button){
                    dynamic_cast<TrigfxButton *>(button)->setOff();
                }

                if(m_valOnChange){
                    m_valOnChange(this, getRadioWidget(button), selfButton == button);
                }
            });
        },

        .onClickDone = false,
        .radioMode = true,

        .attrs
        {
            .data = std::make_any<Widget *>(widget),
        },
        .parent{this},
    }};

    if(getter() == widget){
        button->setDown();
    }
    else{
        button->setOff();
    }

    const auto startX = 0;
    const auto startY = (hasChild() ? (h() + m_itemSpace) : 0) + std::max<int>(button->h(), widget->h()) / 2;

    addChildAt(button, DIR_LEFT, startX                      , startY, true);
    addChildAt(widget, DIR_LEFT, startX + button->w() + m_gap, startY, autoDelete);
}

const Widget *RadioSelector::getter() const
{
    if(m_valGetter){
        return m_valGetter(this);
    }
    else{
        return m_selected;
    }
}

void RadioSelector::setter(Widget *selected)
{
    if(!m_valGetter){
        m_selected = selected;
    }

    if(m_valSetter){
        m_valSetter(this, selected);
    }
}

// ===== valueselector.cpp =====

extern GLDevice *g_glDevice;

ValueSelector::ValueSelector(ValueSelector::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .w = 0,
          .h = std::move(args.h),

          .parent = std::move(args.parent),
      }}

    , m_input
      {{
          .w = std::move(args.input.w),
          .h = [this]{ return h(); },

          .enableIME = std::move(args.input.enableIME),

          .font   = std::move(args.input.font),
          .cursor = std::move(args.input.cursor),
      }}

    , m_up
      {{
          .w = std::move(args.button.w),
          .h = [this]{ return h() / 2; },

          .triangle
          {
              .dir = DIR_UP,

              .w = [](const Widget *self){ return self->w() / 2; },
              .h = [](const Widget *self){ return self->h() / 2; },

              .color = colorf::GREY_A255,
          },

          .frame
          {
              .color = colorf::GREY_A255,
          },

          .onTrigger = std::move(args.upTrigger),
      }}

    , m_down
      {{
          .w = [this]{ return       m_up.w(); },
          .h = [this]{ return h() - m_up.h(); },

          .triangle
          {
              .dir = DIR_DOWN,

              .w = [](const Widget *self){ return self->w() / 2; },
              .h = [](const Widget *self){ return self->h() / 2; },

              .color = colorf::GREY_A255,
          },

          .frame
          {
              .color = colorf::GREY_A255,
          },

          .onTrigger = std::move(args.downTrigger),
      }}

    , m_vflex
      {{
          .childList
          {
              {&m_up  , false},
              {&m_down, false},
          },
      }}

    , m_hflex
      {{
          .v = false,
          .headSpace = 2,

          .childList
          {
              {&m_input, false},
              {&m_vflex, false},
          },

          .parent{this},
      }}

    , m_frame
      {{
          .w = [this]{ return m_hflex.w(); },
          .h = [this]{ return m_hflex.h(); },

          .drawFunc = [](const Widget *self, int dstDrawX, int dstDrawY)
          {
              g_glDevice->drawRectangle(colorf::GREY_A255, dstDrawX, dstDrawY, self->w(), self->h());
          },

          .parent{this},
      }}
{
    setW([this]{ return m_hflex.w(); });
}

// ===== integerselector.cpp =====
IntegerSelector::IntegerSelector(IntegerSelector::InitArgs args)
    : ValueSelector
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),
          .h = std::move(args.h),

          .input
          {
              .w = std::move(args.input.w),
              .font = std::move(args.input.font),
              .cursor = std::move(args.input.cursor),
              .onChange = std::move(args.input.onChange),
              .validate = std::move(args.input.validate),
          },

          .button
          {
              .w = std::move(args.button.w),
          },

          .upTrigger = [r = args.range, tgr = std::move(args.upTrigger), this](int clicks)
          {
              const auto [_, max] = Widget::evalGetter(r, this);
              const auto val = std::stoll(getValue());

              if(val < max){
                  setValue(std::to_string(val + 1).c_str());
                  Button::evalTriggerCBFunc(tgr, this, clicks);
              }
              else if(val > max){
                  setValue(std::to_string(max).c_str());
                  Button::evalTriggerCBFunc(tgr, this, clicks);
              }
          },

          .downTrigger = [r = args.range, tgr = std::move(args.downTrigger), this](int clicks)
          {
              const auto [min, _] = Widget::evalGetter(r, this);
              const auto val = std::stoll(getValue());

              if(val > min){
                  setValue(std::to_string(val - 1).c_str());
                  Button::evalTriggerCBFunc(tgr, this, clicks);
              }
              else if(val < min){
                  setValue(std::to_string(min).c_str());
                  Button::evalTriggerCBFunc(tgr, this, clicks);
              }
          },

          .parent = std::move(args.parent),
      }}
{
    setValue(std::to_string(Widget::evalGetter(args.range, this).first).c_str());
}

std::optional<int> IntegerSelector::getInt() const
{
    if(const auto s = getValue(); s.empty()){
        return std::nullopt;
    }
    else{
        try{
            return std::stoi(s);
        }
        catch(...){
            throw fflpanic("invalid integer string: {}", to_cstr(s));
        }
    }
}

// ===== textinput.cpp =====

TextInput::TextInput(TextInput::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .parent = std::move(args.parent),
      }}

    , m_labelFirst(str_haschar(args.labelFirst) ? new LabelBoard
      {{
          .dir = DIR_LEFT,

          .x = 0,
          .y = [this]{ return h() / 2; },

          .label = args.labelFirst,
          .font = args.font, // no move
          .parent
          {
              .widget = this,
              .autoDelete = true,
          },
      }} : nullptr)

    , m_bg
      {{
          .dir = DIR_LEFT,

          .x = [firstGap = std::move(args.gapFirst), this]
          {
              if(m_labelFirst){
                  return m_labelFirst->dx() + m_labelFirst->w() + Widget::evalSize(firstGap, this);
              }
              else{
                  return 0;
              }
          },
          .y = [this]{ return h() / 2; },

          .v = false,
          .parent{this},
      }}

    , m_labelSecond(str_haschar(args.labelSecond) ? new LabelBoard
      {{
          .dir = DIR_LEFT,

          .x = [secondGap = std::move(args.gapSecond), this]
          {
              return m_bg.dx() + m_bg.w() + Widget::evalSize(secondGap, this);
          },
          .y = [this]{ return h() / 2; },

          .label = args.labelSecond,
          .font = args.font,
          .parent
          {
              .widget = this,
              .autoDelete = true,
          },
      }} : nullptr)

    , m_input
      {{
          .x = [this]{ return m_bg.dx() + m_bg.getInputROI().x; },
          .y = [this]{ return m_bg.dy() + m_bg.getInputROI().y; },
          .w = [this]{ return             m_bg.getInputROI().w; },
          .h = [this]{ return             m_bg.getInputROI().h; },

          .enableIME = std::move(args.enableIME),
          .font = std::move(args.font),

          .onTab = std::move(args.onTab),
          .onCR  = std::move(args.onCR),

          .parent{this},
      }}
{
    m_bg.setInputSize(std::move(args.inputSize));
    setSize([this]
    {
        if(m_labelSecond){
            return m_labelSecond->dx() + m_labelSecond->w();
        }
        else{
            return m_bg.dx() + m_bg.w();
        }
    },

    [this]
    {
        return std::max<int>(
        {
            m_labelFirst  ? m_labelFirst ->h() : 0,
            m_labelSecond ? m_labelSecond->h() : 0,

            m_bg.h(),
            m_input.h(), // no need since it's inside m_bg
        });
    });
}

// ===== textshadowboard.cpp =====
#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
TextShadowBoard::TextShadowBoard(TextShadowBoard::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .w = std::nullopt,
          .h = std::nullopt,

          .parent = std::move(args.parent),
      }}

    , m_textShadow
      {{
          .x = std::move(args.shadowX),
          .y = std::move(args.shadowY),

          .textFunc = args.textFunc, // do NOT move
          .font
          {
              .id    = args.font.id,
              .size  = args.font.size,
              .style = args.font.style,
              .color = std::move(args.shadowColor),
          },

          .blendMode = args.blendMode,
          .parent{this},
      }}

    , m_text
      {{
          .textFunc = std::move(args.textFunc),
          .font     = std::move(args.font),

          .blendMode = std::move(args.blendMode),
          .parent{this},
      }}
{}
#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif

// ===== itembox.cpp =====
ItemBox::ItemBox(ItemBox::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .w = std::nullopt,
          .h = std::nullopt,

          .childList
          {
              {
                  .widget = new Widget
                  {{
                      .attrs
                      {
                          .inst
                          {
                              .name = "Canvas",
                              .moveOnFocus = false,
                          },
                      },
                  }},
                  .autoDelete = true,
              },
          },

          .attrs
          {
              .type
              {
                  .setSize = false,
                  .addChild = false,
                  .removeChild = false,
              },
          },

          .parent = std::move(args.parent),
      }}

    , m_vbox(args.v)
    , m_align(args.align)
    , m_canvas(firstChild())

    , m_headSpace(std::move(args.headSpace))
    , m_itemSpace(std::move(args.itemSpace))
    , m_tailSpace(std::move(args.tailSpace))
    , m_fixedEdgeSize(std::move(args.fixed))
{
    m_canvas->setSize([this]{ return  m_vbox ? m_fixedEdgeSizeEval : (m_headSpaceEval + m_flexibleEdgeSizeEval + m_tailSpaceEval); },
                      [this]{ return !m_vbox ? m_fixedEdgeSizeEval : (m_headSpaceEval + m_flexibleEdgeSizeEval + m_tailSpaceEval); });

    for(auto [widget, autoDelete]: args.childList){
        if(widget){
            m_canvas->addChild(new MarginContainer
            {{
                .dir = [this]
                {
                    switch(m_align){
                        case ItemAlign::UPLEFT   : return          DIR_UPLEFT  ;
                        case ItemAlign::DOWNRIGHT: return m_vbox ? DIR_UPRIGHT : DIR_LEFTDOWN;
                        case ItemAlign::CENTER   : return m_vbox ? DIR_UP      : DIR_LEFT;
                        default: std::unreachable();
                    }
                }(),

                .contained
                {
                    .dir = DIR_LEFT,
                    .widget = widget,
                    .autoDelete = autoDelete,
                },
            }},

            true);
        }
    }

    buildLayout();
}

void ItemBox::addItem(Widget *argWidget, bool argAutoDelete)
{
    if(!argWidget){
        return;
    }

    const auto widgetW = argWidget->w();
    const auto widgetH = argWidget->h();

    const auto widgetFlexEdge  = m_vbox ? widgetH : widgetW;
    const auto widgetFixedEdge = m_vbox ? widgetW : widgetH;

    if(hasShowItem()){
        m_flexibleEdgeSizeEval += m_itemSpaceEval; // update to new widget start position
    }
    else{
        fflassert(m_flexibleEdgeSizeEval == 0);
    }

    if(!m_fixedEdgeSize.has_value() && (widgetFixedEdge > m_fixedEdgeSizeEval)){
        m_fixedEdgeSizeEval = widgetFixedEdge;
    }

    dir8_t d {};
    int    x {};
    int    y {};

    switch(m_align){
        case ItemAlign::UPLEFT:
            {
                d = DIR_UPLEFT;
                x =  m_vbox ? 0 : m_headSpaceEval + m_flexibleEdgeSizeEval;
                y = !m_vbox ? 0 : m_headSpaceEval + m_flexibleEdgeSizeEval;

                break;
            }
        case ItemAlign::DOWNRIGHT:
            {
                d =  m_vbox ? DIR_UPRIGHT : DIR_DOWNLEFT;
                x =  m_vbox ? m_fixedEdgeSizeEval - 1 : m_headSpaceEval + m_flexibleEdgeSizeEval;
                y = !m_vbox ? m_fixedEdgeSizeEval - 1 : m_headSpaceEval + m_flexibleEdgeSizeEval;

                break;
            }
        case ItemAlign::CENTER:
            {
                d =  m_vbox ? DIR_UP : DIR_LEFT;
                x =  m_vbox ? m_fixedEdgeSizeEval / 2 : m_headSpaceEval + m_flexibleEdgeSizeEval;
                y = !m_vbox ? m_fixedEdgeSizeEval / 2 : m_headSpaceEval + m_flexibleEdgeSizeEval;

                break;
            }
        default:
            {
                std::unreachable();
            }
    }

    m_flexibleEdgeSizeEval += widgetFlexEdge; // update to new widget end position
    m_canvas->addChild(new MarginContainer
    {{
        .dir = d,

        .x = x,
        .y = y,

        .w = widgetW,
        .h = widgetH,

        .contained
        {
            .dir = DIR_LEFT,
            .widget = argWidget,
            .autoDelete = argAutoDelete,
        },

        .attrs
        {
            .name = "ItemContainer",
        },
    }},

    true);
}

void ItemBox::removeItem(uint64_t argItemID, bool argTriggerAutoDelete)
{
    if(!argItemID){
        return;
    }

    MarginContainer *container = nullptr;
    m_canvas->foreachChild([argItemID, &container](Widget *child, bool) -> bool
    {
        if(auto mc = dynamic_cast<MarginContainer *>(child)){
            if(mc->contained()->id() == argItemID){
                container = mc;
            }
        }
        return container;
    });

    if(!container){
        return;
    }

    container->clearContained(argTriggerAutoDelete);
    doRemoveContainer(container);
}

bool ItemBox::hasShowItem() const
{
    return lastShowContainer();
}

void ItemBox::flipItemShow(uint64_t childID)
{
    if(auto child = m_canvas->hasDescendant(childID)){
        if(auto container = dynamic_cast<MarginContainer *>(child->parent())){

            bool needUpdateFixedEdgeSize   = false;
            bool needUpdateFixedEdgeOffset = false;

            if(container->localShow()){
                if(!m_fixedEdgeSize.has_value() && (m_vbox ? container->w() : container->h()) >= m_fixedEdgeSizeEval){ // actually cannot be greater
                    needUpdateFixedEdgeSize = true;
                    needUpdateFixedEdgeOffset = true;
                }
            }
            else{
                if(!m_fixedEdgeSize.has_value()){
                    if(const auto fixedEdge = m_vbox ? container->w() : container->h(); fixedEdge > m_fixedEdgeSizeEval){
                        m_fixedEdgeSizeEval = fixedEdge;
                        needUpdateFixedEdgeOffset = true;
                    }
                }
            }

            container->flipShow();
            if(needUpdateFixedEdgeSize){
                updateFixedEdgeSize();
            }

            if(needUpdateFixedEdgeOffset && (m_align != ItemAlign::UPLEFT)){
                updateFixedEdgeOffset();
            }

            updateFlexEdgeOffset(container);
        }
    }
}

void ItemBox::buildLayout()
{
    m_headSpaceEval = Widget::evalSize(m_headSpace, this);
    m_itemSpaceEval = Widget::evalSize(m_itemSpace, this);
    m_tailSpaceEval = Widget::evalSize(m_tailSpace, this);

    if(const auto firstShow = firstShowContainer()){
        updateMarginContainers();

        updateFlexEdgeOffset(firstShow);
        updateFlexEdgeSize(); // side determined by offset

        updateFixedEdgeSize();
        updateFixedEdgeOffset(); // offset determined by size
    }
    else{
        if(m_fixedEdgeSize.has_value()){
            m_fixedEdgeSizeEval = Widget::evalSize(m_fixedEdgeSize.value(), this);
        }
        else{
            m_fixedEdgeSizeEval = 0;
        }
        m_flexibleEdgeSizeEval = 0;
    }
}

void ItemBox::updateMarginContainers()
{
    m_canvas->foreachChild([](Widget *child, bool)
    {
        if(child->localShow()){
            if(auto mc = dynamic_cast<MarginContainer *>(child)){
                mc->setSize(mc->contained()->w(), mc->contained()->h());
            }
        }
    });
}

void ItemBox::updateFlexEdgeSize()
{
    if(const auto lastShow = lastShowContainer()){
        if(m_vbox) m_flexibleEdgeSizeEval = lastShow->dy() + lastShow->h();
        else       m_flexibleEdgeSizeEval = lastShow->dx() + lastShow->w();
    }
    else{
        m_flexibleEdgeSizeEval = 0;
    }
}

void ItemBox::updateFixedEdgeSize()
{
    // always re-evaluate fixed edge size
    // if m_fixedEdgeSize has value, don't call this function except in buildLayout(), which re-evaluates everything

    if(m_fixedEdgeSize.has_value()){
        m_fixedEdgeSizeEval = Widget::evalSize(m_fixedEdgeSize.value(), this);
    }
    else{
        m_fixedEdgeSizeEval = 0;
        m_canvas->foreachChild([this](const Widget *child, bool)
        {
            if(child->localShow()){
                m_fixedEdgeSizeEval = std::max<int>(m_fixedEdgeSizeEval, m_vbox ? child->w() : child->h());
            }
        });
    }
}

void ItemBox::updateFlexEdgeOffset(const Widget *container)
{
    fflassert(container);
    fflassert(m_canvas->hasChild(container->id()));

    bool found = false;
    Widget *lastShow = nullptr;

    m_canvas->foreachChild([container, &found, &lastShow, this](Widget *child, bool)
    {
        if(child == container){
            found = true;
        }

        if(found){
            if(child->localShow()){
                if(m_vbox) child->moveYTo(lastShow ? (lastShow->dy() + lastShow->h() + m_itemSpaceEval) : 0);
                else       child->moveXTo(lastShow ? (lastShow->dx() + lastShow->w() + m_itemSpaceEval) : 0);
            }
        }

        if(child->localShow()){
            lastShow = child;
        }
    });
}

void ItemBox::updateFixedEdgeOffset()
{
    m_canvas->foreachChild([this](Widget *child, bool)
    {
        switch(m_align){
            case ItemAlign::UPLEFT:
                {
                    if(m_vbox) child->moveXTo(0);
                    else       child->moveYTo(0);

                    break;
                }
            case ItemAlign::DOWNRIGHT:
                {
                    if(m_vbox) child->moveXTo(m_fixedEdgeSizeEval - 1);
                    else       child->moveYTo(m_fixedEdgeSizeEval - 1);

                    break;
                }
            case ItemAlign::CENTER:
                {
                    if(m_vbox) child->moveXTo(m_fixedEdgeSizeEval / 2);
                    else       child->moveYTo(m_fixedEdgeSizeEval / 2);

                    break;
                }
            default:
                {
                    std::unreachable();
                }
        }
    });
}

const Widget *ItemBox::findShowContainer(bool forward) const
{
    const Widget *container = nullptr;
    m_canvas->foreachChild(forward, [&container](const Widget *child, bool) -> bool
    {
        if(child->localShow()){
            container = child;
        }
        return container;
    });
    return container;
}

void ItemBox::doRemoveContainer(const MarginContainer *container)
{
    if(!container){
        return;
    }

    fflassert(!container->contained());
    fflassert(m_canvas->hasChild(container->id()));

    const auto widgetShow = container->localShow();
    const auto nextContainer = m_canvas->nextChild(container->id());

    const auto widgetW = widgetShow ? container->w() : 0;
    const auto widgetH = widgetShow ? container->h() : 0;

    const auto widgetFlexEdge  = m_vbox ? widgetH : widgetW;
    const auto widgetFixedEdge = m_vbox ? widgetW : widgetH;

    m_canvas->removeChild(container->id(), true);

    if(!widgetShow){
        return;
    }

    if(hasShowItem()){
        m_flexibleEdgeSizeEval -= widgetFlexEdge;
        m_flexibleEdgeSizeEval -= m_itemSpaceEval;

        if(nextContainer){
            updateFlexEdgeOffset(nextContainer);
        }

        if(!m_fixedEdgeSize.has_value() && (widgetFixedEdge >= m_fixedEdgeSizeEval)){
            updateFixedEdgeSize();
            updateFixedEdgeOffset();
        }
    }
    else{
        if(!m_fixedEdgeSize.has_value()){
            m_fixedEdgeSizeEval = 0;
        }
        m_flexibleEdgeSizeEval = 0;
    }
}

// ===== menuboard.cpp =====

extern GLDevice *g_glDevice;
MenuBoard::MenuBoard(MenuBoard::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .w = std::nullopt,
          .h = std::nullopt,

          .childList
          {
              {
                  .widget = new MarginWrapper
                  {{
                      .margin = std::move(args.margin),
                      .bgDrawFunc = [corner = args.corner](const Widget *self, int dstDrawX, int dstDrawY)
                      {
                          const int w = self->w();
                          const int h = self->h();
                          const int c = Widget::evalSize(corner, self);

                          g_glDevice->fillRectangle(colorf::BLACK_A255, dstDrawX, dstDrawY, w, h, c);
                          g_glDevice->drawRectangle(colorf:: GREY_A255, dstDrawX, dstDrawY, w, h, c);
                      },
                  }},

                  .autoDelete = true,
              },
          },

          .attrs
          {
              .type
              {
                  .setSize  = false,
                  .addChild = false,
              },

              .inst = std::move(args.attrs),
          },

          .parent = std::move(args.parent),
      }}

    , m_itemSpace     (std::move(args.itemSpace))
    , m_separatorSpace(std::move(args.separatorSpace))

    , m_onClickMenu(std::move(args.onClick))
    , m_canvas
      {{
          .fixed = std::move(args.fixed),
      }}
{
    dynamic_cast<MarginWrapper *>(firstChild())->setWrapped(&m_canvas, false);
    for(auto &addItemArgs: args.itemList){
        addMenu(std::move(addItemArgs));
    }
}

void MenuBoard::addMenu(MenuBoard::AddItemArgs args)
{
    if(!args.gfxWidget.widget){
        return;
    }

    int addItemWidth = args.gfxWidget.widget->w();
    int maxItemWidth = 0;

    m_canvas.foreachItem([&maxItemWidth](const Widget *widget, bool)
    {
        if(auto item = dynamic_cast<const MenuItem *>(widget); item->localShow()){
            maxItemWidth = std::max<int>(maxItemWidth, item->getItemWidth());
        }
    });

    m_canvas.addItem(new MenuItem
    {{
        .margin
        {
            .up = [this](const Widget *self)
            {
                if(self == m_canvas.firstChild()){
                    return 0;
                }
                else{
                    return Widget::evalSize(m_itemSpace, this) / 2;
                }
            },

            .down = [addSep = std::move(args.showSeparator), this](const Widget *self)
            {
                if(self == m_canvas.lastChild()){
                    return 0;
                }
                else if(Widget::evalBool(addSep, self)){
                    return (Widget::evalSize(m_itemSpace, this) + 1) / 2 + Widget::evalSize(m_separatorSpace, this);
                }
                else{
                    return (Widget::evalSize(m_itemSpace, this) + 1) / 2;
                }
            },
        },

        .itemSize
        {
            .w = std::max<int>(addItemWidth, maxItemWidth),

            // don't use
            //
            //      .w = [this]{ return m_canvas.w(); },
            //
            // here, because m_canvas.w() is 0 at this moment
            // if add menu in this way, it causes all added MenuItem::itemSize as 0
        },

        .gfxWidget = std::move(args.gfxWidget),
        .subWidget
        {
            .dir = DIR_UPRIGHT,
            .widget = args.subWidget.widget,
            .autoDelete = args.subWidget.autoDelete,
        },

        .showIndicator = std::move(args.showIndicator),
        .showSeparator = [showSep = std::move(args.showSeparator), this](const Widget *self)
        {
            return Widget::evalBool(showSep, this) && (self != m_canvas.lastChild());
        },

        .expandOnHover = true,

        .bgColor = colorf::GREY + colorf::A_SHF(128),
        .onClick = [itemCB = m_onClickMenu, this](Widget *widget)
        {
            Menu::evalClickCBFunc(itemCB, widget); // widget is gfxWidget
            flipShow();
        },
    }},

    true);

    // ItemBox can not provide the ability that:
    //
    //    adding a new item can resize all previously added items if needed
    //    items in ItemBox are independent
    //
    // this needs explicit implementation here

    if(addItemWidth > maxItemWidth){
        m_canvas.foreachItem([addItemWidth](Widget *widget, bool)
        {
            if(auto item = dynamic_cast<MenuItem *>(widget); item->localShow()){
                item->setItemWidth(addItemWidth);
            }
        });
        m_canvas.buildLayout();
    }
}

// ===== pullmenu.cpp =====

extern PNGTexDB *g_progUseDB;
PullMenu::PullMenu(PullMenu::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .w = std::nullopt,
          .h = std::nullopt,

          .attrs
          {
              .inst
              {
                  .moveOnFocus = false,
              },
          },

          .parent = std::move(args.parent),
      }}

    , m_tips
      {{
          .label = args.label.text,
      }}

    , m_tipsCrop
      {{
          .getter = &m_tips,
          .vr
          {
              [wo = std::move(args.label.w), this]{ return Widget::evalSizeOpt(wo, this, [this]{ return m_tips.w(); }); },
              [ho = std::move(args.label.h), this]{ return Widget::evalSizeOpt(ho, this, [this]{ return m_tips.h(); }); },
          },
      }}

    , m_title
      {{
          .label = args.title.text,
      }}

    , m_titleBg
      {{
          .w = [wo = std::move(args.title.w), this]{ return Widget::evalSizeOpt(wo, this, [this]{ return m_title.w() + TexInputBackground::borderSize(false).w; }); },
          .h = [ho = std::move(args.title.h), this]{ return Widget::evalSizeOpt(ho, this, [this]{ return m_title.h() + TexInputBackground::borderSize(false).h; }); },
          .v = false,
      }}

    , m_imgOff {{.w = 22, .h = 22, .texLoadFunc = []{ return g_progUseDB->retrieve(0X00000301); }, .rotate = 1}}
    , m_imgOn  {{.w = 22, .h = 22, .texLoadFunc = []{ return g_progUseDB->retrieve(0X00000300); }, .rotate = 1}}
    , m_imgDown{{.w = 22, .h = 22, .texLoadFunc = []{ return g_progUseDB->retrieve(0X00000302); }, .rotate = 1}}

    , m_button
      {{
          .gfxList
          {
              &m_imgOff,
              &m_imgOn,
              &m_imgDown,
          },

          .onTrigger = [this](int)
          {
              m_menuBoard.setFocus(!m_menuBoard.show());
              m_menuBoard.flipShow();
          },

          .attrs
          {
              .show = std::move(args.showButton),
          },
      }}

    , m_flex
      {{
          .v = false,
          .align = ItemAlign::CENTER,

          .itemSpace = 3,
          .childList
          {
              {&m_tipsCrop, false},
              {&m_titleBg , false},
              {&m_button  , false},
          },

          .parent{this},
      }}

    , m_menuBoard
      {{
          .fixed = std::move(args.menuFixed),
          .margin
          {
              5,
              5,
              5,
              5,
          },

          .corner = 3,
          .itemSpace = 6,
          .separatorSpace = 7,

          .itemList = std::move(args.itemList),
          .onClick  = std::move(args.onClick),
      }}

    , m_menuButton
      {{
          .x = [this]{ return m_titleBg.rdx(this) + m_titleBg.getInputROI().x; },
          .y = [this]{ return m_titleBg.rdy(this) + m_titleBg.getInputROI().y; },

          .itemSize
          {
              .w = [this]{ return m_titleBg.getInputROI().w; },
              .h = [this]{ return m_titleBg.getInputROI().h; },
          },

          .gfxWidget{&m_title},
          .subWidget{&m_menuBoard},

          .bgColor = colorf::GREY + colorf::A_SHF(64),
          .parent{this},
      }}
{}

void PullMenu::setFocus(bool argFocus)
{
    const auto oldFocus = focus();
    const auto newFocus = argFocus;

    Widget::setFocus(argFocus);

    if(oldFocus && !newFocus){
        m_menuBoard.setShow(false);
    }
}

// ===== gfxdebugboard.cpp =====

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

GfxDebugBoard::GfxDebugBoard(GfxDebugBoard::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .w = 400,
          .h = 300,

          .parent = std::move(args.parent),
      }}

    , m_bg
      {{
          .w = [this]{ return w(); },
          .h = [this]{ return h(); },

          .drawFunc = [this](int dstDrawX, int dstDrawY)
          {
              g_glDevice->fillRectangle(colorf::BLACK + colorf::A_SHF(0XF0), dstDrawX, dstDrawY, w(), h());
              g_glDevice->drawRectangle(colorf::WHITE + colorf::A_SHF(0X80), dstDrawX, dstDrawY, w(), h());
          },

          .parent{this},
      }}

    , m_img
      {{
          .texLoadFunc = []
          {
              return g_progUseDB->retrieve(0X18000001);
          },

          .hflip  = args.hflip,
          .vflip  = args.vflip,
          .rotate = args.rotate,
      }}

    , m_srcWidget
      {{
          .w = 150,
          .h = h(),
          .parent{this},
      }}

    , m_imgCanvas
      {{
          .x = 20,
          .y = 20,
          .w = [this]{ return m_srcWidget.w() - 40; },
          .h = 120,

          .parent{&m_srcWidget},
      }}

    , m_imgContainer
      {{
          .wrapped{&m_img},
          .attrs
          {
              .processEvent = [this](Widget *self, const MirEvent &event, bool valid, Widget::ROIMap m)
              {
                  if(!m.calibrate(self)){
                      return false;
                  }

                  if((event.type == MIR_EVENT_MOUSE_MOTION) && valid){
                      if((event.motion.state & MIR_BUTTON_LMASK) && (m.in(to_d(event.motion.x), to_d(event.motion.y)) || self->focus())){
                          self->moveBy(to_d(event.motion.xrel), to_d(event.motion.yrel), m_imgCanvas.roi());
                          return true;
                      }
                  }
                  return false;
              },
          },
          .parent{&m_imgCanvas},
      }}

    , m_imgFrame
      {{
          .w = [this]{ return m_imgCanvas.w(); },
          .h = [this]{ return m_imgCanvas.h(); },

          .drawFunc = [this](const Widget *self, int dstDrawX, int dstDrawY)
          {
              const int w = self->w();
              const int h = self->h();

              const float hr0 = m_cropHSlider_0.getValue();
              const float hr1 = m_cropHSlider_1.getValue();

              const float vr0 = m_cropVSlider_0.getValue();
              const float vr1 = m_cropVSlider_1.getValue();

              g_glDevice->drawRectangle(colorf::WHITE + colorf::A_SHF(0X80), dstDrawX, dstDrawY, w, h);

              g_glDevice->drawLine(colorf::RED_A255, dstDrawX + hr0 * w, dstDrawY, dstDrawX + hr0 * w, dstDrawY + h - 1);
              g_glDevice->drawLine(colorf::RED_A255, dstDrawX + hr1 * w, dstDrawY, dstDrawX + hr1 * w, dstDrawY + h - 1);

              g_glDevice->drawLine(colorf::BLUE_A255, dstDrawX, dstDrawY + vr0 * h, dstDrawX + w - 1, dstDrawY + vr0 * h);
              g_glDevice->drawLine(colorf::BLUE_A255, dstDrawX, dstDrawY + vr1 * h, dstDrawX + w - 1, dstDrawY + vr1 * h);
          },

          .parent{&m_imgCanvas},
      }}

    , m_imgResizeHSlider
      {{
          .bar
          {
              .x = [this]{ return m_imgCanvas.dx(); },
              .y = [this]{ return m_imgCanvas.dy(); },
              .w = [this]{ return m_imgCanvas. w(); },
              .h = 1,

              .v = false,
          },

          .slider
          {
              .w = 10,
              .h = 10,
          },

          .value = 0.5f,

          .sliderWidget
           {
              .dir = DIR_UPLEFT,
              .widget = new GfxShapeBoard
              {{
                  .w = 10,
                  .h = 10,

                  .drawFunc = [](const Widget *self, int dstDrawX, int dstDrawY)
                  {
                      g_glDevice->fillRectangle(colorf::WHITE + colorf::A_SHF(0X80), dstDrawX, dstDrawY, self->w(), self->h());
                  },
              }},
              .autoDelete = true,
           },

          .onChange = [this](float)
          {
              m_imgContainer.moveBy(0, 0, m_imgCanvas.roi());
          },

          .parent{&m_srcWidget},
      }}

    , m_imgResizeVSlider
      {{
          .bar
          {
              .x = [this]{ return m_imgCanvas.dx(); },
              .y = [this]{ return m_imgCanvas.dy(); },
              .w = 1,
              .h = [this]{ return m_imgCanvas.h(); },
          },

          .slider
          {
              .w = 10,
              .h = 10,
          },

          .value = 0.5f,

          .sliderWidget
           {
              .dir = DIR_UPLEFT,
              .widget = new GfxShapeBoard
              {{
                  .w = 10,
                  .h = 10,

                  .drawFunc = [](const Widget *self, int dstDrawX, int dstDrawY)
                  {
                      g_glDevice->fillRectangle(colorf::WHITE + colorf::A_SHF(0X80), dstDrawX, dstDrawY, self->w(), self->h());
                  },
              }},
              .autoDelete = true,
           },

          .onChange = [this](float)
          {
              m_imgContainer.moveBy(0, 0, m_imgCanvas.roi());
          },

          .parent{&m_srcWidget},
      }}

    , m_cropHSlider_0
      {{
          .bar
          {
              .x = [this]{ return m_imgCanvas.dx(); },
              .y = [this]{ return m_imgCanvas.dy() + m_imgCanvas.h() - 1; },
              .w = [this]{ return m_imgCanvas.w(); },
              .h = 1,
              .v = false,
          },

          .slider
          {
              .w = 10,
              .h = 10,
          },

          .value = 0.333f,

          .sliderWidget
           {
              .dir = DIR_UPLEFT,
              .widget = new GfxShapeBoard
              {{
                  .w = 10,
                  .h = 10,

                  .drawFunc = [](const Widget *self, int dstDrawX, int dstDrawY)
                  {
                      g_glDevice->fillRectangle(colorf::WHITE + colorf::A_SHF(0X80), dstDrawX, dstDrawY, self->w(), self->h());
                  },
              }},
              .autoDelete = true,
           },

           .parent{&m_srcWidget},
      }}

    , m_cropHSlider_1
      {{
          .bar
          {
              .x = [this]{ return m_imgCanvas.dx(); },
              .y = [this]{ return m_imgCanvas.dy() + m_imgCanvas.h() - 1; },
              .w = [this]{ return m_imgCanvas.w(); },
              .h = 1,
              .v = false,
          },

          .slider
          {
              .w = 10,
              .h = 10,
          },

          .value = 0.666f,

          .sliderWidget
           {
              .dir = DIR_UPLEFT,
              .widget = new GfxShapeBoard
              {{
                  .w = 10,
                  .h = 10,

                  .drawFunc = [](const Widget *self, int dstDrawX, int dstDrawY)
                  {
                      g_glDevice->fillRectangle(colorf::WHITE + colorf::A_SHF(0X80), dstDrawX, dstDrawY, self->w(), self->h());
                  },
              }},
              .autoDelete = true,
           },

           .parent{&m_srcWidget},
      }}

    , m_cropVSlider_0
      {{
          .bar
          {
              .x = [this]{ return m_imgCanvas.dx() + m_imgCanvas.w() - 1; },
              .y = [this]{ return m_imgCanvas.dy(); },
              .w = 1,
              .h = [this]{ return m_imgCanvas. h(); },
          },

          .slider
          {
              .w = 10,
              .h = 10,
          },

          .value = 0.333f,

          .sliderWidget
          {
             .dir = DIR_UPLEFT,
             .widget = new GfxShapeBoard
             {{
                 .w = 10,
                 .h = 10,

                 .drawFunc = [](const Widget *self, int dstDrawX, int dstDrawY)
                 {
                     g_glDevice->fillRectangle(colorf::WHITE + colorf::A_SHF(0X80), dstDrawX, dstDrawY, self->w(), self->h());
                 },
             }},
             .autoDelete = true,
          },

          .parent{&m_srcWidget},
      }}

    , m_cropVSlider_1
      {{
          .bar
          {
              .x = [this]{ return m_imgCanvas.dx() + m_imgCanvas.w() - 1; },
              .y = [this]{ return m_imgCanvas.dy(); },
              .w = 1,
              .h = [this]{ return m_imgCanvas. h(); },
          },

          .slider
          {
              .w = 10,
              .h = 10,
          },

          .value = 0.666f,

          .sliderWidget
          {
             .dir = DIR_UPLEFT,
             .widget = new GfxShapeBoard
             {{
                 .w = 10,
                 .h = 10,

                 .drawFunc = [](const Widget *self, int dstDrawX, int dstDrawY)
                 {
                     g_glDevice->fillRectangle(colorf::WHITE + colorf::A_SHF(0X80), dstDrawX, dstDrawY, self->w(), self->h());
                 },
             }},
             .autoDelete = true,
          },

          .parent{&m_srcWidget},
      }}

    , m_texSize
      {{
          .x = [this]{ return m_imgCanvas.dx(); },
          .y = [this]{ return m_imgCanvas.dy() + m_imgCanvas.h() + 20; },

          .textFunc = [this]
          {
              return str_printf("TEX (%d, %d)", GLDeviceHelper::getTextureWidth(m_img.getTexture(), 0), GLDeviceHelper::getTextureHeight(m_img.getTexture(), 0));
          },

          .font{.size = 10},
          .parent{&m_srcWidget},
      }}

    , m_imgSize
      {{
          .x = [this]{ return m_texSize.dx(); },
          .y = [this]{ return m_texSize.dy() + m_texSize.h() + 15; },

          .textFunc = [this]
          {
              return str_printf("IMG (%d, %d)", m_img.w(), m_img.h());
          },

          .font{.size = 10},
          .parent{&m_srcWidget},
      }}

    , m_roiInfo
      {{
          .x = [this]{ return m_imgSize.dx(); },
          .y = [this]{ return m_imgSize.dy() + m_imgSize.h() + 15; },

          .textFunc = [this]
          {
              const auto r = getROI();
              return str_printf("ROI (%d, %d, %d, %d)", r.x, r.y, r.w, r.h);
          },

          .font{.size = 10},
          .parent{&m_srcWidget},
      }}

    , m_resizeInfo
      {{
          .x = [this]{ return m_roiInfo.dx(); },
          .y = [this]{ return m_roiInfo.dy() + m_roiInfo.h() + 15; },

          // set textFunc later

          .font{.size = 10},
          .parent{&m_srcWidget},
      }}

    , m_marginInfo
      {{
          .x = [this]{ return m_resizeInfo.dx(); },
          .y = [this]{ return m_resizeInfo.dy() + m_resizeInfo.h() + 15; },

          // set textFunc later

          .font{.size = 10},
          .parent{&m_srcWidget},
      }}

    , m_dstWidget
      {{
          .x = [this]{ return       m_srcWidget.dx() + m_srcWidget.w(); },
          .w = [this]{ return w() - m_srcWidget.dx() - m_srcWidget.w(); },
          .h = h(),

          .parent{this},
      }}

    , m_dstCanvas
      {{
          .x = 20,
          .y = 20,
          .w = [this]{ return m_dstWidget.w() - 40; },
          .h = [this]{ return m_dstWidget.h() - 40; },

          .parent{&m_dstWidget},
      }}

    , m_resizeBoard
      {{
          .getter = &m_img,
          .vr{[this]{ return getROI(); }},

          .bgDrawFunc = [](const Widget *self, int dstDrawX, int dstDrawY)
          {
              g_glDevice->fillRectangle(colorf::BLACK + colorf::A_SHF(0XF0), dstDrawX, dstDrawY, self->w(), self->h());
              g_glDevice->drawRectangle(colorf::WHITE + colorf::A_SHF(0X80), dstDrawX, dstDrawY, self->w(), self->h());
          },

          .fgDrawFunc = [this](const Widget *self, int dstDrawX, int dstDrawY)
          {
              const int w = self->w();
              const int h = self->h();

              const float hr0 = m_marginHSlider_0.getValue();
              const float hr1 = m_marginHSlider_1.getValue();

              const float vr0 = m_marginVSlider_0.getValue();
              const float vr1 = m_marginVSlider_1.getValue();

              g_glDevice->drawRectangle(colorf::WHITE + colorf::A_SHF(0X80), dstDrawX, dstDrawY, w, h);

              g_glDevice->drawLine(colorf::RED_A255, dstDrawX + hr0 * w, dstDrawY, dstDrawX + hr0 * w, dstDrawY + h - 1);
              g_glDevice->drawLine(colorf::RED_A255, dstDrawX + hr1 * w, dstDrawY, dstDrawX + hr1 * w, dstDrawY + h - 1);

              g_glDevice->drawLine(colorf::BLUE_A255, dstDrawX, dstDrawY + vr0 * h, dstDrawX + w - 1, dstDrawY + vr0 * h);
              g_glDevice->drawLine(colorf::BLUE_A255, dstDrawX, dstDrawY + vr1 * h, dstDrawX + w - 1, dstDrawY + vr1 * h);
          },

          .parent{&m_dstCanvas},
      }}

    , m_marginHSlider_0
      {{
          .bar
          {
              .x = [this]{ return m_dstCanvas.dx()                      ; },
              .y = [this]{ return m_dstCanvas.dy() + m_dstCanvas.h() - 1; },
              .w = [this]{ return                    m_dstCanvas.w()    ; },
              .h = 1,
              .v = false,
          },

          .slider
          {
              .w = 10,
              .h = 10,
          },

          .value = 0.333f,
          .checkFunc = [this](float newValue) -> bool
          {
              return checkResizeW(newValue, m_marginHSlider_1.getValue());
          },

          .sliderWidget
           {
              .dir = DIR_UPLEFT,
              .widget = new GfxShapeBoard
              {{
                  .w = 10,
                  .h = 10,

                  .drawFunc = [](const Widget *self, int dstDrawX, int dstDrawY)
                  {
                      g_glDevice->fillRectangle(colorf::WHITE + colorf::A_SHF(0X80), dstDrawX, dstDrawY, self->w(), self->h());
                  },
              }},
              .autoDelete = true,
           },

           .parent{&m_dstWidget},
      }}

    , m_marginHSlider_1
      {{
          .bar
          {
              .x = [this]{ return m_dstCanvas.dx()                      ; },
              .y = [this]{ return m_dstCanvas.dy() + m_dstCanvas.h() - 1; },
              .w = [this]{ return                    m_dstCanvas.w()    ; },
              .h = 1,
              .v = false,
          },

          .slider
          {
              .w = 10,
              .h = 10,
          },

          .value = 0.666f,
          .checkFunc = [this](float newValue) -> bool
          {
              return checkResizeW(newValue, m_marginHSlider_0.getValue());
          },

          .sliderWidget
           {
              .dir = DIR_UPLEFT,
              .widget = new GfxShapeBoard
              {{
                  .w = 10,
                  .h = 10,

                  .drawFunc = [](const Widget *self, int dstDrawX, int dstDrawY)
                  {
                      g_glDevice->fillRectangle(colorf::WHITE + colorf::A_SHF(0X80), dstDrawX, dstDrawY, self->w(), self->h());
                  },
              }},
              .autoDelete = true,
           },

           .parent{&m_dstWidget},
      }}

    , m_marginVSlider_0
      {{
          .bar
          {
              .x = [this]{ return m_dstCanvas.dx() + m_dstCanvas.w() - 1; },
              .y = [this]{ return m_dstCanvas.dy()                      ; },
              .w = 1,
              .h = [this]{ return                    m_dstCanvas.h()    ; },
          },

          .slider
          {
              .w = 10,
              .h = 10,
          },

          .value = 0.333f,
          .checkFunc = [this](float newValue) -> bool
          {
              return checkResizeH(newValue, m_marginVSlider_1.getValue());
          },

          .sliderWidget
          {
             .dir = DIR_UPLEFT,
             .widget = new GfxShapeBoard
             {{
                 .w = 10,
                 .h = 10,

                 .drawFunc = [](const Widget *self, int dstDrawX, int dstDrawY)
                 {
                     g_glDevice->fillRectangle(colorf::WHITE + colorf::A_SHF(0X80), dstDrawX, dstDrawY, self->w(), self->h());
                 },
             }},
             .autoDelete = true,
          },

          .parent{&m_dstWidget},
      }}

    , m_marginVSlider_1
      {{
          .bar
          {
              .x = [this]{ return m_dstCanvas.dx() + m_dstCanvas.w() - 1; },
              .y = [this]{ return m_dstCanvas.dy()                      ; },
              .w = 1,
              .h = [this]{ return                    m_dstCanvas. h()   ; },
          },

          .slider
          {
              .w = 10,
              .h = 10,
          },

          .value = 0.666f,
          .checkFunc = [this](float newValue) -> bool
          {
              return checkResizeH(newValue, m_marginVSlider_0.getValue());
          },

          .sliderWidget
          {
             .dir = DIR_UPLEFT,
             .widget = new GfxShapeBoard
             {{
                 .w = 10,
                 .h = 10,

                 .drawFunc = [](const Widget *self, int dstDrawX, int dstDrawY)
                 {
                     g_glDevice->fillRectangle(colorf::WHITE + colorf::A_SHF(0X80), dstDrawX, dstDrawY, self->w(), self->h());
                 },
             }},
             .autoDelete = true,
          },

          .parent{&m_dstWidget},
      }}
{
    m_img.setSize([this]{ return to_d(m_imgCanvas.w() * m_imgResizeHSlider.getValue()); },
                  [this]{ return to_d(m_imgCanvas.h() * m_imgResizeVSlider.getValue()); });

    m_imgContainer.moveTo(m_imgFrame.dx(), m_imgFrame.dy());
    m_resizeBoard.setGfxMargin(Widget::VarMargin
    {
        .up    = [this]{ return to_d(        std::min<float>(m_marginVSlider_0.getValue(), m_marginVSlider_1.getValue())  * m_dstCanvas.h()); },
        .down  = [this]{ return to_d((1.0f - std::max<float>(m_marginVSlider_0.getValue(), m_marginVSlider_1.getValue())) * m_dstCanvas.h()); },
        .left  = [this]{ return to_d(        std::min<float>(m_marginHSlider_0.getValue(), m_marginHSlider_1.getValue())  * m_dstCanvas.w()); },
        .right = [this]{ return to_d((1.0f - std::max<float>(m_marginHSlider_0.getValue(), m_marginHSlider_1.getValue())) * m_dstCanvas.w()); },
    });

    m_resizeBoard.setGfxResize(Widget::VarSize2D
    {
        [this]{ return m_dstCanvas.w() - m_resizeBoard.margin(2) - m_resizeBoard.margin(3) - std::max<int>(getROI().x, 0) - std::max<int>(m_img.w() - getROI().x - getROI().w, 0); },
        [this]{ return m_dstCanvas.h() - m_resizeBoard.margin(0) - m_resizeBoard.margin(1) - std::max<int>(getROI().y, 0) - std::max<int>(m_img.h() - getROI().y - getROI().h, 0); },
    });

    m_resizeInfo.setTextFunc([this]
    {
        return str_printf("RESIZE (%d, %d)", m_resizeBoard.w(), m_resizeBoard.h());
    });

    m_marginInfo.setTextFunc([this]
    {
        return str_printf("MARGIN (%d, %d, %d, %d)", m_resizeBoard.margin(0), m_resizeBoard.margin(1), m_resizeBoard.margin(2), m_resizeBoard.margin(3));
    });
}

Widget::ROI GfxDebugBoard::getROI() const
{
    const auto canvas_w = m_imgCanvas.w();
    const auto canvas_h = m_imgCanvas.h();

    const float hr0 = m_cropHSlider_0.getValue();
    const float hr1 = m_cropHSlider_1.getValue();

    const float vr0 = m_cropVSlider_0.getValue();
    const float vr1 = m_cropVSlider_1.getValue();
    //
    const auto x0 = static_cast<int>(canvas_w * hr0);
    const auto x1 = static_cast<int>(canvas_w * hr1);

    const auto y0 = static_cast<int>(canvas_h * vr0);
    const auto y1 = static_cast<int>(canvas_h * vr1);

    return Widget::ROI
    {
        .x = std::min<int>(x0, x1) - m_imgContainer.dx(),
        .y = std::min<int>(y0, y1) - m_imgContainer.dy(),

        .w = std::abs(x0 - x1),
        .h = std::abs(y0 - y1),
    };
}

bool GfxDebugBoard::checkResizeW(float value0, float value1) const
{
    const auto roi = getROI();

    const float newDistance = m_dstCanvas.w() * std::abs(value0 - value1);
    const float minDistance = std::max<int>(roi.x, 0) + std::max<int>(m_img.w() - roi.x - roi.w, 0);

    return newDistance >= minDistance;
}

bool GfxDebugBoard::checkResizeH(float value0, float value1) const
{
    const auto roi = getROI();

    const float newDistance = m_dstCanvas.h() * std::abs(value0 - value1);
    const float minDistance = std::max<int>(roi.y, 0) + std::max<int>(m_img.h() - roi.y - roi.h, 0);

    return newDistance >= minDistance;
}

// ===== texaniboard.cpp =====
#include <cfloat>
#include <numeric>

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

TexAniBoard::TexAniBoard(
        Widget::VarDir argDir,
        Widget::VarInt argX,
        Widget::VarInt argY,

        uint32_t argStartTexID,
        size_t   argFrameCount,

        size_t argFps,

        bool argFadeInout,
        bool argLoop,

        Widget *argParent,
        bool    argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),

          .w = [this](const Widget *)
          {
              if(const auto [frame, _] = getDrawFrame(); frame >= 0){
                  if(auto texPtr = g_progUseDB->retrieve(m_startTexID + frame)){
                      return GLDeviceHelper::getTextureWidth(texPtr);
                  }
              }
              return 0;
          },

          .h = [this](const Widget *)
          {
              if(const auto [frame, _] = getDrawFrame(); frame >= 0){
                  if(auto texPtr = g_progUseDB->retrieve(m_startTexID + frame)){
                      return GLDeviceHelper::getTextureHeight(texPtr);
                  }
              }
              return 0;
          },

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , m_startTexID(argStartTexID)
    , m_frameCount(argFrameCount)
    , m_fps(argFps)

    , m_fadeInout(argFadeInout)
    , m_loop(argLoop)

    , m_cropArea
      {{
          .w = [this](const Widget *){ return w(); },
          .h = [this](const Widget *){ return h(); },

          .drawFunc = [this](const Widget *, int drawDstX, int drawDstY)
          {
              const auto [frame, alpha] = getDrawFrame();
              if(frame < 0){
                  return;
              }

              const uint32_t currTexId = m_startTexID + ((frame + 0) % m_frameCount);
              const uint32_t nextTexId = m_startTexID + ((frame + 1) % m_frameCount);

              if(auto currTexPtr = g_progUseDB->retrieve(currTexId)){
                  const GLDeviceHelper::EnableTextureModColor enableModColor(currTexPtr, colorf::WHITE + colorf::A_SHF(255 - alpha));
                  g_glDevice->drawTexture(currTexPtr, drawDstX, drawDstY);
              }

              if(auto nextTexPtr = g_progUseDB->retrieve(nextTexId)){
                  const GLDeviceHelper::EnableTextureModColor enableModColor(nextTexPtr, colorf::WHITE + colorf::A_SHF(alpha));
                  g_glDevice->drawTexture(nextTexPtr, drawDstX, drawDstY);
              }
          },

          .parent{this},
      }}
{}

std::tuple<int, uint8_t> TexAniBoard::getDrawFrame() const
{
    if(m_frameCount == 0){ return {-1, 0}; }
    if(m_fps        == 0){ return { 0, 0}; }

    const double decimalFrame = m_accuTime * m_fps / 1000.0;
    const int    integerFrame = to_dround(std::floor(decimalFrame));

    return
    {
        [integerFrame, this]() -> int // current frame
        {
            if(m_loop){
                return integerFrame % m_frameCount;
            }
            else{
                return std::min<int>(integerFrame, m_frameCount - 1);
            }
        }(),


        [integerFrame, decimalFrame, this]() -> uint8_t // current alpha
        {
            if(m_fadeInout){
                return to_dround(255 * mathf::bound<double>(decimalFrame - integerFrame, 0.0, 1.0));
            }
            return 0;
        }(),
    };
}
