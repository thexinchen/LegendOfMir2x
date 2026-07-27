#pragma once

// ===== merged from controlboard/cbface.hpp =====
#include <cstdint>
#include <optional>
#include "serdesmsg.hpp"
#include "widget.hpp"
#include "imageboard.hpp"
#include "gfxcropboard.hpp"
#include "gfxshapeboard.hpp"

class ProcessRun;
class CBFace: public Widget
{
    private:
        constexpr static int BAR_HEIGHT = 3;

    private:
        ProcessRun *m_processRun;

    private:
        double m_accuTime = 0;

    private:
        ImageBoard m_faceFull;

    private:
        GfxCropBoard m_face;
        GfxShapeBoard m_hpBar;

    private:
        GfxShapeBoard m_drawBuffIDList;

    public:
        CBFace( Widget::VarDir,
                Widget::VarInt,
                Widget::VarInt,

                ProcessRun *,

                Widget * = nullptr,
                bool     = false);

    private:
        double getHPRatio() const;
        uint32_t getFaceTexID() const;
        const std::optional<SDBuffIDList> &getSDBuffIDListOpt() const;

    private:
        void drawBuffIDList(int, int, int, int) const;

    private:
        void updateDefault(double fUpdateTime) override
        {
            m_accuTime += fUpdateTime;
        }
};

// ===== merged from controlboard/cbleft.hpp =====
#include <cstdint>
#include <functional>
#include "widget.hpp"
#include "textboard.hpp"
#include "imageboard.hpp"
#include "tritexbutton.hpp"
#include "gfxcropboard.hpp"

class ProcessRun;
class ControlBoard;
class CBLeft: public Widget
{
    private:
        friend class ControlBoard;

    private:
        ProcessRun *m_processRun;

    private:
        ImageBoard   m_bgFull;
        GfxCropBoard m_bg;

        ImageBoard   m_hpFull;
        GfxCropBoard m_hp;

        ImageBoard   m_mpFull;
        GfxCropBoard m_mp;

        ImageBoard   m_levelBarFull;
        GfxCropBoard m_levelBar;

        ImageBoard   m_inventoryBarFull;
        GfxCropBoard m_inventoryBar;

    private:
        TritexButton m_buttonQuickAccess;

    private:
        TritexButton m_buttonClose;
        TritexButton m_buttonMinize;

    private:
        TextBoard    m_mapGLocFull;
        GfxCropBoard m_mapGLoc;

    private:
        const int m_mapGLocMaxWidth   = 120;
        const int m_mapGLocPixelSpeed =  20;

    private:
        double m_mapGLocAccuTime = 0.0;

    public:
        CBLeft(
                Widget::VarDir,
                Widget::VarInt,
                Widget::VarInt,

                ProcessRun *,
                Widget * = nullptr,
                bool     = false);

    private:
        std::string getMapGLocStr() const;

    protected:
        void updateDefault(double fUpdateTime) override
        {
            m_mapGLocAccuTime += fUpdateTime;
            Widget::updateDefault(fUpdateTime);
        }
};

// ===== merged from controlboard/cblevel.hpp =====
#include "widget.hpp"
#include "trigfxbutton.hpp"

class ProcessRun;
class CBLevel: public TrigfxButton
{
    private:
        ProcessRun *m_processRun;

    private:
        Widget m_canvas;

    public:
        CBLevel(
                Widget::VarDir,
                Widget::VarInt,
                Widget::VarInt,

                ProcessRun *,
                Button::TriggerCBFunc,

                Widget * = nullptr,
                bool     = false);
};

// ===== merged from controlboard/cbmiddle.hpp =====
#include <cstdint>
#include <functional>

#include "widget.hpp"
#include "acbutton.hpp"
#include "texslider.hpp"
#include "layoutboard.hpp"
#include "tritexbutton.hpp"
#include "gfxcropboard.hpp"
#include "alphaonbutton.hpp"
#include "gfxshapeboard.hpp"
#include "gfxresizeboard.hpp"

class ProcessRun;
class ControlBoard;
class CBMiddle: public Widget
{
    public:
        constexpr static int CB_MIDDLE_TEX_HEIGHT = 131;

    private:
        constexpr static int LOG_WINDOW_WIDTH_ORIG = 344;
        constexpr static int CMD_WINDOW_WIDTH_ORIG = 344;

        constexpr static int LOG_WINDOW_HEIGHT = 84;
        constexpr static int CMD_WINDOW_HEIGHT = 15;

        constexpr static int LOG_WINDOW_X =  7;
        constexpr static int LOG_WINDOW_Y = 15;

        constexpr static int CMD_WINDOW_X =   7;
        constexpr static int CMD_WINDOW_Y = 106;

    private:
        friend class ControlBoard;

    private:
        ProcessRun *m_processRun;

    private:
        LayoutBoard &m_logBoard;
        LayoutBoard &m_cmdBoard;

    private:
        int m_cmdBoardCropX = 0;

    private:
        GfxShapeBoard m_bg;

    private:
        CBFace m_face;

    private:
        ImageBoard     m_bgImgFull;
        GfxResizeBoard m_bgImg;

    private:
        TritexButton m_switchMode;

    private:
        TexSlider m_slider;

    private:
        GfxCropBoard m_logView;
        GfxCropBoard m_cmdView;

    public:
        CBMiddle(
                Widget::VarDir,
                Widget::VarInt,
                Widget::VarInt,
                Widget::VarSizeOpt,

                ProcessRun *,

                Widget * = nullptr,
                bool     = false);

    public:
        bool processEventDefault(const MirEvent &, bool, Widget::ROIMap) override;

    private:
        void onCmdCR();
        void onCmdCursorMove();

    private:
        int getLogWindowWidth() const { return w() - m_bgImgFull.w() + LOG_WINDOW_WIDTH_ORIG; }
        int getCmdWindowWidth() const { return w() - m_bgImgFull.w() + CMD_WINDOW_WIDTH_ORIG; }
};

// ===== merged from controlboard/cbmiddleexpand.hpp =====
#include "widget.hpp"
#include "gfxshapeboard.hpp"
#include "gfxcropboard.hpp"
#include "imageboard.hpp"
#include "gfxresizeboard.hpp"
#include "tritexbutton.hpp"
#include "texslider.hpp"
#include "layoutboard.hpp"

class ProcessRun;
class ControlBoard;
class CBMiddleExpand: public Widget
{
    private:
        constexpr static int LOG_WINDOW_WIDTH_ORIG = 432;
        constexpr static int CMD_WINDOW_WIDTH_ORIG = 350;

        constexpr static int LOG_WINDOW_HEIGHT_ORIG = 228;
        constexpr static int CMD_WINDOW_HEIGHT      =  40;

        constexpr static int LOG_WINDOW_X =  7;
        constexpr static int LOG_WINDOW_Y = 15;

        constexpr static int CMD_WINDOW_X =  6; // 1 pixel less, interesting
        constexpr static int CMD_WINDOW_Y = 248;

    private:
        friend class ControlBoard;

    private:
        ProcessRun *m_processRun;

    private:
        LayoutBoard &m_logBoard;
        LayoutBoard &m_cmdBoard;

    private:
        int m_cmdBoardCropY = 0;

    private:
        GfxShapeBoard m_bg;

    private:
        ImageBoard     m_bgImgFull;
        GfxResizeBoard m_bgImg;

    private:
        TritexButton m_switchMode;

    private:
        TritexButton m_buttonEmoji;
        TritexButton m_buttonMute;

    private:
        TexSlider m_slider;

    private:
        GfxCropBoard m_logView;
        GfxCropBoard m_cmdView;

    public:
        CBMiddleExpand(
                Widget::VarDir,
                Widget::VarInt,
                Widget::VarInt,

                Widget::VarSizeOpt,

                ProcessRun *,

                Widget * = nullptr,
                bool     = false);

    public:
        bool processEventDefault(const MirEvent &, bool, Widget::ROIMap);

    private:
        int getLogWindowWidth () const { return w() - m_bgImgFull.w() + LOG_WINDOW_WIDTH_ORIG ; }
        int getLogWindowHeight() const { return h() - m_bgImgFull.h() + LOG_WINDOW_HEIGHT_ORIG; }
        int getCmdWindowWidth () const { return w() - m_bgImgFull.w() + CMD_WINDOW_WIDTH_ORIG ; }

    private:
        void onCmdCR();
        void onCmdCursorMove();
};

// ===== merged from controlboard/cbright.hpp =====
#include <cstdint>
#include <functional>
#include "widget.hpp"
#include "acbutton.hpp"
#include "textboard.hpp"
#include "imageboard.hpp"
#include "tritexbutton.hpp"
#include "gfxcropboard.hpp"
#include "alphaonbutton.hpp"

class ProcessRun;
class ControlBoard;
class CBRight: public Widget
{
    private:
        friend class ControlBoard;

    private:
        ProcessRun *m_processRun;

    private:
        ImageBoard   m_bgFull;
        GfxCropBoard m_bg;

    private:
        AlphaOnButton m_buttonExchange;
        AlphaOnButton m_buttonMiniMap;
        AlphaOnButton m_buttonMagicKey;

    private:
        TritexButton m_buttonInventory;
        TritexButton m_buttonHeroState;
        TritexButton m_buttonHeroMagic;

    private:
        TritexButton m_buttonGuild;
        TritexButton m_buttonTeam;
        TritexButton m_buttonQuest;
        TritexButton m_buttonHorse;
        TritexButton m_buttonRuntimeConfig;
        TritexButton m_buttonFriendChat;

    private:
        ACButton m_buttonAC;
        ACButton m_buttonDC;

    public:
        CBRight(
                Widget::VarDir,
                Widget::VarInt,
                Widget::VarInt,

                ProcessRun *,
                Widget * = nullptr,
                bool     = false);
};

// ===== merged from controlboard/cbtitle.hpp =====
#include "widget.hpp"
#include "imageboard.hpp"
#include "texaniboard.hpp"

//                     |
//                     v
//       +-----+      ---
//      /       \      21
//  +--/  TITLE  \--+ ---
//  | /           \ |  ^
//  +---------------+  |

class ProcessRun;
class CBTitle: public Widget
{
    public:
        constexpr static int UP_HEIGHT = 21;

    private:
        ProcessRun *m_processRun;

    private:
        ImageBoard m_bg;
        TexAniBoard m_arcAni;

    private:
        CBLevel m_level;

    public:
        CBTitle(
                Widget::VarDir,
                Widget::VarInt,
                Widget::VarInt,

                ProcessRun *,

                Widget * = nullptr,
                bool    = true);
};

// ===== merged from controlboard/controlboard.hpp =====
#include <string_view>
#include "widget.hpp"

// for texture 0X00000012 and 0X00000013
// I split it into many parts to fix different screen size
// for screen width is not 800 we build a new interface using these two
//
// 0X00000012 : 800 x 133:  left and right
// 0X00000013 : 456 x 131:  middle log
// 0X00000022 : 127 x 41 :  title
//
//                         +-----------+                           ---
//                          \  title  /                             ^
// +------+==----------------+       +----------------==+--------+  |  --- <-- left/right is 133, middle is 131
// |      $                                        +---+$        | 152  | ---
// |      |                                        |   ||        |  |  133 | 120 as underlay log
// |      |                                        +---+|        |  V   |  |
// +------+---------------------------------------------+--------+ --- -- ---
// ^      ^    ^           ^           ^          ^     ^        ^
// | 178  | 50 |    110    |   127     |    50    | 119 |   166  | = 800
//
// |---fixed---|-------------repeat---------------|---fixed------|

//
// 0X00000027 : 456 x 298: char box frame
//
//                         +-----------+                        ---
//                          \  title  /                          ^
//        +==----------------+       +----------------==+ ---    |  ---- <-- startY
//        $                                             $  ^     |   47
//        |                                             |  |     |  ----
//        |                                             |  |     |   |
//        |                                             |  |     |  196: use to repeat, as m_stretchH
//        |                                             | 298   319  |
//        +---------------------------------------+-----+  |     |  ----
//        |                                       |     |  |     |   55
//        |                                       |() ()|  |     |   |
//        |                                       |     |  v     v   |
// +------+---------------------------------------+-----+--------+ --- -- ---
// ^      ^    ^           ^           ^          ^     ^        ^
// | 178  | 50 |    110    |   127     |    50    | 119 |   166  | = 800
//
// |---fixed---|-------------repeat---------------|---fixed------|

enum
{
    CBLOG_DEF = 0,
    CBLOG_SYS,
    CBLOG_DBG,
    CBLOG_ERR,
};

class CBMiddle;
class ProcessRun;
class ControlBoard: public Widget
{
    private:
        friend class CBLeft;
        friend class CBRight;
        friend class CBTitle;
        friend class CBMiddle;
        friend class CBMiddleExpand;

    private:
        ProcessRun *m_processRun;

    private:
        bool m_expand   = false;
        bool m_maximize = false;
        bool m_minimize = false;

    private:
        LayoutBoard m_logBoard;
        LayoutBoard m_cmdBoard;

    private:
        CBLeft  m_left;
        CBRight m_right;

        CBMiddle       m_middle;
        CBMiddleExpand m_middleExpand;

        CBTitle m_title;

    public:
        ControlBoard(
                ProcessRun *,

                Widget * = nullptr,
                bool     = false);

    public:
        void addXMLLog(const char *);
        void addParLog(const char *);

    public:
        void addLog(int, const char *);

    public:
       TritexButton *getButton(const std::string_view &);

    private:
       void onClickSwitchModeButton(int);

    private:
       void onInputDone();

    public:
       int shiftHeight() const;
};
