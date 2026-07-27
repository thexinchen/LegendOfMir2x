#pragma once
#include <any>
#include <list>
#include <utility>
#include <vector>
#include <string>
#include <concepts>
#include <functional>
#include <variant>
#include <cstdint>
#include <cstring>
#include <optional>

#include "gltex.hpp"
#include <type_traits>
#include "mirevent.hpp"
#include "mathf.hpp"
#include "colorf.hpp"
#include "fflerror.hpp"
#include "protocoldef.hpp"

class Widget;        // size concept
class WidgetTreeNode // tree concept, used by class Widget only
{
    private:
        struct BaseAttrs final
        {
            const bool    addChild = true;
            const bool removeChild = true;

            const std::string name;
        };

    private:
        template<typename InputType, typename  ConstType, typename MutableType> using check_const_cond_t         = std::conditional_t<std::is_const_v<std::remove_reference_t<InputType>>, ConstType, MutableType>;
        template<typename InputType, typename OutputType                      > using check_const_cond_out_ptr_t = check_const_cond_t<InputType, const OutputType *, OutputType *>;

    private:
        template<typename T> using VarTypeHelperBase = std::variant<
            T,
            std::function<T()>,
            std::function<T(const Widget *)>,
            std::function<T(const Widget *, const void *)>>;

        template<typename T> struct VarTypeHelper: public VarTypeHelperBase<T>
        {
            bool fixed() const
            {
                return this->index() == 0;
            }

            using VarTypeHelperBase<T>::VarTypeHelperBase;
        };

    private:
        using alias_VarDir         = VarTypeHelper<dir8_t>;
        using alias_VarInt         = VarTypeHelper<int>;
        using alias_VarU32         = VarTypeHelper<uint32_t>;
        using alias_VarDecimal     = VarTypeHelper<float>;
        using alias_VarSize        = VarTypeHelper<int>;
        using alias_VarBool        = VarTypeHelper<bool>;
        using alias_VarBlendMode   = VarTypeHelper<MirBlendMode>;
        using alias_VarTexLoadFunc = VarTypeHelper<GLTexID >;

    protected:
        // make all var types distinct
        // this is necessary for Widget::transform
        struct VarDir        : public alias_VarDir        { using alias_VarDir        ::alias_VarDir        ; };
        struct VarInt        : public alias_VarInt        { using alias_VarInt        ::alias_VarInt        ; };
        struct VarU32        : public alias_VarU32        { using alias_VarU32        ::alias_VarU32        ; };
        struct VarDecimal    : public alias_VarDecimal    { using alias_VarDecimal    ::alias_VarDecimal    ; };
        struct VarSize       : public alias_VarSize       { using alias_VarSize       ::alias_VarSize       ; };
        struct VarBool       : public alias_VarBool       { using alias_VarBool       ::alias_VarBool       ; };
        struct VarBlendMode  : public alias_VarBlendMode  { using alias_VarBlendMode  ::alias_VarBlendMode  ; };
        struct VarTexLoadFunc: public alias_VarTexLoadFunc{ using alias_VarTexLoadFunc::alias_VarTexLoadFunc; };

    protected:
        using VarIntOpt  = std::optional<VarInt>;
        using VarU32Opt  = std::optional<VarU32>;
        using VarSizeOpt = std::optional<VarSize>;

    protected:
        template<typename T> struct VarGetter: public VarTypeHelper<T>
        {
            using VarTypeHelper<T>::VarTypeHelper;
        };

    private:
        friend class Widget;

    protected:
        struct WADPair final // Widget-Auto-Delete-Pair
        {
            Widget *widget     = nullptr;
            bool    autoDelete = false;
        };

    protected:
        using ChildElement = WADPair;

    private:
        const uint64_t m_id;

    private:
        mutable bool m_inLoop = false;

    private:
        bool m_dead = false; // access to this widget is UB if true

    private:
        Widget *m_parent;
        WidgetTreeNode::BaseAttrs m_attrs; // don't use Widget::m_attrs since in dtor we need to access it

    private:
        std::list<WidgetTreeNode::ChildElement> m_childList; // widget shall NOT access this list directly

    private:
        std::vector<Widget *> m_delayList;

    private:
        WidgetTreeNode(WidgetTreeNode::WADPair, WidgetTreeNode::BaseAttrs); // can only be constructed by Widget::Widget()

    public:
        virtual ~WidgetTreeNode();

    public:
        auto parent(this auto && self, unsigned = 1) -> check_const_cond_out_ptr_t<decltype(self), Widget>;

    public:
        uint64_t id() const
        {
            return m_id;
        }

    public:
        const char *type() const
        {
            return typeid(*this).name();
        }

        const char *name() const
        {
            if(m_attrs.name.empty()){
                return type();
            }
            else{
                return m_attrs.name.c_str();
            }
        }

    public:
        template<std::invocable<const Widget *, bool, const Widget *, bool> F> void sort(F);

    public:
        // complicated function signature, means
        // auto foreachChild(bool, std::invocable<      Widget *, bool> auto f)       -> std::result_type_t<decltype(f),       Widget *, bool>
        // auto foreachChild(bool, std::invocable<const Widget *, bool> auto f) const -> std::result_type_t<decltype(f), const Widget *, bool>
        // auto foreachChild(bool, std::invocable<      Widget *      > auto f)       -> std::result_type_t<decltype(f),       Widget *      >
        // auto foreachChild(bool, std::invocable<const Widget *      > auto f) const -> std::result_type_t<decltype(f), const Widget *      >
        template<typename SELF> auto foreachChild(this SELF &&, bool, std::invocable<check_const_cond_out_ptr_t<SELF, Widget>, bool> auto f) -> std::conditional_t<std::is_same_v<std::invoke_result_t<decltype(f), Widget *, bool>, bool>, bool, void>;
        template<typename SELF> auto foreachChild(this SELF &&,       std::invocable<check_const_cond_out_ptr_t<SELF, Widget>, bool> auto f) -> std::conditional_t<std::is_same_v<std::invoke_result_t<decltype(f), Widget *, bool>, bool>, bool, void>;
        template<typename SELF> auto foreachChild(this SELF &&, bool, std::invocable<check_const_cond_out_ptr_t<SELF, Widget>      > auto f) -> std::conditional_t<std::is_same_v<std::invoke_result_t<decltype(f), Widget *      >, bool>, bool, void>;
        template<typename SELF> auto foreachChild(this SELF &&,       std::invocable<check_const_cond_out_ptr_t<SELF, Widget>      > auto f) -> std::conditional_t<std::is_same_v<std::invoke_result_t<decltype(f), Widget *      >, bool>, bool, void>;

    private:
        void execDeath() noexcept;

    public:
        void moveFront(const Widget *);
        void moveBack (const Widget *);

    public:
        virtual void onDeath() noexcept {}

    public:
        auto firstChild(this auto && self) -> check_const_cond_out_ptr_t<decltype(self), Widget>;
        auto  lastChild(this auto && self) -> check_const_cond_out_ptr_t<decltype(self), Widget>;

    private:
        void doClearChild(std::invocable<const Widget *, bool> auto, bool);
        void doClearChild(std::invocable<const Widget *      > auto, bool);

    public:
        inline void clearChild(std::invocable<const Widget *, bool> auto);
        inline void clearChild(std::invocable<const Widget *      > auto);
        inline void clearChild();

    private:
        void doRemoveChild(uint64_t, bool, bool);
        void doRemoveChildElement(WidgetTreeNode::ChildElement &, bool, bool);

    public:
        virtual void purge();
        virtual void removeChild(uint64_t, bool);

    private:
        void doAddChild(Widget *, bool, bool);

    public:
        virtual void addChild  (Widget *, bool);
        virtual void addChildAt(Widget *, WidgetTreeNode::VarDir, WidgetTreeNode::VarInt, WidgetTreeNode::VarInt, bool);

    public:
        bool hasChild() const
        {
            return firstChild();
        }

    public:
        auto hasChild     (this auto && self, uint64_t                                 ) -> check_const_cond_out_ptr_t<decltype(self), Widget>;
        auto hasChild     (this auto && self, std::invocable<const Widget *, bool> auto) -> check_const_cond_out_ptr_t<decltype(self), Widget>;
        auto hasChild     (this auto && self, std::invocable<const Widget *      > auto) -> check_const_cond_out_ptr_t<decltype(self), Widget>;
        auto hasDescendant(this auto && self, uint64_t                                 ) -> check_const_cond_out_ptr_t<decltype(self), Widget>;
        auto hasDescendant(this auto && self, std::invocable<const Widget *, bool> auto) -> check_const_cond_out_ptr_t<decltype(self), Widget>;
        auto hasDescendant(this auto && self, std::invocable<const Widget *      > auto) -> check_const_cond_out_ptr_t<decltype(self), Widget>;

    public:
        auto prevChild(this auto && self, uint64_t) -> check_const_cond_out_ptr_t<decltype(self), Widget>;
        auto nextChild(this auto && self, uint64_t) -> check_const_cond_out_ptr_t<decltype(self), Widget>;

    public:
        template<std::derived_from<Widget> T> auto hasParent(this auto && self) -> check_const_cond_out_ptr_t<decltype(self), T>;
};

class Widget: public WidgetTreeNode
{
    private:
        friend class WidgetTreeNode;

    private:
        using WidgetTreeNode::check_const_cond_out_ptr_t;

    public:
        using WidgetTreeNode::VarDir;
        using WidgetTreeNode::VarInt;
        using WidgetTreeNode::VarIntOpt;
        using WidgetTreeNode::VarU32;
        using WidgetTreeNode::VarU32Opt;
        using WidgetTreeNode::VarDecimal;
        using WidgetTreeNode::VarSize;
        using WidgetTreeNode::VarSizeOpt;
        using WidgetTreeNode::VarBool;
        using WidgetTreeNode::VarBlendMode;
        using WidgetTreeNode::VarTexLoadFunc;

// --- merged from widget.varstr.hpp ---
private:
    // don't use std::string_view, use const char *
    // both std::string_view and std::string can be constructed from string literal
    // which causes ambiguity if assigned from string literal

    // no need to add std::nullptr_t
    // because const char * is nullable

    using VarStrHelper = std::variant<const char *, // not owning, nullable,
                                      std::string>; //     owning

public:
    class VarStr: public VarStrHelper
    {
        public:
            using VarStrHelper::VarStrHelper;

        public:
            const char *c_str() const
            {
                return std::visit(VarDispatcher
                {
                    [](const        char *varg){ return varg ? varg : ""; },
                    [](const std::string &varg){ return varg.c_str()    ; },
                },

                *this);
            }

        public:
            bool empty() const
            {
                return c_str()[0] == '\0';
            }

        public:
            size_t size() const
            {
                return std::visit(VarDispatcher
                {
                    [](const        char *varg){ return varg ? std::strlen(varg) : 0; },
                    [](const std::string &varg){ return varg.size()                 ; },
                },

                *this);
            }

        public:
            std::string str() &
            {
                return std::string(c_str());
            }

            std::string str() &&
            {
                if(auto sptr = std::get_if<std::string>(this)){
                    return std::move(*sptr);
                }
                else{
                    return std::string(c_str());
                }
            }
    };

public:
    using VarStrFunc = std::variant<

            // no need of std::nullptr_t
            // because const char * is nullable here

            const char *,  // direct value, not owning, nullable
            std::string,   // direct value,     owning

            std::function<Widget::VarStr()>,
            std::function<Widget::VarStr(const Widget *)>,
            std::function<Widget::VarStr(const Widget *, const void *)>>;

public:
    static Widget::VarStr evalStrFunc(const Widget::VarStrFunc &, const Widget *, const void * = nullptr);

// --- end widget.varstr.hpp ---
// --- merged from widget.offset2d.hpp ---
public:
    struct IntOffset2D final
    {
        int x = 0;
        int y = 0;
    };

    class VarOffset2D final
    {
        private:
            std::variant<Widget::VarGetter<Widget::IntOffset2D>, std::tuple<Widget::VarInt, Widget::VarInt>> m_varOffset;

        public:
            VarOffset2D()
                : m_varOffset(std::make_tuple(0, 0)) // prefer decoupled offset
            {}

            VarOffset2D(Widget::VarGetter<Widget::IntOffset2D> arg)
                : m_varOffset(std::in_place_type<Widget::VarGetter<Widget::IntOffset2D>>, std::move(arg))
            {}

            VarOffset2D(Widget::VarInt arg1, Widget::VarInt arg2)
                : m_varOffset(std::in_place_type<std::tuple<Widget::VarInt, Widget::VarInt>>, std::move(arg1), std::move(arg2))
            {}

        public:
            int x(const Widget *widget, const void * arg = nullptr) const
            {
                return std::visit(VarDispatcher
                {
                    [widget, arg](const Widget::VarGetter<Widget::IntOffset2D> &varg)
                    {
                        return Widget::evalGetter<Widget::IntOffset2D>(varg, widget, arg).x;
                    },

                    [widget, arg](const std::tuple<Widget::VarInt, Widget::VarInt> &varg)
                    {
                        return Widget::evalInt(std::get<0>(varg), widget, arg);
                    },
                },

                m_varOffset);
            }

            int y(const Widget *widget, const void * arg = nullptr) const
            {
                return std::visit(VarDispatcher
                {
                    [widget, arg](const Widget::VarGetter<Widget::IntOffset2D> &varg)
                    {
                        return Widget::evalGetter<Widget::IntOffset2D>(varg, widget, arg).y;
                    },

                    [widget, arg](const std::tuple<Widget::VarInt, Widget::VarInt> &varg)
                    {
                        return Widget::evalInt(std::get<1>(varg), widget, arg);
                    },
                },

                m_varOffset);
            }

        public:
            Widget::IntOffset2D offset(const Widget *widget, const void * arg = nullptr) const
            {
                return std::visit(VarDispatcher
                {
                    [widget, arg](const Widget::VarGetter<Widget::IntOffset2D> &varg)
                    {
                        return Widget::evalGetter<Widget::IntOffset2D>(varg, widget, arg);
                    },

                    [widget, arg](const std::tuple<Widget::VarInt, Widget::VarInt> &varg)
                    {
                        return Widget::IntOffset2D
                        {
                            .x = Widget::evalInt(std::get<0>(varg), widget, arg),
                            .y = Widget::evalInt(std::get<1>(varg), widget, arg),
                        };
                    },
                },

                m_varOffset);
            }

        public:
            bool combined() const
            {
                return std::holds_alternative<Widget::VarGetter<Widget::IntOffset2D>>(m_varOffset);
            }
    };

// --- end widget.offset2d.hpp ---
// --- merged from widget.size2d.hpp ---
public:
    struct IntSize2D final
    {
        int w = 0;
        int h = 0;
    };

    class VarSize2D final
    {
        private:
            std::variant<Widget::VarGetter<Widget::IntSize2D>, std::tuple<Widget::VarSize, Widget::VarSize>> m_varSize;

        public:
            VarSize2D()
                : m_varSize(std::make_tuple(0, 0)) // prefer decoupled size
            {}

            VarSize2D(Widget::VarGetter<Widget::IntSize2D> arg)
                : m_varSize(std::in_place_type<Widget::VarGetter<Widget::IntSize2D>>, std::move(arg))
            {}

            VarSize2D(Widget::VarSize arg1, Widget::VarSize arg2)
                : m_varSize(std::in_place_type<std::tuple<Widget::VarSize, Widget::VarSize>>, std::move(arg1), std::move(arg2))
            {}

        public:
            int w(const Widget *widget, const void * arg = nullptr) const
            {
                return std::visit(VarDispatcher
                {
                    [widget, arg](const Widget::VarGetter<Widget::IntSize2D> &varg)
                    {
                        return std::max<int>(Widget::evalGetter<Widget::IntSize2D>(varg, widget, arg).w, 0);
                    },

                    [widget, arg](const std::tuple<Widget::VarSize, Widget::VarSize> &varg)
                    {
                        return Widget::evalSize(std::get<0>(varg), widget, arg);
                    },
                },

                m_varSize);
            }

            int h(const Widget *widget, const void * arg = nullptr) const
            {
                return std::visit(VarDispatcher
                {
                    [widget, arg](const Widget::VarGetter<Widget::IntSize2D> &varg)
                    {
                        return std::max<int>(Widget::evalGetter<Widget::IntSize2D>(varg, widget, arg).h, 0);
                    },

                    [widget, arg](const std::tuple<Widget::VarSize, Widget::VarSize> &varg)
                    {
                        return Widget::evalSize(std::get<1>(varg), widget, arg);
                    },
                },

                m_varSize);
            }

        public:
            Widget::IntSize2D size(const Widget *widget, const void * arg = nullptr) const
            {
                return std::visit(VarDispatcher
                {
                    [widget, arg](const Widget::VarGetter<Widget::IntSize2D> &varg)
                    {
                        const auto [w, h] = Widget::evalGetter<Widget::IntSize2D>(varg, widget, arg);
                        return Widget::IntSize2D
                        {
                            .w = std::max<int>(w, 0),
                            .h = std::max<int>(h, 0),
                        };
                    },

                    [widget, arg](const std::tuple<Widget::VarSize, Widget::VarSize> &varg)
                    {
                        return Widget::IntSize2D
                        {
                            .w = Widget::evalSize(std::get<0>(varg), widget, arg),
                            .h = Widget::evalSize(std::get<1>(varg), widget, arg),
                        };
                    },
                },

                m_varSize);
            }

        public:
            bool combined() const
            {
                return std::holds_alternative<Widget::VarGetter<Widget::IntSize2D>>(m_varSize);
            }
    };

// --- end widget.size2d.hpp ---

    public:
        using VarDrawFunc = std::variant<std::nullptr_t,
                                         std::function<void(                        int, int)>,
                                         std::function<void(const Widget *,         int, int)>,
                                         std::function<void(const Widget *, void *, int, int)>>;

        template<typename T> using VarUpdateFunc = std::variant<std::nullptr_t,
                                         std::function<void(                        const T &)>,
                                         std::function<void(const Widget *,         const T &)>,
                                         std::function<void(const Widget *, void *, const T &)>>;

        template<typename T> using VarCheckFunc = std::variant<std::nullptr_t,
                                         std::function<bool(                        const T &)>,
                                         std::function<bool(const Widget *,         const T &)>,
                                         std::function<bool(const Widget *, void *, const T &)>>;

    public:
        template<typename T> struct Margin final
        {
            T up    = 0;
            T down  = 0;
            T left  = 0;
            T right = 0;

            decltype(auto) operator[](this auto && self, size_t i)
            {
                switch(i){
                    case 0 : return self.up;
                    case 1 : return self.down;
                    case 2 : return self.left;
                    case 3 : return self.right;
                    default: throw fflpanic("invalid margin index: {}", i);
                }
            }
        };

        using IntMargin = Widget::Margin<int>;
        using VarMargin = Widget::Margin<Widget::VarSize>;

        struct FontConfig final
        {
            // fontex has WenQuanYi Song bitmap font support
            // but to avoid bitmap zoom in/out, use specified pt in filename, i.e.
            //
            //     id: 07, pt 12, filename 07_fusion-pixel-12px-monospaced-zh_hans.TTF
            //     id: 08, pt 12, filename 08_fusion-pixel-12px-proportional-zh_hans.TTF
            //     id: 09, pt 15, filename 09_WenQuanYi_Bitmap_Song_15_px.TTF
            //     id: 10, pt 15, filename 0A_WenQuanYi_Bitmap_Song_15_px.TTF
            //     id: 11, pt 15, filename 0B_WenQuanYi_Bitmap_Song_15_px.TTF
            //     id: 12, pt 18, filename 0C_WenQuanYi_Bitmap_Song_18_px.TTF
            //     id: 13, pt 18, filename 0D_WenQuanYi_Bitmap_Song_18_px.TTF
            //
            // when using id 11, please use pt 15 only

            uint8_t id    = 11; // default font
            uint8_t size  = 15;
            uint8_t style =  0;

            Widget::VarU32   color = colorf::WHITE_A255;
            Widget::VarU32 bgColor = 0U;
        };

        struct CursorConfig final
        {
            int width = 2;
            Widget::VarU32 color = colorf::WHITE_A255;
        };

    public:
// --- merged from widget.roi.hpp ---
struct ROI final
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    Widget::IntOffset2D offset() const noexcept
    {
        return {x, y};
    }

    Widget::IntSize2D size() const noexcept
    {
        return
        {
            std::max<int>(w, 0),
            std::max<int>(h, 0),
        };
    }

    bool empty() const noexcept
    {
        return w <= 0 || h <= 0;
    }

    operator bool () const noexcept
    {
        return !empty();
    }

    bool in(int argX, int argY) const noexcept
    {
        return mathf::pointInRectangle<int>(argX, argY, x, y, w, h);
    }

    bool overlap(const Widget::ROI &r) const noexcept
    {
        return mathf::rectangleOverlap<int>(x, y, w, h, r.x, r.y, r.w, r.h);
    }

    Widget::ROI clone() const noexcept
    {
        return *this;
    }

    Widget::ROI & crop(const Widget::ROI &r)
    {
        mathf::cropSegment<int>(x, w, r.x, r.w);
        mathf::cropSegment<int>(y, h, r.y, r.h);
        return *this;
    }
};

class VarROI final
{
    private:
        std::variant<Widget::VarGetter<Widget::ROI>, std::tuple<Widget::VarOffset2D, Widget::VarSize2D>> m_varROI;

    public:
        VarROI()
            : m_varROI(std::tuple<Widget::VarOffset2D, Widget::VarSize2D>{}) // prefer decoupled roi
        {}

    public:
        VarROI(Widget::VarGetter<Widget::ROI> arg)
            : m_varROI(std::in_place_type<Widget::VarGetter<Widget::ROI>>, std::move(arg))
        {}

        VarROI(Widget::VarOffset2D argOff, Widget::VarSize2D argSize)
            : m_varROI(std::in_place_type<std::tuple<Widget::VarOffset2D, Widget::VarSize2D>>, std::move(argOff), std::move(argSize))
        {}

        VarROI(Widget::VarSize2D argSize)
            : m_varROI(std::in_place_type<std::tuple<Widget::VarOffset2D, Widget::VarSize2D>>, Widget::VarOffset2D{}, std::move(argSize))
        {}

        VarROI(Widget::VarOffset2D argOff, Widget::VarSize argW, Widget::VarSize argH)
            : m_varROI(std::in_place_type<std::tuple<Widget::VarOffset2D, Widget::VarSize2D>>, std::move(argOff), Widget::VarSize2D(std::move(argW), std::move(argH)))
        {}

        VarROI(Widget::VarSize argW, Widget::VarSize argH)
            : m_varROI(std::in_place_type<std::tuple<Widget::VarOffset2D, Widget::VarSize2D>>, Widget::VarOffset2D{}, Widget::VarSize2D(std::move(argW), std::move(argH)))
        {}

        VarROI(Widget::VarInt argX, Widget::VarInt argY, Widget::VarSize2D argSize)
            : m_varROI(std::in_place_type<std::tuple<Widget::VarOffset2D, Widget::VarSize2D>>, Widget::VarOffset2D(std::move(argX), std::move(argY)), std::move(argSize))
        {}

        VarROI(Widget::VarInt argX, Widget::VarInt argY, Widget::VarSize argW, Widget::VarSize argH)
            : m_varROI(std::in_place_type<std::tuple<Widget::VarOffset2D, Widget::VarSize2D>>, Widget::VarOffset2D(std::move(argX), std::move(argY)), Widget::VarSize2D(std::move(argW), std::move(argH)))
        {}

    public:
        Widget::ROI roi(const Widget *widget, const void *arg = nullptr) const
        {
            return std::visit(VarDispatcher
            {
                [widget, arg](const Widget::VarGetter<Widget::ROI> &varg)
                {
                    return Widget::evalGetter<Widget::ROI>(varg, widget, arg);
                },

                [widget, arg](const std::tuple<Widget::VarOffset2D, Widget::VarSize2D> &varg)
                {
                    const auto [x, y] = std::get<0>(varg).offset(widget, arg);
                    const auto [w, h] = std::get<1>(varg).  size(widget, arg);

                    return Widget::ROI
                    {
                        .x = x,
                        .y = y,
                        .w = w,
                        .h = h,
                    };
                },
            },

            m_varROI);
        }

        Widget::IntOffset2D offset(const Widget *widget, const void *arg = nullptr) const
        {
            return std::visit(VarDispatcher
            {
                [widget, arg](const Widget::VarGetter<Widget::ROI> &varg)
                {
                    return Widget::evalGetter<Widget::ROI>(varg, widget, arg).offset();
                },

                [widget, arg](const std::tuple<Widget::VarOffset2D, Widget::VarSize2D> &varg)
                {
                    return std::get<0>(varg).offset(widget, arg);
                },
            },

            m_varROI);
        }

        Widget::IntSize2D size(const Widget *widget, const void *arg = nullptr) const
        {
            return std::visit(VarDispatcher
            {
                [widget, arg](const Widget::VarGetter<Widget::ROI> &varg)
                {
                    return Widget::evalGetter<Widget::ROI>(varg, widget, arg).size();
                },

                [widget, arg](const std::tuple<Widget::VarOffset2D, Widget::VarSize2D> &varg)
                {
                    return std::get<1>(varg).size(widget, arg);
                },
            },

            m_varROI);
        }

    public:
        int x(const Widget *widget, const void *arg = nullptr) const
        {
            return std::visit(VarDispatcher
            {
                [widget, arg](const Widget::VarGetter<Widget::ROI> &varg)
                {
                    return Widget::evalGetter<Widget::ROI>(varg, widget, arg).x;
                },

                [widget, arg](const std::tuple<Widget::VarOffset2D, Widget::VarSize2D> &varg)
                {
                    return std::get<0>(varg).x(widget, arg);
                },
            },

            m_varROI);
        }

        int y(const Widget *widget, const void *arg = nullptr) const
        {
            return std::visit(VarDispatcher
            {
                [widget, arg](const Widget::VarGetter<Widget::ROI> &varg)
                {
                    return Widget::evalGetter<Widget::ROI>(varg, widget, arg).y;
                },

                [widget, arg](const std::tuple<Widget::VarOffset2D, Widget::VarSize2D> &varg)
                {
                    return std::get<0>(varg).y(widget, arg);
                },
            },

            m_varROI);
        }

        int w(const Widget *widget, const void *arg = nullptr) const
        {
            return std::visit(VarDispatcher
            {
                [widget, arg](const Widget::VarGetter<Widget::ROI> &varg)
                {
                    return Widget::evalGetter<Widget::ROI>(varg, widget, arg).w;
                },

                [widget, arg](const std::tuple<Widget::VarOffset2D, Widget::VarSize2D> &varg)
                {
                    return std::get<1>(varg).w(widget, arg);
                },
            },

            m_varROI);
        }

        int h(const Widget *widget, const void *arg = nullptr) const
        {
            return std::visit(VarDispatcher
            {
                [widget, arg](const Widget::VarGetter<Widget::ROI> &varg)
                {
                    return Widget::evalGetter<Widget::ROI>(varg, widget, arg).h;
                },

                [widget, arg](const std::tuple<Widget::VarOffset2D, Widget::VarSize2D> &varg)
                {
                    return std::get<1>(varg).h(widget, arg);
                },
            },

            m_varROI);
        }

    public:
        bool combinedOffset() const
        {
            return std::visit(VarDispatcher
            {
                [](const Widget::VarGetter<Widget::ROI> &)
                {
                    return true;
                },

                [](const std::tuple<Widget::VarOffset2D, Widget::VarSize2D> &varg)
                {
                    return std::get<0>(varg).combined();
                },
            },

            m_varROI);
        }

        bool combinedSize() const
        {
            return std::visit(VarDispatcher
            {
                [](const Widget::VarGetter<Widget::ROI> &)
                {
                    return true;
                },

                [](const std::tuple<Widget::VarOffset2D, Widget::VarSize2D> &varg)
                {
                    return std::get<1>(varg).combined();
                },
            },

            m_varROI);
        }

        bool combinedROI() const
        {
            return std::holds_alternative<Widget::VarGetter<Widget::ROI>>(m_varROI);
        }
};

class VarROIOpt final
{
    private:
        std::optional<Widget::VarROI> m_varROIOpt;

    public:
        VarROIOpt() = default;
        VarROIOpt(std::nullopt_t): VarROIOpt() {};

    public:
        template<typename... Args> explicit VarROIOpt(Args&&... args)
            : m_varROIOpt(Widget::VarROI(std::forward<Args>(args)...))
        {}

    public:
        VarROIOpt(const Widget::ROI &r)
            : VarROIOpt(r.x, r.y, r.w, r.h)
        {}

        VarROIOpt(const Widget::VarROI &vr)
            : m_varROIOpt(vr)
        {}

    public:
        auto operator -> (this auto && self)
        {
            return std::addressof(self.m_varROIOpt.value());
        }

    public:
        bool has_value() const
        {
            return m_varROIOpt.has_value();
        }

        decltype(auto) value(this auto && self)
        {
            return self.m_varROIOpt.value();
        }

        Widget::VarROI value_or(Widget::VarROI r) const
        {
            return m_varROIOpt.value_or(r);
        }
};

class ROIOpt final
{
    private:
        std::optional<Widget::ROI> m_roiOpt;

    public:
        ROIOpt() = default;
        ROIOpt(std::nullopt_t): ROIOpt() {};

    public:
        ROIOpt(const Widget::ROI &roi)
            : m_roiOpt(roi)
        {}

    public:
        ROIOpt(int argX, int argY, int argW, int argH)
            : m_roiOpt(Widget::ROI{argX, argY, argW, argH})
        {}

    public:
        ROIOpt(int argW, int argH)
            : ROIOpt(0, 0, argW, argH)
        {}

        ROIOpt(Widget::IntSize2D size)
            : ROIOpt(0, 0, size.w, size.h)
        {}

        ROIOpt(Widget::IntOffset2D offset, Widget::IntSize2D size)
            : ROIOpt(offset.x, offset.y, size.w, size.h)
        {}

    public:
        auto operator -> (this auto && self)
        {
            return std::addressof(self.m_roiOpt.value());
        }

    public:
        bool has_value() const
        {
            return m_roiOpt.has_value();
        }

        decltype(auto) value(this auto && self)
        {
            return self.m_roiOpt.value();
        }

        Widget::ROI value_or(Widget::ROI r) const
        {
            return m_roiOpt.value_or(r);
        }
};

struct ROIMap final
{
    dir8_t dir = DIR_UPLEFT;

    int x = 0;
    int y = 0;

    Widget::ROIOpt ro = std::nullopt;

    bool empty() const
    {
        if(ro.has_value()){
            return ro->empty();
        }
        else{
            throw fflpanic("ro empty");
        }
    }

    operator bool () const
    {
        return !empty();
    }

    bool in(int pixelX, int pixelY) const
    {
        if(ro.has_value()){
            return Widget::ROI{x, y, ro->w, ro->h}.in(pixelX, pixelY);
        }
        else{
            throw fflpanic("ro empty");
        }
    }

    template<typename T> bool in(const T &t) const
    {
        const auto [tx, ty] = t; return in(tx, ty);
    }

    Widget::ROIMap clone() const
    {
        return *this;
    }

    Widget::ROIMap & calibrate(const Widget *widget)
    {
        if(!ro.has_value()){
            if(widget){
                ro = widget->roi();
            }
            else{
                throw fflpanic("invalid widget");
            }
        }

        if(dir != DIR_UPLEFT){
            x  -= xSizeOff(dir, [row = ro->w]{ return row; });
            y  -= ySizeOff(dir, [roh = ro->h]{ return roh; });
            dir = DIR_UPLEFT;
        }

        if(widget){
            crop(widget->roi());
        }

        if(x < 0){
            ro->x -= x;
            ro->w  = std::max<int>(ro->w + x, 0);
            x = 0;
        }

        if(y < 0){
            ro->y -= y;
            ro->h  = std::max<int>(ro->h + y, 0);
            y = 0;
        }

        return *this;
    }

    Widget::ROIMap & crop(const Widget::ROI &r)
    {
        if(!ro.has_value()){
            throw fflpanic("ro empty");
        }

        if(dir != DIR_UPLEFT){
            x  -= xSizeOff(dir, [row = ro->w]{ return row; });
            y  -= ySizeOff(dir, [roh = ro->h]{ return roh; });
            dir = DIR_UPLEFT;
        }

        const auto oldX = ro->x;
        const auto oldY = ro->y;

        ro->crop(r);

        x += (ro->x - oldX);
        y += (ro->y - oldY);

        return *this;
    }

    Widget::ROIMap map(int dx, int dy, const Widget::ROI &cr) const
    {
        // maps from parent's m to child's cm
        // cr is child's cropped ROI in itself, child's (0, 0) is at (dx, dy) in parent

        auto cm = clone().crop(Widget::ROI
        {
            .x = cr.x + dx,
            .y = cr.y + dy,
            .w = cr.w,
            .h = cr.h,
        });

        cm.ro->x -= dx;
        cm.ro->y -= dy;

        return cm;
    }

    Widget::ROIMap create(const Widget::ROI &cr) const
    {
        // maps from parent's m to child's cm
        // cr is child's full ROI in parent, child's (0, 0) is at (cr.x, cr.y) in parent

        return map(cr.x, cr.y, Widget::ROI
        {
            .x = 0,
            .y = 0,
            .w = cr.w,
            .h = cr.h,
        });
    }
};

template<typename T> Widget::ROI makeROI(const T &t)
{
    const auto [x, y, w, h] = t; return Widget::ROI
    {
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };
}

template<typename U, typename V> Widget::ROI makeROI(const U &u, const V &v)
{
    const auto [x, y] = u;
    const auto [w, h] = v; return Widget::ROI
    {
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };
}

template<typename T> Widget::ROI makeROI(int x, int y, const T &t)
{
    const auto [w, h] = t; return Widget::ROI
    {
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };
}

template<typename T> Widget::ROI makeROI(const T &t, int w, int h)
{
    const auto [x, y] = t; return Widget::ROI
    {
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };
}

// --- end widget.roi.hpp ---

    public:
        struct TypeAttrs final // per class attributes
        {
            const bool setSize = true;

            const bool    addChild = true;
            const bool removeChild = true;
        };

        struct InstAttrs final // per instance attributes
        {
            std::string name;
            std::any    data;

            Widget::VarBool show = true;
            Widget::VarBool active = true;

            bool focus = false;
            bool moveOnFocus = true;

            std::function<void(      Widget *                                         )> afterResize  = nullptr;
            std::function<bool(      Widget *, const MirEvent &, bool, Widget::ROIMap)> processEvent = nullptr;
            std::function<void(      Widget *, double                                 )> update       = nullptr;
            std::function<void(const Widget *,                          Widget::ROIMap)> draw         = nullptr;
        };

        struct InitAttrs final
        {
            Widget::TypeAttrs type;
            Widget::InstAttrs inst;
        };

    public:
        struct AddChildArgs final
        {
            Widget *widget = nullptr;

            Widget::VarDir dir = DIR_UPLEFT;
            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            bool autoDelete = false;
        };

        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;
            Widget::VarInt x   = 0;
            Widget::VarInt y   = 0;

            Widget::VarSizeOpt w = 0; // nullopt means auto-resize
            Widget::VarSizeOpt h = 0;

            std::vector<Widget::AddChildArgs> childList;

            Widget::InitAttrs attrs;
            Widget::WADPair  parent;
        };

    public:
        using WidgetTreeNode::ChildElement;

    public:
        static dir8_t           evalDir        (const Widget::VarDir         &, const Widget *, const void * = nullptr);
        static int              evalInt        (const Widget::VarInt         &, const Widget *, const void * = nullptr);
        static uint32_t         evalU32        (const Widget::VarU32         &, const Widget *, const void * = nullptr);
        static float            evalDecimal    (const Widget::VarDecimal     &, const Widget *, const void * = nullptr);
        static int              evalSize       (const Widget::VarSize        &, const Widget *, const void * = nullptr);
        static bool             evalBool       (const Widget::VarBool        &, const Widget *, const void * = nullptr);
        static MirBlendMode    evalBlendMode  (const Widget::VarBlendMode   &, const Widget *, const void * = nullptr);
        static GLTexID     evalTexLoadFunc(const Widget::VarTexLoadFunc &, const Widget *, const void * = nullptr);

    public:
        static int evalSizeOpt(const Widget::VarSizeOpt &, const Widget *,               const auto &);
        static int evalSizeOpt(const Widget::VarSizeOpt &, const Widget *, const void *, const auto &);

    public:
        static int evalU32Opt(const Widget::VarU32Opt &, const Widget *,               const auto &);
        static int evalU32Opt(const Widget::VarU32Opt &, const Widget *, const void *, const auto &);

    public:
        template<typename T> static T evalGetter(const Widget::VarGetter<T> &, const Widget *, const void * = nullptr);

    public:
        template<typename Func> static VarInt     transform(VarInt    , Func &&);
        template<typename Func> static VarSize    transform(VarSize   , Func &&);
        template<typename Func> static VarSizeOpt transform(VarSizeOpt, Func &&);

    public:
        static bool  hasDrawFunc(const Widget::VarDrawFunc &);
        static void execDrawFunc(const Widget::VarDrawFunc &, const Widget *,         int, int);
        static void execDrawFunc(const Widget::VarDrawFunc &, const Widget *, void *, int, int);

    public:
        template<typename T> static bool  hasUpdateFunc(const Widget::VarUpdateFunc<T> &);
        template<typename T> static void execUpdateFunc(const Widget::VarUpdateFunc<T> &, const Widget *,         const T &);
        template<typename T> static void execUpdateFunc(const Widget::VarUpdateFunc<T> &, const Widget *, void *, const T &);

    public:
        template<typename T> static bool  hasCheckFunc(const Widget::VarCheckFunc<T> &);
        template<typename T> static bool execCheckFunc(const Widget::VarCheckFunc<T> &, const Widget *,         const T &);
        template<typename T> static bool execCheckFunc(const Widget::VarCheckFunc<T> &, const Widget *, void *, const T &);

    private:
// --- merged from widget.recursion.hpp ---
class RecursionDetector final
{
    private:
        bool &m_flag;

    public:
        RecursionDetector(bool &flag, const char *type, const char *func)
            : m_flag(flag)
        {
            if(m_flag){
                throw fflpanic("recursion detected in {}::{}", type, func);
            }
            else{
                m_flag = true;
            }
        }

        ~RecursionDetector()
        {
            m_flag = false;
        }
};

// --- end widget.recursion.hpp ---

    private:
        Widget::VarDir m_dir;

    private:
        std::pair<Widget::VarInt, int> m_x;
        std::pair<Widget::VarInt, int> m_y;

    private:
        Widget::VarSizeOpt m_w;
        Widget::VarSizeOpt m_h;

    protected:
        Widget::InitAttrs m_attrs;

    protected:
        std::pair<Widget::VarBool &, bool> m_show   {m_attrs.inst.show  , false};
        std::pair<Widget::VarBool &, bool> m_active {m_attrs.inst.active, false};

    private:
        mutable bool m_hCalc = false;
        mutable bool m_wCalc = false;

    public:
        explicit Widget(Widget::InitArgs);

// --- merged from widget.sizeoff.hpp ---
private:
    static int sizeOff(auto && func, int index)
    {
        /**/ if(index <  0) return          0;
        else if(index == 0) return func() / 2;
        else                return func() - 1;
    }

public:
    static int xSizeOff(dir8_t argDir, auto && argFunc)
    {
        switch(argDir){
            case DIR_UPLEFT   : return sizeOff(argFunc, -1);
            case DIR_UP       : return sizeOff(argFunc,  0);
            case DIR_UPRIGHT  : return sizeOff(argFunc,  1);
            case DIR_RIGHT    : return sizeOff(argFunc,  1);
            case DIR_DOWNRIGHT: return sizeOff(argFunc,  1);
            case DIR_DOWN     : return sizeOff(argFunc,  0);
            case DIR_DOWNLEFT : return sizeOff(argFunc, -1);
            case DIR_LEFT     : return sizeOff(argFunc, -1);
            default           : return sizeOff(argFunc,  0);
        }
    }

    static int ySizeOff(dir8_t argDir, auto && argFunc)
    {
        switch(argDir){
            case DIR_UPLEFT   : return sizeOff(argFunc, -1);
            case DIR_UP       : return sizeOff(argFunc, -1);
            case DIR_UPRIGHT  : return sizeOff(argFunc, -1);
            case DIR_RIGHT    : return sizeOff(argFunc,  0);
            case DIR_DOWNRIGHT: return sizeOff(argFunc,  1);
            case DIR_DOWN     : return sizeOff(argFunc,  1);
            case DIR_DOWNLEFT : return sizeOff(argFunc,  1);
            case DIR_LEFT     : return sizeOff(argFunc,  0);
            default           : return sizeOff(argFunc,  0);
        }
    }

// --- end widget.sizeoff.hpp ---

    public:
        virtual void update       (double) final;
        virtual void updateDefault(double);

    public:
        virtual bool processEvent      (const MirEvent &, bool, Widget::ROIMap) final;
        virtual bool processEventRoot  (const MirEvent &, bool, Widget::ROIMap) final;
        virtual bool processEventParent(const MirEvent &, bool, Widget::ROIMap) final;

    protected:
        // for draw() and processEventDefault(), it doesn't check show()
        //
        //     1. if need to manually draw a widget, we ignore show() result
        //     2. if draw by its parent, parent needs to check show()
        //
        // but processEventDefault() and draw() needs to check if given ROI is empty
        // we agree that if a widget is not show() or ROI is empty, it
        //
        //     1. won't accept any event
        //     2. won't alter any internal state, directly bypass the event
        //     3. parent needs to change focus outside, not inside widget itself
        //
        virtual bool processEventDefault(const MirEvent &, bool, Widget::ROIMap);

    public:
        virtual void draw       (                                  Widget::ROIMap) const final;
        virtual void drawRoot   (                                  Widget::ROIMap) const final;
        virtual void drawChild  (const Widget *,                   Widget::ROIMap) const final;
        virtual void drawAsChild(const Widget *, dir8_t, int, int, Widget::ROIMap) const final;

    protected:
        virtual void drawDefault(Widget::ROIMap) const;

    public:
        virtual void afterResize() final;

    protected:
        virtual void afterResizeDefault();

    public:
        virtual int w() const;
        virtual int h() const;

    public:
        bool varWOpt  () const { return !m_w.has_value(); }
        bool varHOpt  () const { return !m_h.has_value(); }
        bool varWFixed() const { return  m_w.has_value() && m_w->fixed(); }
        bool varHFixed() const { return  m_h.has_value() && m_h->fixed(); }

    public:
        int maxChildCoverWExcept(const Widget *) const;
        int maxChildCoverHExcept(const Widget *) const;

    public:
        int dx() const;
        int dy() const;

    public:
        virtual int rdx(const Widget * = nullptr) const final;
        virtual int rdy(const Widget * = nullptr) const final;

    public:
        Widget::ROI roi(const Widget *widget = nullptr) const
        {
            if(widget && (widget != this)){
                return {rdx(widget), rdy(widget), w(), h()};
            }
            else{
                return {0, 0, w(), h()};
            }
        }

    public:
        auto & data(this auto && self)
        {
            return self.m_attrs.inst.data;
        }

    public:
        bool      focus() const;
        bool localFocus() const;
        void  flipFocus();

        virtual void setFocus(bool);
        virtual bool consumeFocus(bool, Widget * = nullptr) final;

        auto focusedChild     (this auto && self) -> check_const_cond_out_ptr_t<decltype(self), Widget>;
        auto focusedDescendant(this auto && self) -> check_const_cond_out_ptr_t<decltype(self), Widget>;

    public:
        bool       show() const;
        bool  localShow() const;
        void   flipShow();
        void    setShow(Widget::VarBool);

    public:
        bool      active() const;
        bool localActive() const;
        void  flipActive();
        void   setActive(Widget::VarBool);

    public:
        void moveXTo(Widget::VarInt);
        void moveYTo(Widget::VarInt);


        void moveTo(                Widget::VarInt, Widget::VarInt);
        void moveBy(                Widget::VarInt, Widget::VarInt);
        void moveBy(                Widget::VarInt, Widget::VarInt, const Widget::ROI &);
        void moveAt(Widget::VarDir, Widget::VarInt, Widget::VarInt);

    public:
        virtual void setW(Widget::VarSizeOpt) final;
        virtual void setH(Widget::VarSizeOpt) final;
        virtual void setSize(Widget::VarSizeOpt, Widget::VarSizeOpt) final;

    public:
        virtual std::string dumpTree() const final;
        virtual void        dumpJsonFile(const char *) const final;

    private:
        virtual std::vector<std::string> dumpTreeExt() const { return {}; }
};

// --- merged from widget.implement.hpp ---
auto WidgetTreeNode::parent(this auto && self, unsigned level) -> check_const_cond_out_ptr_t<decltype(self), Widget>
{
    check_const_cond_out_ptr_t<decltype(self), Widget> p = std::addressof(self);
    for(; p && (level > 0); level--){
        p = p->m_parent;
    }

    if(p && p->m_dead){
        throw fflpanic("accessing dead widget: {}", p->name());
    }
    return p;
}

template<std::invocable<const Widget *, bool, const Widget *, bool> F> void WidgetTreeNode::sort(F f)
{
    m_childList.sort([&f](const auto &x, const auto &y)
    {
        if(x.widget && y.widget){
            return f(x.widget, x.autoDelete, y.widget, y.autoDelete);
        }
        else if(x.widget){
            return true;
        }
        else{
            return false;
        }
    });
}

template<typename SELF> auto WidgetTreeNode::foreachChild(this SELF && self, bool forward, std::invocable<check_const_cond_out_ptr_t<SELF, Widget>, bool> auto f) -> std::conditional_t<std::is_same_v<std::invoke_result_t<decltype(f), Widget *, bool>, bool>, bool, void>
{
    const ValueKeeper keepValue(self.m_inLoop, true);
    constexpr bool hasBoolResult = std::is_same_v<std::invoke_result_t<decltype(f), Widget *, bool>, bool>;

    if(forward){
        for(auto p = self.m_childList.begin(); p != self.m_childList.end(); ++p){
            if(p->widget){
                if constexpr (hasBoolResult){
                    if(f(p->widget, p->autoDelete)){
                        return true;
                    }
                }
                else{
                    f(p->widget, p->autoDelete);
                }
            }
        }

        if constexpr (hasBoolResult){
            return false;
        }
    }
    else{
        for(auto p = self.m_childList.rbegin(); p != self.m_childList.rend(); ++p){
            if(p->widget){
                if constexpr (hasBoolResult){
                    if(f(p->widget, p->autoDelete)){
                        return true;
                    }
                }
                else{
                    f(p->widget, p->autoDelete);
                }
            }
        }

        if constexpr (hasBoolResult){
            return false;
        }
    }
}

template<typename SELF> auto WidgetTreeNode::foreachChild(this SELF && self, std::invocable<check_const_cond_out_ptr_t<SELF, Widget>, bool> auto f) -> std::conditional_t<std::is_same_v<std::invoke_result_t<decltype(f), Widget *, bool>, bool>, bool, void>
{
    if constexpr (std::is_same_v<std::invoke_result_t<decltype(f), check_const_cond_out_ptr_t<SELF, Widget>, bool>, bool>){
        return self.foreachChild(true, f);
    }
    else{
        self.foreachChild(true, f);
    }
}

template<typename SELF> auto WidgetTreeNode::foreachChild(this SELF && self, bool forward, std::invocable<check_const_cond_out_ptr_t<SELF, Widget>> auto f) -> std::conditional_t<std::is_same_v<std::invoke_result_t<decltype(f), Widget *>, bool>, bool, void>
{
    return self.foreachChild(forward, [&f](auto widget, bool)
    {
        return f(widget);
    });
}

template<typename SELF> auto WidgetTreeNode::foreachChild(this SELF && self, std::invocable<check_const_cond_out_ptr_t<SELF, Widget>> auto f) -> std::conditional_t<std::is_same_v<std::invoke_result_t<decltype(f), Widget *>, bool>, bool, void>
{
    return self.foreachChild([&f](auto widget, bool)
    {
        return f(widget);
    });
}

auto WidgetTreeNode::firstChild(this auto && self) -> check_const_cond_out_ptr_t<decltype(self), Widget>
{
    for(auto &child: self.m_childList){
        if(child.widget){
            return child.widget;
        }
    }
    return nullptr;
}

auto WidgetTreeNode::lastChild(this auto && self) -> check_const_cond_out_ptr_t<decltype(self), Widget>
{
    for(auto p = self.m_childList.rbegin(); p != self.m_childList.rend(); ++p){
        if(p->widget){
            return p->widget;
        }
    }
    return nullptr;
}

void WidgetTreeNode::doClearChild(std::invocable<const Widget *, bool> auto f, bool ignoreCanRemoveChild)
{
    for(auto &child: m_childList){
        if(child.widget){
            if(f(child.widget, child.autoDelete)){
                doRemoveChildElement(child, true, ignoreCanRemoveChild);
            }
        }
    }
}

void WidgetTreeNode::doClearChild(std::invocable<const Widget *> auto f, bool ignoreCanRemoveChild)
{
    doClearChild([&f](const Widget *child, bool){ return f(child); }, ignoreCanRemoveChild);
}

void WidgetTreeNode::clearChild(std::invocable<const Widget *, bool> auto f){ doClearChild(f, false); }
void WidgetTreeNode::clearChild(std::invocable<const Widget *      > auto f){ doClearChild(f, false); }

void WidgetTreeNode::clearChild()
{
    clearChild([](const Widget *){ return true; });
}

auto WidgetTreeNode::hasChild(this auto && self, uint64_t argID) -> check_const_cond_out_ptr_t<decltype(self), Widget>
{
    for(auto p = self.m_childList.begin(); p != self.m_childList.end(); ++p){
        if(p->widget && p->widget->id() == argID){
            return p->widget;
        }
    }
    return nullptr;
}

auto WidgetTreeNode::hasChild(this auto && self, std::invocable<const Widget *, bool> auto f) -> check_const_cond_out_ptr_t<decltype(self), Widget>
{
    for(auto &child: self.m_childList){
        if(child.widget && f(child.widget, child.autoDelete)){
            return child.widget;
        }
    }
    return nullptr;
}

auto WidgetTreeNode::hasChild(this auto && self, std::invocable<const Widget *> auto f) -> check_const_cond_out_ptr_t<decltype(self), Widget>
{
    return self.hasChild([&f](const Widget *child, bool){ return f(child); });
}

auto WidgetTreeNode::hasDescendant(this auto && self, uint64_t argID) -> check_const_cond_out_ptr_t<decltype(self), Widget>
{
    for(auto p = self.m_childList.begin(); p != self.m_childList.end(); ++p){
        if(p->widget){
            if(p->widget->id() == argID){
                return p->widget;
            }
            else if(auto descendant = p->widget->hasDescendant(argID)){
                return descendant;
            }
        }
    }
    return nullptr;
}

auto WidgetTreeNode::hasDescendant(this auto && self, std::invocable<const Widget *, bool> auto f) -> check_const_cond_out_ptr_t<decltype(self), Widget>
{
    for(auto &child: self.m_childList){
        if(child.widget){
            if(f(child.widget, child.autoDelete)){
                return child.widget;
            }
            else if(auto descendant = child.widget->hasDescendant(f)){
                return descendant;
            }
        }
    }
    return nullptr;
}

auto WidgetTreeNode::hasDescendant(this auto && self, std::invocable<const Widget *> auto f) -> check_const_cond_out_ptr_t<decltype(self), Widget>
{
    return self.hasDescendant([&f](const Widget *child, bool){ return f(child); });
}

auto WidgetTreeNode::prevChild(this auto && self, uint64_t childID) -> check_const_cond_out_ptr_t<decltype(self), Widget>
{
    for(auto p = self.m_childList.rbegin(); p != self.m_childList.rend(); ++p){
        if(p->widget && (p->widget->id() == childID)){
            ++p;
            for(; p != self.m_childList.rend(); ++p){
                if(p->widget){
                    return p->widget;
                }
            }
            return nullptr;
        }
    }
    throw fflpanic("can not find child {}", childID);
}

auto WidgetTreeNode::nextChild(this auto && self, uint64_t childID) -> check_const_cond_out_ptr_t<decltype(self), Widget>
{
    for(auto p = self.m_childList.begin(); p != self.m_childList.end(); ++p){
        if(p->widget && (p->widget->id() == childID)){
            ++p;
            for(; p != self.m_childList.end(); ++p){
                if(p->widget){
                    return p->widget;
                }
            }
            return nullptr;
        }
    }
    throw fflpanic("can not find child {}", childID);
}

template<std::derived_from<Widget> T> auto WidgetTreeNode::hasParent(this auto && self) -> check_const_cond_out_ptr_t<decltype(self), T>
{
    for(auto p = self.parent(); p; p = p->parent()){
        if constexpr (std::is_const_v<std::remove_reference_t<decltype(self)>>){
            if(dynamic_cast<const T *>(p)){
                return static_cast<const T *>(p);
            }
        }
        else{
            if(dynamic_cast<T *>(p)){
                return static_cast<T *>(p);
            }
        }
    }
    return nullptr;
}

int Widget::evalSizeOpt(const Widget::VarSizeOpt &varSizeOpt, const Widget *widget, const auto &f)
{
    if(varSizeOpt.has_value()){
        return evalSize(varSizeOpt.value(), widget, nullptr);
    }
    else{
        return f();
    }
}

int Widget::evalSizeOpt(const Widget::VarSizeOpt &varSizeOpt, const Widget *widget, const void *arg, const auto &f)
{
    if(varSizeOpt.has_value()){
        return evalSize(varSizeOpt.value(), widget, arg);
    }
    else{
        return f();
    }
}

int Widget::evalU32Opt(const Widget::VarU32Opt &varU32Opt, const Widget *widget, const auto &f)
{
    if(varU32Opt.has_value()){
        return evalU32(varU32Opt.value(), widget, nullptr);
    }
    else{
        return f();
    }
}

int Widget::evalU32Opt(const Widget::VarU32Opt &varU32Opt, const Widget *widget, const void *arg, const auto &f)
{
    if(varU32Opt.has_value()){
        return evalU32(varU32Opt.value(), widget, arg);
    }
    else{
        return f();
    }
}

template<typename T> T Widget::evalGetter(const Widget::VarGetter<T> &varGetter, const Widget *widget, const void *arg)
{
    return std::visit(VarDispatcher
    {
        [](const T &varg)
        {
            return varg;
        },

        [](const std::function<T()> &varg)
        {
            return varg ? varg() : T{};
        },

        [widget](const std::function<T(const Widget *)> &varg)
        {
            return varg ? varg(widget) : T{};
        },

        [widget, arg](const std::function<T(const Widget *, const void *)> &varg)
        {
            return varg ? varg(widget, arg) : T{};
        },
    },

    varGetter);
}

template<typename Func> Widget::VarInt Widget::transform(Widget::VarInt varInt, Func && func)
{
    if(varInt.index() == 0){
        return func(std::get<int>(varInt));
    }
    else{
        return [varInt = std::move(varInt), func = std::decay_t<Func>(std::forward<Func>(func))](const Widget *widget)
        {
            return func(Widget::evalInt(varInt, widget, nullptr));
        };
    }
}

template<typename Func> Widget::VarSize Widget::transform(Widget::VarSize varSize, Func && func)
{
    if(varSize.index() == 0){
        return func(std::get<int>(varSize));
    }
    else{
        return [varSize = std::move(varSize), func = std::decay_t<Func>(std::forward<Func>(func))](const Widget *widget)
        {
            return func(Widget::evalSize(varSize, widget, nullptr));
        };
    }
}

template<typename Func> Widget::VarSizeOpt Widget::transform(Widget::VarSizeOpt varSize, Func && func)
{
    if(varSize.has_value()){
        return transform(varSize.value(), std::forward<Func>(func));
    }
    else{
        return std::nullopt;
    }
}

template<typename T> bool Widget::hasUpdateFunc(const Widget::VarUpdateFunc<T> &varUpdateFunc)
{
    return std::visit(VarDispatcher
    {
        [](const std::function<void(                        const T &)> &varg) -> bool { return !!varg; },
        [](const std::function<void(const Widget *,         const T &)> &varg) -> bool { return !!varg; },
        [](const std::function<void(const Widget *, void *, const T &)> &varg) -> bool { return !!varg; },

        [](std::nullptr_t){ return false; },
    },

    varUpdateFunc);
}

template<typename T> void Widget::execUpdateFunc(const Widget::VarUpdateFunc<T> &varUpdateFunc, const Widget *widget, const T &arg)
{
    Widget::execUpdateFunc(varUpdateFunc, widget, nullptr, arg);
}

template<typename T> void Widget::execUpdateFunc(const Widget::VarUpdateFunc<T> &varUpdateFunc, const Widget *widget, void *argPtr, const T &arg)
{
    std::visit(VarDispatcher
    {
        [                arg](const std::function<void(                        const T &)> &varg) { if(varg){ varg(                arg); }},
        [widget,         arg](const std::function<void(const Widget *,         const T &)> &varg) { if(varg){ varg(widget,         arg); }},
        [widget, argPtr, arg](const std::function<void(const Widget *, void *, const T &)> &varg) { if(varg){ varg(widget, argPtr, arg); }},

        [](std::nullptr_t){},
    },

    varUpdateFunc);
}

template<typename T> bool Widget::hasCheckFunc(const Widget::VarCheckFunc<T> &varCheckFunc)
{
    return std::visit(VarDispatcher
    {
        [](const std::function<bool(                        const T &)> &varg) -> bool { return !!varg; },
        [](const std::function<bool(const Widget *,         const T &)> &varg) -> bool { return !!varg; },
        [](const std::function<bool(const Widget *, void *, const T &)> &varg) -> bool { return !!varg; },

        [](std::nullptr_t){ return false; },
    },

    varCheckFunc);
}

template<typename T> bool Widget::execCheckFunc(const Widget::VarCheckFunc<T> &varCheckFunc, const Widget *widget, const T &arg)
{
    return Widget::execCheckFunc(varCheckFunc, widget, nullptr, arg);
}

template<typename T> bool Widget::execCheckFunc(const Widget::VarCheckFunc<T> &varCheckFunc, const Widget *widget, void *argPtr, const T &arg)
{
    return std::visit(VarDispatcher
    {
        [                arg](const std::function<bool(                        const T &)> &varg) { return varg ? varg(                arg) : true; },
        [widget,         arg](const std::function<bool(const Widget *,         const T &)> &varg) { return varg ? varg(widget,         arg) : true; },
        [widget, argPtr, arg](const std::function<bool(const Widget *, void *, const T &)> &varg) { return varg ? varg(widget, argPtr, arg) : true; },

        [](std::nullptr_t){ return true; },
    },

    varCheckFunc);
}

auto Widget::focusedChild(this auto && self) -> check_const_cond_out_ptr_t<decltype(self), Widget>
{
    check_const_cond_out_ptr_t<decltype(self), Widget> focusedWidget = nullptr;
    self.foreachChild([&focusedWidget, &self](auto widget, bool)
    {
        if(widget->focus()){
            if(focusedWidget){
                throw fflpanic("{} has multiple focused child: {} and {}", self.name(), focusedWidget->name(), widget->name());
            }
            else{
                focusedWidget = widget;
            }
        }
    });

    if(self.m_attrs.inst.focus){
        if(focusedWidget){
            throw fflpanic("{} and its child {} has focus simutaneously", self.name(), focusedWidget->name());
        }
        return std::addressof(self);
    }

    else if(focusedWidget){
        return focusedWidget;
    }

    else{
        return nullptr;
    }
}

auto Widget::focusedDescendant(this auto && self) -> check_const_cond_out_ptr_t<decltype(self), Widget>
{
    if(auto widget = self.focusedChild()){
        if(widget == std::addressof(self)){
            return std::addressof(self);
        }
        else{
            return widget->focusedDescendant();
        }
    }
    return nullptr;
}

// --- end widget.implement.hpp ---
