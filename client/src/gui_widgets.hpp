#pragma once

// ===== button.hpp =====
#include <cstddef>
#include <cstdint>
#include <variant>
#include <optional>
#include <functional>

class Widget;
namespace Button
{
    using    OverCBFunc = std::variant<std::nullptr_t, std::function<void(         )>, std::function<void(Widget *           )>>;
    using   ClickCBFunc = std::variant<std::nullptr_t, std::function<void(bool, int)>, std::function<void(Widget *, bool, int)>>;
    using TriggerCBFunc = std::variant<std::nullptr_t, std::function<void(      int)>, std::function<void(Widget *,       int)>>;

    void evalOverCBFunc   (const Button::OverCBFunc    &, Widget *           );
    void evalClickCBFunc  (const Button::ClickCBFunc   &, Widget *, bool, int);
    void evalTriggerCBFunc(const Button::TriggerCBFunc &, Widget *,       int);

    struct SeffIDList
    {
        std::optional<uint32_t> onOverIn  = {};
        std::optional<uint32_t> onOverOut = {};
        std::optional<uint32_t> onClick   = 0X01020000 + 105;
    };
}

// ===== gfxshapeboard.hpp =====
#include "gui_core.hpp"

class GfxShapeBoard: public Widget
{
    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;
            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarSize w = 0;
            Widget::VarSize h = 0;

            Widget::VarDrawFunc drawFunc = nullptr;

            Widget::InstAttrs attrs {};
            Widget::WADPair  parent {};
        };

    private:
        Widget::VarDrawFunc m_drawFunc;

    public:
        explicit GfxShapeBoard(GfxShapeBoard::InitArgs);

    public:
        void drawDefault(Widget::ROIMap) const override;
};

// ===== imageboard.hpp =====
#include "mirevent.hpp"
#include "colorf.hpp"

// check drawTextureEx comments for how image flip/rotation works
//
//      ---H-->
//
//    x--y | y--x
//    |  | | |  |    |    Vflip = Hflip + 180
//    +--+ | +--+    |    Hflip = Vflip + 180
// --------+-------  V    Hflip + Vflip = 180
//    +--+ | +--+    |
//    |  | | |  |    v
//    x--y | y--x

// top-left corners of image and widget are aligned
// widget uses original image width/height if widget size is given by {}, otherwise rescaling applied
class ImageBoard: public Widget
{
    protected:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;
            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarSizeOpt w = std::nullopt; // {} means image width , otherwise rescale the image
            Widget::VarSizeOpt h = std::nullopt; // {} means image height, otherwise rescale the image

            Widget::VarTexLoadFunc texLoadFunc = GLTexID{};

            bool hflip  = false;
            bool vflip  = false;
            int  rotate = 0;

            Widget::VarU32 modColor = colorf::WHITE_A255;
            Widget::VarBlendMode blendMode = MIR_BLENDMODE_BLEND;

            Widget::WADPair parent {};
        };

    private:
        Widget::VarU32 m_varColor;
        Widget::VarBlendMode m_varBlendMode;

    private:
        Widget::VarTexLoadFunc m_loadFunc;

    private:
        std::pair<bool, int> m_xformPair;

    private:
        bool &m_hflip  = m_xformPair.first;
        int  &m_rotate = m_xformPair.second;

    public:
        ImageBoard(ImageBoard::InitArgs);

    public:
        void drawDefault(Widget::ROIMap) const override;

    public:
        void setColor(Widget::VarU32 color)
        {
            m_varColor = std::move(color);
        }

        void setLoadFunc(VarTexLoadFunc func)
        {
            m_loadFunc = std::move(func);
        }

    public:
        void setFlipRotate(bool hflip, bool vflip, int rotate)
        {
            m_xformPair = getHFlipRotatePair(hflip, vflip, rotate);
        }

    private:
        static std::pair<bool, int> getHFlipRotatePair(bool hflip, bool vflip, int rotate) // h/v flip first, then rotate
        {
            if     (hflip && vflip) return {false, (((rotate + 2) % 4) + 4) % 4};
            else if(hflip         ) return { true, (((rotate    ) % 4) + 4) % 4};
            else if(         vflip) return { true, (((rotate + 2) % 4) + 4) % 4};
            else                    return {false, (((rotate    ) % 4) + 4) % 4};
        }

    public:
        GLTexID getTexture() const
        {
            return Widget::evalTexLoadFunc(m_loadFunc, this);
        }

    public:
        bool transposed() const
        {
            return m_rotate % 2;
        }
};

// ===== inputline.hpp =====
#include "gui_textengine.hpp"
#include "ime.hpp"

class InputLine: public Widget
{
    public:
        struct CursorArgs final
        {
            Widget::VarSize w = 2;
            Widget::VarU32 color = colorf::WHITE_A255;
        };

    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;
            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarSizeOpt w = 0;
            Widget::VarSizeOpt h = 0;

            Widget::VarInt enableIME = IME_DISABLE;

            Widget::FontConfig font {};
            InputLine::CursorArgs cursor {};

            std::function<void()>            onTab    = nullptr;
            std::function<void()>            onCR     = nullptr;
            std::function<void(std::string)> onChange = nullptr;
            std::function<bool(std::string)> validate = nullptr;

            Widget::WADPair parent {};
        };

    protected:
        Widget::VarInt m_imeEnabled;

    protected:
        XMLTypeset m_tpset;

    protected:
        int m_cursor = 0;

    protected:
        double m_cursorBlink = 0.0;
        InputLine::CursorArgs m_cursorArgs;

    protected:
        std::function<void()>            m_onTab;
        std::function<void()>            m_onCR;
        std::function<void(std::string)> m_onChange;
        std::function<bool(std::string)> m_validate;

    public:
        InputLine(InputLine::InitArgs);

    public:
        bool processEventDefault(const MirEvent &, bool, Widget::ROIMap) override;

    public:
        void setFocus(bool) override;

    public:
        void drawDefault(Widget::ROIMap) const override;

    public:
        void updateDefault(double ms) override
        {
            m_cursorBlink += ms;
        }

    public:
        std::string getRawString() const
        {
            return m_tpset.getRawString();
        }

    public:
        virtual void clear();

    public:
        void deleteChar();
        void insertChar(char);
        void setInput(const char *);
        void insertUTF8String(const char *);
};

// ===== labelboard.hpp =====
#include <string>
#include <vector>

#include "lalign.hpp"

class LabelBoard: public Widget
{
    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;
            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            const char8_t *label = nullptr;

            Widget::FontConfig font {};

            LabelBoard::InstAttrs attrs {};
            Widget::WADPair      parent {};
        };

    private:
        XMLTypeset m_tpset;

    public:
        LabelBoard(LabelBoard::InitArgs);

    public:
        ~LabelBoard() = default;

    public:
        void loadXML(const char *);
        void setText(const char8_t *, ...);

    public:
        void setFont(uint8_t);
        void setFontSize(uint8_t);
        void setFontStyle(uint8_t);

    public:
        void setFontColor(Widget::VarU32);
        void setImageMaskColor(Widget::VarU32);

    public:
        uint32_t   color() const { return m_tpset.  color(); }
        uint32_t bgColor() const { return m_tpset.bgColor(); }

    public:
        void clear()
        {
            m_tpset.clear();
        }

        bool empty() const
        {
            return m_tpset.empty();
        }

    public:
        std::string getXML() const
        {
            return m_tpset.getXML();
        }

        std::string getText() const
        {
            return m_tpset.getText();
        }

    public:
        void drawDefault(Widget::ROIMap) const override;

    public:
        int w() const override { return m_tpset.px() + m_tpset.pw(); }
        int h() const override { return m_tpset.py() + m_tpset.ph(); }
};

// ===== itemflex.hpp =====
#include <utility>
#include <initializer_list>
#include <concepts>
#include "itemalign.hpp"

class ItemFlex: public Widget
{
    #include "itemflex.api.hpp"

    private:
        const bool m_vbox;
        const ItemAlign m_align;

    private:
        Widget *m_canvas;

    private:
        Widget::VarSize m_headSpace;
        Widget::VarSize m_itemSpace;
        Widget::VarSize m_tailSpace;

    private:
        Widget::VarSizeOpt m_fixedEdgeSize;

    public:
        ItemFlex(ItemFlex::InitArgs);

    private:
        int canvasW() const;
        int canvasH() const;

    private:
        const Widget *lastShowChild() const;
};

void ItemFlex::clearItem(std::invocable<const Widget *, bool> auto f)
{
    m_canvas->clearChild(f);
}

void ItemFlex::clearItem()
{
    clearItem([](const Widget *, bool){ return true; });
}

auto ItemFlex::foreachItem(this auto && self, bool forward, auto func)
{
    return self.m_canvas->foreachChild(forward, func);
}

auto ItemFlex::foreachItem(this auto && self, auto func)
{
    return self.foreachItem(true, func);
}

// ===== itempair.hpp =====

class ItemPair: public Widget
{
    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarSizeOpt flex = std::nullopt;

            bool v = true;
            ItemAlign align = ItemAlign::UPLEFT;

            Widget::WADPair  first {};
            Widget::WADPair second {};
            Widget::WADPair parent {};
        };

    public:
        ItemPair(ItemPair::InitArgs);
};

// ===== gfxdupboard.hpp =====

class GfxDupBoard: public Widget
{
    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;
            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarSize w = 0;
            Widget::VarSize h = 0;

            Widget::VarGetter<Widget *> getter = nullptr; // not-owning
            Widget::VarROIOpt vro {};

            Widget::InitAttrs attrs {};
            Widget::WADPair  parent {};
        };

    private:
        Widget::VarGetter<Widget *> m_getter;
        Widget::VarROIOpt m_vro;

    public:
        GfxDupBoard(GfxDupBoard::InitArgs args)
            : Widget
              {{
                  .dir = std::move(args.dir),

                  .x = std::move(args.x),
                  .y = std::move(args.y),
                  .w = std::move(args.w),
                  .h = std::move(args.h),

                  .attrs = std::move(args.attrs),
                  .parent = std::move(args.parent),
              }}

            , m_getter(std::move(args.getter))
            , m_vro(std::move(args.vro))
        {}

    public:
        Widget *gfxWidget() const
        {
            return Widget::evalGetter(m_getter, this);
        }

        Widget::ROI gfxCropROI() const
        {
            if(m_vro.has_value()){
                return m_vro->roi(this, nullptr);
            }
            else if(const auto gfxPtr = gfxWidget()){
                return gfxPtr->roi();
            }
            else{
                throw fflpanic("gfxCropROI");
            }
        }

    private:
        void gridHelper(this auto && self, Widget::ROIMap m, auto && func)
        {
            if(!m.calibrate(std::addressof(self))){
                return;
            }

            if(auto gfxPtr = self.gfxWidget()){
                if(const auto cr = self.gfxCropROI()){
                    if(!gfxPtr->roi().crop(cr)){
                        return;
                    }

                    for(int yi = m.ro->y / cr.h; yi * cr.h < m.ro->y + m.ro->h; ++yi){
                        for(int xi = m.ro->x / cr.w; xi * cr.w < m.ro->x + m.ro->w; ++xi){
                            func(gfxPtr, m.map(xi * cr.w - cr.x, yi * cr.h - cr.y, cr));
                        }
                    }
                }
            }
        }

    public:
        void drawDefault(Widget::ROIMap m) const override
        {
            gridHelper(m, [](const auto *widget, const auto &cm){ widget->draw(cm); });
        }

    public:
        bool processEventDefault(const MirEvent &e, bool valid, Widget::ROIMap m) override
        {
            bool takenEvent = false;
            gridHelper(m, [&e, valid, &takenEvent](auto *widget, const auto &cm)
            {
                takenEvent |= widget->processEvent(e, valid && !takenEvent, cm);
            });

            return takenEvent;
        }
};

// ===== menu.hpp =====

namespace Menu
{
    struct ItemSize final // size crop of widget in menu
    {
        Widget::VarSizeOpt w = std::nullopt;
        Widget::VarSizeOpt h = std::nullopt;
    };

    using ClickCBFunc = std::variant<std::nullptr_t,
                                     std::function<void(        )>,
                                     std::function<void(Widget *)>>;

    void evalClickCBFunc(const ClickCBFunc &, Widget *);
}

// ===== buttonbase.hpp =====
#include "bevent.hpp"
#include "sysconst.hpp"

class ButtonBase: public Widget
{
    private:
        struct InitArgs
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarSizeOpt w = 0;
            Widget::VarSizeOpt h = 0;

            Button::OverCBFunc onOverIn  = nullptr;
            Button::OverCBFunc onOverOut = nullptr;

            Button::ClickCBFunc onClick = nullptr;
            Button::TriggerCBFunc onTrigger = nullptr;

            Button::SeffIDList seff {};

            int offXOnOver = 0;
            int offYOnOver = 0;

            int offXOnClick = 0;
            int offYOnClick = 0;

            bool onClickDone = true;
            bool radioMode   = false;

            Widget::InstAttrs attrs {};
            Widget::WADPair  parent {};
        };

    private:
        class InnButtonState final
        {
            private:
                int m_state[2]
                {
                    BEVENT_OFF,
                        BEVENT_OFF,
                };

            public:
                void setState(int state) noexcept
                {
                    m_state[0] = m_state[1];
                    m_state[1] = state;
                }

            public:
                int getState()     const noexcept { return m_state[1]; }
                int getPrevState() const noexcept { return m_state[0]; }
        };

    private:
        InnButtonState m_state;

    protected:
        const bool m_onClickDone;
        const bool m_radioMode;

    protected:
        const Button::SeffIDList m_seff;

    protected:
        const int m_offset[3][2];

    protected:
        Button::OverCBFunc m_onOverIn;
        Button::OverCBFunc m_onOverOut;

    protected:
        Button::ClickCBFunc m_onClick;
        Button::TriggerCBFunc m_onTrigger;

    public:
        ButtonBase(ButtonBase::InitArgs);

    public:
        bool processEventDefault(const MirEvent &, bool, Widget::ROIMap) override;

    protected:
        int offX() const { return m_offset[getState()][0]; }
        int offY() const { return m_offset[getState()][1]; }

    public:
        int     getState() const { return m_state.    getState(); }
        int getPrevState() const { return m_state.getPrevState(); }

    public:
        void setState(int state)
        {
            m_state.setState(state);
        }

    public:
        bool getOnClickDone() const
        {
            return m_onClickDone;
        }

        bool getRadioMode() const
        {
            return m_radioMode;
        }

    public:
        void setOff () { setState(BEVENT_OFF ); }
        void setOn  () { setState(BEVENT_ON  ); }
        void setDown() { setState(BEVENT_DOWN); }

    private:
        void onOverIn();
        void onOverOut();
        void onClick(bool, int);
        void onTrigger(int);
        void onBadEvent();
};

// ===== trigfxbutton.hpp =====
#include <array>

class TrigfxButton: public ButtonBase
{
    public:
        using TrigfxFunc = std::variant<std::nullptr_t,
                                        std::function<const Widget *(                   )>,
                                        std::function<const Widget *(                int)>,
                                        std::function<const Widget *(const Widget *, int)>>;

    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;
            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            TrigfxButton::TrigfxFunc      gfxFunc {};
            std::array<const Widget *, 3> gfxList {};

            Button::SeffIDList seff {};

            Button::OverCBFunc onOverIn  = nullptr;
            Button::OverCBFunc onOverOut = nullptr;

            Button::ClickCBFunc onClick = nullptr;
            Button::TriggerCBFunc onTrigger = nullptr;

            int offXOnOver = 0;
            int offYOnOver = 0;

            int offXOnClick = 0;
            int offYOnClick = 0;

            bool onClickDone = true;
            bool radioMode   = false;

            Widget::InstAttrs attrs {};
            Widget::WADPair  parent {};
        };

    private:
        TrigfxButton::TrigfxFunc      m_gfxFunc;
        std::array<const Widget *, 3> m_gfxList;

    public:
        TrigfxButton(TrigfxButton::InitArgs);

    public:
        void drawDefault(Widget::ROIMap) const override;

    public:
        void setGfxFunc(TrigfxButton::TrigfxFunc gfxFunc)
        {
            m_gfxFunc = gfxFunc;
        }

        void setGfxList(const std::array<const Widget *, 3> &gfxList)
        {
            m_gfxList = gfxList;
        }

        void setGfxList(const Widget *gfxWidget)
        {
            m_gfxList = {gfxWidget, gfxWidget, gfxWidget};
        }

    private:
        const Widget *evalGfxWidget     (std::optional<int> = std::nullopt) const;
        const Widget *evalGfxWidgetValid(                                 ) const; // search the first valid gfx pointer

    public:
        int w() const override { if(auto gfxPtr = evalGfxWidgetValid()){ return gfxPtr->w(); } else { return 0; }}
        int h() const override { if(auto gfxPtr = evalGfxWidgetValid()){ return gfxPtr->h(); } else { return 0; }}
};

// ===== textboard.hpp =====
#include <tuple>


class TextBoard: public Widget
{
    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;
            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarStrFunc textFunc {};
            Widget::FontConfig font {};

            Widget::VarBlendMode blendMode = MIR_BLENDMODE_BLEND;

            Widget::InstAttrs attrs {};
            Widget::WADPair  parent {};
        };

    private:
        Widget::FontConfig m_font;
        Widget::VarStrFunc m_textFunc;

    private:
        ImageBoard m_image;

    public:
        TextBoard(TextBoard::InitArgs);

    public:
        void setFont(uint8_t argFont)
        {
            m_font.id = argFont;
        }

        void setFontSize(uint8_t argFontSize)
        {
            m_font.size = argFontSize;
        }

        void setFontStyle(uint8_t argFontStyle)
        {
            m_font.style = argFontStyle;
        }

        void setFontColor(Widget::VarU32 argColor)
        {
            m_font.color = std::move(argColor);
        }

        void setTextFunc(Widget::VarStrFunc argTextFunc)
        {
            m_textFunc = std::move(argTextFunc);
        }

    public:
        std::tuple<std::string, std::string> fontName() const;

    public:
        Widget::VarStr getVStr() const
        {
            return Widget::evalStrFunc(m_textFunc, this);
        }

    public:
        std::string getText() const
        {
            return getVStr().str();
        }

        bool empty() const
        {
            return getVStr().empty();
        }

    public:
        void drawDefault(Widget::ROIMap m) const override
        {
            return m_image.draw(m);
        }
};

// ===== gfxcropboard.hpp =====

class GfxCropBoard: public Widget
{
    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarGetter<Widget *> getter = nullptr; // not-owning
            Widget::VarROI vr {};

            Widget::VarDrawFunc bgDrawFunc = nullptr;
            Widget::VarDrawFunc fgDrawFunc = nullptr;

            Widget::VarMargin margin {};

            Widget::InitAttrs attrs {};
            Widget::WADPair  parent {};
        };

    private:
        Widget::VarGetter<Widget *> m_getter;

    private:
        Widget::VarROI    m_vr;
        Widget::VarMargin m_margin;

    private:
        GfxShapeBoard *m_bgBoard;
        GfxShapeBoard *m_fgBoard;

    public:
        GfxCropBoard(GfxCropBoard::InitArgs args)
            : Widget
              {{
                  .dir = std::move(args.dir),

                  .x = std::move(args.x),
                  .y = std::move(args.y),

                  .attrs = std::move(args.attrs),
                  .parent = std::move(args.parent),
              }}

            , m_getter(std::move(args.getter))
            , m_vr    (std::move(args.vr    ))
            , m_margin(std::move(args.margin))

            , m_bgBoard(Widget::hasDrawFunc(args.bgDrawFunc) ? new GfxShapeBoard
              {{
                  .w = [this]{ return w(); },
                  .h = [this]{ return h(); },

                  .drawFunc = std::move(args.bgDrawFunc),

              }} : nullptr)

            , m_fgBoard(Widget::hasDrawFunc(args.fgDrawFunc) ? new GfxShapeBoard
              {{
                  .w = [this]{ return w(); },
                  .h = [this]{ return h(); },

                  .drawFunc = std::move(args.fgDrawFunc),

              }} : nullptr)
        {
            // respect blank space by over-cropping
            // if cropped size bigger than gfx size, it's fill with blank

            setSize([this]{ return margin(2) + gfxCropW() + margin(3); },
                    [this]{ return margin(0) + gfxCropH() + margin(1); });

            if(m_bgBoard){ Widget::addChild(m_bgBoard, true); }
            if(m_fgBoard){ Widget::addChild(m_fgBoard, true); }
        }

    private:
        void cropHelper(this auto && self, const Widget::ROIMap &m, auto && func)
        {
            if(auto gfxPtr = self.gfxWidget()){
                if(const auto r = self.gfxCropROI()){
                    func(gfxPtr, m.map(self.margin(2) - r.x, self.margin(0) - r.y, r));
                }
            }
        }

    public:
        void drawDefault(Widget::ROIMap m) const override
        {
            if(!m.calibrate(this)){
                return;
            }

            if(m_bgBoard){
                drawChild(m_bgBoard, m);
            }

            cropHelper(m, [](const auto *widget, const auto &cm)
            {
                widget->draw(cm);
            });

            if(m_fgBoard){
                drawChild(m_fgBoard, m);
            }
        }

    public:
        bool processEventDefault(const MirEvent &e, bool valid, Widget::ROIMap m) override
        {
            if(!m.calibrate(this)){
                return false;
            }

            bool tookEvent = false;
            cropHelper(m, [&e, valid, &tookEvent](auto *widget, const auto &cm)
            {
                tookEvent = widget->processEvent(e, valid, cm);
            });

            return tookEvent;
        }

    public:
        Widget *gfxWidget() const
        {
            return Widget::evalGetter(m_getter, this);
        }

    public:
        int gfxCropX() const { return m_vr.x(this); }
        int gfxCropY() const { return m_vr.y(this); }
        int gfxCropW() const { return m_vr.w(this); }
        int gfxCropH() const { return m_vr.h(this); }

    public:
        Widget::IntOffset2D gfxCropOffset() const
        {
            return m_vr.offset(this);
        }

        Widget::IntSize2D gfxCropSize() const
        {
            return m_vr.size(this);
        }

        Widget::ROI gfxCropROI() const
        {
            return m_vr.roi(this);
        }

    public:
        int margin(int index) const
        {
            return Widget::evalSize(m_margin[index], this);
        }

        Widget::IntMargin margin() const
        {
            return
            {
                .up    = margin(0),
                .down  = margin(1),
                .left  = margin(2),
                .right = margin(3),
            };
        }

    public:
        std::vector<std::string> dumpTreeExt() const override
        {
            std::vector<std::string> attrs;

            if(const auto widget = gfxWidget()){
                attrs.push_back(str_printf(R"("gfxWidget":{"name":"%s","id":%llu})", widget->name(), to_llu(widget->id())));
            }

            const auto r = gfxCropROI();
            attrs.push_back(str_printf(R"("roi":{"x":%d,"y":%d,"w":%d,"h":%d})", r.x, r.y, r.w, r.h));

            const auto m = margin();
            attrs.push_back(str_printf(R"("margin":{"up":%d,"down":%d,"left":%d,"right":%d})", m.up, m.down, m.left, m.right));

            return attrs;
        }
};

// ===== margincontainer.hpp =====

class MarginContainer: public Widget
{
    public:
        struct ContainedWidget final
        {
            Widget::VarDir dir = DIR_NONE;

            Widget * widget     = nullptr;
            bool     autoDelete = false;
        };

    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarSizeOpt w = 0;
            Widget::VarSizeOpt h = 0;

            ContainedWidget contained {};

            Widget::VarDrawFunc bgDrawFunc = nullptr;
            Widget::VarDrawFunc fgDrawFunc = nullptr;

            Widget::InstAttrs attrs {};
            Widget::WADPair  parent {};
        };

    private:
        Widget *m_canvas; // the unique child

    private:
        ContainedWidget m_contained;

    private:
        GfxShapeBoard *m_bgBoard;
        GfxShapeBoard *m_fgBoard;

    public:
        MarginContainer(MarginContainer::InitArgs args)
            : Widget
              {{
                  .dir = std::move(args.dir),

                  .x = std::move(args.x),
                  .y = std::move(args.y),

                  .w = std::move(args.w),
                  .h = std::move(args.h),

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
                                  }
                              }
                          }},
                          .autoDelete = true,
                      },
                  },

                  .attrs
                  {
                      .type
                      {
                          .addChild = false,
                          .removeChild = false,
                      },
                      .inst = std::move(args.attrs),
                  },
                  .parent = std::move(args.parent),
              }}

            , m_canvas(firstChild())

            , m_contained(std::move(args.contained))
            , m_bgBoard(Widget::hasDrawFunc(args.bgDrawFunc) ? new GfxShapeBoard
              {{
                  .w = [this]{ return m_canvas->w(); },
                  .h = [this]{ return m_canvas->h(); },

                  .drawFunc = std::move(args.bgDrawFunc),

              }} : nullptr)

            , m_fgBoard(Widget::hasDrawFunc(args.fgDrawFunc) ? new GfxShapeBoard
              {{
                  .w = [this]{ return m_canvas->w(); },
                  .h = [this]{ return m_canvas->h(); },

                  .drawFunc = std::move(args.fgDrawFunc),

              }} : nullptr)
        {
            if(m_bgBoard){
                m_canvas->addChild(m_bgBoard, true);
            }

            if(m_contained.widget){
                doSetContained();
            }

            if(m_fgBoard){
                m_canvas->addChild(m_fgBoard, true);
            }

            m_canvas->setSize([this]
            {
                if(varWOpt()){
                    if(m_contained.widget){
                        return m_contained.widget->w();
                    }
                    return 0;
                }
                return w();
            },

            [this]
            {
                if(varHOpt()){
                    if(m_contained.widget){
                        return m_contained.widget->h();
                    }
                    return 0;
                }
                return h();
            });
        }

    public:
        auto contained(this auto && self) -> std::conditional_t<std::is_const_v<std::remove_reference_t<decltype(self)>>, const Widget *, Widget *>
        {
            return self.m_contained.widget;
        }

        auto containedPair(this auto && self) -> std::pair<std::conditional_t<std::is_const_v<std::remove_reference_t<decltype(self)>>, const Widget *, Widget *>, bool>
        {
            return {self.m_contained.widget, self.m_contained.autoDelete};
        }

    public:
        void setContained(Widget::VarDir argDir, Widget *argWidget, bool argAutoDelete)
        {
            if(m_contained.widget){
                m_canvas->removeChild(m_contained.widget->id(), m_contained.widget != argWidget); // same widget may change autoDelete
            }

            m_contained = ContainedWidget{.dir = std::move(argDir), .widget = argWidget, .autoDelete = argAutoDelete};
            doSetContained();
        }

        void clearContained(bool argTriggerAutoDelete)
        {
            if(m_contained.widget){
                m_canvas->removeChild(m_contained.widget->id(), argTriggerAutoDelete);
                m_contained = ContainedWidget{};
            }
        }

    private:
        void doSetContained()
        {
            if(m_contained.widget){
                m_canvas->addChildAt(m_contained.widget,

                [this]{ return                  Widget::evalDir(m_contained.dir, this)                                  ; },
                [this]{ return Widget::xSizeOff(Widget::evalDir(m_contained.dir, this), [this]{ return m_canvas->w(); }); },
                [this]{ return Widget::ySizeOff(Widget::evalDir(m_contained.dir, this), [this]{ return m_canvas->h(); }); },

                m_contained.autoDelete);
            }
        }
};

// ===== marginwrapper.hpp =====

class MarginWrapper: public Widget
{
    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::WADPair wrapped {};
            Widget::VarMargin margin {};

            Widget::VarDrawFunc bgDrawFunc = nullptr;
            Widget::VarDrawFunc fgDrawFunc = nullptr;

            Widget::InstAttrs attrs {};
            Widget::WADPair  parent {};
        };

    private:
        Widget *m_canvas; // the unique child

    private:
        Widget::VarMargin m_margin;

    private:
        Widget::WADPair m_wrapped;

    private:
        GfxShapeBoard *m_bgBoard;
        GfxShapeBoard *m_fgBoard;

    public:
        explicit MarginWrapper(MarginWrapper::InitArgs args)
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
                                  }
                              }
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
                      .inst = std::move(args.attrs),
                  },
                  .parent = std::move(args.parent),
              }}

            , m_canvas(firstChild())

            , m_margin (std::move(args.margin))
            , m_wrapped(std::move(args.wrapped))

            , m_bgBoard(Widget::hasDrawFunc(args.bgDrawFunc) ? new GfxShapeBoard
              {{
                  .w = [this]{ return w(); },
                  .h = [this]{ return h(); },

                  .drawFunc = std::move(args.bgDrawFunc),

              }} : nullptr)

            , m_fgBoard(Widget::hasDrawFunc(args.fgDrawFunc) ? new GfxShapeBoard
              {{
                  .w = [this]{ return w(); },
                  .h = [this]{ return h(); },

                  .drawFunc = std::move(args.fgDrawFunc),

              }} : nullptr)
        {
            if(m_bgBoard){
                m_canvas->addChild(m_bgBoard, true);
            }

            if(m_wrapped.widget){
                doSetWrapped();
            }

            if(m_fgBoard){
                m_canvas->addChild(m_fgBoard, true);
            }

            m_canvas->setSize([this]{ return (m_wrapped.widget ? m_wrapped.widget->w() : 0) + Widget::evalSize(m_margin[2], this) + Widget::evalSize(m_margin[3], this); },
                              [this]{ return (m_wrapped.widget ? m_wrapped.widget->h() : 0) + Widget::evalSize(m_margin[0], this) + Widget::evalSize(m_margin[1], this); });
        }

    public:
        auto wrapped(this auto && self) -> std::conditional_t<std::is_const_v<std::remove_reference_t<decltype(self)>>, const Widget *, Widget *>
        {
            return self.m_wrapped.widget;
        }

        auto wrappedPair(this auto && self) -> std::pair<std::conditional_t<std::is_const_v<std::remove_reference_t<decltype(self)>>, const Widget *, Widget *>, bool>
        {
            return {self.m_wrapped.widget, self.m_wrapped.autoDelete};
        }

    public:
        void setWrapped(Widget *argWidget, bool argAutoDelete)
        {
            if(m_wrapped.widget){
                m_canvas->removeChild(m_wrapped.widget->id(), m_wrapped.widget != argWidget); // same widget may change autoDelete
            }

            m_wrapped = Widget::WADPair{.widget = argWidget, .autoDelete = argAutoDelete};
            doSetWrapped();
        }

        void clearWrapped(bool argTriggerAutoDelete)
        {
            if(m_wrapped.widget){
                m_canvas->removeChild(m_wrapped.widget->id(), argTriggerAutoDelete);
                m_wrapped = Widget::WADPair{};
            }
        }

    private:
        void doSetWrapped()
        {
            if(m_wrapped.widget){
                m_canvas->addChildAt(m_wrapped.widget, DIR_UPLEFT, [this]
                {
                    return Widget::evalSize(m_margin[2], this);
                },

                [this]
                {
                    return Widget::evalSize(m_margin[0], this);
                },

                m_wrapped.autoDelete);
            }
        }
};

// ===== gfxresizeboard.hpp =====

class GfxResizeBoard: public Widget
{
    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarGetter<Widget *> getter = nullptr; // not-owning
            Widget::VarROI vr {};

            Widget::VarSize2D resize {};
            Widget::VarMargin margin {};

            Widget::VarDrawFunc bgDrawFunc = nullptr;
            Widget::VarDrawFunc fgDrawFunc = nullptr;

            Widget::InitAttrs attrs {};
            Widget::WADPair  parent {};
        };

    private:
        Widget::VarGetter<Widget *> m_getter;
        Widget::VarROI m_vr;

    private:
        Widget::VarSize2D m_resize;
        Widget::VarMargin m_margin;

    private:
        GfxShapeBoard *m_bgBoard;
        GfxShapeBoard *m_fgBoard;

    public:
        GfxResizeBoard(GfxResizeBoard::InitArgs args)
            : Widget
              {{
                  .dir = std::move(args.dir),

                  .x = std::move(args.x),
                  .y = std::move(args.y),

                  .attrs = std::move(args.attrs),
                  .parent = std::move(args.parent),
              }}

            , m_getter(std::move(args.getter))
            , m_vr(std::move(args.vr))

            , m_resize(std::move(args.resize))
            , m_margin(std::move(args.margin))

            , m_bgBoard(Widget::hasDrawFunc(args.bgDrawFunc) ? new GfxShapeBoard
              {{
                  .w = [this]{ return w(); },
                  .h = [this]{ return h(); },

                  .drawFunc = std::move(args.bgDrawFunc),

              }} : nullptr)

            , m_fgBoard(Widget::hasDrawFunc(args.fgDrawFunc) ? new GfxShapeBoard
              {{
                  .w = [this]{ return w(); },
                  .h = [this]{ return h(); },

                  .drawFunc = std::move(args.fgDrawFunc),

              }} : nullptr)
        {
            setSize([this]
            {
                const auto vx = m_vr.x(this);
                const auto vw = m_vr.w(this);
                const auto rw = m_resize.w(this);

                if(auto gfxPtr = gfxWidget()){ return margin(2) + std::max<int>(vx, 0) + rw + std::max<int>(gfxPtr->w() - (vx + vw), 0) + margin(3); }
                else                         { return margin(2) + std::max<int>(vx, 0) + rw                                             + margin(3); }
            },

            [this]
            {
                const auto vy = m_vr.y(this);
                const auto vh = m_vr.h(this);
                const auto rh = m_resize.h(this);

                if(auto gfxPtr = gfxWidget()){ return margin(0) + std::max<int>(vy, 0) + rh + std::max<int>(gfxPtr->h() - (vy + vh), 0) + margin(1); }
                else                         { return margin(0) + std::max<int>(vy, 0) + rh                                             + margin(1); }
            });

            if(m_bgBoard){ Widget::addChild(m_bgBoard, true); }
            if(m_fgBoard){ Widget::addChild(m_fgBoard, true); }
        }

    public:
        Widget *gfxWidget() const
        {
            return Widget::evalGetter(m_getter, this);
        }

        void setGfxWidget(Widget::VarGetter<Widget *> getter)
        {
            m_getter = std::move(getter);
        }

    public:
        Widget::ROI gfxCropROI() const
        {
            return m_vr.roi(this, nullptr);
        }

        void setGfxCropROI(Widget::VarROI vr)
        {
            m_vr = std::move(vr);
        }

    public:
        Widget::IntMargin gfxMargin() const
        {
            return Widget::IntMargin
            {
                .up    = margin(0),
                .down  = margin(1),
                .left  = margin(2),
                .right = margin(3),
            };
        }

        void setGfxMargin(Widget::VarMargin margin)
        {
            m_margin = std::move(margin);
        }

    public:
        Widget::ROI gfxResizeROI() const
        {
            return Widget::ROI
            {
                .x = margin(2) + m_vr.x(this),
                .y = margin(0) + m_vr.y(this),
                .w =         m_resize.w(this),
                .h =         m_resize.h(this),
            };
        }

        void setGfxResize(Widget::VarSize2D resize)
        {
            m_resize = std::move(resize);
        }

    public:
        int margin(int index) const
        {
            return Widget::evalSize(m_margin[index], this);
        }

    private:
        void gridHelper(this auto && self, Widget::ROIMap m, auto && func)
        {
            auto gfxWidget = self.gfxWidget();
            if(!gfxWidget){
                return;
            }

            const auto fnOp = [gfxWidget, m = std::cref(m), &func](const Widget::ROI &cr, int dx, int dy, std::optional<std::pair<int, int>> dupSize = std::nullopt)
            {
                if(!cr){
                    return;
                }

                const int dw = dupSize.has_value() ? dupSize->first  : cr.w;
                const int dh = dupSize.has_value() ? dupSize->second : cr.h;

                if(const auto cm = m.get().create({dx, dy, dw, dh})){
                    GfxCropBoard crop{{.getter = gfxWidget, .vr{cr}}};
                    if(dupSize.has_value()){
                        GfxDupBoard dup{{.w = dupSize->first, .h = dupSize->second, .getter = &crop}};
                        func(&dup, cm);
                    }
                    else{
                        func(&crop, cm);
                    }
                }
            };

            const auto r = self.gfxCropROI();

            const int mx = self.margin(2);
            const int my = self.margin(0);

            const int ox = std::max<int>(r.x, 0);
            const int oy = std::max<int>(r.y, 0);

            const int cw = gfxWidget->w();
            const int ch = gfxWidget->h();

            const auto [rw, rh] = self.m_resize.size(&self);

            fnOp({        0,         0,            r.x,            r.y}, mx          , my                                                          ); // top-left
            fnOp({      r.x,         0,            r.w,            r.y}, mx + ox     , my          , std::make_pair(rw            ,            r.y)); // top-middle
            fnOp({r.x + r.w,         0, cw - r.x - r.w,            r.y}, mx + ox + rw, my                                                          ); // top-right
            fnOp({        0,       r.y,            r.x,            r.h}, mx          , my + oy     , std::make_pair(           r.x, rh            )); // middle-left
            fnOp({      r.x,       r.y,            r.w,            r.h}, mx + ox     , my + oy     , std::make_pair(rw            , rh            )); // middle
            fnOp({r.x + r.w,       r.y, cw - r.x - r.w,            r.h}, mx + ox + rw, my + oy     , std::make_pair(cw - r.x - r.w, rh            )); // middle-right
            fnOp({        0, r.y + r.h,            r.x, ch - r.y - r.h}, mx          , my + oy + rh                                                ); // bottom-left
            fnOp({      r.x, r.y + r.h,            r.w, ch - r.y - r.h}, mx + ox     , my + oy + rh, std::make_pair(rw            , ch - r.y - r.h)); // bottom-middle
            fnOp({r.x + r.w, r.y + r.h, cw - r.x - r.w, ch - r.y - r.h}, mx + ox + rw, my + oy + rh                                                ); // bottom-right
        }

    public:
        void drawDefault(Widget::ROIMap m) const override
        {
            if(!m.calibrate(this)){
                return;
            }

            if(m_bgBoard){
                drawChild(m_bgBoard, m);
            }

            gridHelper(m, [](const auto *widget, const auto &cm)
            {
                widget->draw(cm);
            });

            if(m_fgBoard){
                drawChild(m_fgBoard, m);
            }
        }

        bool processEventDefault(const MirEvent &e, bool valid, Widget::ROIMap m) override
        {
            if(!m.calibrate(this)){
                return false;
            }

            bool takenEvent = false;
            gridHelper(m, [&e, valid, &takenEvent](auto *widget, const auto &cm)
            {
                takenEvent |= widget->processEvent(e, valid && !takenEvent, cm);
            });

            return takenEvent;
        }
};

// ===== tritexbutton.hpp =====
#include "gldevice.hpp"

class TritexButton: public ButtonBase
{
    private:
        struct TritexIDList final
        {
            std::optional<uint32_t> off  = std::nullopt;
            std::optional<uint32_t> on   = std::nullopt;
            std::optional<uint32_t> down = std::nullopt;

            decltype(auto) operator[](this auto && self, size_t index)
            {
                switch(index){
                    case  0: return self.off;
                    case  1: return self.on;
                    case  2: return self.down;
                    default: throw fflpanic("index out of range: {}", index);
                }
            }
        };

    private:
        using TritexIDFunc = std::variant<std::nullptr_t,
                                          std::function<std::optional<uint32_t>(                   )>,
                                          std::function<std::optional<uint32_t>(                int)>,
                                          std::function<std::optional<uint32_t>(const Widget *, int)>>;

    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;
            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            TritexButton::TritexIDFunc texIDFunc {};
            TritexButton::TritexIDList texIDList {};

            Button::SeffIDList seff {};

            Button::OverCBFunc onOverIn  = nullptr;
            Button::OverCBFunc onOverOut = nullptr;

            Button::ClickCBFunc onClick = nullptr;
            Button::TriggerCBFunc onTrigger = nullptr;

            int offXOnOver = 0;
            int offYOnOver = 0;

            int offXOnClick = 0;
            int offYOnClick = 0;

            bool onClickDone = true;
            bool radioMode   = false;

            Widget::VarU32Opt modColor = std::nullopt;

            Widget::InstAttrs attrs {};
            Widget::WADPair  parent {};
        };

    private:
        TritexButton::TritexIDFunc m_texIDFunc;
        TritexButton::TritexIDList m_texIDList;

    private:
        const Widget::VarU32 m_modColor;

    private:
        double m_accuBlinkTime = 0.0;
        std::optional<std::tuple<unsigned, unsigned, unsigned>> m_blinkTime = {}; // {off, on} in ms

    private:
        ImageBoard m_img;

    public:
        TritexButton(TritexButton::InitArgs);

    public:
        void drawDefault(Widget::ROIMap) const override;

    public:
        void setTexIDList(const TritexButton::TritexIDList &texIDList)
        {
            m_texIDFunc = nullptr;
            m_texIDList = texIDList;
        }

        void setTexIDFunc(TritexButton::TritexIDFunc texIDFunc)
        {
            m_texIDFunc = std::move(texIDFunc);
        }

        void setBlinkTime(unsigned offTime, unsigned onTime, unsigned activeTotalTime = 0)
        {
            m_blinkTime = std::make_tuple(offTime, onTime, activeTotalTime);
            m_accuBlinkTime = 0.0;
        }

        void stopBlink()
        {
            m_blinkTime.reset();
            m_accuBlinkTime = 0.0;
        }

    public:
        void updateDefault(double fUpdateTime) override
        {
            if(m_blinkTime.has_value()){
                m_accuBlinkTime += fUpdateTime;
                if(const auto activeTotalTime = std::get<2>(m_blinkTime.value()); activeTotalTime > 0 && m_accuBlinkTime > activeTotalTime){
                    m_blinkTime.reset();
                }
            }
        }

    private:
        GLTexID evalGfxTexture     (std::optional<int> = std::nullopt) const;
        GLTexID evalGfxTextureValid(                                 ) const; // search the first valid Texture

    public:
        int w() const override { return GLDeviceHelper::getTextureWidth (evalGfxTextureValid(), 0); }
        int h() const override { return GLDeviceHelper::getTextureHeight(evalGfxTextureValid(), 0); }
};

// ===== acbutton.hpp =====
#include <unordered_map>

class ProcessRun;
class ACButton: public TrigfxButton
{
    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            ProcessRun *proc = nullptr;
            std::vector<std::string> names {};

            Widget::WADPair parent {};
        };

    private:
        ProcessRun *m_proc;
        const std::unordered_map<std::string, uint32_t> m_texMap;

    private:
        size_t m_buttonIndex = 0;
        const std::vector<std::string> m_buttonNameList;

    private:
        ImageBoard m_img;
        TextBoard  m_text;
        ItemPair   m_gfxCanvas;

    public:
        ACButton(ACButton::InitArgs);

    private:
        const std::string &buttonName() const
        {
            return m_buttonNameList.at(m_buttonIndex);
        }
};

// ===== alphaonbutton.hpp =====
class AlphaOnButton: public TrigfxButton
{
    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            int onOffX = 0;
            int onOffY = 0;

            int onRadius = 0;

            Widget::VarU32 modColor  = colorf::WHITE_A255;
            Widget::VarU32 downTexID = 0U;

            Button::OverCBFunc onOverIn  = nullptr;
            Button::OverCBFunc onOverOut = nullptr;

            Button::ClickCBFunc onClick = nullptr;
            Button::TriggerCBFunc onTrigger = nullptr;

            bool triggerOnDone = true;

            Widget::WADPair parent {};
        };

    private:
        const int m_onOffX;
        const int m_onOffY;
        const int m_onRadius;

    private:
        ImageBoard m_down;
        Widget     m_on;  // hold cover
        Widget     m_off; // placeholder for off state, no gfx effect

    public:
        AlphaOnButton(AlphaOnButton::InitArgs);
};

// ===== menuitem.hpp =====

class MenuItem: public Widget
{
    public:
        constexpr static int INDICATOR_W = 9;
        constexpr static int INDICATOR_H = 5;

    private:
        struct SubWidgetArgs final
        {
            dir8_t dir = DIR_UPRIGHT;

            Widget *widget     = nullptr;
            bool    autoDelete = false;
        };

        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarMargin margin {};
            Menu::ItemSize  itemSize {}; // margin not included

            Widget::WADPair         gfxWidget {};
            MenuItem::SubWidgetArgs subWidget {};

            Widget::VarBool showIndicator = false;
            Widget::VarBool showSeparator = false;
            Widget::VarBool expandOnHover = false;

            Widget::VarU32 bgColor = 0U;
            Menu::ClickCBFunc onClick = nullptr;

            Widget::WADPair parent {};
        };

    private:
        Widget       *m_subWidget;
        TrigfxButton *m_gfxButton;

    private:
        Menu::ItemSize m_itemSize;

    private:
        Widget m_gfxWidgetCrop;
        GfxShapeBoard m_indicator; // a small triangle indicates submenu exists

    private:
        ItemPair m_canvas;
        MarginWrapper m_wrapper;

    public:
        MenuItem(MenuItem::InitArgs);

    public:
        auto gfxWidget(this auto &&self)
        {
            return self.m_gfxWidgetCrop.firstChild();
        }

        auto subWidget(this auto &&self)
        {
            return self.m_subWidget;
        }

    public:
        void setItemWidth (Widget::VarSizeOpt argItemWidth ){ m_itemSize.w = std::move(argItemWidth ); }
        void setItemHeight(Widget::VarSizeOpt argItemHeight){ m_itemSize.h = std::move(argItemHeight); }

    public:
        int getItemWidth () const { return m_gfxWidgetCrop.w(); }
        int getItemHeight() const { return m_gfxWidgetCrop.h(); }

    public:
        void drawDefault(Widget::ROIMap m) const override;
        bool processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m) override;
};

// ===== menubutton.hpp =====
// menu button to expand a menu board
// this gui system does not support menubar
//
// +--------+
// |  Proj  |             <--- menu button
// +--------+----------+
// |  Open     CTRL+O  |  <--- menu item --+
// +-------------------+                   |
// |  Save     CTRL+S  |  <--- menu item --+- menu board
// +-------------------+

class MenuButton: public MenuItem
{
    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarMargin margin {};
            Menu::ItemSize  itemSize {}; // margin not included

            Widget::WADPair gfxWidget {};
            Widget::WADPair subWidget {};

            Widget::VarBool expandOnHover = false;
            Widget::VarU32  bgColor = 0U;

            Widget::WADPair parent {};
        };

    public:
        MenuButton(MenuButton::InitArgs);
};

// ===== gfxdirbutton.hpp =====
class GfxDirButton: public TrigfxButton
{
    private:
        struct TriangleArgs final
        {
            Widget::VarDir dir = DIR_UP;

            Widget::VarSizeOpt w = std::nullopt;
            Widget::VarSizeOpt h = std::nullopt;

            Widget::VarU32 color = colorf::WHITE_A255;
        };

        struct FrameArgs final
        {
            Widget::VarBool show = true;
            Widget::VarU32 color = colorf::WHITE_A255;
        };

        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarSize w = 0;
            Widget::VarSize h = 0;

            GfxDirButton::TriangleArgs triangle {};
            GfxDirButton::FrameArgs frame {};

            Button::TriggerCBFunc onTrigger = nullptr;
            Widget::WADPair parent {};
        };

    private:
        GfxShapeBoard m_gfxDrawer;

    public:
        GfxDirButton(GfxDirButton::InitArgs);
};

// ===== sliderbase.hpp =====
// +----------------------------- InitArgs::{dir, x, y}
// |
// v     +---+
// *-----|   |----------------+
// |     |   |                |
// +-----|   |----------------+
//       +---+    ^
//         ^      |
//         |      +-------------- bar
//         +--------------------- slider

#include <climits>

class SliderBase: public Widget
{
    protected:
        struct BarArgs final
        {
            // full widget's location is decided by bar position and size
            // slider position and size are relative to bar

            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarSize w = 0;
            Widget::VarSize h = 0;

            bool v = true; // vertical bar
        };

        struct SliderArgs final
        {
            // value center of slider may not be at the geometric center
            // can happen when using tex as slider

            // (cx, cy) overlaps with bar geometric center when slider value is 0.5, in pixel-level

            Widget::VarIntOpt cx = std::nullopt;
            Widget::VarIntOpt cy = std::nullopt;

            Widget::VarSize w = 10;
            Widget::VarSize h = 10;
        };

        struct BarBgWidget final
        {
            Widget::VarInt ox = 0; // offset (ox, oy) that overlaps with bar DIR_UPLEFT corner
            Widget::VarInt oy = 0;

            Widget *widget     = nullptr;
            bool    autoDelete = false;
        };

    private:
        struct InitArgs final
        {
            BarArgs bar {};
            SliderArgs slider {};

            float value = 0.0f;
            Widget::VarCheckFunc<float> checkFunc = nullptr;

            BarBgWidget                          bgWidget {};
            MarginContainer::ContainedWidget    barWidget {};
            MarginContainer::ContainedWidget sliderWidget {};

            Widget::VarUpdateFunc<float> onChange = nullptr;
            Widget::WADPair parent {};
        };

    private:
        float m_value;
        int   m_sliderState = BEVENT_OFF;

    private:
        Widget::VarCheckFunc<float> m_checkFunc;

    private:
        uint64_t m_bgWidgetID = 0;
        std::optional<std::pair<Widget::VarInt, Widget::VarInt>> m_bgOff;

    private:
        const BarArgs m_barArgs;
        const SliderArgs m_sliderArgs;

    private:
        const Widget::VarUpdateFunc<float> m_onChange;

    private:
        MarginContainer m_bar;
        MarginContainer m_slider;

    private:
        GfxShapeBoard m_debugDraw;

    public:
        SliderBase(SliderBase::InitArgs);

    public:
        bool processEventDefault(const MirEvent &, bool, Widget::ROIMap) override;

    public:
        int sliderState() const
        {
            return m_sliderState;
        }

    public:
        bool vbar() const
        {
            return m_barArgs.v;
        }

        float getValue() const
        {
            return m_value;
        }

    public:
        void setValue(float, bool); // force set
        void addValue(float, bool);

    protected:
        float pixel2Value(int) const;

    public:
        Widget::ROI getBarROI(int, int) const;

    public:
        Widget::ROI getSliderROI(int, int) const;
        std::tuple<int, int> getValueCenter(int, int) const;

    protected:
        bool inSlider(int, int, Widget::ROIMap) const;

    public:
        void setBarBgWidget(Widget::VarInt, Widget::VarInt, Widget *, bool);

    private:
        int sliderXAtValueFromBar(float, int) const; // only depends on barArgs and sliderArgs
        int sliderYAtValueFromBar(float, int) const; // ....

    private:
        std::optional<int> bgXFromBar(int) const;
        std::optional<int> bgYFromBar(int) const;

    private:
        int widgetXFromBar(int barX) const { return std::min<int>({barX, sliderXAtValueFromBar(0.0f, barX), bgXFromBar(barX).value_or(INT_MAX)}); }
        int widgetYFromBar(int barY) const { return std::min<int>({barY, sliderYAtValueFromBar(0.0f, barY), bgYFromBar(barY).value_or(INT_MAX)}); }
};

// ===== texslider.hpp =====
#include "totype.hpp"

class TexSlider: public SliderBase
{
    protected:
        using SliderBase::BarArgs;
        using SliderBase::BarBgWidget;

    private:
        struct InitArgs final
        {
            BarArgs bar {};

            int index = 0;
            float value = 0.0f;

            BarBgWidget bgWidget {};

            Widget::VarUpdateFunc<float> onChange = nullptr;
            Widget::WADPair parent {};
        };

    private:
        struct SliderTexInfo
        {
            // define the texture center
            // some slider textures are not in good shape then not using the (w / 2, h / 2)

            const int ox;
            const int oy;

            const int cover;
            const uint32_t texID;
        };

        constexpr static SliderTexInfo m_sliderTexInfoList []
        {
            { 7,  7, 6, 0X00000080},
            { 8,  8, 7, 0X00000081},
            { 7,  8, 5, 0X00000088},
            {10, 12, 7, 0X00000089},
            {13, 15, 8, 0X0000008A},
        };

    private:
        static constexpr auto getSliderTexInfo(int index)
        {
            fflassert(index >= 0);
            fflassert(index < static_cast<int>(std::size(m_sliderTexInfoList)));
            return m_sliderTexInfoList + index;
        }

    public:
        TexSlider(TexSlider::InitArgs);
};

// ===== texinputbackground.hpp =====

//   texID: 0X00000460
//
//   up and down side borders are of 2 pixels:
//
//      1. use 3 pixels in GfxResizeBoard resize ROI then dark/light pixel doesn't repeat
//      2. getInputROI() still uses 2 pixel border for input area inside
//
//   |<-3->|             v
//   +-----------------  -
//   |        border     2
//   |     +-----------  -  -
//   |     |             ^  ^          ---- 1 pixel dark
//   |     |                |          --+
//   |     |                5          --+- 3 pixel gray and can repeat
//   |     |                |          --+
//   |     |                v  v       ---- 1 pixel light
//   |     +-----------     -  -
//   |        border           2
//   +-----------------        -
//                             ^
//

class TexInputBackground: public Widget
{
    public:
        constexpr static int SLOT_FIXED_EDGE_SIZE = 9;

    public:
        constexpr static int MIN_WIDTH  = 6; // because tex has border pixels
        constexpr static int MIN_HEIGHT = 6;

    public:
        static Widget::ROI fromInputROI(bool, Widget::ROI);

    public:
        static Widget::IntSize2D borderSize(bool);

    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarSize w = 0;
            Widget::VarSize h = 0;

            bool v = true;
            Widget::WADPair parent {};
        };

    private:
        ImageBoard m_img;
        GfxResizeBoard m_resize;

    public:
        TexInputBackground(TexInputBackground::InitArgs);

    public:
        bool v() const noexcept
        {
            return m_img.transposed();
        }

    public:
        Widget::ROI getInputROI() const;

    public:
        void setInputSize(Widget::VarSize2D);
        void setInputSize(Widget::VarSize, Widget::VarSize);
};

// ===== texsliderbar.hpp =====

class TexSliderBar: public TexSlider
{
    public:
        constexpr static int BAR_FIXED_EDGE_SIZE = 5;

    protected:
        using TexSlider::BarArgs;
        using TexSlider::BarBgWidget;

    private:
        struct InitArgs final
        {
            BarArgs bar {};

            int index = 0;
            float value = 0.0f;

            Widget::VarUpdateFunc<float> onChange = nullptr;
            Widget::WADPair parent {};
        };

    private:
        TexInputBackground m_bg;

    private:
        ImageBoard m_imgBar; // 5 pixels height, horizontal direction is identical
        GfxDupBoard   m_bar;

    public:
        TexSliderBar(TexSliderBar::InitArgs);
};

// ===== checkbox.hpp =====

class CheckBox: public Widget
{
    public:
        using BoolGetter = std::variant<std::nullptr_t,
                                        std::function<bool()>,
                                        std::function<bool(const Widget *)>>;

        using BoolSetter = std::variant<std::nullptr_t,
                                        std::function<void(bool)>,
                                        std::function<void(Widget *, bool)>>;

        using TriggerFunc = std::variant<std::nullptr_t,
                                        std::function<void(bool)>,
                                        std::function<void(Widget *, bool)>>;

    public:
        static bool evalBoolGetter (const CheckBox::BoolGetter  &, const Widget *);
        static void evalBoolSetter (      CheckBox::BoolSetter  &,       Widget *, bool);
        static void evalTriggerFunc(      CheckBox::TriggerFunc &,       Widget *, bool);

        static bool hasBoolGetter (const CheckBox::BoolGetter  &);
        static bool hasBoolSetter (const CheckBox::BoolSetter  &);
        static bool hasTriggerFunc(const CheckBox::TriggerFunc &);

    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarSizeOpt w = std::nullopt;
            Widget::VarSizeOpt h = std::nullopt;

            Widget::VarU32 color = colorf::RGBA(231, 231, 189, 128);

            CheckBox::BoolGetter  getter   = nullptr; // use m_innVal if getter is not provided
            CheckBox::BoolSetter  setter   = nullptr;
            CheckBox::TriggerFunc onChange = nullptr;

            Widget::WADPair parent {};
        };

    private:
        bool m_innVal = false;

    private:
        Widget::VarU32 m_color;

    private:
        CheckBox::BoolGetter  m_valGetter;
        CheckBox::BoolSetter  m_valSetter;
        CheckBox::TriggerFunc m_valOnChange;

    private:
        ImageBoard m_img;

    private:
        GfxShapeBoard m_box;

    public:
        CheckBox(CheckBox::InitArgs);

    public:
        bool processEventDefault(const MirEvent &, bool, Widget::ROIMap) override;

    public:
        void setColor(Widget::VarU32 color)
        {
            m_color = std::move(color);
        }

    public:
        void toggle();

    public:
        bool getter(    ) const;
        void setter(bool);

    public:
        bool rawGetter(    ) const;
        void rawSetter(bool);
};

// ===== checklabel.hpp =====

class CheckLabel: public Widget
{
    public:
        using BoolGetter  = CheckBox::BoolGetter;
        using BoolSetter  = CheckBox::BoolSetter;
        using TriggerFunc = CheckBox::TriggerFunc;

        constexpr static auto evalBoolGetter  = CheckBox::evalBoolGetter;
        constexpr static auto evalBoolSetter  = CheckBox::evalBoolSetter;
        constexpr static auto evalTriggerFunc = CheckBox::evalTriggerFunc;

        constexpr static auto hasBoolGetter  = CheckBox::hasBoolGetter;
        constexpr static auto hasBoolSetter  = CheckBox::hasBoolSetter;
        constexpr static auto hasTriggerFunc = CheckBox::hasTriggerFunc;

    protected:
        struct BoxArgs final
        {
            Widget::VarSizeOpt w = std::nullopt;
            Widget::VarSizeOpt h = std::nullopt;
            Widget::VarU32 color = colorf::RGBA(231, 231, 189, 128);
        };

        struct LabelArgs final
        {
            const char8_t     *text {};
            Widget::FontConfig font {};
        };

    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            bool boxFirst = true;
            Widget::VarSizeOpt gap = std::nullopt;

            CheckLabel::  BoxArgs   box {};
            CheckLabel::LabelArgs label {};

            CheckLabel::BoolGetter  getter   = nullptr; // widget is CheckLabel
            CheckLabel::BoolSetter  setter   = nullptr;
            CheckLabel::TriggerFunc onChange = nullptr;

            Widget::WADPair parent {};
        };

    private:
        bool m_hoverColor = false;

    private:
        CheckBox m_box;
        LabelBoard m_label;

    public:
        CheckLabel(CheckLabel::InitArgs);

    public:
        bool processEventDefault(const MirEvent &, bool, Widget::ROIMap) override;

    public:
        bool getter() const
        {
            return m_box.getter();
        }

        void setter(bool val)
        {
            m_box.setter(val);
        }

    public:
        void setFocus(bool argFocus) override
        {
            Widget::setFocus(false);
            if(argFocus){
                m_box.setFocus(true);
            }
        }
};

// ===== radioselector.hpp =====

    // |<------width------->|
    //        gap
    //     ->|   |<-
    // +-----+---+----------+              -
    // |     |   |          |              ^
    // | ( ) |   |  Widget  | |            |
    // |     |   |          | v            |
    // +-----+---+----------+ -            |
    // |                    | item space   +- height
    // +-----+---+----------+ -            |
    // |     |   |          | ^            |
    // |     |   |          | |            |
    // | (x) |   |  Widget  |              |
    // |     |   |          |              |
    // |     |   |          |              v
    // +-----+---+----------+              -
    //
    // size in auto-scaling mode
    //

class RadioSelector: public Widget
{
    private:
        class InternalRadioButton: public TrigfxButton
        {
            using TrigfxButton::TrigfxButton;
        };

    private:
        const int m_gap;
        const int m_itemSpace;

    private:
        Widget *m_selected = nullptr;

    private:
        std::function<const Widget *(const Widget * /* self */                                           )> m_valGetter;
        std::function<void          (      Widget * /* self */, Widget * /* child */                     )> m_valSetter;
        std::function<void          (      Widget * /* self */, Widget * /* child */, bool /* selected */)> m_valOnChange;

    private:
        ImageBoard m_imgOff;
        ImageBoard m_imgOn;
        ImageBoard m_imgDown;

    public:
        RadioSelector(Widget::VarDir,

                Widget::VarInt,
                Widget::VarInt,

                int = 5, // gap
                int = 5, // item space

                std::initializer_list<std::tuple<Widget *, bool>> = {},

                std::function<const Widget *(const Widget *                )> = nullptr,
                std::function<void          (      Widget *, Widget *      )> = nullptr,
                std::function<void          (      Widget *, Widget *, bool)> = nullptr,

                Widget * = nullptr,
                bool     = false);

    public:
        void append(Widget *, bool);

    public:
        const Widget *getter(        ) const;
        void          setter(Widget *);

    public:
        auto foreachRadioButton(std::invocable<Widget *> auto f)
        {
            constexpr bool hasBoolResult = std::is_same_v<std::invoke_result_t<decltype(f), Widget *>, bool>;
            if constexpr (hasBoolResult){
                return foreachChild([&f](Widget *child, bool)
                {
                    if(dynamic_cast<RadioSelector::InternalRadioButton *>(child)){
                        if(f(child)){
                            return true;
                        }
                    }
                    return false;
                });
            }
            else{
                foreachChild([&f](Widget *child, bool)
                {
                    if(dynamic_cast<RadioSelector::InternalRadioButton *>(child)){
                        f(child);
                    }
                });
            }
        }

        auto foreachRadioButton(std::invocable<const Widget *> auto f) const
        {
            constexpr bool hasBoolResult = std::is_same_v<std::invoke_result_t<decltype(f), const Widget *>, bool>;
            if constexpr (hasBoolResult){
                return foreachChild([&f](const Widget *child, bool)
                {
                    if(dynamic_cast<const RadioSelector::InternalRadioButton *>(child)){
                        if(f(child)){
                            return true;
                        }
                    }
                    return false;
                });
            }
            else{
                foreachChild([&f](const Widget *child, bool)
                {
                    if(dynamic_cast<const RadioSelector::InternalRadioButton *>(child)){
                        f(child);
                    }
                });
            }
        }

        auto foreachRadioWidget(std::invocable<Widget *> auto f)
        {
            constexpr bool hasBoolResult = std::is_same_v<std::invoke_result_t<decltype(f), Widget *>, bool>;
            if constexpr (hasBoolResult){
                return foreachRadioButton([&f](Widget *button)
                {
                    return f(getRadioWidget(button));
                });
            }
            else{
                foreachRadioButton([&f](Widget *button)
                {
                    f(getRadioWidget(button));
                });
            }
        }

        auto foreachRadioWidget(std::invocable<const Widget *> auto f) const
        {
            constexpr bool hasBoolResult = std::is_same_v<std::invoke_result_t<decltype(f), const Widget *>, bool>;
            if constexpr (hasBoolResult){
                return foreachRadioButton([&f](const Widget *button)
                {
                    return f(getRadioWidget(button));
                });
            }
            else{
                foreachRadioButton([&f](const Widget *button)
                {
                    f(getRadioWidget(button));
                });
            }
        }

    public:
        static Widget *getRadioWidget(Widget *button)
        {
            fflassert(button);
            fflassert(dynamic_cast<RadioSelector::InternalRadioButton *>(button));
            return button->data().has_value() ? std::any_cast<Widget *>(button->data()) : nullptr;
        }

        static const Widget *getRadioWidget(const Widget *button)
        {
            fflassert(button);
            fflassert(dynamic_cast<const RadioSelector::InternalRadioButton *>(button));
            return button->data().has_value() ? std::any_cast<Widget *>(button->data()) : nullptr;
        }
};

// ===== valueselector.hpp =====

class ValueSelector: public Widget
{
    private:
        struct InputArgs final
        {
            Widget::VarSize w = 0;
            Widget::VarInt enableIME = IME_DISABLE;

            Widget::FontConfig font {};
            InputLine::CursorArgs cursor {};

            std::function<void(std::string)> onChange = nullptr;
            std::function<bool(std::string)> validate = nullptr;
        };

        struct ButtonArgs final
        {
            Widget::VarSize w = 0;
        };

        struct InitArgs
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarSize h = 0;

            ValueSelector::InputArgs   input {};
            ValueSelector::ButtonArgs button {};

            Widget::VarBool finizeOnChange = true;

            Button::TriggerCBFunc   upTrigger = nullptr;
            Button::TriggerCBFunc downTrigger = nullptr;

            Widget::WADPair parent {};
        };

    private:
        InputLine m_input;

    private:
        GfxDirButton m_up;
        GfxDirButton m_down;

    private:
        ItemFlex m_vflex;
        ItemFlex m_hflex;

    private:
        GfxShapeBoard m_frame;

    public:
        std::string getValue() const
        {
            return m_input.getRawString();
        }

        void setValue(std::string value)
        {
            m_input.setInput(value.c_str());
        }

    public:
        ValueSelector(ValueSelector::InitArgs);
};

// ===== integerselector.hpp =====

class IntegerSelector: public ValueSelector
{
    public:
        struct InputArgs final
        {
            Widget::VarSize w = 0;
            Widget::FontConfig font {};
            InputLine::CursorArgs cursor {};

            std::function<void(std::string)> onChange = nullptr;
            std::function<bool(std::string)> validate = nullptr;
        };

        struct ButtonArgs final
        {
            Widget::VarSize w = 0;
        };

    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarSize h = 0;

            IntegerSelector:: InputArgs  input {};
            IntegerSelector::ButtonArgs button {};

            Button::TriggerCBFunc   upTrigger = nullptr;
            Button::TriggerCBFunc downTrigger = nullptr;

            Widget::VarGetter<std::pair<int, int>> range {};

            Widget::WADPair parent {};
        };

    public:
        IntegerSelector(IntegerSelector::InitArgs);

    public:
        std::optional<int> getInt() const;
};

// ===== textinput.hpp =====

class TextInput: public Widget
{
    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            const char8_t *labelFirst  = nullptr;
            const char8_t *labelSecond = nullptr;

            Widget::FontConfig font {};

            Widget::VarSize gapFirst  = 3;
            Widget::VarSize gapSecond = 3;

            Widget::VarInt    enableIME = IME_DISABLE;
            Widget::VarSize2D inputSize {};

            std::function<void()> onTab = nullptr;
            std::function<void()> onCR  = nullptr;

            Widget::WADPair parent {};
        };

    private:
        LabelBoard *m_labelFirst;

    private:
        TexInputBackground m_bg;

    private:
        LabelBoard *m_labelSecond;

    private:
        InputLine m_input;

    public:
        TextInput(TextInput::InitArgs);

    public:
        bool processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m) override
        {
            return m_input.processEventParent(event, valid, m);
        }
};

// ===== passwordbox.hpp =====

class PasswordBox: public InputLine
{
    private:
        using CursorArgs = InputLine::CursorArgs;

    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;
            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarSizeOpt w = 0;
            Widget::VarSizeOpt h = 0;

            Widget::VarBool security = true;

            Widget::FontConfig font {};
            PasswordBox::CursorArgs cursor {};

            std::function<void()>            onTab    = nullptr;
            std::function<void()>            onCR     = nullptr;
            std::function<void(std::string)> onChange = nullptr;

            Widget::WADPair parent {};
        };

    private:
        Widget::VarBool m_security;

    private:
        std::string m_passwordString;

    public:
        PasswordBox(PasswordBox::InitArgs args)
            : InputLine
              {{
                  .dir = std::move(args.dir),

                  .x = std::move(args.x),
                  .y = std::move(args.y),

                  .w = std::move(args.w),
                  .h = std::move(args.h),

                  .font = std::move(args.font),
                  .cursor = std::move(args.cursor),

                  .onTab    = std::move(args.onTab),
                  .onCR     = std::move(args.onCR),
                  .onChange = std::move(args.onChange),

                  .parent = std::move(args.parent),
              }}

            , m_security(std::move(args.security))
        {}

    public:
        bool processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m) override
        {
            const auto result = InputLine::processEventDefault(event, valid, m);
            if(security()){
                const auto inputString = getRawString();
                if(inputString.size() + 1 == m_passwordString.size()){
                    // delete one char
                    m_passwordString.erase(m_cursor, 1);
                }

                else if(inputString.size() == m_passwordString.size() + 1){
                    // insert one char
                    m_passwordString.insert(m_cursor - 1, 1, inputString[m_cursor - 1]);
                    deleteChar();
                    insertChar('*');
                }

                else if(inputString.size() != m_passwordString.size()){
                    throw fflpanic("password box input error");
                }
            }
            return result;
        }

    public:
        std::string getPasswordString() const
        {
            return security() ? m_passwordString : getRawString();
        }

        void clear() override
        {
            InputLine::clear();
            m_passwordString.clear();
        }

    public:
        void setSecurity(Widget::VarBool argSecurity)
        {
            m_security = std::move(argSecurity);
        }

        bool security() const
        {
            return Widget::evalBool(m_security, this);
        }
};

// ===== textshadowboard.hpp =====

class TextShadowBoard: public Widget
{
    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarInt shadowX = 0;
            Widget::VarInt shadowY = 0;

            Widget::VarStrFunc textFunc {};
            Widget::FontConfig font {};

            Widget::VarU32       shadowColor = colorf::BLACK + colorf::A_SHF(128);
            Widget::VarBlendMode blendMode   = MIR_BLENDMODE_BLEND;

            Widget::WADPair parent {};
        };

    private:
        TextBoard m_textShadow;
        TextBoard m_text;

    public:
        TextShadowBoard(TextShadowBoard::InitArgs);
};

// ===== itembox.hpp =====

class ItemBox: public Widget
{
    #include "itemflex.api.hpp"

    private:
        const bool m_vbox;
        const ItemAlign m_align;

    private:
        Widget *m_canvas;

    private:
        Widget::VarSize m_headSpace;
        Widget::VarSize m_itemSpace;
        Widget::VarSize m_tailSpace;

    private:
        Widget::VarSizeOpt m_fixedEdgeSize;

    private:
        int m_headSpaceEval = 0;
        int m_itemSpaceEval = 0;
        int m_tailSpaceEval = 0;

        int m_fixedEdgeSizeEval = 0;
        int m_flexibleEdgeSizeEval = 0;

    public:
        ItemBox(ItemBox::InitArgs);

    private:
        void updateMarginContainers();

        void updateFlexEdgeSize();
        void updateFixedEdgeSize();

        void updateFlexEdgeOffset(const Widget *);
        void updateFixedEdgeOffset();

    private:
        const Widget *firstShowContainer() const { return findShowContainer(true ); }
        const Widget * lastShowContainer() const { return findShowContainer(false); }

    private:
        const Widget *findShowContainer(bool foward) const;

    private:
        void doRemoveContainer(const MarginContainer *);
};

void ItemBox::clearItem(std::invocable<const Widget *, bool> auto func)
{
    m_canvas->foreachChild([func, this](auto container, bool)
    {
        if(auto mc = dynamic_cast<MarginContainer *>(container)){
            if(const auto [item, autoDelete] = mc->containedPair(); func(item, autoDelete)){
                mc->clearContained(true);
            }
        }
    });

    // update offset
    // doRemoveContainer does offset update

    m_canvas->foreachChild([this](auto container, bool)
    {
        if(auto mc = dynamic_cast<MarginContainer *>(container); !mc->contained()){
            doRemoveContainer(mc);
        }
    });
}

void ItemBox::clearItem()
{
    clearItem([](const Widget *, bool){ return true; });
}

auto ItemBox::foreachItem(this auto && self, bool forward, auto func)
{
    return self.m_canvas->foreachChild(forward, [func](auto container, bool)
    {
        const auto [item, autoDelete] = dynamic_cast<MarginContainer *>(container)->containedPair();
        return func(item, autoDelete);
    });
}

auto ItemBox::foreachItem(this auto && self, auto func)
{
    return self.foreachItem(true, func);
}

// ===== menuboard.hpp =====

class MenuBoard: public Widget
{
    public:
        struct AddItemArgs final
        {
            Widget::WADPair gfxWidget {};
            Widget::WADPair subWidget {};

            Widget::VarBool showIndicator = false;
            Widget::VarBool showSeparator = false;
        };

    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarSizeOpt fixed = std::nullopt; // margin not included
            Widget::VarMargin margin = {};

            Widget::VarSize         corner = 0;
            Widget::VarSize      itemSpace = 0;
            Widget::VarSize separatorSpace = 0;

            std::vector<MenuBoard::AddItemArgs> itemList {};
            Menu::ClickCBFunc onClick = nullptr;

            Widget::InstAttrs attrs {};
            Widget::WADPair  parent {};
        };

    private:
        const Widget::VarSize m_itemSpace;
        const Widget::VarSize m_separatorSpace;

    private:
        Menu::ClickCBFunc m_onClickMenu;

    private:
        ItemBox m_canvas; // holding all menu items

    public:
        MenuBoard(MenuBoard::InitArgs);

    public:
        void addMenu(MenuBoard::AddItemArgs);
};

// ===== pullmenu.hpp =====

class PullMenu: public Widget
{
    private:
        struct LabelCropArgs final
        {
            const char8_t *text = nullptr;

            Widget::VarSizeOpt w = std::nullopt;
            Widget::VarSizeOpt h = std::nullopt;
        };

        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            PullMenu::LabelCropArgs label {};
            PullMenu::LabelCropArgs title {};

            Widget::VarBool   showButton = false;
            Widget::VarSizeOpt menuFixed = std::nullopt; // menu item width, margin not included

            std::vector<MenuBoard::AddItemArgs> itemList {};
            Menu::ClickCBFunc onClick = nullptr;

            Widget::WADPair parent {};
        };

    private:
        LabelBoard   m_tips;
        GfxCropBoard m_tipsCrop;

    private:
        LabelBoard         m_title; // crop by MenuButton
        TexInputBackground m_titleBg;

    private:
        ImageBoard m_imgOff;
        ImageBoard m_imgOn;
        ImageBoard m_imgDown;

    private:
        TrigfxButton m_button;

    private:
        ItemFlex m_flex; // hold label/bg/button

    private:
        MenuBoard  m_menuBoard;
        MenuButton m_menuButton;

    public:
        PullMenu(PullMenu::InitArgs);

    public:
        auto getTips (this auto && self) { return std::addressof(self.m_tips ); }
        auto getTitle(this auto && self) { return std::addressof(self.m_title); }

    public:
        void setFocus(bool) override;

    public:
        Widget::IntSize2D fixedSize() const
        {
            return{m_flex.w(), m_flex.h()};
        }

    public:
        void addMenu(MenuBoard::AddItemArgs args)
        {
            m_menuBoard.addMenu(std::move(args));
        }
};

// ===== gfxdebugboard.hpp =====

class GfxDebugBoard: public Widget
{
    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;
            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            bool hflip  = false;
            bool vflip  = false;
            int  rotate = 0;

            Widget::WADPair parent {};
        };

    private:
        GfxShapeBoard m_bg;

    private:
        ImageBoard m_img;

    private:
        Widget          m_srcWidget;
        Widget                  m_imgCanvas;
        MarginWrapper                   m_imgContainer;
        GfxShapeBoard                  m_imgFrame;

        SliderBase              m_imgResizeHSlider;
        SliderBase              m_imgResizeVSlider;

        SliderBase              m_cropHSlider_0;
        SliderBase              m_cropHSlider_1;
        SliderBase              m_cropVSlider_0;
        SliderBase              m_cropVSlider_1;

        TextBoard               m_texSize;
        TextBoard               m_imgSize;
        TextBoard               m_roiInfo;
        TextBoard               m_resizeInfo;
        TextBoard               m_marginInfo;

    private:
        Widget          m_dstWidget;
        Widget                  m_dstCanvas;
        GfxResizeBoard                  m_resizeBoard;

        SliderBase              m_marginHSlider_0;
        SliderBase              m_marginHSlider_1;
        SliderBase              m_marginVSlider_0;
        SliderBase              m_marginVSlider_1;

    public:
        GfxDebugBoard(GfxDebugBoard::InitArgs);

    private:
        bool checkResizeW(float, float) const;
        bool checkResizeH(float, float) const;

    public:
        Widget::ROI getROI() const; // take img's DIR_UPLEFT as (0, 0)
};

// ===== texaniboard.hpp =====

class TexAniBoard: public Widget
{
    private:
        uint32_t m_startTexID;
        size_t   m_frameCount;
        size_t   m_fps;

    private:
        bool m_fadeInout;
        bool m_loop;

    private:
        double m_accuTime = 0;

    private:
        GfxShapeBoard m_cropArea;

    public:
        TexAniBoard(
                Widget::VarDir,
                Widget::VarInt,
                Widget::VarInt,

                uint32_t,
                size_t,
                size_t,

                bool,
                bool = true,

                Widget * = nullptr,
                bool     = false);

    public:
        void updateDefault(double fUpdateTime) override
        {
            m_accuTime += fUpdateTime;
        }

    private:
        std::tuple<int, uint8_t> getDrawFrame() const;
};

// ===== wmdaniboard.hpp =====
class WMDAniBoard: public TexAniBoard
{
    public:
        WMDAniBoard(
                dir8_t argDir,

                int argX,
                int argY,

                Widget * argParent = nullptr,
                bool     argAutoDelete = false)

            : TexAniBoard
              {
                  argDir,
                  argX,
                  argY,

                  0X04000010,
                  10,
                  8,

                  true,
                  true,

                  argParent,
                  argAutoDelete,
              }
        {}
};

// ===== baseframeboard.hpp =====

// ProgUse 0X00000450.PNG
// for corners are identical squares of size 58 x 58
// left side squares can be smaller but use same size for simplicity
//
//                       +------+
//                       |      |
//                       v      |
// +=--+---------------+--=+    |
// |   |               |   |<---+--- 58 x 58
// +---+---------------+---+
// |   |               |   |
// |   |               |   |
// |   |               |   |
// |   |               |   |
// +---+---------------+---+
// |   |               | O |
// +=--+---------------+--=+ (510 x 468)

class BaseFrameBoard: public Widget
{
    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;
            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            Widget::VarSize w = 0;
            Widget::VarSize h = 0;

            Widget::WADPair parent {};
        };

    private:
        static constexpr      int m_cornerSize = 58;
        static constexpr uint32_t m_frameTexID = 0X00000450;

    private:
        ImageBoard m_frame;
        GfxResizeBoard m_frameBoard;

    private:
        TritexButton m_close;

    public:
        BaseFrameBoard(BaseFrameBoard::InitArgs);
};

// ===== dirrectangle.hpp =====

class DirRectangle: public Widget
{
    private:
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            struct TriangleArgs
            {
                Widget::VarDir   dir = DIR_UP;
                Widget::VarSize maxW = 0; // may shrink if no enough space
                Widget::VarSize    h = 0;
            }
            triangle {};

            struct RectangleArgs
            {
                Widget::VarSize r = 0;
                Widget::VarSize w = 0;
                Widget::VarSize h = 0;
            }
            rectangle {};

            Widget::VarU32 bgColor = colorf::WHITE_A255;
            Widget::VarU32 fgColor = 0U;

            Widget::WADPair parent {};
        };

    public:
        explicit DirRectangle(DirRectangle::InitArgs);
};
