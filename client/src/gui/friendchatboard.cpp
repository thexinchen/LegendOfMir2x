#include "friendchatboard.hpp"

// ===== merged from friendchatboard/chatitemref.cpp =====
#include "gldevice.hpp"
#include "processrun.hpp"

extern GLDevice *g_glDevice;

ChatItemRef::ChatItemRef(
        Widget::VarDir argDir,
        Widget::VarInt argX,
        Widget::VarInt argY,

        int argMaxWidth,

        bool argForceWidth,
        bool argShowButton,

        uint64_t argRef,
        std::string argLayoutXML,

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

    , m_crossBgColor(colorf::GREY + colorf::A_SHF(255))
    , m_background
      {{
          .w = [this](const Widget *){ return w(); },
          .h = [this](const Widget *){ return h(); },

          .drawFunc = [](const Widget *self, int dstDrawX, int dstDrawY)
          {
              g_glDevice->fillRectangle(colorf::GREY + colorf::A_SHF(200), dstDrawX, dstDrawY, self->w(), self->h(), ChatItemRef::CORNER);
          },

          .parent{this},
      }}

    , m_cross
      {{
          .label = u8"×", // multiplication sign for better symmetry
          .font
          {
              .id = 1,
              .size = ChatItemRef::CROSS_FONT_SIZES[0],
          },
      }}

    , m_crossBg
      {{
          .w = ChatItemRef::BUTTON_D,
          .h = ChatItemRef::BUTTON_D,

          .drawFunc = [this](const Widget *, int drawDstX, int drawDstY)
          {
              if(auto texPtr = g_glDevice->getCover(ChatItemRef::BUTTON_R, 360)){
                  const GLDeviceHelper::EnableRenderBlendMode enableBlendMode(MIR_BLENDMODE_BLEND);
                  const GLDeviceHelper::EnableTextureModColor enableModColor(texPtr, m_crossBgColor);
                  g_glDevice->drawTexture(texPtr, drawDstX, drawDstY);
              }
          },
      }}

    , m_crossButtonGfx
      {{
          .w = [this](const Widget *){ return m_crossBg.w(); },
          .h = [this](const Widget *){ return m_crossBg.h(); },

          .childList
          {
              {&m_crossBg, DIR_NONE, [this](const Widget *){ return m_crossButtonGfx.w() / 2; }, [this](const Widget *){ return m_crossButtonGfx.h() / 2; }, false},
              {&m_cross  , DIR_NONE, [this](const Widget *){ return m_crossButtonGfx.w() / 2; }, [this](const Widget *){ return m_crossButtonGfx.h() / 2; }, false},
          },
      }}

    , m_crossButton
      {{
          .dir = DIR_RIGHT,
          .x = [this]{ return w() - ChatItemRef::BUTTON_MARGIN - 1; },
          .y = [this]{ return h() / 2;                              },

          .gfxList
          {
              &m_crossButtonGfx,
              &m_crossButtonGfx,
              &m_crossButtonGfx,
          },

          .onOverIn = [this](Widget *)
          {
              m_crossBgColor = colorf::BLUE + colorf::A_SHF(64);
              m_cross.setFontSize(ChatItemRef::CROSS_FONT_SIZES[1]);
          },

          .onOverOut = [this](Widget *)
          {
              m_crossBgColor = colorf::GREY + colorf::A_SHF(255);
              m_cross.setFontSize(ChatItemRef::CROSS_FONT_SIZES[0]);
          },

          .onClick = [this](Widget *, bool clickDone, int)
          {
              if(clickDone){
              }
              else{
                  m_cross.setFontSize(ChatItemRef::CROSS_FONT_SIZES[2]);
              }
          },

          .onTrigger = [this](Widget *, int)
          {
              setShow(false);
          },

          .parent{this},
      }}

    , m_refer(argRef)

    , m_message
      {{
          .x = ChatItemRef::MARGIN,
          .y = ChatItemRef::MARGIN,

          .lineWidth = std::max<int>(1, argShowButton ? (argMaxWidth - ChatItemRef::MARGIN - ChatItemRef::BUTTON_MARGIN * 2 - ChatItemRef::BUTTON_D)
                                                      : (argMaxWidth - ChatItemRef::MARGIN * 2)),
          .initXML = argLayoutXML.c_str(),

          .font
          {
              .id = 1,
              .size = 10,
          },

          .parent{this},
      }}
{
    m_crossButton.setShow(argShowButton);

    if(argForceWidth){
        setW(std::max<int>(0, argMaxWidth));
    }
    else{
        setW([argShowButton, this](const Widget *)
        {
            if(argShowButton){
                return ChatItemRef::MARGIN + m_message.w() + ChatItemRef::BUTTON_MARGIN * 2 + ChatItemRef::BUTTON_D;
            }
            else{
                return ChatItemRef::MARGIN * 2 + m_message.w();
            }
        });
    }

    setH([this](const Widget *)
    {
        return std::max<int>(ChatItemRef::MARGIN * 2 + m_message.h(), ChatItemRef::BUTTON_D);
    });
}

// ===== merged from friendchatboard/chatitem.cpp =====
#include "utf8f.hpp"
#include "gldevice.hpp"
#include "processrun.hpp"

extern GLDevice *g_glDevice;

ChatItem::ChatItem(ChatItem::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .w = std::nullopt,
          .h = std::nullopt,

          .attrs
          {
              .type
              {
                  .setSize = false,
              },
          },

          .parent = std::move(args.parent),
      }}

    , pending(args.pending)
    , msgID  (args.msgID  )

    , showName  (args.showName  )
    , avatarLeft(args.avatarLeft)

    , bgColor(std::move(args.bgColor))

    , avatar
      {{
          .w = ChatItem::AVATAR_WIDTH,
          .h = ChatItem::AVATAR_HEIGHT,

          .texLoadFunc = std::move(args.texLoadFunc),
      }}

    , name
      {{
          .label = args.name,
          .font
          {
              .size = 10,
          },
      }}

    , message
      {{
          .lineWidth = std::max<int>(1, args.maxWidth - ChatItem::AVATAR_WIDTH - ChatItem::GAP - ChatItem::TRIANGLE_WIDTH - ChatItem::MESSAGE_MARGIN * 2),
          .initXML   = to_cstr(args.message),

          .onClickText = [this](const std::unordered_map<std::string, std::string> &attrList, int event)
          {
              if(event != BEVENT_RELEASE){
                  return;
              }

              const auto idstr = LayoutBoard::findAttrValue(attrList, "id");
              fflassert(idstr);

              const auto id = to_sv(idstr);
              if(id == SYS_AFRESP){
                  const auto cpidstr = LayoutBoard::findAttrValue(attrList, "cpid");
                  fflassert(cpidstr);

                  FriendChatBoard::getParentBoard(this)->queryChatPeer(SDChatPeerID(std::stoull(cpidstr)), [attrList, this](const SDChatPeer *sdCP, bool)
                  {
                      // sdCP is null when the server can't resolve the referenced peer (deleted account, etc.); nothing meaningful to do
                      if(!sdCP){
                          return;
                      }
                      if(LayoutBoard::findAttrValue(attrList, "accept")){
                          FriendChatBoard::getParentBoard(this)->requestAcceptAddFriend(*sdCP);
                          if(LayoutBoard::findAttrValue(attrList, "addfriend")){
                              if(FriendChatBoard::getParentBoard(this)->findFriendChatPeer(sdCP->cpid())){
                                  FriendChatBoard::getParentBoard(this)->m_processRun->addCBParLog(u8R"###(<par bgcolor="rgb(0x00, 0x80, 0x00)"><t color="red">%s</t>已经是你的好友。</par>)###", to_cstr(sdCP->name));
                              }
                              else{
                                  FriendChatBoard::getParentBoard(this)->requestAddFriend(*sdCP, false);
                              }
                          }
                      }
                      else if(LayoutBoard::findAttrValue(attrList, "reject")){
                          FriendChatBoard::getParentBoard(this)->requestRejectAddFriend(*sdCP);
                          if(LayoutBoard::findAttrValue(attrList, "block")){
                              FriendChatBoard::getParentBoard(this)->requestBlockPlayer(*sdCP);
                          }
                      }
                  });
              }
          },
      }}

    , background
      {{
          .w = [this](const Widget *){ return ChatItem::MESSAGE_MARGIN * 2 + std::max<int>(message.w(), ChatItem::MESSAGE_MIN_WIDTH ) + ChatItem::TRIANGLE_WIDTH; },
          .h = [this](const Widget *){ return ChatItem::MESSAGE_MARGIN * 2 + std::max<int>(message.h(), ChatItem::MESSAGE_MIN_HEIGHT)                           ; },

          .drawFunc = [this](const Widget *, int drawDstX, int drawDstY)
          {
              const uint32_t drawBGColor = Widget::evalU32Opt(bgColor, this, [this]
              {
                  if(avatarLeft){
                      return colorf::RED + colorf::A_SHF(128);
                  }
                  else if(pending){
                      return colorf::fadeRGBA(colorf::GREY + colorf::A_SHF(128), colorf::GREEN + colorf::A_SHF(128), std::fabs(std::fmod(accuTime / 1000.0, 2.0) - 1.0));
                  }
                  else{
                      return colorf::GREEN + colorf::A_SHF(128);
                  }
              });

              g_glDevice->fillRectangle(
                      drawBGColor,

                      drawDstX + (avatarLeft ? ChatItem::TRIANGLE_WIDTH : 0),
                      drawDstY,

                      std::max<int>(message.w(), ChatItem::MESSAGE_MIN_WIDTH ) + ChatItem::MESSAGE_MARGIN * 2,
                      std::max<int>(message.h(), ChatItem::MESSAGE_MIN_HEIGHT) + ChatItem::MESSAGE_MARGIN * 2,

                      ChatItem::MESSAGE_CORNER);

              const auto triangleX1_avatarLeft = drawDstX;
              const auto triangleX2_avatarLeft = drawDstX + ChatItem::TRIANGLE_WIDTH - 1;
              const auto triangleX3_avatarLeft = drawDstX + ChatItem::TRIANGLE_WIDTH - 1;

              const auto triangleX1_avatarRight = drawDstX + ChatItem::MESSAGE_MARGIN * 2 + std::max<int>(message.w(), ChatItem::MESSAGE_MIN_WIDTH) + ChatItem::TRIANGLE_WIDTH - 1;
              const auto triangleX2_avatarRight = drawDstX + ChatItem::MESSAGE_MARGIN * 2 + std::max<int>(message.w(), ChatItem::MESSAGE_MIN_WIDTH);
              const auto triangleX3_avatarRight = drawDstX + ChatItem::MESSAGE_MARGIN * 2 + std::max<int>(message.w(), ChatItem::MESSAGE_MIN_WIDTH);

              const auto triangleY1_showName = drawDstY + (ChatItem::AVATAR_HEIGHT - ChatItem::NAME_HEIGHT) / 2;
              const auto triangleY2_showName = drawDstY + (ChatItem::AVATAR_HEIGHT - ChatItem::NAME_HEIGHT) / 2 - ChatItem::TRIANGLE_HEIGHT / 2;
              const auto triangleY3_showName = drawDstY + (ChatItem::AVATAR_HEIGHT - ChatItem::NAME_HEIGHT) / 2 + ChatItem::TRIANGLE_HEIGHT / 2;

              const auto triangleY1_hideName = drawDstY + ChatItem::AVATAR_HEIGHT / 2;
              const auto triangleY2_hideName = drawDstY + ChatItem::AVATAR_HEIGHT / 2 - ChatItem::TRIANGLE_HEIGHT / 2;
              const auto triangleY3_hideName = drawDstY + ChatItem::AVATAR_HEIGHT / 2 + ChatItem::TRIANGLE_HEIGHT / 2;

              if(avatarLeft){
                  if(showName) g_glDevice->fillTriangle(drawBGColor, triangleX1_avatarLeft, triangleY1_showName, triangleX2_avatarLeft, triangleY2_showName, triangleX3_avatarLeft, triangleY3_showName);
                  else         g_glDevice->fillTriangle(drawBGColor, triangleX1_avatarLeft, triangleY1_hideName, triangleX2_avatarLeft, triangleY2_hideName, triangleX3_avatarLeft, triangleY3_hideName);
              }
              else{
                  if(showName) g_glDevice->fillTriangle(drawBGColor, triangleX1_avatarRight, triangleY1_showName, triangleX2_avatarRight, triangleY2_showName, triangleX3_avatarRight, triangleY3_showName);
                  else         g_glDevice->fillTriangle(drawBGColor, triangleX1_avatarRight, triangleY1_hideName, triangleX2_avatarRight, triangleY2_hideName, triangleX3_avatarRight, triangleY3_hideName);
              }
          },
      }}

    , msgref(args.msgRefID.has_value() ? new ChatItemRef
      {
          DIR_UPLEFT,
          0,
          0,
          300,

          false,
          false,

          args.msgRefID.value(),
          to_cstr(args.messageRef),

      } : nullptr)
{
    if(avatarLeft){
        addChildAt(&avatar, DIR_UPLEFT, 0, 0, false);
        if(showName){
            addChildAt(&name      , DIR_LEFT  ,                  ChatItem::AVATAR_WIDTH + ChatItem::GAP + ChatItem::TRIANGLE_WIDTH                           , ChatItem::NAME_HEIGHT / 2                       , false);
            addChildAt(&background, DIR_UPLEFT,                  ChatItem::AVATAR_WIDTH + ChatItem::GAP                                                      , ChatItem::NAME_HEIGHT                           , false);
            addChildAt(&message   , DIR_UPLEFT,                  ChatItem::AVATAR_WIDTH + ChatItem::GAP + ChatItem::TRIANGLE_WIDTH + ChatItem::MESSAGE_MARGIN, ChatItem::NAME_HEIGHT + ChatItem::MESSAGE_MARGIN, false);
        }
        else{
            addChildAt(&background, DIR_UPLEFT,                  ChatItem::AVATAR_WIDTH + ChatItem::GAP                                                      , 0                                               , false);
            addChildAt(&message   , DIR_UPLEFT,                  ChatItem::AVATAR_WIDTH + ChatItem::GAP + ChatItem::TRIANGLE_WIDTH + ChatItem::MESSAGE_MARGIN, ChatItem::MESSAGE_MARGIN                        , false);
        }
    }
    else{
        const auto fnRealWidth = [this]()
        {
            return ChatItem::AVATAR_WIDTH + ChatItem::GAP + ChatItem::TRIANGLE_WIDTH + std::max<int>
            ({
                showName ? name.w() : 0,
                std::max<int>(message.w(), ChatItem::MESSAGE_MIN_WIDTH) + ChatItem::MESSAGE_MARGIN * 2,
                msgref ? msgref->w() : 0,
            });
        };

        addChildAt(&avatar, DIR_UPRIGHT, [fnRealWidth](const Widget *){ return fnRealWidth() - 1; }, 0, false);
        if(showName){
            addChildAt(&name      , DIR_RIGHT  , [fnRealWidth](const Widget *){ return fnRealWidth() - 1 - ChatItem::AVATAR_WIDTH - ChatItem::GAP - ChatItem::TRIANGLE_WIDTH                           ; }, ChatItem::NAME_HEIGHT / 2                       , false);
            addChildAt(&background, DIR_UPRIGHT, [fnRealWidth](const Widget *){ return fnRealWidth() - 1 - ChatItem::AVATAR_WIDTH - ChatItem::GAP                                                      ; }, ChatItem::NAME_HEIGHT                           , false);
            addChildAt(&message   , DIR_UPRIGHT, [fnRealWidth](const Widget *){ return fnRealWidth() - 1 - ChatItem::AVATAR_WIDTH - ChatItem::GAP - ChatItem::TRIANGLE_WIDTH - ChatItem::MESSAGE_MARGIN; }, ChatItem::NAME_HEIGHT + ChatItem::MESSAGE_MARGIN, false);
        }
        else{
            addChildAt(&background, DIR_UPRIGHT, [fnRealWidth](const Widget *){ return fnRealWidth() - 1 - ChatItem::AVATAR_WIDTH - ChatItem::GAP                                                      ; }, 0                                               , false);
            addChildAt(&message   , DIR_UPRIGHT, [fnRealWidth](const Widget *){ return fnRealWidth() - 1 - ChatItem::AVATAR_WIDTH - ChatItem::GAP - ChatItem::TRIANGLE_WIDTH - ChatItem::MESSAGE_MARGIN; }, ChatItem::MESSAGE_MARGIN                        , false);
        }
    }

    if(msgref){
        if(avatarLeft) addChildAt(msgref, DIR_UPLEFT , [this](const Widget *){ return message.dx()                   - ChatItem::MESSAGE_MARGIN; }, [this](const Widget *){ return message.dy() + message.h() - 1 + ChatItem::REF_GAP; }, true);
        else           addChildAt(msgref, DIR_UPRIGHT, [this](const Widget *){ return message.dx() + message.w() - 1 + ChatItem::MESSAGE_MARGIN; }, [this](const Widget *){ return message.dy() + message.h() - 1 + ChatItem::REF_GAP; }, true);
    }
}

void ChatItem::setMaxWidth(int argWidth)
{
    message.setLineWidth(argWidth - ChatItem::AVATAR_WIDTH - ChatItem::GAP - ChatItem::TRIANGLE_WIDTH - ChatItem::MESSAGE_MARGIN * 2);
}

void ChatItem::updateDefault(double fUpdateTime)
{
    accuTime += fUpdateTime;
}

bool ChatItem::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    if(!valid){
        return consumeFocus(false);
    }

    if(true
            && event.type == MIR_EVENT_MOUSE_BUTTON_UP
            && event.button.button == MIR_BUTTON_RIGHT
            && m.create(background.roi()).in(to_d(event.button.x), to_d(event.button.y))){

        if(auto chatPage = hasParent<ChatPage>()){
            if(chatPage->menu){
                // null before destroy: any callback during destruction sees nullptr, not a dangling pointer
                const auto oldMenuID = chatPage->menu->id();
                chatPage->menu = nullptr;
                chatPage->removeChild(oldMenuID, true);
            }

            chatPage->addChildAt((chatPage->menu = new MenuBoard
            {{
                .fixed = 200,
                .margin
                {
                    5,
                    5,
                    5,
                    5,
                },

                .corner = 3,
                .itemSpace = 5,
                .separatorSpace = 6,

                .itemList
                {
                    {{new LabelBoard{{.label=u8"引用", .attrs{.data = std::make_any<std::string>("引用")}}}, true}},
                    {{new LabelBoard{{.label=u8"复制", .attrs{.data = std::make_any<std::string>("复制")}}}, true}},
                },

                .onClick = [this](Widget *item) // create new menu board whenever click a new chat item
                {
                    if(const auto op = std::any_cast<std::string>(item->data()); op == "引用"){
                        std::string textStr = message.getText();
                        fflassert(utf8f::valid(textStr));

                        if(const auto size = utf8::distance(textStr.begin(), textStr.end()); size > 50){
                            auto p = textStr.begin();
                            utf8::advance(p, 50, textStr.end());
                            textStr.resize(std::distance(textStr.begin(), p));
                            textStr.append("...");
                        }
                        hasParent<ChatPage>()->enableChatRef(msgID.value(), "<layout>" + xmlf::toParString("%s：%s", name.getText().c_str(), textStr.c_str()) + "</layout>");
                    }
                },
            }}),

            DIR_UPLEFT,
            to_d(event.button.x) - (m.x - m.ro->x),
            to_d(event.button.y) - (m.y - m.ro->y),
            true);

            chatPage->menu->setShow(true);
            chatPage->menu->setFocus(true);
        }

        setFocus(false);
        return true;
    }

    if(Widget::processEventDefault(event, valid, m)){
        if(!focus()){
            setFocus(true);
        }

        if(auto chatPage = hasParent<ChatPage>()){
            if(chatPage->menu){
                // null before destroy: any callback during destruction sees nullptr, not a dangling pointer
                const auto oldMenuID = chatPage->menu->id();
                chatPage->menu = nullptr;
                chatPage->removeChild(oldMenuID, true);
            }
        }
        return true;
    }
    return false;
}

// ===== merged from friendchatboard/chatinputcontainer.cpp =====
#include "client.hpp"
#include "pngtexdb.hpp"
#include "gldevice.hpp"
#include "processrun.hpp"

extern Client *g_client;
extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

ChatInputContainer::ChatInputContainer(
        Widget::VarDir  argDir,
        Widget::VarInt  argX,
        Widget::VarInt  argY,
        Widget::VarSizeOpt argW,

        Widget *argParent,
        bool    argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),
          .w = std::move(argW),
          .h = [this]()
          {
              return mathf::bound<int>(layout.h(), ChatPage::INPUT_MIN_HEIGHT, ChatPage::INPUT_MAX_HEIGHT);
          },

          .attrs
          {
              .inst
              {
                  .afterResize = [this](Widget *)
                  {
                      layout.setLineWidth(this->w());
                  },
              }
          },

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , layout
      {{
          .lineWidth = this->w(),

          .canEdit = true,
          .enableIME = [this]
          {
              return FriendChatBoard::getParentBoard(this)->m_processRun->getRuntimeConfig<RTCFG_IME>();
          },

          .font
          {
              .id = 1,
              .size = 12,
          },

          .lineAlign = LALIGN_JUSTIFY,

          .onCR = [this](bool shiftHold)
          {
              if(shiftHold){
                  return false;
              }

              if(!layout.hasToken()){
                  return true;
              }

              auto message = layout.getXML();
              layout.clear();

              auto chatBoard = FriendChatBoard::getParentBoard(this);
              auto chatPage  = dynamic_cast<ChatPage *>(chatBoard->m_uiPageList[UIPage_CHAT].page);

              const SDChatMessage chatMessage
              {
                  .refer = chatPage->refopt(),
                  .from  = chatBoard->m_processRun->getMyHero()->cpid(),
                  .to    = chatPage->peer.cpid(),
                  .message = cerealf::serialize(message),
              };

              chatPage->disableChatRef();
              chatPage->chat.append(chatMessage, [chatMessage, this](const ChatItem *chatItem)
              {
                  CMChatMessageHeader cmCMH;
                  std::memset(&cmCMH, 0, sizeof(cmCMH));

                  cmCMH.toCPID = chatMessage.to.asU64();
                  cmCMH.hasRef = to_boolint(chatMessage.refer.has_value());
                  cmCMH.refID  = chatMessage.refer.value_or(0);

                  std::string msgbuf;
                  msgbuf = as_sv(cmCMH);
                  msgbuf.append(chatMessage.message.begin(), chatMessage.message.end());

                  const auto widgetID = chatItem->id();
                  FriendChatBoard::getParentBoard(this)->addMessagePending(widgetID, chatMessage);
                  g_client->send({CM_CHATMESSAGE, msgbuf}, [widgetID, this](uint8_t headCode, const uint8_t *buf, size_t bufSize)
                  {
                      switch(headCode){
                          case SM_OK:
                              {
                                  const auto sdCMDBS = cerealf::deserialize<SDChatMessageDBSeq>(buf, bufSize);
                                  FriendChatBoard::getParentBoard(this)->finishMessagePending(widgetID, sdCMDBS);
                                  break;
                              }
                          default:
                              {
                                  throw fflpanic("failed to send message");
                              }
                      }
                  });
              });

              return true;
          },

          .parent{this},
      }}
{
    // there is mutual dependency
    // height of input container depends on height of layout
    //
    // layout always attach to buttom of input container, so argX needs container height
    // in initialization list we can not call this->h() since initialization of layout is not done yet
    layout.moveAt(DIR_DOWNLEFT, 0, [this](const Widget *){ return this->h() - 1; });
}

// ===== merged from friendchatboard/chatitemcontainer.cpp =====
#include "hero.hpp"
#include "pngtexdb.hpp"
#include "gldevice.hpp"
#include "processrun.hpp"
#include "margincontainer.hpp"

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

ChatItemContainer::ChatItemContainer(
        Widget::VarDir  argDir,
        Widget::VarInt  argX,
        Widget::VarInt  argY,
        Widget::VarSizeOpt argW,
        Widget::VarSizeOpt argH,

        Widget *argParent,
        bool    argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),
          .w = std::move(argW),
          .h = std::move(argH),

          .attrs
          {
              .inst
              {
                  .afterResize = [this](Widget *)
                  {
                      canvas.foreachItem([this](Widget *chatItemBox, bool)
                      {
                          auto chatItemWidget = dynamic_cast<MarginContainer *>(chatItemBox)->contained();
                          auto chatItem       = dynamic_cast<ChatItem *>(chatItemWidget);

                          // can be nomsgBox or opsBox

                          if(chatItem){
                              chatItem->setMaxWidth(chatItemMaxWidth());
                          }
                      });

                      Widget::afterResizeDefault();
                  },
              },

          },

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , canvas
      {{
          .y = [this](const Widget *self)
          {
              if(self->h() < this->h()){
                  return 0;
              }

              return -1 * to_dround((self->h() - this->h()) * FriendChatBoard::getParentBoard(this)->m_uiPageList[UIPage_CHAT].slider->getValue());
          },

          .fixed = [this]{ return w(); },
          .itemSpace = ChatItemContainer::ITEM_SPACE,

          .parent{this},
      }}

    , nomsg
      {{
          .label = u8"没有任何聊天记录，现在就开始聊天吧！",
          .font
          {
              .color = colorf::GREY_A255,
          },
      }}

    , ops
      {{
          .lineWidth = 300,
          .initXML = "<layout><par>...</par></layout>",

          .onClickText = [this](const std::unordered_map<std::string, std::string> &attrList, int event)
          {
              if(event == BEVENT_RELEASE){
                  if(const auto id = LayoutBoard::findAttrValue(attrList, "id", nullptr)){
                      if(to_sv(id) == "添加"){
                          FriendChatBoard::getParentBoard(this)->requestAddFriend(getChatPeer(), false);
                      }
                      else if(to_sv(id) == "屏蔽"){
                      }
                  }
              }
          },
      }}

    , nomsgBox
      {{
          .w = [this]{ return canvas.w(); },
          .h = [this]{ return nomsg .h() + 2 * ChatItemContainer::BACKGROUND_MARGIN; },

          .contained
          {
              .widget = std::addressof(nomsg),
          },

          .bgDrawFunc = [this](int startDstX, int startDstY)
          {
              const auto roi = nomsg.roi(this);
              g_glDevice->fillRectangle(colorf::RGB(231, 231, 189) + colorf::A_SHF(64),
                      startDstX + roi.x - ChatItemContainer::BACKGROUND_MARGIN,
                      startDstY + roi.y - ChatItemContainer::BACKGROUND_MARGIN,
                      roi.w + ChatItemContainer::BACKGROUND_MARGIN * 2,
                      roi.h + ChatItemContainer::BACKGROUND_MARGIN * 2, ChatItemContainer::BACKGROUND_CORNER);
          },
      }}

    , opsBox
      {{
          .w = [this]{ return canvas.w(); },
          .h = [this]{ return ops.h() + 2 * ChatItemContainer::BACKGROUND_MARGIN; },

          .contained
          {
              .widget = std::addressof(ops),
          },

          .bgDrawFunc = [this](const Widget *, int startDstX, int startDstY)
          {
              g_glDevice->fillRectangle(colorf::RGB(231, 231, 189) + colorf::A_SHF(64),
                      startDstX - ChatItemContainer::BACKGROUND_MARGIN,
                      startDstY - ChatItemContainer::BACKGROUND_MARGIN,
                      ops.w() + ChatItemContainer::BACKGROUND_MARGIN * 2,
                      ops.h() + ChatItemContainer::BACKGROUND_MARGIN * 2, ChatItemContainer::BACKGROUND_CORNER);
          },
      }}
{
    canvas.addItem(&nomsgBox, false);
}

int ChatItemContainer::chatItemMaxWidth() const
{
    return canvas.w() - ChatItem::TRIANGLE_WIDTH - ChatItem::GAP - ChatItem::AVATAR_WIDTH;
}

const SDChatPeer &ChatItemContainer::getChatPeer() const
{
    return hasParent<ChatPage>()->peer;
}

void ChatItemContainer::clearChatItem(bool keepNomsg)
{
    canvas.clearItem([keepNomsg, this](const Widget *item, bool)
    {
        if(keepNomsg){
            return item != &nomsgBox;
        }
        return true;
    });
}

void ChatItemContainer::append(const SDChatMessage &sdCM, std::function<void(const ChatItem *)> fnOp)
{
    auto chatItem = new ChatItem
    {{
        .maxWidth =  chatItemMaxWidth(), // cannot auto-stretch
        .pending  = !sdCM.seq.has_value(),

        .msgID    = sdCM.seq.has_value() ? std::make_optional(sdCM.seq.value().id) : std::nullopt,
        .msgRefID = sdCM.refer,

        .name = u8"...",
        .message = to_u8rawstr(cerealf::deserialize<std::string>(sdCM.message)).c_str(),
        .messageRef = sdCM.refer.has_value() ? u8"<layout><par>...</par></layout>" : nullptr,

        .texLoadFunc = []{ return g_progUseDB->retrieve(0X010007CF); },

        .showName   = sdCM.from != FriendChatBoard::getParentBoard(this)->m_processRun->getMyHeroChatPeer().cpid(),
        .avatarLeft = sdCM.from != FriendChatBoard::getParentBoard(this)->m_processRun->getMyHeroChatPeer().cpid(),
    }};

    auto chatItemBox = new MarginContainer
    {{
        .w = [this    ]{ return    canvas.w(); },
        .h = [chatItem]{ return chatItem->h(); },

        .contained
        {
            .dir = [chatItem](const Widget *)
            {
                return chatItem->avatarLeft ? DIR_LEFT : DIR_RIGHT;
            },

            .widget = chatItem,
            .autoDelete = true,
        },
    }};

    canvas.removeItem(nomsgBox.id(), false);
    canvas.removeItem(  opsBox.id(), false);

    canvas.addItem(chatItemBox, true);

    if(sdCM.from.group()){
        ops.loadXML(R"###(<layout><par>信息来源于群消息，请保护隐私。</par></layout>)###");
    }
    else if(sdCM.from.player()){
        ops.loadXML(R"###(<layout><par>对方不是你的好友，你可以<event id="添加">添加</event>对方为好友，或者<event id="屏蔽">屏蔽</event>对方的消息。</par></layout>)###");
    }

    hasParent<FriendChatBoard>()->queryChatPeer(sdCM.from, [widgetID = chatItem->id(), sdCM, fnOp = std::move(fnOp), this](const SDChatPeer *peer, bool)
    {
        // peer may be null when the server can't resolve the sender (deleted account, etc.); fall back to a placeholder instead of asserting
        if(auto chatItem = dynamic_cast<ChatItem *>(canvas.hasDescendant(widgetID))){
            const auto from = sdCM.from;
            const auto job    = (peer && peer->player()) ? peer->player()->job    : 0;
            const auto gender = (peer && peer->player()) ? peer->player()->gender : false;

            chatItem->name.setText(u8"%s", peer ? peer->name.c_str() : "[未知]");
            chatItem->avatar.setLoadFunc([from, job, gender](const Widget *)
            {
                if     (from == SDChatPeerID(CPR_SPECIAL, SYS_CHATDBID_SYSTEM)) return g_progUseDB->retrieve(0X00001100);
                else if(from == SDChatPeerID(CPR_SPECIAL, SYS_CHATDBID_GROUP )) return g_progUseDB->retrieve(0X00001300);
                else                                                           return g_progUseDB->retrieve(Hero::faceGfxID(gender, job));
            });

            if(fnOp){
                fnOp(chatItem);
            }
        }
        else{
            if(fnOp){
                fnOp(nullptr);
            }
        }
    });

    if(sdCM.refer.has_value()){
        hasParent<FriendChatBoard>()->queryChatMessage(sdCM.refer.value(), [widgetID = chatItem->id(), this](const SDChatMessage *refMsg, bool)
        {
            if(auto chatItem = dynamic_cast<ChatItem *>(canvas.hasDescendant(widgetID))){
                if(!refMsg){
                    chatItem->msgref->loadXML(R"###(<layout><par><t color="RED">引用的信息不存在或者已被删除</t></par></layout>)###");
                    return;
                }

                hasParent<FriendChatBoard>()->queryChatPeer(refMsg->from, [compMsg = refMsg->message, widgetID, this](const SDChatPeer *peer, bool)
                {
                    if(auto chatItem = dynamic_cast<ChatItem *>(hasDescendant(widgetID))){
                        const auto xmlStr = cerealf::deserialize<std::string>(compMsg);
                        tinyxml2::XMLDocument xmlDoc(true, tinyxml2::PEDANTIC_WHITESPACE);

                        if(xmlDoc.Parse(xmlStr.c_str()) != tinyxml2::XML_SUCCESS){
                            throw fflpanic("tinyxml2::XMLDocument::Parse() failed: {}", xmlStr);
                        }

                        fflassert(xmlf::checkNodeName(xmlDoc.FirstChild(), "layout"));
                        fflassert(xmlf::checkNodeName(xmlDoc.FirstChild()->FirstChild(), "par"));

                        auto nameText = str_printf("%s：", peer ? peer->name.c_str() : "[未知]");
                        auto nameTextNode = xmlDoc.NewText(nameText.c_str());

                        xmlDoc.FirstChild()->FirstChild()->InsertFirstChild(nameTextNode);
                        chatItem->msgref->loadXML(xmlf::toString(&xmlDoc));
                    }
                });
            }
        });
    }

    const bool needOps = [this]
    {
        if(getChatPeer().special()){
            return false;
        }

        if(getChatPeer().group()){
            return false;
        }

        if(getChatPeer().player() && getChatPeer().id == FriendChatBoard::getParentBoard(this)->m_processRun->getMyHeroDBID()){
            return false;
        }

        return !FriendChatBoard::getParentBoard(this)->findFriendChatPeer(getChatPeer().cpid());
    }();

    if(needOps){
        canvas.addItem(&opsBox, false);
    }
}

// ===== merged from friendchatboard/chatpage.cpp =====
#include "gldevice.hpp"

extern GLDevice *g_glDevice;

ChatPage::ChatPage(
        Widget::VarDir  argDir,
        Widget::VarInt  argX,
        Widget::VarInt  argY,
        Widget::VarSizeOpt argW,
        Widget::VarSizeOpt argH,

        Widget *argParent,
        bool argAutoDelete)

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

    , background
      {{
          .w = [this](const Widget *){ return w(); },
          .h = [this](const Widget *){ return h(); },

          .drawFunc = [this](const Widget *, int drawDstX, int drawDstY)
          {
              // ChatPage = top + sepLine + bottom
              const int bottomHeight = UIPage_MARGIN + ChatPage::SEP_MARGIN + ChatPage::INPUT_MARGIN * 2 + input.h() + (showref() ? (chatref->h() + ChatPage::CHATREF_GAP) : 0);
              const int sepLineDY    = h() - bottomHeight - 1;

              g_glDevice->drawLine(
                      colorf::RGBA(231, 231, 189, 64),

                      drawDstX,
                      drawDstY + sepLineDY,

                      drawDstX + w(),
                      drawDstY + sepLineDY);

              g_glDevice->fillRectangle(
                      colorf::RGBA(231, 231, 189, 32),

                      drawDstX,
                      drawDstY + sepLineDY + 1,

                      w(),
                      bottomHeight);

              g_glDevice->fillRectangle(
                      colorf::BLACK + colorf::A_SHF(255),

                      drawDstX + UIPage_MARGIN,
                      drawDstY + sepLineDY + ChatPage::SEP_MARGIN,

                      w() - UIPage_MARGIN * 2,
                      ChatPage::INPUT_MARGIN * 2 + input.h(),

                      ChatPage::INPUT_CORNER);

              g_glDevice->drawRectangle(
                      colorf::RGBA(231, 231, 189, 96),

                      drawDstX + UIPage_MARGIN,
                      drawDstY + sepLineDY + ChatPage::SEP_MARGIN,

                      w() - UIPage_MARGIN * 2,
                      ChatPage::INPUT_MARGIN * 2 + input.h(),

                      ChatPage::INPUT_CORNER);
          },

          .parent{this},
      }}

    , input
      {
          DIR_DOWNLEFT,
          UIPage_MARGIN + ChatPage::INPUT_MARGIN,
          [this](const Widget *)
          {
              return h() - UIPage_MARGIN - (showref() ? (chatref->h() + ChatPage::CHATREF_GAP) : 0) - ChatPage::INPUT_MARGIN - 1;
          },

          [this](const Widget *)
          {
              return w() - UIPage_MARGIN * 2 - ChatPage::INPUT_MARGIN * 2;
          },

          this,
          false,
      }

    , chat
      {
          DIR_UPLEFT,
          UIPage_MARGIN,
          UIPage_MARGIN,

          [this](const Widget *)
          {
              return w() - UIPage_MARGIN * 2;
          },

          [this](const Widget *)
          {
              return h() - UIPage_MARGIN * 2 - ChatPage::SEP_MARGIN * 2 - 1 - ChatPage::INPUT_MARGIN * 2 - input.h() - (showref() ? (chatref->h() + ChatPage::CHATREF_GAP) : 0);
          },

          this,
          false,
      }
{}

bool ChatPage::showref() const
{
    return chatref && chatref->show();
}

bool ChatPage::showmenu() const
{
    return menu && menu->show();
}

std::optional<uint64_t> ChatPage::refopt() const
{
    if(showref()){
        return chatref->refer();
    }
    return std::nullopt;
}

void ChatPage::enableChatRef(uint64_t refMsgID, std::string xmlStr)
{
    if(chatref){
        // null before destroy: any callback during destruction sees nullptr, not a dangling pointer
        const auto oldID = chatref->id();
        chatref = nullptr;
        removeChild(oldID, true);
    }
    chatref = ChatPage::createChatItemRef(refMsgID, std::move(xmlStr), this, true);
}

void ChatPage::disableChatRef()
{
    if(chatref){
        const auto oldID = chatref->id();
        chatref = nullptr;
        removeChild(oldID, true);
    }
}

void ChatPage::afterResizeDefault()
{
    chat .afterResize();
    input.afterResize();

    if(!showref()){
        return;
    }

    enableChatRef(chatref->refer(), chatref->getXML());
}

bool ChatPage::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    if(!valid){
        return consumeFocus(false);
    }

    if(showref()){
        if(chatref->processEventParent(event, valid, m)){
            return true;
        }
    }

    if(showmenu()){
        if(menu->processEvent(event, valid, m)){
            return true;
        }
    }

    switch(event.type){
        case MIR_EVENT_KEY_DOWN:
            {
                switch(event.key.key){
                    case MIRK_RETURN:
                        {
                            if(input.focus()){
                                return Widget::processEventDefault(event, valid, m);
                            }
                            else{
                                setFocus(false);
                                return input.consumeFocus(true, std::addressof(input.layout));
                            }
                        }
                    default:
                        {
                            return Widget::processEventDefault(event, valid, m);
                        }
                }
            }
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
            {
                if(m.create(input.roi()).in(to_d(event.button.x), to_d(event.button.y))){
                    setFocus(false);
                    return input.consumeFocus(true, std::addressof(input.layout));
                }

                if(chat.processEventParent(event, true, m)){
                    return true;
                }

                if(m.in(to_d(event.button.x), to_d(event.button.y))){
                    if(menu){
                        removeChild(menu->id(), true);
                        menu = nullptr;
                    }
                    return consumeFocus(true);
                }

                return false;
            }
        default:
            {
                return Widget::processEventDefault(event, valid, m);
            }
    }
}

ChatItemRef *ChatPage::createChatItemRef(uint64_t msgID, std::string xmlStr, Widget *self, bool autoDelete)
{
    return new ChatItemRef
    {
        DIR_DOWNLEFT,
        UIPage_MARGIN,
        [self](const Widget *){ return self->h() - UIPage_MARGIN - 1; },

        self->w() - 24, // can not stretch
        true,
        true,

        msgID,
        xmlStr.c_str(),

        self,
        autoDelete,
    };
}

// ===== merged from friendchatboard/chatpreviewitem.cpp =====
#include "hero.hpp"
#include "pngtexdb.hpp"
#include "gldevice.hpp"
#include "processrun.hpp"

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

ChatPreviewItem::ChatPreviewItem(
        Widget::VarDir  argDir,
        Widget::VarInt  argX,
        Widget::VarInt  argY,
        Widget::VarSizeOpt argW,

        const SDChatPeerID &argCPID,
        const char8_t *argChatXMLStr,

        Widget *argParent,
        bool   argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),
          .w = std::move(argW),
          .h = ChatPreviewItem::HEIGHT,

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , cpid(argCPID)

    , avatar
      {{
          .x = ChatPreviewItem::ITEM_MARGIN,
          .y = ChatPreviewItem::ITEM_MARGIN,

          .w = ChatPreviewItem::AVATAR_WIDTH,
          .h = ChatPreviewItem::HEIGHT - ChatPreviewItem::ITEM_MARGIN * 2,

          .texLoadFunc = [this](const Widget *) -> GLTexID
          {
              return g_progUseDB->retrieve(0X010007CF);
          },

          .blendMode = MIR_BLENDMODE_NONE,
          .parent{this},
      }}

    , name
      {{
          .dir = DIR_LEFT,
          .x = ChatPreviewItem::ITEM_MARGIN + ChatPreviewItem::AVATAR_WIDTH + ChatPreviewItem::GAP,
          .y = ChatPreviewItem::ITEM_MARGIN + ChatPreviewItem::NAME_HEIGHT / 2,

          .label = u8"未知用户",
          .font
          {
              .id = 1,
              .size = 14,
          },

          .parent{this},
      }}

    , message
      {{
          .initXML = to_cstr(argChatXMLStr),
          .parLimit = 1,

          .font
          {
              .id = 1,
              .size = 12,
              .color = colorf::GREY_A255,
          },
      }}

    , messageClip
      {{
          .x = ChatPreviewItem::ITEM_MARGIN + ChatPreviewItem::AVATAR_WIDTH + ChatPreviewItem::GAP,
          .y = ChatPreviewItem::ITEM_MARGIN + ChatPreviewItem::NAME_HEIGHT,

          .w = [this](const Widget *)
          {
              return w() - ChatPreviewItem::ITEM_MARGIN * 2 - ChatPreviewItem::AVATAR_WIDTH - ChatPreviewItem::GAP;
          },

          .h = ChatPreviewItem::HEIGHT - ChatPreviewItem::ITEM_MARGIN * 2 - ChatPreviewItem::NAME_HEIGHT,

          .childList
          {
              {&message, DIR_UPLEFT, 0, 0, false},
          },

          .parent
          {
              .widget = this,
          }
      }}

    , selected
      {{
          .w = [this]{ return w(); },
          .h = [this]{ return h(); },

          .drawFunc = [this](int drawDstX, int drawDstY)
          {
              if(Widget::ROIMap{.x=drawDstX, .y=drawDstY, .ro{roi()}}.in(GLDeviceHelper::getMousePLoc())){
                  g_glDevice->fillRectangle(colorf::RGB(231, 231, 189) + colorf::A_SHF(64), drawDstX, drawDstY, w(), h());
                  g_glDevice->drawRectangle(colorf::RGB(231, 231, 189) + colorf::A_SHF(64), drawDstX, drawDstY, w(), h());
              }
              else{
                  g_glDevice->drawRectangle(colorf::RGB(231, 231, 189) + colorf::A_SHF(32), drawDstX, drawDstY, w(), h());
              }
          },

          .parent{this},
      }}
{
    FriendChatBoard::getParentBoard(this)->queryChatPeer(this->cpid, [canvas = parent(), widgetID = id(), this](const SDChatPeer *peer, bool)
    {
        if(!canvas->hasChild(widgetID)){
            return;
        }

        if(!peer){
            return;
        }

        this->name.setText(u8"%s", peer->name.c_str());
        this->avatar.setLoadFunc([dbid = peer->id, group = peer->group(), gender = peer->player() ? peer->player()->gender : false, job = peer->player() ? peer->player()->job : 0](const Widget *)
        {
            if     (group                      ) return g_progUseDB->retrieve(0X00001300);
            else if(dbid == SYS_CHATDBID_SYSTEM) return g_progUseDB->retrieve(0X00001100);
            else                                 return g_progUseDB->retrieve(Hero::faceGfxID(gender, job));
        });
    });
}

bool ChatPreviewItem::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    if(!valid){
        return consumeFocus(false);
    }

    switch(event.type){
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
            {
                if(m.in(to_d(event.button.x), to_d(event.button.y))){
                    FriendChatBoard::getParentBoard(this)->m_processRun->requestLatestChatMessage({this->cpid.asU64()}, 50, true, true);
                    FriendChatBoard::getParentBoard(this)->queryChatPeer(this->cpid, [canvas = this->parent(), widgetID = this->id(), this](const SDChatPeer *peer, bool)
                    {
                        if(!peer){
                            return;
                        }

                        if(!canvas->hasChild(widgetID)){
                            return;
                        }

                        auto boardPtr = FriendChatBoard::getParentBoard(this);

                        boardPtr->setChatPeer(*peer, true);
                        boardPtr->setUIPage(UIPage_CHAT);
                    });
                    return consumeFocus(true);
                }
                return false;
            }
        default:
            {
                return false;
            }
    }
}

// ===== merged from friendchatboard/chatpreviewpage.cpp =====

ChatPreviewPage::ChatPreviewPage(
        Widget::VarDir argDir,
        Widget::VarInt argX,
        Widget::VarInt argY,

        Widget::VarSizeOpt argW,
        Widget::VarSizeOpt argH,

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

    , canvas
      {{
          .w = [this]{ return w(); },
          .h = {},

          .parent
          {
              .widget = this,
          }
      }}
{}

void ChatPreviewPage::updateChatPreview(const SDChatPeerID &sdCPID, const std::string &argMsg)
{
    ChatPreviewItem *child = dynamic_cast<ChatPreviewItem *>(canvas.hasChild([sdCPID](const Widget *widgetPtr, bool)
    {
        if(auto preview = dynamic_cast<const ChatPreviewItem *>(widgetPtr); preview && preview->cpid == sdCPID){
            return true;
        }
        return false;
    }));

    if(child){
        child->message.loadXML(argMsg.c_str());
    }
    else{
        child = new ChatPreviewItem
        {
            DIR_UPLEFT,
            0,
            0,
            [this](const Widget *){ return w(); },

            sdCPID,
            to_u8rawstr(argMsg).c_str(),

            &canvas, // image load func uses getParentBoard(this)
            true,
        };
    }

    canvas.moveFront(child);

    int startY = 0;
    canvas.foreachChild([&startY](Widget *widget, bool)
    {
        widget->moveAt(DIR_UPLEFT, 0, startY);
        startY += widget->h();
    });
}

// ===== merged from friendchatboard/frienditem.cpp =====
#include "gldevice.hpp"

extern GLDevice *g_glDevice;

FriendItem::FriendItem(
        Widget::VarDir  argDir,
        Widget::VarInt  argX,
        Widget::VarInt  argY,
        Widget::VarSizeOpt argW,

        const SDChatPeerID &argCPID,

        const char8_t *argNameStr,
        std::function<GLTexID (const Widget *)> argLoadImageFunc,

        std::function<void(FriendItem *)> argOnClick,
        std::pair<Widget *, bool> argFuncWidget,

        Widget *argParent,
        bool argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),
          .w = std::move(argW),
          .h = FriendItem::HEIGHT,

          .childList
          {
              {
                  argFuncWidget.first,
                  DIR_RIGHT,
                  UIPage_MIN_WIDTH - UIPage_MARGIN * 2 - FriendItem::FUNC_MARGIN - 1,
                  FriendItem::HEIGHT / 2,
                  argFuncWidget.second,
              },
          },

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , cpid(argCPID)
    , funcWidgetID(argFuncWidget.first ? argFuncWidget.first->id() : 0)
    , onClick(std::move(argOnClick))

    , hovered
      {{
          .w = [this]{ return w(); },
          .h = [this]{ return h(); },

          .drawFunc = [this](const Widget *self, int drawDstX, int drawDstY)
          {
              if(Widget::ROIMap{.x=drawDstX, .y=drawDstY, .ro{self->roi()}}.in(GLDeviceHelper::getMousePLoc())){
                  g_glDevice->fillRectangle(colorf::RGB(231, 231, 189) + colorf::A_SHF(64), drawDstX, drawDstY, w(), h());
                  g_glDevice->drawRectangle(colorf::RGB(231, 231, 189) + colorf::A_SHF(64), drawDstX, drawDstY, w(), h());
              }
              else{
                  g_glDevice->drawRectangle(colorf::RGB(231, 231, 189) + colorf::A_SHF(32), drawDstX, drawDstY, w(), h());
              }
          },

          .parent{this},
      }}

    , avatar
      {{
          .x = FriendItem::ITEM_MARGIN,
          .y = FriendItem::ITEM_MARGIN,

          .w = FriendItem::AVATAR_WIDTH,
          .h = FriendItem::HEIGHT - FriendItem::ITEM_MARGIN * 2,

          .texLoadFunc = std::move(argLoadImageFunc),

          .blendMode = MIR_BLENDMODE_NONE,
          .parent{this},
      }}

    , name
      {{
          .dir = DIR_LEFT,
          .x = FriendItem::ITEM_MARGIN + FriendItem::AVATAR_WIDTH + FriendItem::GAP,
          .y = FriendItem::HEIGHT / 2,

          .label = argNameStr,
          .font
          {
              .id = 1,
              .size = 14,
              .color = colorf::WHITE_A255,
          },

          .parent{this},
      }}
{}

void FriendItem::setFuncWidget(Widget *argFuncWidget, bool argAutoDelete)
{
    clearChild([this](const Widget *widget, bool)
    {
        return this->funcWidgetID == widget->id();
    });

    addChild(argFuncWidget, argAutoDelete);
}

bool FriendItem::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    if(!valid){
        return consumeFocus(false);
    }

    switch(event.type){
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
            {
                if(Widget::processEventDefault(event, valid, m)){
                    return consumeFocus(true);
                }
                else if(m.in(to_d(event.button.x), to_d(event.button.y))){
                    if(onClick){
                        onClick(this);
                    }
                    return consumeFocus(true);
                }
                else{
                    return false;
                }
            }
        default:
            {
                return Widget::processEventDefault(event, valid, m);
            }
    }
}

// ===== merged from friendchatboard/friendlistpage.cpp =====
#include "hero.hpp"
#include "pngtexdb.hpp"

extern PNGTexDB *g_progUseDB;

FriendListPage::FriendListPage(Widget::VarDir argDir,

        Widget::VarInt argX,
        Widget::VarInt argY,

        Widget::VarSizeOpt argW,
        Widget::VarSizeOpt argH,

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

    , canvas
      {{
          .w = [this](const Widget *){ return w(); },
          .h = {},

          .parent
          {
              .widget = this,
          }
      }}
{}

void FriendListPage::append(const SDChatPeer &peer, std::function<void(FriendItem *)> argOnClick, std::pair<Widget *, bool> argFuncWidget)
{
    canvas.addChildAt(new FriendItem
    {
        DIR_UPLEFT,
        0,
        0,
        [this](const Widget *){ return w(); }, // use FriendListPage::w()

        SDChatPeerID(CPR_PLAYER, peer.id),
        to_u8rawstr(peer.name).c_str(),

        [peer](const Widget *)
        {
            if     (peer.group()                  ) return g_progUseDB->retrieve(0X00001300);
            else if(peer.id == SYS_CHATDBID_SYSTEM) return g_progUseDB->retrieve(0X00001100);
            else if(peer.player()                 ) return g_progUseDB->retrieve(Hero::faceGfxID(peer.player()->gender, peer.player()->job));
            else                                    return g_progUseDB->retrieve(0X00001100); // unexpected: not group/system/player -- fall back to system icon
        },

        std::move(argOnClick),
        std::move(argFuncWidget),
    },

    DIR_UPLEFT, 0, canvas.h(), true);
}

// ===== merged from friendchatboard/pagecontrol.cpp =====

PageControl::PageControl(
        Widget::VarDir argDir,
        Widget::VarInt argX,
        Widget::VarInt argY,

        int argSpace,

        std::initializer_list<std::pair<Widget *, bool>> argChildList,

        Widget *argParent,
        bool    argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),
          .w = {},
          .h = {},

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}
{
    int maxH = 0;
    for(auto &[widgetPtr, autoDelete]: argChildList){
        maxH = std::max<int>(maxH, widgetPtr->h());
    }

    int offX = 0;
    for(auto &[widgetPtr, autoDelete]: argChildList){
        addChildAt(widgetPtr, DIR_UPLEFT, offX, (maxH - widgetPtr->h()) / 2, autoDelete);
        offX += widgetPtr->w();
        offX += argSpace;
    }
}

// ===== merged from friendchatboard/searchinputline.cpp =====
#include "clientmsg.hpp"
#include "client.hpp"
#include "pngtexdb.hpp"

extern Client *g_client;
extern PNGTexDB *g_progUseDB;

SearchInputLine::SearchInputLine(Widget::VarDir argDir,

        Widget::VarInt argX,
        Widget::VarInt argY,

        Widget *argParent,
        bool    argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),

          .w = SearchInputLine::WIDTH,
          .h = SearchInputLine::HEIGHT,

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , image
      {{
          .texLoadFunc = [](const Widget *){ return g_progUseDB->retrieve(0X00000460); },
      }}

    , inputbg
      {{
          .getter = &image,
          .vr
          {
              3,
              3,
              image.w() - 6,
              2,
          },

          .resize
          {
              [this]{ return w() - 6; },
              [this]{ return h() - (image.h() - 2); },
          },

          .parent{this},
      }}

    , icon
      {{
          .dir = DIR_NONE,
          .x = SearchInputLine::ICON_WIDTH / 2 + SearchInputLine::ICON_MARGIN + 3,
          .y = SearchInputLine::HEIGHT     / 2,

          .w = std::min<int>(SearchInputLine::ICON_WIDTH, SearchInputLine::HEIGHT - 3 * 2),
          .h = std::min<int>(SearchInputLine::ICON_WIDTH, SearchInputLine::HEIGHT - 3 * 2),

          .texLoadFunc = [](const Widget *) { return g_progUseDB->retrieve(0X00001200); },

          .blendMode = MIR_BLENDMODE_NONE,
          .parent{this},
      }}

    , input
      {{
          .x = 3 + SearchInputLine::ICON_MARGIN + SearchInputLine::ICON_WIDTH + SearchInputLine::GAP,
          .y = 3,

          .w = SearchInputLine::WIDTH  - 3 * 2 - SearchInputLine::ICON_MARGIN - SearchInputLine::ICON_WIDTH - SearchInputLine::GAP,
          .h = SearchInputLine::HEIGHT - 3 * 2,

          .enableIME = IME_DISABLE,
          .font
          {
              .id = 1,
              .size = 14,
          },

          .onCR = [this]
          {
              hasParent<SearchPage>()->candidates.setShow(true);
              hasParent<SearchPage>()->autocompletes.setShow(false);
          },

          .onChange = [this](std::string query)
          {
              hint.setShow(query.empty());

              if(query.empty()){
                  hasParent<SearchPage>()->candidates.clearChild();
                  hasParent<SearchPage>()->autocompletes.clearChild();

                  hasParent<SearchPage>()->candidates.setShow(false);
                  hasParent<SearchPage>()->autocompletes.setShow(true);
              }
              else{
                  CMQueryChatPeerList cmQPC;
                  std::memset(&cmQPC, 0, sizeof(cmQPC));

                  cmQPC.input.assign(query);
                  g_client->send({CM_QUERYCHATPEERLIST, cmQPC}, [query = std::move(query), this](uint8_t headCode, const uint8_t *data, size_t size)
                  {
                      switch(headCode){
                          case SM_OK:
                            {
                                hasParent<SearchPage>()->candidates.clearChild();
                                hasParent<SearchPage>()->autocompletes.clearChild();

                                for(const auto &candidate: cerealf::deserialize<SDChatPeerList>(data, size)){
                                    hasParent<SearchPage>()->appendFriendItem(candidate);
                                    hasParent<SearchPage>()->appendAutoCompletionItem(query == std::to_string(candidate.id), candidate, [&candidate, &query]
                                    {
                                        if(const auto pos = candidate.name.find(query); pos != std::string::npos){
                                            return str_printf(R"###(<par>%s<t color="red">%s</t>%s（%llu）</par>)###", candidate.name.substr(0, pos).c_str(), query.c_str(), candidate.name.substr(pos + query.size()).c_str(), to_llu(candidate.id));
                                        }
                                        else if(std::to_string(candidate.id) == query){
                                            return str_printf(R"###(<par>%s（<t color="red">%llu</t>）</par>)###", candidate.name.c_str(), to_llu(candidate.id));
                                        }
                                        else{
                                            return str_printf(R"###(<par>%s（%llu）</par>)###", candidate.name.c_str(), to_llu(candidate.id));
                                        }
                                    }());
                                }
                                break;
                            }
                        default:
                            {
                                throw fflpanic("query failed in server");
                            }
                      }
                  });
              }
          },

          .parent{this},
      }}

    , hint
      {{
          .x = this->input.dx(),
          .y = this->input.dy(),

          .label = u8"输入用户ID或角色名",
          .font
          {
              .id = 1,
              .size = 14,
              .color = colorf::GREY_A255,
          },

          .parent{this},
      }}
{}

// ===== merged from friendchatboard/searchautocompletionitem.cpp =====
#include "pngtexdb.hpp"
#include "gldevice.hpp"

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

SearchAutoCompletionItem::SearchAutoCompletionItem(Widget::VarDir argDir,

        Widget::VarInt argX,
        Widget::VarInt argY,

        bool argByID,
        SDChatPeer argCandidate,

        const char *argLabelXMLStr,

        Widget *argParent,
        bool    argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),

          .w = SearchAutoCompletionItem::WIDTH,
          .h = SearchAutoCompletionItem::HEIGHT,

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , byID(argByID)
    , candidate(std::move(argCandidate))

    , background
      {{
          .w = this->w(),
          .h = this->h(),

          .drawFunc = [this](const Widget *, int drawDstX, int drawDstY)
          {
              if(Widget::ROIMap{.x=drawDstX, .y=drawDstY, .ro{roi()}}.in(GLDeviceHelper::getMousePLoc())){
                  g_glDevice->fillRectangle(colorf::RGB(231, 231, 189) + colorf::A_SHF(64), drawDstX, drawDstY, w(), h());
                  g_glDevice->drawRectangle(colorf::RGB(231, 231, 189) + colorf::A_SHF(64), drawDstX, drawDstY, w(), h());
              }
              else{
                  g_glDevice->fillRectangle(colorf::GREY               + colorf::A_SHF(64), drawDstX, drawDstY, w(), h());
                  g_glDevice->drawRectangle(colorf::RGB(231, 231, 189) + colorf::A_SHF(32), drawDstX, drawDstY, w(), h());
              }
          },

          .parent{this},
      }}

    , icon
      {{
          .dir = DIR_NONE,
          .x = SearchAutoCompletionItem::ICON_WIDTH / 2 + SearchAutoCompletionItem::ICON_MARGIN + 3,
          .y = SearchAutoCompletionItem::HEIGHT     / 2,

          .w = std::min<int>(SearchAutoCompletionItem::ICON_WIDTH, SearchAutoCompletionItem::HEIGHT - 3 * 2),
          .h = std::min<int>(SearchAutoCompletionItem::ICON_WIDTH, SearchAutoCompletionItem::HEIGHT - 3 * 2),

          .texLoadFunc = [](const Widget *) { return g_progUseDB->retrieve(0X00001200); },

          .blendMode = MIR_BLENDMODE_NONE,
          .parent{this},
      }}

    , label
      {{
          .x = 3 + SearchAutoCompletionItem::ICON_MARGIN + SearchAutoCompletionItem::ICON_WIDTH + SearchAutoCompletionItem::GAP,
          .y = 3,

          .font
          {
              .id = 1,
              .size = 14,
          },

          .parent{this},
      }}
{
    if(str_haschar(argLabelXMLStr)){
        label.loadXML(argLabelXMLStr);
    }
    else{
        label.loadXML(str_printf(R"###(<par>%s（%llu）</par>)###", candidate.name.c_str(), to_llu(candidate.id)).c_str());
    }
}

bool SearchAutoCompletionItem::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    if(!valid){
        return consumeFocus(false);
    }

    switch(event.type){
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
            {
                if(m.in(to_d(event.button.x), to_d(event.button.y))){
                    hasParent<SearchPage>()->candidates.setShow(true);
                    hasParent<SearchPage>()->autocompletes.setShow(false);
                    hasParent<SearchPage>()->input.input.setInput(byID ? std::to_string(candidate.id).c_str() : candidate.name.c_str());
                    return consumeFocus(true);
                }
                return false;
            }
        default:
            {
                return Widget::processEventDefault(event, valid, m);
            }
    }
}

// ===== merged from friendchatboard/searchpage.cpp =====
#include "hero.hpp"
#include "client.hpp"
#include "pngtexdb.hpp"
#include "gldevice.hpp"
#include "processrun.hpp"

extern Client *g_client;
extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

SearchPage::SearchPage(Widget::VarDir argDir,

        Widget::VarInt argX,
        Widget::VarInt argY,

        Widget *argParent,
        bool    argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),

          .w = UIPage_MIN_WIDTH  - UIPage_MARGIN * 2,
          .h = UIPage_MIN_HEIGHT - UIPage_MARGIN * 2,

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , input
      {
          DIR_UPLEFT,
          0,
          0,

          this,
          false,
      }

    , clear
      {{
          .dir = DIR_LEFT,
          .x = input.dx() + input.w() + SearchPage::CLEAR_GAP,
          .y = input.dy() + input.h() / 2,

          .initXML = R"###(<layout><par><event id="clear">清空</event></par></layout>)###",
          .font
          {
              .id = 1,
              .size = 15,
          },

          .onClickText = [this](const std::unordered_map<std::string, std::string> &attrList, int event)
          {
              if(event == BEVENT_RELEASE){
                  if(const auto id = LayoutBoard::findAttrValue(attrList, "id", nullptr)){
                      input.input.clear();
                  }
              }
          },

          .parent{this},
      }}

    , autocompletes
      {{
          .y = SearchInputLine::HEIGHT,

          .w = SearchPage::WIDTH,
          .h = SearchPage::HEIGHT - SearchInputLine::HEIGHT,

          .parent
          {
              .widget = this,
          }
      }}

    , candidates
      {{
          .y = SearchInputLine::HEIGHT,

          .w = SearchPage::WIDTH,
          .h = SearchPage::HEIGHT - SearchInputLine::HEIGHT,

          .parent
          {
              .widget = this,
          }
      }}
{
    candidates.setShow(false);
}

void SearchPage::appendFriendItem(const SDChatPeer &candidate)
{
    int maxY = 0;
    candidates.foreachChild([&maxY](const Widget *widget, bool)
    {
        maxY = std::max<int>(maxY, widget->dy() + widget->h());
    });

    candidates.addChild(new FriendItem
    {
        DIR_UPLEFT,
        0,
        maxY,
        [this](const Widget *){ return w(); }, // use SearchPage::w()

        SDChatPeerID(CPR_PLAYER, candidate.id),
        to_u8rawstr(candidate.name).c_str(),

        // guard against non-player candidates (group/special); server can send them and the deref would UB
        [gender = candidate.player() ? candidate.player()->gender : false,
         job    = candidate.player() ? candidate.player()->job    : 0    ](const Widget *)
        {
            return g_progUseDB->retrieve(Hero::faceGfxID(gender, job));
        },

        nullptr,

        {
            (candidate.id == FriendChatBoard::getParentBoard(this)->m_processRun->getMyHero()->dbid()) ? nullptr : new LayoutBoard
            {{
                .initXML = R"###(<layout><par><event id="add">添加</event></par></layout>)###",
                .font
                {
                    .id = 1,
                    .size = 12,
                },

                .onClickText= [candidate, this](const std::unordered_map<std::string, std::string> &attrList, int event)
                {
                    if(event == BEVENT_PRESS){
                        if(const auto id = LayoutBoard::findAttrValue(attrList, "id"); to_sv(id) == "add"){
                            FriendChatBoard::getParentBoard(this)->requestAddFriend(candidate, true);
                        }
                    }
                },
            }},

            true,
        },
    }, true);
}

void SearchPage::appendAutoCompletionItem(bool byID, const SDChatPeer &candidate, const std::string &xmlStr)
{
    int maxY = 0;
    autocompletes.foreachChild([&maxY](const Widget *widget, bool)
    {
        maxY = std::max<int>(maxY, widget->dy() + widget->h());
    });

    autocompletes.addChild(new SearchAutoCompletionItem
    {
        DIR_UPLEFT,
        0,
        maxY,

        byID,
        candidate,
        xmlStr.c_str(),

    }, true);
}

// ===== merged from friendchatboard/friendchatboard.cpp =====
#include <initializer_list>
#include "gldevice.hpp"
#include "client.hpp"
#include "hero.hpp"
#include "pngtexdb.hpp"
#include "processrun.hpp"

extern Client *g_client;
extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

FriendChatBoard::FriendChatBoard(Widget::VarInt argX, Widget::VarInt argY, ProcessRun *runPtr, Widget *argParent, bool argAutoDelete)
    : Widget
      {{
          .x = std::move(argX),
          .y = std::move(argY),

          .w = UIPage_BORDER[2] + UIPage_MIN_WIDTH  + UIPage_BORDER[3],
          .h = UIPage_BORDER[0] + UIPage_MIN_HEIGHT + UIPage_BORDER[1],

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , m_processRun(runPtr)
    , m_cachedChatPeerList
      {
          SDChatPeer
          {
              .id = SYS_CHATDBID_SYSTEM,
              .name = "系统助手",
          },

          SDChatPeer
          {
              .id = SYS_CHATDBID_GROUP,
              .name = "群管理助手",
          },

          m_processRun->getMyHeroChatPeer(),
      }

    , m_frame
      {{
          .texLoadFunc = [](const Widget *){ return g_progUseDB->retrieve(0X00000800); },
      }}

    , m_frameCropDup
      {{
          .getter = &m_frame,
          .vr
          {
              55,
              95,
              230, // = 285 - 55,
              250, // = 345 - 95,
          },

          .resize
          {
              [this]{ return w() - (m_frame.w() - 230); },
              [this]{ return h() - (m_frame.h() - 250); },
          },

          .parent{this},
      }}

    , m_background
      {{
          .texLoadFunc = [](const Widget *){ return g_progUseDB->retrieve(0X00000810); },
          .modColor = colorf::RGBA(160, 160, 160, 255),
      }}

    , m_backgroundCropDup
      {{
          .getter = &m_background,
          .vr
          {
              0,
              0,
              510,
              187,
          },

          .resize
          {
              [this](){ return w() - (m_background.w() - 510); },
              [this](){ return h() - (m_background.h() - 187); },
          },

          .parent{this},
      }}

    , m_close
      {{
          .x = [this]{ return w() - 38; },
          .y = [this]{ return h() - 40; },

          .texIDList
          {
              .on   = 0X0000001C,
              .down = 0X0000001D,
          },

          .onTrigger = [this](Widget *, int)
          {
              setShow(false);
          },

          .parent{this},
      }}

    , m_dragArea
      {{
          .w = [this]{ return w(); },
          .h = [this]{ return h(); },
          .drawFunc = [this](int drawDstX, int drawDstY)
          {
              if(const auto [needDraw, drawColor] = [drawDstX, drawDstY, this] -> std::tuple<bool, uint32_t>
              {
                  if(m_dragIndex.has_value()){
                      return {true, colorf::RGBA(231, 231, 189, 96)};
                  }

                  const auto [mousePX, mousePY] = GLDeviceHelper::getMousePLoc();

                  const auto eventDX = mousePX - drawDstX;
                  const auto eventDY = mousePY - drawDstY;

                  if(getEdgeDragIndex(eventDX, eventDY).has_value()){
                      return {true, colorf::RGBA(255, 255, 255, 64)};
                  }

                  return {false, 0};
              }();

              needDraw){
                  g_glDevice->fillRectangle(drawColor, drawDstX                             , drawDstY                             , w()                 , UIPage_DRAGBORDER[0]);
                  g_glDevice->fillRectangle(drawColor, drawDstX                             , drawDstY + h() - UIPage_DRAGBORDER[1], w()                 , UIPage_DRAGBORDER[1]);
                  g_glDevice->fillRectangle(drawColor, drawDstX                             , drawDstY                             , UIPage_DRAGBORDER[2], h()                 );
                  g_glDevice->fillRectangle(drawColor, drawDstX + w() - UIPage_DRAGBORDER[3], drawDstY                             , UIPage_DRAGBORDER[3], h()                 );
              }
          },
          .parent{this},
      }}

    , m_uiPageList
      {
          UIPage // UIPage_CHAT
          {
              .title = new LabelBoard
              {{
                  .dir = DIR_NONE,
                  .x = [this]{ return 45 + (w() - 45 - 190) / 2; },
                  .y = [    ]{ return 29; },

                  .label = u8"好友名称",
                  .font
                  {
                      .id = 1,
                      .size = 14,
                  },

                  .parent{this},
              }},

              .control = new PageControl
              {
                  DIR_RIGHT,
                  [this]{ return w() - 42; },
                  29,
                  2,

                  {
                      {
                          new TritexButton
                          {{
                              .texIDList
                              {
                                  .off  = 0X000008F0,
                                  .on   = 0X000008F0,
                                  .down = 0X000008F1,
                              },

                              .onTrigger = [this](Widget *, int)
                              {
                                  setUIPage(UIPage_CHATPREVIEW);
                              },
                          }},

                          true,
                      },

                      {
                          new TritexButton
                          {{
                              .texIDList
                              {
                                  .off  = 0X00000023,
                                  .on   = 0X00000023,
                                  .down = 0X00000024,
                              },

                              .onTrigger = [this](Widget *, int)
                              {
                              },
                          }},

                          true,
                      },

                      {
                          new TritexButton
                          {{
                              .texIDList
                              {
                                  .off  = 0X000008B0,
                                  .on   = 0X000008B0,
                                  .down = 0X000008B1,
                              },

                              .onTrigger = [this](Widget *, int)
                              {
                              },
                          }},

                          true,
                      },

                      {
                          new TritexButton
                          {{
                              .texIDList
                              {
                                  .off  = 0X00000590,
                                  .on   = 0X00000590,
                                  .down = 0X00000591,
                              },

                              .onTrigger = [this](Widget *, int)
                              {
                              },
                          }},

                          true,
                      },

                  },

                  this,
                  true,
              },

              .slider = new TexSlider
              {{
                  .bar
                  {
                      .x = [this]{ return w() - 30; },
                      .y = 70,
                      .w = 9,
                      .h = [this]{ return h() - 140; },
                      .v = true,
                  },

                  .index = 3,
                  .parent{this},
              }},

              .page = new ChatPage
              {
                  DIR_UPLEFT,
                  UIPage_BORDER[2],
                  UIPage_BORDER[0],

                  [this]{ return w() - UIPage_BORDER[2] - UIPage_BORDER[3]; }, // UIPage_MARGIN included
                  [this]{ return h() - UIPage_BORDER[0] - UIPage_BORDER[1]; },

                  this,
                  true,
              },

              .enter = [this](int, UIPage *uiPage)
              {
                  const auto title = [chatPage = dynamic_cast<ChatPage *>(uiPage->page), this]()
                  {
                      if(chatPage->peer.group() || chatPage->peer.special() || findChatPeer({CPR_PLAYER, chatPage->peer.id})){
                          return chatPage->peer.name;
                      }
                      else if(chatPage->peer.id == m_processRun->getMyHeroDBID()){
                          return str_printf("自己 %s", chatPage->peer.name.c_str());
                      }
                      else{
                          return str_printf("陌生人 %s", chatPage->peer.name.c_str());
                      }
                  }();
                  uiPage->title->setText(u8"%s", title.c_str());
              },
          },

          UIPage // UIPage_CHATPREVIEW
          {
              .title = new LabelBoard
              {{
                  .dir = DIR_NONE,
                  .x = [this]{ return 45 + (w() - 45 - 190) / 2; },
                  .y = 29,

                  .label = u8"【聊天记录】",
                  .font
                  {
                      .id = 1,
                      .size = 14,
                  },

                  .parent{this},
              }},

              .control = new PageControl
              {
                  DIR_RIGHT,
                  [this]{ return w() - 42; },
                  29,
                  2,

                  {
                      {
                          new TritexButton
                          {{
                              .texIDList
                              {
                                  .off  = 0X00000160,
                                  .on   = 0X00000160,
                                  .down = 0X00000161,
                              },

                              .onTrigger = [this](Widget *, int)
                              {
                                  setUIPage(UIPage_FRIENDLIST);
                              },
                          }},

                          true,
                      },
                  },

                  this,
                  true,
              },

              .slider = new TexSlider
              {{
                  .bar
                  {
                      .x = [this]{ return w() - 30; },
                      .y = 70,
                      .w = 9,
                      .h = [this]{ return h() - 140; },
                      .v = true,
                  },

                  .index = 3,
                  .parent{this},
              }},

              .page = new ChatPreviewPage
              {
                  DIR_UPLEFT,
                  UIPage_BORDER[2] + UIPage_MARGIN,
                  UIPage_BORDER[0] + UIPage_MARGIN,

                  [this]{ return w() - UIPage_BORDER[2] - UIPage_BORDER[3] - 2 * UIPage_MARGIN; },
                  [this]{ return h() - UIPage_BORDER[0] - UIPage_BORDER[1] - 2 * UIPage_MARGIN; },

                  this,
                  true,
              },
          },

          UIPage // UIPage_FRIENDLIST
          {
              .title = new LabelBoard
              {{
                  .dir = DIR_NONE,
                  .x = [this]{ return 45 + (w() - 45 - 190) / 2; },
                  .y = 29,

                  .label = u8"【好友列表】",
                  .font
                  {
                      .id = 1,
                      .size = 14,
                      .color = colorf::WHITE_A255,
                  },

                  .parent{this},
              }},

              .control = new PageControl
              {
                  DIR_RIGHT,
                  [this]{ return w() - 42; },
                  29,
                  2,

                  {
                      {
                          new TritexButton
                          {{
                              .texIDList
                              {
                                  .off  = 0X000008F0,
                                  .on   = 0X000008F0,
                                  .down = 0X000008F1,
                              },

                              .onTrigger = [this](Widget *, int)
                              {
                                  setUIPage(UIPage_CHATPREVIEW);
                              },
                          }},

                          true,
                      },

                      {
                          new TritexButton
                          {{
                              .texIDList
                              {
                                  .off  = 0X00000900,
                                  .on   = 0X00000900,
                                  .down = 0X00000901,
                              },

                              .onTrigger = [this](Widget *, int)
                              {
                                  setUIPage(UIPage_FRIENDSEARCH);
                              },
                          }},

                          true,
                      },

                      {
                          new TritexButton
                          {{
                              .texIDList
                              {
                                  .off  = 0X00000170,
                                  .on   = 0X00000170,
                                  .down = 0X00000171,
                              },

                              .onTrigger = [this](Widget *, int)
                              {
                                  setUIPage(UIPage_CREATEGROUP);
                              },
                          }},

                          true,
                      },
                  },

                  this,
                  true,
              },

              .slider = new TexSlider
              {{
                  .bar
                  {
                      .x = [this]{ return w() - 30; },
                      .y = 70,
                      .w = 9,
                      .h = [this]{ return h() - 140; },
                      .v = true,
                  },

                  .index = 3,
                  .parent{this},
              }},

              .page = new FriendListPage
              {
                  DIR_UPLEFT,
                  UIPage_BORDER[2] + UIPage_MARGIN,
                  UIPage_BORDER[0] + UIPage_MARGIN,

                  [this]{ return w() - UIPage_BORDER[2] - UIPage_BORDER[3] - 2 * UIPage_MARGIN; },
                  [this]{ return h() - UIPage_BORDER[0] - UIPage_BORDER[1] - 2 * UIPage_MARGIN; },

                  this,
                  true,
              },
          },

          UIPage // UIPage_FRIENDSEARCH
          {
              .title = new LabelBoard
              {{
                  .dir = DIR_NONE,
                  .x = [this]{ return 45 + (w() - 45 - 190) / 2; },
                  .y = 29,

                  .label = u8"【查找用户】",
                  .font
                  {
                      .id = 1,
                      .size = 14,
                  },

                  .parent{this},
              }},

              .control = new PageControl
              {
                  DIR_RIGHT,
                  [this]{ return w() - 42; },
                  29,
                  2,

                  {
                      {
                          new TritexButton
                          {{
                              .texIDList
                              {
                                  .off  = 0X000008F0,
                                  .on   = 0X000008F0,
                                  .down = 0X000008F1,
                              },

                              .onTrigger = [this](Widget *, int)
                              {
                                  setUIPage(UIPage_CHATPREVIEW);
                              },
                          }},

                          true,
                      },
                  },

                  this,
                  true,
              },

              .slider = new TexSlider
              {{
                  .bar
                  {
                      .x = [this]{ return w() - 30; },
                      .y = 70,
                      .w = 9,
                      .h = [this]{ return h() - 140; },
                      .v = true,
                  },

                  .index = 3,
                  .parent{this},
              }},

              .page = new SearchPage
              {
                  DIR_UPLEFT,
                  UIPage_BORDER[2] + UIPage_MARGIN,
                  UIPage_BORDER[0] + UIPage_MARGIN,

                  this,
                  true,
              },
          },

          UIPage // UIPage_CREATEGROUP
          {
              .title = new LabelBoard
              {{
                  .dir = DIR_NONE,
                  .x = [this]{ return 45 + (w() - 45 - 190) / 2; },
                  .y = 29,

                  .label = u8"【创建群聊】",
                  .font
                  {
                      .id = 1,
                      .size = 14,
                  },

                  .parent{this},
              }},

              .control = new PageControl
              {
                  DIR_RIGHT,
                  [this]{ return w() - 42; },
                  29,
                  2,

                  {
                      {
                          new TritexButton
                          {{
                              .texIDList
                              {
                                  .off  = 0X000008F0,
                                  .on   = 0X000008F0,
                                  .down = 0X000008F1,
                              },

                              .onTrigger = [this](Widget *, int)
                              {
                                  setUIPage(UIPage_CHATPREVIEW);
                              },
                          }},

                          true,
                      },

                      {
                          new TritexButton
                          {{
                              .texIDList
                              {
                                  .off  = 0X00000910,
                                  .on   = 0X00000910,
                                  .down = 0X00000911,
                              },

                              .onTrigger = [this](Widget *, int)
                              {
                                  std::vector<uint32_t> dbidList;
                                  dynamic_cast<FriendListPage *>(m_uiPageList[UIPage_CREATEGROUP].page)->canvas.foreachChild([&dbidList](const Widget *widget, bool)
                                  {
                                      if(const auto friendItem = dynamic_cast<const FriendItem *>(widget)){
                                          if(const auto checkBox = dynamic_cast<const CheckBox *>(friendItem->hasChild(friendItem->funcWidgetID)); checkBox->getter()){
                                              dbidList.push_back(friendItem->cpid.id());
                                          }
                                      }
                                  });

                                  if(dbidList.empty()){
                                      return;
                                  }

                                  if(dbidList.size() > CMCreateChatGroup().list.capacity()){
                                      throw fflpanic("selected too many friends, max {}", CMCreateChatGroup().list.capacity());
                                  }

                                  auto inputBoardPtr = dynamic_cast<InputStringBoard *>(m_processRun->getWidget("InputStringBoard"));

                                  inputBoardPtr->waitInput(u8"<layout><par>请输入你要建立的群名称</par></layout>", false, [dbidList, this](std::u8string inputString)
                                  {
                                      if(inputString.empty()){
                                          m_processRun->addCBLog(CBLOG_ERR, u8"无效输入:%s", to_cstr(inputString));
                                          return;
                                      }

                                      CMCreateChatGroup cmCCG;
                                      std::memset(&cmCCG, 0, sizeof(cmCCG));

                                      cmCCG.name.assign(inputString);
                                      cmCCG.list.assign(dbidList.begin(), dbidList.end());

                                      g_client->send({CM_CREATECHATGROUP, cmCCG}, [this](uint8_t headCode, const uint8_t *buf, size_t size)
                                      {
                                          switch(headCode){
                                              case SM_OK:
                                                  {
                                                      addGroup(cerealf::deserialize<SDChatPeer>(buf, size));
                                                      break;
                                                  }
                                              default:
                                                  {
                                                      throw fflpanic("failed to create group");
                                                  }
                                          }
                                      });
                                  });
                              },
                          }},

                          true,
                      },

                      {
                          new TritexButton
                          {{
                              .texIDList
                              {
                                  .off  = 0X00000860,
                                  .on   = 0X00000860,
                                  .down = 0X00000861,
                              },

                              .onTrigger = [this](Widget *, int)
                              {
                                  dynamic_cast<FriendListPage *>(m_uiPageList[UIPage_CREATEGROUP].page)->canvas.foreachChild([](Widget *widget, bool)
                                  {
                                      if(auto friendItem = dynamic_cast<FriendItem *>(widget)){
                                          if(auto checkBox = dynamic_cast<CheckBox *>(friendItem->hasChild(friendItem->funcWidgetID)); checkBox->getter()){
                                              checkBox->toggle();
                                          }
                                      }
                                  });
                              },
                          }},

                          true,
                      },
                  },

                  this,
                  true,
              },

              .slider = new TexSlider
              {{
                  .bar
                  {
                      .x = [this]{ return w() - 30; },
                      .y = 70,
                      .w = 9,
                      .h = [this]{ return h() - 140; },
                      .v = true,
                  },

                  .index = 3,
                  .parent{this},
              }},

              .page = new FriendListPage
              {
                  DIR_UPLEFT,
                  UIPage_BORDER[2] + UIPage_MARGIN,
                  UIPage_BORDER[0] + UIPage_MARGIN,
                  [this]{ return w() - UIPage_BORDER[2] - UIPage_BORDER[3] - 2 * UIPage_MARGIN; },
                  [this]{ return h() - UIPage_BORDER[0] - UIPage_BORDER[1] - 2 * UIPage_MARGIN; },

                  this,
                  true,
              },

              .enter = [this](int, UIPage *uiPage)
              {
                  auto listPage = dynamic_cast<FriendListPage *>(uiPage->page);
                  listPage->canvas.clearChild([this](const Widget *widget, bool)
                  {
                      return std::find_if(m_sdFriendList.begin(), m_sdFriendList.end(), [widget](const auto &x)
                      {
                          return dynamic_cast<const FriendItem *>(widget)->cpid.id() == x.id;

                      }) == m_sdFriendList.end();
                  });

                  for(const auto &peer: m_sdFriendList){
                      if(!listPage->canvas.hasChild([&peer](const Widget *widget, bool)
                      {
                          return dynamic_cast<const FriendItem *>(widget)->cpid.id() == peer.id;

                      })){
                          listPage->append(peer, [](FriendItem *item)
                          {
                              if(auto friendItem = dynamic_cast<FriendItem *>(item)){
                                  if(auto checkBox = dynamic_cast<CheckBox *>(friendItem->hasChild(friendItem->funcWidgetID))){
                                      checkBox->toggle();
                                  }
                              }
                          },

                          {
                              new CheckBox
                              {{
                                  .w = FriendItem::HEIGHT / 3,
                                  .h = FriendItem::HEIGHT / 3,
                              }},
                              true,
                          });
                      }
                  }
              },
          },
      }
{
    setShow(false);
}

void FriendChatBoard::drawDefault(Widget::ROIMap m) const
{
    if(!m.calibrate(this)){
        return;
    }

    for(const auto &p:
    {
        static_cast<const Widget *>(&m_backgroundCropDup),
        static_cast<const Widget *>( m_uiPageList[m_uiPage].page),
        static_cast<const Widget *>(&m_frameCropDup),
        static_cast<const Widget *>( m_uiPageList[m_uiPage].title),
        static_cast<const Widget *>( m_uiPageList[m_uiPage].control),
        static_cast<const Widget *>( m_uiPageList[m_uiPage].slider),
        static_cast<const Widget *>(&m_close),
        static_cast<const Widget *>(&m_dragArea),
    }){
        int drawDstX = m.x;
        int drawDstY = m.y;
        int drawSrcX = m.ro->x;
        int drawSrcY = m.ro->y;
        int drawSrcW = m.ro->w;
        int drawSrcH = m.ro->h;

        if(mathf::cropChildROI(
                    &drawSrcX, &drawSrcY,
                    &drawSrcW, &drawSrcH,
                    &drawDstX, &drawDstY,

                    w(),
                    h(),

                    p->dx(),
                    p->dy(),
                    p-> w(),
                    p-> h())){
            p->draw({.x=drawDstX, .y=drawDstY, .ro{drawSrcX, drawSrcY, drawSrcW, drawSrcH}});
        }
    }
}

bool FriendChatBoard::processEventDefault(const MirEvent &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    if(!valid){
        m_dragIndex.reset();
        return consumeFocus(false);
    }

    if(m_close                        .processEventParent(event, valid, m)){ return true; }
    if(m_uiPageList[m_uiPage].slider ->processEventParent(event, valid, m)){ return true; }
    if(m_uiPageList[m_uiPage].page   ->processEventParent(event, valid, m)){ return true; }
    if(m_uiPageList[m_uiPage].control->processEventParent(event, valid, m)){ return true; }

    switch(event.type){
        case MIR_EVENT_KEY_DOWN:
            {
                if(focus()){
                    switch(event.key.key){
                        case MIRK_ESCAPE:
                            {
                                setShow(false);
                                setFocus(false);
                                return true;
                            }
                        default:
                            {
                                return false;
                            }
                    }
                }
                return false;
            }
        case MIR_EVENT_MOUSE_BUTTON_DOWN:
            {
                if(m.create(m_uiPageList[m_uiPage].page->roi()).in(to_d(event.button.x), to_d(event.button.y))){
                    if(m_uiPageList[m_uiPage].page->processEventParent(event, true, m)){
                        return consumeFocus(true, m_uiPageList[m_uiPage].page);
                    }
                }

                const auto mapXDiff = m.x - m.ro->x;
                const auto mapYDiff = m.y - m.ro->y;

                m_dragIndex = getEdgeDragIndex(to_d(event.button.x) - mapXDiff, to_d(event.button.y) - mapYDiff);
                return consumeFocus(m.in(to_d(event.button.x), to_d(event.button.y)));
            }
        case MIR_EVENT_MOUSE_BUTTON_UP:
            {
                m_dragIndex.reset();
                return consumeFocus(m.in(to_d(event.button.x), to_d(event.button.y)));
            }
        case MIR_EVENT_MOUSE_MOTION:
            {
                if(event.motion.state & MIR_BUTTON_LMASK){
                    if(m_dragIndex.has_value()){
                        bool sizeChanged = false;
                        const auto fnAdjustW = [&sizeChanged, this](int dw, bool adjustOff)
                        {
                            const int oldW = w();
                            const int newW = std::max<int>(oldW + dw, UIPage_BORDER[2] + UIPage_MIN_WIDTH + UIPage_BORDER[3]);

                            if(oldW != newW){
                                sizeChanged = true;
                                setW(newW);
                                if(adjustOff){
                                    moveBy(oldW - newW, 0);
                                }
                            }
                        };

                        const auto fnAdjustH = [&sizeChanged, this](int dh, bool adjustOff)
                        {
                            const int oldH = h();
                            const int newH = std::max<int>(oldH + dh, UIPage_BORDER[0] + UIPage_MIN_HEIGHT + UIPage_BORDER[1]);

                            if(oldH != newH){
                                sizeChanged = true;
                                setH(newH);
                                if(adjustOff){
                                    moveBy(0, oldH - newH);
                                }
                            }
                        };

                        if     (m_dragIndex.value() == 0){ fnAdjustW(-to_d(event.motion.xrel), 1); fnAdjustH(-to_d(event.motion.yrel), 1); }
                        else if(m_dragIndex.value() == 1){                                   fnAdjustH(-to_d(event.motion.yrel), 1); }
                        else if(m_dragIndex.value() == 2){ fnAdjustW( to_d(event.motion.xrel), 0); fnAdjustH(-to_d(event.motion.yrel), 1); }
                        else if(m_dragIndex.value() == 3){ fnAdjustW(-to_d(event.motion.xrel), 1);                                   }
                        else if(m_dragIndex.value() == 4){ fnAdjustW( to_d(event.motion.xrel), 0);                                   }
                        else if(m_dragIndex.value() == 5){ fnAdjustW(-to_d(event.motion.xrel), 1); fnAdjustH( to_d(event.motion.yrel), 0); }
                        else if(m_dragIndex.value() == 6){                                   fnAdjustH( to_d(event.motion.yrel), 0); }
                        else                             { fnAdjustW( to_d(event.motion.xrel), 0); fnAdjustH( to_d(event.motion.yrel), 0); }

                        if(sizeChanged){
                            afterResize();
                        }
                    }
                    else{
                        const auto remapXDiff = m.x - m.ro->x;
                        const auto remapYDiff = m.y - m.ro->y;

                        const auto [rendererW, rendererH] = g_glDevice->getRendererSize();

                        const int maxX = rendererW - w();
                        const int maxY = rendererH - h();

                        const int newX = std::max<int>(0, std::min<int>(maxX, remapXDiff + to_d(event.motion.xrel)));
                        const int newY = std::max<int>(0, std::min<int>(maxY, remapYDiff + to_d(event.motion.yrel)));

                        moveBy(newX - remapXDiff, newY - remapYDiff);
                    }
                    return consumeFocus(true);
                }
                return false;
            }
        case MIR_EVENT_MOUSE_WHEEL:
            {
                if(m_uiPageList[m_uiPage].page->focus()){
                    if(m_uiPageList[m_uiPage].page->processEvent(event, true, m)){
                        return consumeFocus(true, m_uiPageList[m_uiPage].page);
                    }
                }
                return false;
            }
        default:
            {
                return false;
            }
    }
}

void FriendChatBoard::addFriendListChatPeer(const SDChatPeerID &sdCPID)
{
    queryChatPeer(sdCPID, [this](const SDChatPeer *peer, bool)
    {
        if(!peer){
            return;
        }

        dynamic_cast<FriendListPage *>(m_uiPageList[UIPage_FRIENDLIST].page)->append(*peer, [peerInst = *peer, this](FriendItem *item)
        {
            setChatPeer(peerInst, true);
            setUIPage(UIPage_CHAT);
            m_processRun->requestLatestChatMessage({item->cpid.asU64()}, 50, true, true);
        });
    });
}

void FriendChatBoard::setFriendList(const SDFriendList &sdFL)
{
    m_sdFriendList = sdFL;
    std::unordered_set<uint64_t> seenCPIDList;

    const auto fnAddFriend = [&seenCPIDList, this](const SDChatPeerID &sdCPID)
    {
        if(!seenCPIDList.contains(sdCPID.asU64())){
            seenCPIDList.insert(sdCPID.asU64());
            addFriendListChatPeer(sdCPID);
        }
    };

    fnAddFriend(SDChatPeerID(CPR_SPECIAL, SYS_CHATDBID_SYSTEM));
    fnAddFriend(m_processRun->getMyHero()->cpid());

    for(const auto &sdCP: sdFL){
        fnAddFriend(sdCP.cpid());
    }
}

const SDChatPeer *FriendChatBoard::findFriendChatPeer(const SDChatPeerID &sdCPID) const
{
    const auto fnOp = [&sdCPID](const SDChatPeer &peer)
    {
        return peer.cpid() == sdCPID;
    };

    if(auto p = std::find_if(m_sdFriendList.begin(), m_sdFriendList.end(), fnOp); p != m_sdFriendList.end()){
        return std::addressof(*p);
    }

    return nullptr;
}

const SDChatPeer *FriendChatBoard::findChatPeer(const SDChatPeerID &sdCPID) const
{
    const auto fnOp = [&sdCPID](const SDChatPeer &peer)
    {
        return peer.cpid() == sdCPID;
    };

    if(auto p = std::find_if(m_sdFriendList.begin(), m_sdFriendList.end(), fnOp); p != m_sdFriendList.end()){
        return std::addressof(*p);
    }

    if(auto p = std::find_if(m_cachedChatPeerList.begin(), m_cachedChatPeerList.end(), fnOp); p != m_cachedChatPeerList.end()){
        return std::addressof(*p);
    }

    return nullptr;
}

void FriendChatBoard::queryChatMessage(uint64_t argMsgID, std::function<void(const SDChatMessage *, bool)> argOp)
{
    if(auto p = m_cachedChatMessageList.find(argMsgID); p != m_cachedChatMessageList.end()){
        if(argOp){
            argOp(std::addressof(p->second), false);
        }
    }

    else{
        CMQueryChatMessage cmQCM;
        std::memset(&cmQCM, 0, sizeof(cmQCM));

        cmQCM.msgid = argMsgID;
        g_client->send({CM_QUERYCHATMESSAGE, cmQCM}, [argMsgID, argOp = std::move(argOp), this](uint8_t headCode, const uint8_t *data, size_t size)
        {
            if(headCode == SM_OK){
                const auto sdCM = cerealf::deserialize<SDChatMessage>(data, size);
                fflassert(sdCM.seq.has_value());
                fflassert(sdCM.seq.value().id == argMsgID);

                auto iter = m_cachedChatMessageList.emplace(argMsgID, sdCM);
                if(argOp){
                    argOp(std::addressof(iter.first->second), true);
                }
            }
            else if(argOp){
                argOp(nullptr, true);
            }
        });
    }
}

void FriendChatBoard::queryChatPeer(const SDChatPeerID &sdCPID, std::function<void(const SDChatPeer *, bool)> argOp)
{
    if(auto p = sdCPID.group() ? nullptr : findChatPeer(sdCPID)){
        if(argOp){
            argOp(p, false);
        }
    }

    else{
        CMQueryChatPeerList cmQCPL;
        std::memset(&cmQCPL, 0, sizeof(cmQCPL));

        cmQCPL.input.assign(std::to_string(sdCPID.id()));
        g_client->send({CM_QUERYCHATPEERLIST, cmQCPL}, [sdCPID, argOp = std::move(argOp), this](uint8_t headCode, const uint8_t *data, size_t size)
        {
            switch(headCode){
                case SM_OK:
                  {
                      if(const auto sdPCL = cerealf::deserialize<SDChatPeerList>(data, size); sdPCL.empty()){
                          if(argOp){
                              argOp(nullptr, true);
                          }
                          return;
                      }
                      else{
                          for(const auto &peer: sdPCL){
                              if(peer.cpid() == sdCPID){
                                  if(argOp){
                                      argOp(&peer, true);
                                  }
                                  return;
                              }
                          }

                          if(argOp){
                              argOp(nullptr, true);
                          }
                          return;
                      }
                  }
              default:
                  {
                      throw fflpanic("query failed in server");
                  }
            }
        });
    }
}

void FriendChatBoard::addMessage(std::optional<uint64_t> localPendingID, const SDChatMessage &sdCM)
{
    fflassert(sdCM.seq.has_value());
    const auto peerCPID = [&sdCM, this]
    {
        if(sdCM.from == m_processRun->getMyHero()->cpid()){
            return sdCM.to;
        }
        else if(sdCM.to == m_processRun->getMyHero()->cpid()){
            return sdCM.from;
        }
        else if(sdCM.to.group() && findFriendChatPeer(sdCM.to)){
            return sdCM.to;
        }
        else{
            throw fflpanic("received invalid chat message: from {}, to {}, self {}", sdCM.from.asU64(), sdCM.to.asU64(), m_processRun->getMyHero()->cpid().asU64());
        }
    }();

    auto peerIter = std::find_if(m_friendMessageList.begin(), m_friendMessageList.end(), [peerCPID, &sdCM](const auto &item)
    {
        return item.cpid == peerCPID;
    });

    if(peerIter == m_friendMessageList.end()){
        m_friendMessageList.emplace_front(peerCPID);
    }
    else if(peerIter != m_friendMessageList.begin()){
        m_friendMessageList.splice(m_friendMessageList.begin(), m_friendMessageList, peerIter, std::next(peerIter));
    }

    peerIter = m_friendMessageList.begin();
    if(std::find_if(peerIter->list.begin(), peerIter->list.end(), [&sdCM](const auto &msg){ return msg.seq.value().id == sdCM.seq.value().id; }) == peerIter->list.end()){
        peerIter->unread++;
        peerIter->list.push_back(sdCM);

        auto chatPage = dynamic_cast<ChatPage *>(m_uiPageList[UIPage_CHAT].page);
        auto chatPreviewPage = dynamic_cast<ChatPreviewPage *>(m_uiPageList[UIPage_CHATPREVIEW].page);

        if(peerIter->list.size() >= 2 && peerIter->list.back().seq.value().timestamp < peerIter->list.rbegin()[1].seq.value().timestamp){
            std::sort(peerIter->list.begin(), peerIter->list.end(), [](const auto &x, const auto &y)
            {
                if(x.seq.value().timestamp != y.seq.value().timestamp){
                    return x.seq.value().timestamp < y.seq.value().timestamp;
                }
                else{
                    return x.seq.value().id < y.seq.value().id;
                }
            });

            if(chatPage->peer.cpid() == peerIter->cpid){
                loadChatPage();
            }
        }
        else{
            if(chatPage->peer.cpid() == peerIter->cpid){
                if(localPendingID.has_value()){
                    if(auto p = chatPage->chat.canvas.hasDescendant(localPendingID.value())){
                        dynamic_cast<ChatItem *>(p)->pending = false;
                        dynamic_cast<ChatItem *>(p)->msgID   = sdCM.seq.value().id;
                    }
                }
                else{
                    chatPage->chat.append(peerIter->list.back(), nullptr);
                }
            }
        }

        if(chatPage->peer.cpid() == peerIter->cpid){
            if(chatPage->chat.h() >= chatPage->chat.canvas.h()){
                m_uiPageList[UIPage_CHAT].slider->setShow(false);
            }
            else{
                m_uiPageList[UIPage_CHAT].slider->setShow(true);
                m_uiPageList[UIPage_CHAT].slider->setValue(1.0, false);
            }
        }

        chatPreviewPage->updateChatPreview(peerCPID, cerealf::deserialize<std::string>(peerIter->list.back().message));
    }
}

void FriendChatBoard::addMessagePending(uint64_t localPendingID, const SDChatMessage &sdCM)
{
    fflassert(!sdCM.seq.has_value());
    if(!m_localMessageList.emplace(localPendingID, sdCM).second){
        throw fflpanic("adding a pending message with local pending id which has already been used: {}", localPendingID);
    }
}

void FriendChatBoard::finishMessagePending(size_t localPendingID, const SDChatMessageDBSeq &sdCMDBS)
{
    if(auto p = m_localMessageList.find(localPendingID); p != m_localMessageList.end()){
        auto chatMessage = std::move(p->second);
        m_localMessageList.erase(p);

        chatMessage.seq = sdCMDBS;
        addMessage(localPendingID, chatMessage);
    }
    else{
        throw fflpanic("invalid local pending message id: {}", localPendingID);
    }
}

void FriendChatBoard::setChatPeer(const SDChatPeer &sdCP, bool forceReload)
{
    if(auto chatPage = dynamic_cast<ChatPage *>(m_uiPageList[UIPage_CHAT].page); (chatPage->peer.id != sdCP.id) || forceReload){
        chatPage->peer = sdCP;
        loadChatPage();
    }
}

void FriendChatBoard::setUIPage(int uiPage)
{
    fflassert(uiPage >= 0, uiPage);
    fflassert(uiPage < UIPage_END, uiPage);

    const auto fromPage = m_uiPage;
    const auto   toPage =   uiPage;

    if(fromPage != toPage){
        if(m_uiPageList[fromPage].exit){
            m_uiPageList[fromPage].exit(toPage, std::addressof(m_uiPageList[fromPage]));
        }

        // update state BEFORE enter() so any callback that reads m_uiPage sees the new page
        m_uiLastPage = fromPage;
        m_uiPage     =   toPage;

        if(m_uiPageList[toPage].enter){
            m_uiPageList[toPage].enter(fromPage, std::addressof(m_uiPageList[toPage]));
        }

        m_uiPageList[fromPage].page->setFocus(false);
        m_uiPageList[  toPage].page->setFocus(true );
    }
}

void FriendChatBoard::loadChatPage()
{
    auto chatPage = dynamic_cast<ChatPage *>(m_uiPageList[UIPage_CHAT].page);
    chatPage->chat.clearChatItem(true);

    for(const auto &elem: m_friendMessageList){
        if(elem.cpid == chatPage->peer.cpid()){
            for(const auto &msg: elem.list){
                chatPage->chat.append(msg, nullptr);
            }
            break;
        }
    }

    for(const auto &[localID, sdCM]: m_localMessageList){
        if(sdCM.to == chatPage->peer.cpid()){
            chatPage->chat.append(sdCM, nullptr);
        }
    }
}

void FriendChatBoard::addGroup(const SDChatPeer &sdCP)
{
    if(findChatPeer(sdCP.cpid())){
        return;
    }

    m_sdFriendList.push_back(sdCP);
    addFriendListChatPeer(sdCP.cpid());
    dynamic_cast<ChatPreviewPage *>(m_uiPageList[UIPage_CHATPREVIEW].page)->updateChatPreview(sdCP.cpid(), R"###(<layout><par>你已经加入了群聊，现在就可以聊天了。</par></layout>)###");
}

FriendChatBoard *FriendChatBoard::getParentBoard(Widget *widget)
{
    fflassert(widget);
    while(widget){
        if(auto p = dynamic_cast<FriendChatBoard *>(widget)){
            return p;
        }
        else{
            widget = widget->parent();
        }
    }
    throw fflpanic("widget is not a decedent of FriendChatBoard");
}

const FriendChatBoard *FriendChatBoard::getParentBoard(const Widget *widget)
{
    fflassert(widget);
    while(widget){
        if(auto p = dynamic_cast<const FriendChatBoard *>(widget)){
            return p;
        }
        else{
            widget = widget->parent();
        }
    }
    throw fflpanic("widget is not a decedent of FriendChatBoard");
}

void FriendChatBoard::requestAddFriend(const SDChatPeer &argCP, bool switchToChatPreview)
{
    CMAddFriend cmAF;
    std::memset(&cmAF, 0, sizeof(cmAF));

    cmAF.cpid = argCP.cpid().asU64();
    g_client->send({CM_ADDFRIEND, cmAF}, [argCP, switchToChatPreview, this](uint8_t headCode, const uint8_t *buf, size_t bufSize)
    {
        switch(headCode){
            case SM_OK:
                {
                    switch(const auto sdAFN = cerealf::deserialize<SDAddFriendNotif>(buf, bufSize); sdAFN.notif){
                        case AF_ACCEPTED:
                            {
                                onAddFriendAccepted(argCP);
                                if(switchToChatPreview){
                                    setUIPage(UIPage_CHATPREVIEW);
                                }
                                break;
                            }
                        case AF_REJECTED:
                            {
                                onAddFriendRejected(argCP);
                                break;
                            }
                        case AF_EXIST:
                            {
                                m_processRun->addCBParLog(u8R"###(<par bgcolor="rgb(0x00, 0x80, 0x00)">重复添加好友<t color="red">%s</t></par>)###", to_cstr(argCP.name));
                                break;
                            }
                        case AF_PENDING:
                            {
                                m_processRun->addCBParLog(u8R"###(<par bgcolor="rgb(0x00, 0x80, 0x00)">等待<t color="red">%s</t>处理你的好友验证</par>)###", to_cstr(argCP.name));
                                break;
                            }
                        case AF_BLOCKED:
                            {
                                m_processRun->addCBParLog(u8R"###(<par bgcolor="rgb(0x00, 0x80, 0x00)">你已经被<t color="red">%s</t>加入了黑名单</par>)###", to_cstr(argCP.name));
                                break;
                            }
                        default:
                            {
                                break;
                            }
                    }
                    break;
                }
            default:
                {
                    m_processRun->addCBParLog(u8R"###(<par bgcolor="rgb(0x00, 0x80, 0x00)">无效的请求。</par>)###");
                    break;
                }
        }
    });
}

void FriendChatBoard::requestAcceptAddFriend(const SDChatPeer &argCP)
{
    CMAcceptAddFriend cmAAF;
    std::memset(&cmAAF, 0, sizeof(cmAAF));

    cmAAF.cpid = argCP.cpid().asU64();
    g_client->send({CM_ACCEPTADDFRIEND, cmAAF}, [argCP, this](uint8_t headCode, const uint8_t *, size_t)
    {
        switch(headCode){
            case SM_OK:
                {
                    m_processRun->addCBParLog(u8R"###(<par bgcolor="rgb(0x00, 0x80, 0x00)">你已经通过<t color="red">%s</t>的好友申请</par>)###", to_cstr(argCP.name));
                    break;
                }
            default:
                {
                    m_processRun->addCBParLog(u8R"###(<par bgcolor="red">无效的请求。</par>)###");
                    break;
                }
        }
    });
}

void FriendChatBoard::requestRejectAddFriend(const SDChatPeer &argCP)
{
    CMRejectAddFriend cmRAF;
    std::memset(&cmRAF, 0, sizeof(cmRAF));

    cmRAF.cpid = argCP.cpid().asU64();
    g_client->send({CM_REJECTADDFRIEND, cmRAF}, [argCP, this](uint8_t headCode, const uint8_t *, size_t)
    {
        switch(headCode){
            case SM_OK:
                {
                    m_processRun->addCBParLog(u8R"###(<par bgcolor="rgb(0x00, 0x80, 0x00)">你已经拒绝<t color="red">%s</t>的好友申请。</par>)###", to_cstr(argCP.name));
                    break;
                }
            default:
                {
                    m_processRun->addCBParLog(u8R"###(<par bgcolor="red">无效的请求。</par>)###");
                    break;
                }
        }
    });
}

void FriendChatBoard::requestBlockPlayer(const SDChatPeer &argCP)
{
    CMBlockPlayer cmBP;
    std::memset(&cmBP, 0, sizeof(cmBP));

    cmBP.cpid = argCP.cpid().asU64();
    g_client->send({CM_BLOCKPLAYER, cmBP}, [argCP, this](uint8_t headCode, const uint8_t *, size_t)
    {
        switch(headCode){
            case SM_OK:
                {
                    m_processRun->addCBParLog(u8R"###(<par bgcolor="rgb(0x00, 0x80, 0x00)">你已经拉黑<t color="red">%s</t>。</par>)###", to_cstr(argCP.name));
                    break;
                }
            default:
                {
                    m_processRun->addCBParLog(u8R"###(<par bgcolor="rgb(0x00, 0x80, 0x00)">无效的拉黑请求。</par>)###");
                    break;
                }
        }
    });
}

void FriendChatBoard::onAddFriendAccepted(const SDChatPeer &argCP)
{
    if(!findFriendChatPeer(argCP.cpid())){
        m_sdFriendList.push_back(argCP);
        addFriendListChatPeer(argCP.cpid());

        m_processRun->addCBParLog(u8R"###(<par bgcolor="rgb(0x00, 0x80, 0x00)"><t color="red">%s</t>已经通过了你的好友请求。</par>)###", to_cstr(argCP.name));
        dynamic_cast<ChatPreviewPage *>(m_uiPageList[UIPage_CHATPREVIEW].page)->updateChatPreview(argCP.cpid(), str_printf(R"###(<layout><par><t color="red">%s</t>已经通过你的好友申请，现在可以开始聊天了。</par></layout>)###", to_cstr(argCP.name)));
    }
}

void FriendChatBoard::onAddFriendRejected(const SDChatPeer &argCP)
{
    m_processRun->addCBParLog(u8R"###(<par bgcolor="rgb(0x00, 0x80, 0x00)"><t color="red">%s</t>已经拒绝了你的好友请求。</par>)###", to_cstr(argCP.name));
}

std::optional<int> FriendChatBoard::getEdgeDragIndex(int eventDX, int eventDY) const
{
    // ->|w0|<----w1------->|w2|<-   |
    //   x0 x1              x2       v
    //   +--+---------------+--+ y0  -
    //   |0)|       1)      |2)|     h0
    //   +--+---------------+--+ y1  -
    //   |  |               |  |     ^
    //   |  |               |  |     |
    //   |3)|               |4)|     h1
    //   |  |               |  |     |
    //   |  |               |  |     v
    //   +--+---------------+--+ y2  -
    //   |5)|       6)      |7)|     h2
    //   +--+---------------+--+     -
    //                               ^
    //                               |

    const int x0 = 0;
    const int x1 = x0       + UIPage_DRAGBORDER[2];
    const int x2 = x0 + w() - UIPage_DRAGBORDER[3];

    const int y0 = 0;
    const int y1 = y0       + UIPage_DRAGBORDER[0];
    const int y2 = y0 + h() - UIPage_DRAGBORDER[1];

    const int w0 =                              UIPage_DRAGBORDER[2];
    const int w1 = w() - UIPage_DRAGBORDER[2] - UIPage_DRAGBORDER[3];
    const int w2 =                              UIPage_DRAGBORDER[3];

    const int h0 =                              UIPage_DRAGBORDER[0];
    const int h1 = h() - UIPage_DRAGBORDER[0] - UIPage_DRAGBORDER[1];
    const int h2 =                              UIPage_DRAGBORDER[1];

    if     (mathf::pointInRectangle<int>(eventDX, eventDY, x0, y0, w0, h0)) return 0;
    else if(mathf::pointInRectangle<int>(eventDX, eventDY, x1, y0, w1, h0)) return 1;
    else if(mathf::pointInRectangle<int>(eventDX, eventDY, x2, y0, w2, h0)) return 2;
    else if(mathf::pointInRectangle<int>(eventDX, eventDY, x0, y1, w0, h1)) return 3;
    else if(mathf::pointInRectangle<int>(eventDX, eventDY, x2, y1, w2, h1)) return 4;
    else if(mathf::pointInRectangle<int>(eventDX, eventDY, x0, y2, w0, h2)) return 5;
    else if(mathf::pointInRectangle<int>(eventDX, eventDY, x1, y2, w1, h2)) return 6;
    else if(mathf::pointInRectangle<int>(eventDX, eventDY, x2, y2, w2, h2)) return 7;
    else                                                                  return std::nullopt;
}
