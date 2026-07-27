#pragma once

// ===== merged from friendchatboard/friendchatboardconst.hpp =====
constexpr int UIPage_DRAGBORDER[4] // for board resize
{
    10,
    10,
    12,
    10,
};

constexpr int UIPage_BORDER[4]
{
    54,
    10,
    13,
    38,
};

constexpr int UIPage_MARGIN     =   4; // drawing area margin
constexpr int UIPage_MIN_WIDTH  = 400; // the area excludes border area, margin included
constexpr int UIPage_MIN_HEIGHT = 400;

enum UIPageType: int
{
    UIPage_CHAT = 0,
    UIPage_CHATPREVIEW,
    UIPage_FRIENDLIST,
    UIPage_FRIENDSEARCH,
    UIPage_CREATEGROUP,
    UIPage_END,
};

// ===== merged from friendchatboard/chatitemref.hpp =====
#include <string>
#include "gui_core.hpp"
#include "gui_widgets.hpp"
#include "layoutboard.hpp"

class ChatItemRef: public Widget
{
    //  ->|                              |<------ WIDTH = MARGIN * 2 + message.w()
    //  ->| |<----------------------------------- MARGIN
    //  ->||<------------------------------------ CORNER
    //    /------------------------------\  -
    //    | +----------------------+     |  |
    //    | |        message       | (x) |  +---- HEIGHT = MARGIN * 2 + message.h()
    //    | +----------------------+     |  |
    //    \------------------------------/  -
    //                           ->| |<---------- BUTTON_MARGIN
    //                              ->||<-------- BUTTON_R
    //                               ->| |<------ BUTTON_MARGIN

    public:
        constexpr static int MARGIN = 3;
        constexpr static int CORNER = 3;

        constexpr static int BUTTON_MARGIN = 5;

        constexpr static int BUTTON_R = 7;
        constexpr static int BUTTON_D = ChatItemRef::BUTTON_R * 2 - 1;

        constexpr static uint8_t CROSS_FONT_SIZES[3] {13, 13, 8};

    private:
        uint32_t m_crossBgColor;

    private:
        GfxShapeBoard m_background; // round corner rectangle

    private:
        LabelBoard     m_cross;
        GfxShapeBoard m_crossBg;        // round cover under x
        Widget         m_crossButtonGfx; // merge cross and crossBg to be a single gfx-widget, then use it in TrigfxButton
        TrigfxButton   m_crossButton;    //

    private:
        uint64_t m_refer;

    private:
        LayoutBoard m_message;

    public:
        ChatItemRef(
                Widget::VarDir,
                Widget::VarInt,
                Widget::VarInt,
                int, // max width

                bool, // force max width
                bool, // show x button

                uint64_t,
                std::string,

                Widget * = nullptr,
                bool     = false);

    public:
        std::string getXML() const
        {
            return m_message.getXML();
        }

        uint64_t refer() const
        {
            return m_refer;
        }

    public:
        void loadXML(std::string argXMLStr)
        {
            m_message.loadXML(argXMLStr.c_str());
        }
};

// ===== merged from friendchatboard/chatitem.hpp =====
#include <cstdint>
#include <optional>
#include <functional>
#include "gui_core.hpp"

//            WIDTH
// |<----------------------->|
//       GAP
//     ->| |<-
// +-----+     +--------+      -
// |     |     |  name  |      | NAME_HEIGHT
// |     |     +--------+      -
// |     |     /-------------\             <----+
// | IMG |     | ........... |                  |
// |     |     | ........... |                  |
// |     |    /  ........... |                  |
// |     |  <    ........... |                  |
// |     |    \  ........... |                  |
// |     |  ^  | ........... |                  | background includes messsage round-corner background box and the triangle area
// +-----+  |  | ........... |                  |
//          |  | ........... |                  |
//          |  \-------------/<- MESSAGE_CORNER |  -
//          |            ->| |<-                |  ^
//          |             MESSAGE_MARGIN        |  |
//          +-----------------------------------+  +-- REF_GAP
//                                                 |
//             /---------\                         -
//             | msg ref |
//             \---------/
//
//
//            -->|  |<-- TRIANGLE_WIDTH
//                2 +                + 2                    -
//      -----+     /|                |\     +-----          ^
//           |    / |                | \    |               |
//    avatar | 1 +  |                |  + 1 | avatar        | TRIANGLE_HEIGHT
//           |    \ |                | /    |               |
//      -----+     \|                |/     +-----          v
//                3 +                + 3                    -
//           |<->| GAP                  |<->| GAP
//               ^                      ^
//               |                      |
//               +-- background startX  +-- background endX

struct ChatItem: public Widget
{
    constexpr static int AVATAR_WIDTH  = 35;
    constexpr static int AVATAR_HEIGHT = AVATAR_WIDTH * 94 / 84;

    constexpr static int GAP = 5;
    constexpr static int NAME_HEIGHT = 20;

    constexpr static int TRIANGLE_WIDTH  = 4;
    constexpr static int TRIANGLE_HEIGHT = 6;

    constexpr static int MESSAGE_MARGIN = 5;
    constexpr static int MESSAGE_CORNER = 3;

    constexpr static int MESSAGE_MIN_WIDTH  = 10; // handling small size message
    constexpr static int MESSAGE_MIN_HEIGHT = 10;

    constexpr static int REF_GAP = 10;

    struct InitArgs
    {
        Widget::VarDir dir = DIR_UPLEFT;

        Widget::VarInt x = 0;
        Widget::VarInt y = 0;

        int  maxWidth = 100;
        bool pending  = true;

        std::optional<uint64_t>    msgID = std::nullopt;
        std::optional<uint64_t> msgRefID = std::nullopt;

        const char8_t *name       = nullptr;
        const char8_t *message    = nullptr;
        const char8_t *messageRef = nullptr;

        Widget::VarTexLoadFunc texLoadFunc = GLTexID{};
        Widget::VarU32Opt      bgColor     = std::nullopt;

        bool showName   = true;
        bool avatarLeft = true;

        Widget::WADPair parent {};
    };

    bool pending = true;
    double accuTime = 0.0;

    // nullopt when message id is pending
    // also used to support fake chat message that has no message id
    std::optional<uint64_t> msgID = std::nullopt;

    const bool showName;
    const bool avatarLeft;
    const Widget::VarU32Opt bgColor;

    ImageBoard avatar;
    LabelBoard name;

    LayoutBoard message;
    GfxShapeBoard background;

    ChatItemRef * const msgref = nullptr;

    ChatItem(ChatItem::InitArgs);

    void setMaxWidth(int);
    void updateDefault(double) override;
    bool processEventDefault(const MirEvent &, bool, Widget::ROIMap) override;
};

// ===== merged from friendchatboard/chatinputcontainer.hpp =====
#include "gui_core.hpp"
#include "layoutboard.hpp"

struct ChatInputContainer: public Widget
{
    LayoutBoard layout;
    ChatInputContainer(
            Widget::VarDir,
            Widget::VarInt,
            Widget::VarInt,
            Widget::VarSizeOpt, // width only

            Widget * = nullptr,
            bool     = false);
};

// ===== merged from friendchatboard/chatitemcontainer.hpp =====
#include <array>
#include <string>
#include <unordered_map>
#include "serdesmsg.hpp"
#include "gui_core.hpp"
#include "layoutboard.hpp"

struct ChatItemContainer: public Widget
{
    constexpr static int ITEM_SPACE = 5;
    constexpr static int BACKGROUND_MARGIN = 3;
    constexpr static int BACKGROUND_CORNER = 4;

    // use canvas to hold all chat item
    // then we can align canvas always to buttom when needed
    //
    // when scroll we can only move canvas inside this container
    // no need to move chat item one by one

    ItemBox canvas;

    LabelBoard nomsg; // show when there is no chat message
    LayoutBoard ops;  // all kinds of ops, including block strangers, add friends, etc

    MarginContainer nomsgBox;
    MarginContainer   opsBox;

    ChatItemContainer(
            Widget::VarDir,
            Widget::VarInt,
            Widget::VarInt,

            Widget::VarSizeOpt,
            Widget::VarSizeOpt,

            Widget * = nullptr,
            bool     = false);

    int chatItemMaxWidth() const;
    const SDChatPeer &getChatPeer() const;

    void clearChatItem(bool);
    void append(const SDChatMessage &, std::function<void(const ChatItem *)>);
};

// ===== merged from friendchatboard/chatpage.hpp =====
#include <array>
#include <string>
#include <optional>
#include <unordered_map>
#include "serdesmsg.hpp"
#include "gui_core.hpp"

struct ChatPage: public Widget
{
    // chat page is different, it uses the UIPage_MARGIN area
    // because we fill different color to chat area and input area
    //
    //         |<--- UIPage_MIN_WIDTH ---->|
    //       ->||<-- UIPage_MARGIN                       v
    //       - +---------------------------+             -
    //       ^ |+-------------------------+|           - -
    //       | || +------+                ||           ^ ^
    //       | || |******|                ||           | |
    //       | || +------+                ||           | + UIPage_MARGIN
    //       | ||                +------+ ||           |
    //  U    | ||                |******| ||           |
    //  I    | ||                +------+ ||           |
    //  P    | || +------------+          ||           +-- UIPage_MIN_HEIGHT
    //  a    | || |************|          ||           |         - UIPage_MARGIN * 2            // top & bottom margin
    //  g ---+ || |*****       |          ||           |         -    SEP_MARGIN * 2 - 1        // middle area between chat area and input area
    //  e    | || +------------+          ||           |         -  INPUT_MARGIN * 2            //
    //  |    | ||                         ||           |         - input.h()                    //
    //  H    | ||       chat area         ||         | |         - (showref() ? (chatref->h() + CHATREF_GAP) : 0)
    //  E    | ||                         ||         v v
    //  I    | |+-------------------------+|       | - -
    //  G    | +===========================+       v   SEP_MARGIN * 2 + 1
    //  H    | |  +---------------------+  |       - -
    //  T    | | / +-------------------+ \ |     - -<- INPUT_MARGIN
    //       | ||  |*******************|  ||     ^ ^
    //       | ||  |****input area*****|  ||   | +---- input.h()
    //       | ||  |*******************|  || | v v
    //       | | \ +-------------------+ / | v - -                +--- showref() ? chatref->h() : 0
    //       | |  +---------------------+  | - -                  |
    //       | |                           |   ^                  v
    //       | |+-------------------------+| - |                  -
    //       | ||        ChatRef      (x) || ^ +-- INPUT_MARGIN
    //       v |+-------------------------+| |                    -
    //       - +---------------------------+ +---- showref() ? CHATREF_GAP : 0
    //       ->||<---- UIPage_MARGIN
    //       -->| |<--  INPUT_CORNER
    //       -->|  |<-  INPUT_MARGIN
    //             |<--- input.w() --->|

    constexpr static int SEP_MARGIN = 2;

    constexpr static int INPUT_CORNER = 8;
    constexpr static int INPUT_MARGIN = 8;

    constexpr static int INPUT_MIN_HEIGHT =  10;
    constexpr static int INPUT_MAX_HEIGHT = 200;

    constexpr static int CHATREF_GAP = 5;

    SDChatPeer peer;
    GfxShapeBoard background;

    ChatItemRef *chatref = nullptr;

    ChatInputContainer input;
    ChatItemContainer  chat;

    MenuBoard *menu = nullptr;

    ChatPage(
            Widget::VarDir,
            Widget::VarInt,
            Widget::VarInt,
            Widget::VarSizeOpt,
            Widget::VarSizeOpt,

            Widget * = nullptr,
            bool     = false);

    bool showref() const;
    bool showmenu() const;

    std::optional<uint64_t> refopt() const;

    void  enableChatRef(uint64_t, std::string);
    void disableChatRef();

    static ChatItemRef *createChatItemRef(uint64_t, std::string, Widget *, bool);

    void afterResizeDefault() override;
    bool processEventDefault(const MirEvent &, bool, Widget::ROIMap) override;
};

// ===== merged from friendchatboard/chatpreviewitem.hpp =====
#include "serdesmsg.hpp"
#include "gui_core.hpp"
#include "layoutboard.hpp"

struct ChatPreviewItem: public Widget
{
    constexpr static int HEIGHT = 50;

    constexpr static int ITEM_MARGIN = 5;
    constexpr static int GAP = 10;

    constexpr static int NAME_HEIGHT = 24;
    constexpr static int AVATAR_WIDTH = (HEIGHT - ITEM_MARGIN * 2) * 84 / 94; // original avatar size: 84 x 94

    // ITEM_MARGIN   GAP
    //   ->|  |<-   |<->|
    //     +------------------------------+ -                           -
    //     |                              | | ITEM_MARGIN               |
    //     |  +-+---+   +------+          | -             -             |
    //     |  |1|   |   | name |          |               | NAME_HEIGHT |
    //     |  +-+   |   +------+          |               -             | HEIGHT
    //     |  | IMG |   +--------------+  |                             |
    //     |  |     |   |latest message|  |                             |
    //     |  +-----+   +--------------+  |                             |
    //     |                              |                             |
    //     +------------------------------+                             -
    //
    //        |<--->|
    //      AVATAR_WIDTH
    //
    //     |<---------------------------->| UIPage_MIN_WIDTH - UIPage_MARGIN * 2
    //
    const SDChatPeerID cpid;

    ImageBoard avatar;
    LabelBoard name;

    LayoutBoard message;
    Widget      messageClip;

    GfxShapeBoard selected;

    ChatPreviewItem(
            Widget::VarDir,
            Widget::VarInt,
            Widget::VarInt,
            Widget::VarSizeOpt,

            const SDChatPeerID &,
            const char8_t *,

            Widget * = nullptr,
            bool     = false);

    bool processEventDefault(const MirEvent &, bool, Widget::ROIMap) override;
};

// ===== merged from friendchatboard/chatpreviewpage.hpp =====
#include <string>
#include "serdesmsg.hpp"
#include "gui_core.hpp"

struct ChatPreviewPage: public Widget
{
    Widget canvas;
    ChatPreviewPage(Widget::VarDir,

            Widget::VarInt,
            Widget::VarInt,
            Widget::VarSizeOpt,
            Widget::VarSizeOpt,

            Widget * = nullptr,
            bool     = false);

    void updateChatPreview(const SDChatPeerID &, const std::string &);
};

// ===== merged from friendchatboard/frienditem.hpp =====
#include <functional>
#include "serdesmsg.hpp"
#include "gui_core.hpp"
#include "layoutboard.hpp"

struct FriendItem: public Widget
{
    //   ITEM_MARGIN                    | ITRM_MARGIN
    // ->| |<-                          v
    //   +---------------------------+ - -
    //   | +-----+                   | ^ -
    //   | |     | +------+ +------+ | | ^
    //   | | IMG | | NAME | | FUNC | | | HEIGHT
    //   | |     | +------+ +------+ | |
    //   | +-----+                   | v
    //   +---------------------------+ -
    //         ->| |<-          -->| |<-- FUNC_MARGIN
    //           GAP
    //   |<------------------------->| UIPage_MIN_WIDTH - UIPage_MARGIN * 2

    constexpr static int HEIGHT = 40;
    constexpr static int ITEM_MARGIN = 5;
    constexpr static int AVATAR_WIDTH = (HEIGHT - ITEM_MARGIN * 2) * 84 / 94;

    constexpr static int GAP = 5;
    constexpr static int FUNC_MARGIN = 5;

    SDChatPeerID cpid;

    uint64_t funcWidgetID;
    std::function<void(FriendItem *)> onClick;

    GfxShapeBoard hovered;

    ImageBoard avatar;
    LabelBoard name;

    FriendItem(Widget::VarDir,
            Widget::VarInt,
            Widget::VarInt,
            Widget::VarSizeOpt, // flexible width

            const SDChatPeerID &,

            const char8_t *,
            std::function<GLTexID (const Widget *)>,

            std::function<void(FriendItem *)> = nullptr,
            std::pair<Widget *, bool> argFuncWidget = {},

            Widget * = nullptr,
            bool     = false);

    void setFuncWidget(Widget *, bool);
    bool processEventDefault(const MirEvent &, bool, Widget::ROIMap) override;
};

// ===== merged from friendchatboard/friendlistpage.hpp =====
#include <functional>
#include <utility>
#include "serdesmsg.hpp"
#include "gui_core.hpp"

struct FriendListPage: public Widget
{
    Widget canvas;
    FriendListPage(Widget::VarDir,
            Widget::VarInt,
            Widget::VarInt,

            Widget::VarSizeOpt,
            Widget::VarSizeOpt,

            Widget * = nullptr,
            bool     = false);

    void append(const SDChatPeer &, std::function<void(FriendItem *)> = nullptr, std::pair<Widget *, bool> = {});
};

// ===== merged from friendchatboard/pagecontrol.hpp =====
#include <utility>
#include <initializer_list>
#include "gui_core.hpp"

struct PageControl: public Widget
{
    PageControl(
            Widget::VarDir,
            Widget::VarInt,
            Widget::VarInt,

            int,

            std::initializer_list<std::pair<Widget *, bool>>,

            Widget * = nullptr,
            bool     = false);
};

// ===== merged from friendchatboard/searchinputline.hpp =====
#include "gui_core.hpp"
#include "layoutboard.hpp"

struct SearchInputLine: public Widget
{
    // o: (0,0)
    // x: (3,3) : fixed by gfx resource border
    //
    //   o==========================+ -
    //   | x+---+ +---------------+ | ^
    //   | ||O、| |xxxxxxx        | | | HEIGHT
    //   | ++---+ +---------------+ | v
    //   +==========================+ -
    //
    //   |<------- WIDTH --------->|
    //      |<->|
    //   ICON_WIDTH
    //
    //   ->||<- ICON_MARGIN
    //
    //       -->| |<-- GAP

    constexpr static int WIDTH = UIPage_MIN_WIDTH - UIPage_MARGIN * 2 - 60;
    constexpr static int HEIGHT = 30;

    constexpr static int ICON_WIDTH = 20;
    constexpr static int ICON_MARGIN = 5;
    constexpr static int GAP = 5;

    ImageBoard image;
    GfxResizeBoard inputbg;

    ImageBoard icon;
    InputLine  input;
    LabelBoard hint;

    SearchInputLine(Widget::VarDir,

            Widget::VarInt,
            Widget::VarInt,

            Widget * = nullptr,
            bool     = false);
};

// ===== merged from friendchatboard/searchautocompletionitem.hpp =====
#include "serdesmsg.hpp"
#include "gui_core.hpp"

struct SearchAutoCompletionItem: public Widget
{
    // o: (0,0)
    // x: (3,3)
    //
    //   o--------------------------+ -
    //   | x+---+ +---------------+ | ^
    //   | ||O、| |label          | | | HEIGHT
    //   | ++---+ +---------------+ | v
    //   +--------------------------+ -
    //   |<-------- WIDTH --------->|
    //      |<->|
    //   ICON_WIDTH
    //
    //   ->||<- ICON_MARGIN
    //
    //       -->| |<-- GAP

    constexpr static int WIDTH = UIPage_MIN_WIDTH - UIPage_MARGIN * 2;
    constexpr static int HEIGHT = 30;

    constexpr static int ICON_WIDTH = 20;
    constexpr static int ICON_MARGIN = 5;
    constexpr static int GAP = 5;

    const bool byID; // when clicked, fill input automatically by ID if true, or name if false
    const SDChatPeer candidate;

    GfxShapeBoard background;

    ImageBoard icon;
    LabelBoard label;

    SearchAutoCompletionItem(Widget::VarDir,

            Widget::VarInt,
            Widget::VarInt,

            bool,
            SDChatPeer,

            const char * = nullptr,

            Widget * = nullptr,
            bool     = false);

    bool processEventDefault(const MirEvent &, bool, Widget::ROIMap) override;
};

// ===== merged from friendchatboard/searchpage.hpp =====
#include <string>
#include "serdesmsg.hpp"
#include "gui_core.hpp"
#include "layoutboard.hpp"

struct SearchPage: public Widget
{
    //                  -->| |<-- CLEAR_GAP
    // |<----------WIDTH----------->|
    // +-------------------+ +------+
    // |      INPUT        | | 清空 |
    // +-------------------+ +------+
    // | auto completion item       |
    // +----------------------------+
    // | auto completion item       |
    // +----------------------------+

    constexpr static int WIDTH  = UIPage_MIN_WIDTH  - UIPage_MARGIN * 2;
    constexpr static int HEIGHT = UIPage_MIN_HEIGHT - UIPage_MARGIN * 2;

    constexpr static int CLEAR_GAP = 10;

    SearchInputLine input;
    LayoutBoard clear;

    Widget autocompletes;
    Widget candidates;

    SearchPage(Widget::VarDir,

            Widget::VarInt,
            Widget::VarInt,

            Widget * = nullptr,
            bool     = false);

    void appendFriendItem(const SDChatPeer &);
    void appendAutoCompletionItem(bool, const SDChatPeer &, const std::string &);
};

// ===== merged from friendchatboard/friendchatboard.hpp =====
#include <array>
#include <string>
#include <optional>
#include <unordered_map>
#include "serdesmsg.hpp"
#include "gui_core.hpp"

class ProcessRun;
class FriendChatBoard: public Widget
{
    private:
        friend struct ChatItem;
        friend struct ChatItemContainer;
        friend struct ChatInputContainer;
        friend struct ChatPreviewItem;
        friend struct SearchPage;

    private:
        struct UIPage
        {
            LabelBoard * const title   = nullptr;
            Widget     * const control = nullptr;
            TexSlider  * const slider  = nullptr;
            Widget     * const page    = nullptr;

            std::function<void(int, UIPage *)> enter = nullptr;
            std::function<void(int, UIPage *)> exit  = nullptr;
        };

    private:
        struct FriendMessage
        {
            SDChatPeerID cpid;
            size_t unread = 0;
            std::vector<SDChatMessage> list;
        };

    private:
        ProcessRun *m_processRun;

    private:
        std::optional<int> m_dragIndex {};

    private:
        SDFriendList m_sdFriendList;
        std::list<SDChatPeer> m_cachedChatPeerList;

    private:
        std::unordered_map<uint64_t, SDChatMessage> m_cachedChatMessageList;

    private:
        std::unordered_map<uint64_t, SDChatMessage> m_localMessageList;
        std::list<FriendMessage> m_friendMessageList;

    private:
        ImageBoard m_frame;
        GfxResizeBoard m_frameCropDup;

    private:
        ImageBoard m_background;
        GfxResizeBoard m_backgroundCropDup;

    private:
        TritexButton m_close;

    private:
        GfxShapeBoard m_dragArea;

    private:
        int m_uiLastPage = UIPage_CHATPREVIEW;
        int m_uiPage     = UIPage_CHATPREVIEW;
        std::array<FriendChatBoard::UIPage, UIPage_END> m_uiPageList; // {buttons, page}

    public:
        FriendChatBoard(
                Widget::VarInt,
                Widget::VarInt,

                ProcessRun *,

                Widget * = nullptr,
                bool     = false);

    public:
        void drawDefault(Widget::ROIMap) const override;

    public:
        bool processEventDefault(const MirEvent &, bool, Widget::ROIMap) override;

    public:
        const SDChatPeer *findChatPeer      (const SDChatPeerID &) const;
        const SDChatPeer *findFriendChatPeer(const SDChatPeerID &) const;

    private:
        void queryChatMessage(uint64_t, std::function<void(const SDChatMessage *, bool)>);
        void queryChatPeer(const SDChatPeerID &, std::function<void(const SDChatPeer *, bool /* async */)>);

    public:
        void addMessage(std::optional<uint64_t>, const SDChatMessage &);
        void addMessagePending(uint64_t, const SDChatMessage &);

    public:
        void finishMessagePending(size_t, const SDChatMessageDBSeq &);

    public:
        void setFriendList(const SDFriendList &);

    public:
        void setChatPeer(const SDChatPeer &, bool);
        void setUIPage(int);

    public:
        void loadChatPage();

    public:
        static       FriendChatBoard *getParentBoard(      Widget *);
        static const FriendChatBoard *getParentBoard(const Widget *);

    public:
        void addGroup(const SDChatPeer &);
        void addFriendListChatPeer(const SDChatPeerID &);

    public:
        void requestAddFriend      (const SDChatPeer &, bool);
        void requestAcceptAddFriend(const SDChatPeer &);
        void requestRejectAddFriend(const SDChatPeer &);
        void requestBlockPlayer    (const SDChatPeer &);

    public:
        void onAddFriendAccepted(const SDChatPeer &);
        void onAddFriendRejected(const SDChatPeer &);

    private:
        std::optional<int> getEdgeDragIndex(int, int) const;
};
