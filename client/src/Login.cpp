#include "Login.hpp"

// ===== processlogo.cpp =====
#include "client.hpp"
#include "gui_texture.hpp"
#include "gldevice.hpp"
#include "clientargparser.hpp"

extern Client *g_client;
extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;
extern ClientArgParser *g_clientArgParser;

void ProcessLogo::processEvent(const MirEvent &event)
{
    switch(event.type){
        case MIR_EVENT_KEY_DOWN:
            {
                switch(event.key.key){
                    case MIRK_SPACE:
                    case MIRK_ESCAPE:
                        {
                            g_client->requestProcess(PROCESSID_SYRC);
                        }
                        break;
                    default:
                        break;
                }
                break;
            }
        default:
            break;
    }
}

void ProcessLogo::update(double fDTime)
{
    m_totalTime += fDTime;
    if(m_totalTime >= m_fullTime || g_clientArgParser->autoLogin){
        g_client->requestProcess(PROCESSID_SYRC);
    }
}

void ProcessLogo::draw() const
{
    GLDeviceHelper::RenderNewFrame newFrame;
    if(auto texPtr = g_progUseDB->retrieve(0X00000000)){
        const auto c = to_u8(to_dround(255 * colorRatio()));
        const GLDeviceHelper::EnableTextureModColor modColor(texPtr, colorf::RGBA(c, c, c, 0XFF));

        const auto winW = g_glDevice->getRendererWidth();
        const auto winH = g_glDevice->getRendererHeight();
        g_glDevice->drawTexture(texPtr, 0, 0, 0, 0, winW, winH);
    }
}

double ProcessLogo::colorRatio() const
{
    const double fRatio = m_totalTime / m_fullTime;
    if(fRatio < m_timeR1){
        return fRatio / m_timeR1;
    }

    else if(fRatio < m_timeR1 + m_timeR2){
        return 1.0;
    }

    else{
        return 1.0 - (fRatio - m_timeR1 - m_timeR2) / (1.0 - m_timeR1 - m_timeR2);
    }
}

// ===== processsync.cpp =====
#include "log.hpp"
#include "gui_font.hpp"

extern Client *g_client;
extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

ProcessSync::ProcessSync()
    : Process()
    , m_canvas
      {{
          .w = 800,
          .h = 600,
      }}

    , m_barFull
      {{
          .texLoadFunc = [](const Widget *){ return g_progUseDB->retrieve(0X00000002); },
      }}

    , m_bar
      {{
          .x = 112,
          .y = 528,

          .getter = &m_barFull,
          .vr
          {
              [this]{ return m_barFull.w() * m_ratio / 100; },
              [this]{ return m_barFull.h()                ; },
          },

          .parent{&m_canvas},
      }}

    , m_bgImg
      {{
          .texLoadFunc = []{ return g_progUseDB->retrieve(0X00000001); },
          .parent{&m_canvas},
      }}

    , m_barText
      {{
          .dir = DIR_NONE,

          .x = m_bar.dx() + (m_barFull.w() / 2),
          .y = m_bar.dy() + (m_bar    .h() / 2),

          .textFunc = "Connecting...",
          .font
          {
              .id = 1,
              .size = 10,
          },

          .parent{&m_canvas},
      }}
{}

void ProcessSync::processEvent(const MirEvent &event)
{
    switch(event.type){
        case MIR_EVENT_KEY_DOWN:
            {
                if(event.key.key == MIRK_ESCAPE){
                    g_client->requestProcess(PROCESSID_LOGIN);
                }
                break;
            }
        default:
            {
                break;
            }
    }
}

void ProcessSync::update(double fUpdateTime)
{
    if(m_ratio >= 100){
        g_client->requestProcess(PROCESSID_LOGIN);
        return;
    }

    m_ratio += (fUpdateTime > 0.0 ? 1 : 0);
}

void ProcessSync::draw() const
{
    const GLDeviceHelper::RenderNewFrame newFrame;
    m_canvas.drawRoot({});
}

// ===== processlogin.cpp =====
#include "audiodevice.hpp"
#include <cstring>
#include <iostream>
#include <algorithm>

#include "message.hpp"
#include "bgmusicdb.hpp"
#include "buildconfig.hpp"
#include "messagestackboard.hpp"

extern Log *g_mir2xLog;
extern Client *g_client;
extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;
extern AudioDevice *g_audioDevice;
extern BGMusicDB *g_bgmDB;
extern ClientArgParser *g_clientArgParser;

ProcessLogin::ProcessLogin()
	: Process()
        , m_canvas
          {{
              .w = 800,
              .h = 600,
          }}

	, m_button1{{.x = 150, .y = 482, .texIDList{.off = 0X00000005, .on = 0X00000006, .down = 0X00000007}, .onTrigger = [this](Widget *, int){ doCreateAccount (); }, .parent{&m_canvas}}}
	, m_button2{{.x = 352, .y = 482, .texIDList{.off = 0X00000008, .on = 0X00000009, .down = 0X0000000A}, .onTrigger = [this](Widget *, int){ doChangePassword(); }, .parent{&m_canvas}}}
	, m_button3{{.x = 554, .y = 482, .texIDList{.off = 0X0000000B, .on = 0X0000000C, .down = 0X0000000D}, .onTrigger = [this](Widget *, int){ doExit ();          }, .parent{&m_canvas}}}
        , m_button4{{.x = 600, .y = 536, .texIDList{.off = 0X0000000E, .on = 0X0000000F, .down = 0X00000010}, .onTrigger = [this](Widget *, int){ doLogin();          }, .parent{&m_canvas}}}

	, m_idBox
          {{
              .x = 159,
              .y = 540,

              .w = 146,
              .h =  18,

              .font
              {
                  .id = 2,
                  .size = 18,
              },

              .onTab = [this]
              {
                  m_passwordBox.setFocus(true);
                  m_idBox      .setFocus(false);
              },

              .onCR = [this]
              {
                  doLogin();
              },

              .parent{&m_canvas},
          }}

	, m_passwordBox
          {{
              .x = 409,
              .y = 540,

              .w = 146,
              .h =  18,

              .font
              {
                  .id = 2,
                  .size = 18,
              },

              .onTab = [this]
              {
                  m_idBox      .setFocus(true);
                  m_passwordBox.setFocus(false);
              },

              .onCR = [this]
              {
                  doLogin();
              },

              .parent{&m_canvas},
          }}

    , m_buildSignature
      {{
          .textFunc = [](const Widget *)
          {
              return str_printf("编译版本号:%s", getBuildSignature());
          },

          .font
          {
              .id = 1,
              .size = 14,
              .color = colorf::YELLOW_A255,
          },

          .parent{&m_canvas},
      }}

    , m_notifyBoardBg
      {{
          .drawFunc = [](const Widget *self, int drawDstX, int drawDstY)
          {
              g_glDevice->fillRectangle(colorf::RGBA(0, 0,   0, 128), drawDstX, drawDstY, self->w(), self->h(), 8);
              g_glDevice->drawRectangle(colorf::RGBA(0, 0, 255, 128), drawDstX, drawDstY, self->w(), self->h(), 8);
          },

          .parent{&m_canvas},
      }}

    , m_notifyBoard
      {{
          .dir = DIR_NONE,
          .x = [this]{ return m_canvas.w() / 2; },
          .y = [this]{ return m_canvas.h() / 2; },
          .width = 0,
          .font
          {
              .id = 1,
              .size = 15,
              .color = colorf::YELLOW_A255,
          },
          .showTime = 5000,
          .entryLimit = 10,
          .align = ItemAlign::CENTER,
          .parent{&m_canvas},
      }}
{
    m_notifyBoard  .setShow([this]{ return !m_notifyBoard.empty(); });
    m_notifyBoardBg.setShow([this]{ return !m_notifyBoard.empty(); });

    m_notifyBoardBg.moveAt(DIR_UPLEFT, [this]{ return m_notifyBoard.dx() - 10; }, [this]{ return m_notifyBoard.dy() - 10; });
    m_notifyBoardBg.setSize(           [this]{ return m_notifyBoard. w() + 20; }, [this]{ return m_notifyBoard. h() + 20; });

    g_audioDevice->playBGM(g_bgmDB->retrieve(0X00040007));
    if(g_clientArgParser->autoLogin.has_value()){
        sendLogin(g_clientArgParser->autoLogin.value().first, g_clientArgParser->autoLogin.value().second);
    }
}

void ProcessLogin::update(double fUpdateTime)
{
    m_canvas.update(fUpdateTime);
}

void ProcessLogin::draw() const
{
    GLDeviceHelper::RenderNewFrame newFrame;
    g_glDevice->drawTexture(g_progUseDB->retrieve(0X00000003),   0,  75);
    g_glDevice->drawTexture(g_progUseDB->retrieve(0X00000004),   0, 465);
    g_glDevice->drawTexture(g_progUseDB->retrieve(0X00000011), 103, 536);

    m_canvas.drawRoot({});
}

void ProcessLogin::processEvent(const MirEvent &event)
{
    switch(event.type){
        case MIR_EVENT_KEY_DOWN:
            {
                switch(event.key.key){
                    case MIRK_TAB:
                        {
                            if(true
                                    && !m_idBox      .focus()
                                    && !m_passwordBox.focus()){

                                m_idBox      .setFocus(true);
                                m_passwordBox.setFocus(false);
                                return;
                            }
                        }
                    default:
                        {
                            break;
                        }
                }
            }
        default:
            {
                break;
            }
    }

    m_canvas.processEventRoot(event, true, {});
}

void ProcessLogin::doLogin()
{
    const auto idStr  = m_idBox.getRawString();
    const auto pwdStr = m_passwordBox.getPasswordString();

    if(idStr.empty() || pwdStr.empty()){
        m_notifyBoard.addMessage(u8"无效的账号或密码");
    }
    else{
        // don't check id/password by idstf functions
        // this allows some test account like: (test, 123456)
        // but when creating account, changing password we need to be extremely careful

        sendLogin(idStr, pwdStr);
    }
}

void ProcessLogin::doCreateAccount()
{
    g_client->requestProcess(PROCESSID_CREATEACCOUNT);
}

void ProcessLogin::doChangePassword()
{
    g_client->requestProcess(PROCESSID_CHANGEPASSWORD);
}

void ProcessLogin::doExit()
{
    std::exit(0);
}

void ProcessLogin::sendLogin(const std::string &id, const std::string &password)
{
    CMLogin cmL;
    std::memset(&cmL, 0, sizeof(cmL));

    cmL.id.assign(id);
    cmL.password.assign(password);
    g_client->send({CM_LOGIN, cmL});
}

// ===== processloginnet.cpp =====
#include "fflerror.hpp"

extern Client *g_client;
void ProcessLogin::on_SM_LOGINOK(const uint8_t *, size_t)
{
    g_client->requestProcess(PROCESSID_SELECTCHAR);
}

void ProcessLogin::on_SM_LOGINERROR(const uint8_t *buf, size_t)
{
    const auto smLE = ServerMsg::conv<SMLoginError>(buf);
    switch(smLE.error){
        case LOGINERR_NOACCOUNT:
            {
                m_notifyBoard.addMessage(u8"无效的账号或密码");
                return;
            }
        case LOGINERR_MULTILOGIN:
            {
                m_notifyBoard.addMessage(u8"该账号已经登录");
                return;
            }
        default:
            {
                throw fflreach();
            }
    }
}

// ===== processcreateaccount.cpp =====
#include <regex>

#include "idstrf.hpp"

extern Client *g_client;
extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

ProcessCreateAccount::ProcessCreateAccount()
    : Process()
    , m_LBID        {{.label = u8"账号"    , .font{.size = 15}}}
    , m_LBPwd       {{.label = u8"密码"    , .font{.size = 15}}}
    , m_LBPwdConfirm{{.label = u8"确认密码", .font{.size = 15}}}
    , m_boxID
      {{
          .dir = DIR_LEFT,

          .x = m_x + 129 + 6, // offset + start of box in gfx + offset for input char
          .y = m_y +  85,

          .w = 186,
          .h =  28,

          .font
          {
              .id = 2,
              .size = 15,
          },

          .onTab = [this]
          {
              m_boxID        .setFocus(false);
              m_boxPwd       .setFocus(true );
              m_boxPwdConfirm.setFocus(false);
          },

          .onCR = [this]
          {
              doPostAccount();
          },
      }}

    , m_boxPwd
      {{
          .dir = DIR_LEFT,

          .x = m_x + 129 + 6,
          .y = m_y + 143,

          .w = 186,
          .h =  28,

          .font
          {
              .id = 2,
              .size = 15,
          },

          .onTab = [this]
          {
              m_boxID        .setFocus(false);
              m_boxPwd       .setFocus(false);
              m_boxPwdConfirm.setFocus(true );
          },

          .onCR = [this]
          {
              doPostAccount();
          },
      }}

    , m_boxPwdConfirm
      {{
          .dir = DIR_LEFT,

          .x = m_x + 129 + 6,
          .y = m_y + 198,

          .w = 186,
          .h =  28,

          .font
          {
              .id = 2,
              .size = 15,
          },

          .onTab = [this]
          {
              m_boxID        .setFocus(true );
              m_boxPwd       .setFocus(false);
              m_boxPwdConfirm.setFocus(false);
          },

          .onCR = [this]
          {
              doPostAccount();
          },
      }}

    , m_LBCheckID        {{.font{.id = 0, .size = 15, .color = colorf::RGBA(0xFF, 0X00, 0X00, 0XFF)}}}
    , m_LBCheckPwd       {{.font{.id = 0, .size = 15, .color = colorf::RGBA(0xFF, 0X00, 0X00, 0XFF)}}}
    , m_LBCheckPwdConfirm{{.font{.id = 0, .size = 15, .color = colorf::RGBA(0xFF, 0X00, 0X00, 0XFF)}}}

    , m_submit
      {{
          .x = m_x + 189,
          .y = m_y + 233,

          .texIDList
          {
              .on   = 0X0800000B,
              .down = 0X0800000C,
          },

          .onTrigger = [this](Widget *, int)
          {
              doPostAccount();
          },

          .radioMode = true,
      }}

    , m_quit
      {{
          .x = m_x + 400,
          .y = m_y + 267,

          .texIDList
          {
              .on   = 0X0000001C,
              .down = 0X0000001D,
          },

          .onTrigger = [this](Widget *, int)
          {
              doExit();
          },

          .radioMode = true,
      }}

    , m_infoStr
      {{
          .dir = DIR_NONE,
          .x = 400,
          .y = 190,

          .font
          {
              .id = 1,
              .size = 15,
              .color = colorf::YELLOW_A255,
          },
      }}
{
    m_boxID.setFocus(true);
    m_boxPwd.setFocus(false);
    m_boxPwdConfirm.setFocus(false);
}

void ProcessCreateAccount::update(double fUpdateTime)
{
    m_boxID        .update(fUpdateTime);
    m_boxPwd       .update(fUpdateTime);
    m_boxPwdConfirm.update(fUpdateTime);
}

void ProcessCreateAccount::draw() const
{
    const GLDeviceHelper::RenderNewFrame newFrame;
    g_glDevice->drawTexture(g_progUseDB->retrieve(0X00000003), 0, 75);
    g_glDevice->drawTexture(g_progUseDB->retrieve(0X00000004), 0, 75, 0, 0, 800, 450);

    m_boxID.drawRoot({});
    m_boxPwd.drawRoot({});
    m_boxPwdConfirm.drawRoot({});
    g_glDevice->drawTexture(g_progUseDB->retrieve(0X0A000000), m_x, m_y);

    const auto fnDrawInput = [](int x, int y, int dx, auto &title, auto &check)
    {
        //           (x, y)
        // +-------+  +-------------+  +-------+
        // |       |  |             |  |       |
        // | title |  x anhong      |  | check |
        // |       |  |             |  |       |
        // +-------+  +-------------+  +-------+
        //      -->|  |<--  186  -->|  |<--
        //          dx               dx

        title.draw({.dir=DIR_RIGHT, .x{x - dx      }, .y=y});
        check.draw({.dir=DIR_LEFT , .x{x + dx + 186}, .y=y});
    };

    fnDrawInput(m_x + 129, m_y +  85, 10, m_LBID        , m_LBCheckID        );
    fnDrawInput(m_x + 129, m_y + 143, 10, m_LBPwd       , m_LBCheckPwd       );
    fnDrawInput(m_x + 129, m_y + 198, 10, m_LBPwdConfirm, m_LBCheckPwdConfirm);

    m_submit.drawRoot({});
    m_quit  .drawRoot({});

    if(hasInfo()){
        g_glDevice->fillRectangle(colorf::BLUE + colorf::A_SHF(32), 0, 75, 800, 450);
        m_infoStr.drawRoot({});
    }
}

void ProcessCreateAccount::processEvent(const MirEvent &event)
{
    if(m_quit.processEventRoot(event, true, {})){
        return;
    }

    if(hasInfo()){
        g_glDevice->flushEvent(MIR_EVENT_KEY_DOWN);
        return;
    }

    if(m_submit.processEventRoot(event, true, {})){
        return;
    }

    switch(event.type){
        case MIR_EVENT_KEY_DOWN:
            {
                switch(event.key.key){
                    case MIRK_TAB:
                        {
                            Widget * boxPtrList[]
                            {
                                &m_boxID,
                                &m_boxPwd,
                                &m_boxPwdConfirm,
                            };

                            for(size_t i = 0; i < std::extent_v<decltype(boxPtrList)>; ++i){
                                if(boxPtrList[i]->focus()){
                                    for(size_t j = 0; j < std::extent_v<decltype(boxPtrList)>; ++j){
                                        boxPtrList[j]->setFocus(j == ((i + 1) % std::extent_v<decltype(boxPtrList)>));
                                    }
                                    break;
                                }
                            }
                            return;
                        }
                    default:
                        {
                            break;
                        }
                }
            }
        default:
            {
                break;
            }
    }

    // widget idbox and pwdbox are not independent from each other
    // tab in one box will grant focus to another

    m_boxID        .processEventRoot(event, true, {});
    m_boxPwd       .processEventRoot(event, true, {});
    m_boxPwdConfirm.processEventRoot(event, true, {});

    localCheck();
}

void ProcessCreateAccount::doExit()
{
    g_client->requestProcess(PROCESSID_LOGIN);
}

void ProcessCreateAccount::doPostAccount()
{
    const auto idStr = m_boxID.getRawString();
    const auto pwdStr = m_boxPwd.getPasswordString();
    const auto pwdConfirmStr = m_boxPwdConfirm.getPasswordString();

    if(!idstrf::isEmail(idStr.c_str())){
        setInfoStr(u8"无效账号", 2);
        clearInput();
        return;
    }

    if(!idstrf::isPassword(pwdStr.c_str())){
        setInfoStr(u8"无效密码", 2);
        m_boxPwd.clear();
        m_boxPwdConfirm.clear();
        return;
    }

    if(pwdStr != pwdConfirmStr){
        setInfoStr(u8"两次密码输入不一致", 2);
        m_boxPwd.clear();
        m_boxPwdConfirm.clear();
        return;
    }

    setInfoStr(u8"提交中");

    CMCreateAccount cmCA;
    std::memset(&cmCA, 0, sizeof(cmCA));

    cmCA.id.assign(idStr);
    cmCA.password.assign(pwdStr);
    g_client->send({CM_CREATEACCOUNT, cmCA});
}

void ProcessCreateAccount::localCheck()
{
    const auto idStr = m_boxID.getRawString();
    const auto pwdStr = m_boxPwd.getPasswordString();
    const auto pwdConfirmStr = m_boxPwdConfirm.getPasswordString();

    const auto fnCheckInput = [](const std::string &s, auto &check, bool good)
    {
        if(s.empty()){
            check.setText(u8"");
        }
        else if(good){
            check.loadXML(to_cstr(str_printf(u8"<par><t color=\"green\">√</t></par>").c_str()));
        }
        else{
            check.loadXML(to_cstr(str_printf(u8"<par><t color=\"red\">×</t></par>").c_str()));
        }
    };

    fnCheckInput(idStr, m_LBCheckID, idstrf::isEmail(idStr.c_str()));
    fnCheckInput(pwdStr, m_LBCheckPwd, idstrf::isPassword(pwdStr.c_str()));
    fnCheckInput(pwdConfirmStr, m_LBCheckPwdConfirm, idstrf::isPassword(pwdConfirmStr.c_str()) && pwdStr == pwdConfirmStr);
}

void ProcessCreateAccount::clearInput()
{
    m_boxID.clear();
    m_boxPwd.clear();
    m_boxPwdConfirm.clear();
}

bool ProcessCreateAccount::hasInfo() const
{
    if(m_infoStr.empty()){
        return false;
    }

    if(m_infoStrSec == 0){
        return true;
    }
    return m_infoStrTimer.diff_sec() < m_infoStrSec;
}

void ProcessCreateAccount::setInfoStr(const char8_t *s)
{
    m_infoStr.setText(str_haschar(s) ? s : u8"");
    m_infoStrSec = 0;
}

void ProcessCreateAccount::setInfoStr(const char8_t *s, uint32_t sec)
{
    m_infoStr.setText(str_haschar(s) ? s : u8"");
    m_infoStrSec = sec;
    m_infoStrTimer.reset();
}

// ===== processcreateaccountnet.cpp =====
#include <cstdint>
#include "servermsg.hpp"

void ProcessCreateAccount::on_SM_CREATEACCOUNTOK(const uint8_t *, size_t)
{
    setInfoStr(u8"注册成功", 2);
    m_boxID.setFocus(false);
    m_boxPwd.setFocus(false);
    m_boxPwdConfirm.setFocus(false);
}

void ProcessCreateAccount::on_SM_CREATEACCOUNTERROR(const uint8_t *buf, size_t)
{
    const auto smCAE = ServerMsg::conv<SMCreateAccountError>(buf);
    switch(smCAE.error){
        case CRTACCERR_ACCOUNTEXIST:
            {
                setInfoStr(u8"账号已存在", 2);
                clearInput();

                m_boxID.setFocus(true);
                m_boxPwd.setFocus(false);
                m_boxPwdConfirm.setFocus(false);
                return;
            }
        case CRTACCERR_BADACCOUNT:
            {
                setInfoStr(u8"无效的账号", 2);
                clearInput();

                m_boxID.setFocus(true);
                m_boxPwd.setFocus(false);
                m_boxPwdConfirm.setFocus(false);
                return;
            }
        case CRTACCERR_BADPASSWORD:
            {
                setInfoStr(u8"无效的密码", 2);
                clearInput();

                m_boxID.setFocus(true);
                m_boxPwd.setFocus(false);
                m_boxPwdConfirm.setFocus(false);
                return;
            }
        default:
            {
                throw fflreach();
            }
    }
}

// ===== processselectchar.cpp =====
#include "jobf.hpp"
#include "pathf.hpp"
#include "soundeffectdb.hpp"
#include "layoutboard.hpp"

extern Log *g_mir2xLog;
extern Client *g_client;
extern BGMusicDB *g_bgmDB;
extern SoundEffectDB *g_seffDB;
extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;
extern AudioDevice *g_audioDevice;
extern PNGTexOffDB *g_selectCharDB;

ProcessSelectChar::ProcessSelectChar()
    : Process()
    , m_canvas
      {{
          .w = 800,
          .h = 600,
      }}

    , m_start {{ .x = 335, .y =  75, .texIDList{.off = 0X0C000030, .on = 0X0C000030, .down = 0X0C000031}, .onTrigger = [this](Widget *, int){ onStart (); }, .parent{&m_canvas}}}
    , m_create{{ .x = 565, .y = 130, .texIDList{.off = 0X0C000010, .on = 0X0C000010, .down = 0X0C000011}, .onTrigger = [this](Widget *, int){ onCreate(); }, .parent{&m_canvas}}}
    , m_delete{{ .x = 110, .y = 305, .texIDList{.off = 0X0C000020, .on = 0X0C000020, .down = 0X0C000021}, .onTrigger = [this](Widget *, int){ onDelete(); }, .parent{&m_canvas}}}
    , m_exit  {{ .x =  45, .y = 544, .texIDList{.off = 0X0C000040, .on = 0X0C000040, .down = 0X0C000041}, .onTrigger = [this](Widget *, int){ onExit  (); }, .parent{&m_canvas}}}

    , m_notifyBoardBg
      {{
          .drawFunc = [](const Widget *self, int drawDstX, int drawDstY)
          {
              g_glDevice->fillRectangle(colorf::RGBA(0, 0,   0, 128), drawDstX, drawDstY, self->w(), self->h(), 8);
              g_glDevice->drawRectangle(colorf::RGBA(0, 0, 255, 128), drawDstX, drawDstY, self->w(), self->h(), 8);
          },

          .parent{&m_canvas},
      }}

    , m_notifyBoard
      {{
          .dir = DIR_NONE,
          .x = [this]{ return m_canvas.w() / 2; },
          .y = [this]{ return m_canvas.h() / 2; },
          .width = 0,
          .font
          {
              .id = 1,
              .size = 15,
              .color = colorf::YELLOW_A255,
          },
          .showTime = 5000,
          .entryLimit = 1,
          .align = ItemAlign::CENTER,
          .parent{&m_canvas},
      }}

    , m_deleteInput
      {
          DIR_NONE,
          400,
          300,

          true,
      }
{
    m_start .setShow([this]{ return hasChar(); });
    m_create.setShow([this]{ return hasChar(); });
    m_delete.setShow([this]{ return hasChar(); });

    m_notifyBoard.addMessage(u8"正在下载游戏角色");

    g_client->send(CM_QUERYCHAR);
    g_audioDevice->playBGM(g_bgmDB->retrieve(0X00040002));
}

ProcessSelectChar::~ProcessSelectChar()
{
    g_audioDevice->stopBGM();
    g_audioDevice->stopSoundEffect();
}

void ProcessSelectChar::update(double fUpdateTime)
{
    m_charAniTime += fUpdateTime;
    m_canvas.update(fUpdateTime);

    if(hasChar()){
        switchCharGfx();
    }
}

void ProcessSelectChar::draw() const
{
    GLDeviceHelper::RenderNewFrame newFrame;
    if(auto texPtr = g_progUseDB->retrieve(0X0C000000)){
        g_glDevice->drawTexture(texPtr, 0, 0);
    }

    m_canvas.drawRoot({});

    if(hasChar()){
        drawChar();
        drawCharName();
    }

    if(m_deleteInput.show()){
        m_deleteInput.drawRoot({});
    }
}

void ProcessSelectChar::processEvent(const MirEvent &event)
{
    bool tookEvent = false;

    tookEvent |= m_deleteInput.processEventRoot(event, !tookEvent, {});
    tookEvent |= m_canvas     .processEventRoot(event, !tookEvent, {});

    if(!tookEvent){
    }
}

void ProcessSelectChar::onStart()
{
    if(m_smChar.has_value()){
        if(m_smChar.value().name.empty()){
            m_notifyBoard.addMessage(u8"请先创建游戏角色");
        }
        else{
            g_client->send(CM_ONLINE);
        }
    }
    else{
        m_notifyBoard.addMessage(u8"正在下载游戏角色");
    }
}

void ProcessSelectChar::onCreate()
{
    if(m_smChar.has_value()){
        if(m_smChar.value().name.empty()){
            g_client->requestProcess(PROCESSID_CREATECHAR);
        }
        else{
            m_notifyBoard.addMessage(u8"一个账号只能创建一个游戏角色");
        }
    }
    else{
        m_notifyBoard.addMessage(u8"正在下载游戏角色");
    }
}

void ProcessSelectChar::onDelete()
{
    if(hasChar()){
        m_deleteInput.setShow(true);
        m_deleteInput.waitInput(u8"<layout><par>删除的角色将无法还原，请谨慎操作。如果确定删除，请输入游戏密码，并点击YES。</par></layout>", true, [this](std::u8string inputString)
        {
            CMDeleteChar cmDC;
            std::memset(&cmDC, 0, sizeof(cmDC));
            if(inputString.empty() || inputString.size() > SYS_PWDSIZE){
                m_notifyBoard.addMessage(u8"无效的密码");
            }
            else{
                cmDC.password.assign(inputString);
                g_client->send({CM_DELETECHAR, cmDC});
            }
            m_deleteInput.setShow(false);
        });
    }
    else{
        m_notifyBoard.addMessage(u8"此账号没有角色");
    }
}

void ProcessSelectChar::onExit()
{
    g_client->requestProcess(PROCESSID_SELECTCHAR);
}

void ProcessSelectChar::drawCharName() const
{
    if(m_smChar.has_value() && !m_smChar.value().name.empty()){
        const auto exp = m_smChar.value().exp;
        const auto name = m_smChar.value().name.to_str();

        fflassert(str_haschar(name));

        std::u8string xmlStr;
        xmlStr += str_printf(u8R"###( <layout> )###""\n");
        xmlStr += str_printf(u8R"###(     <par color='RGB(237,226,200)'>角色：%s</par> )###""\n", to_cstr(name));
        xmlStr += str_printf(u8R"###(     <par color='RGB(175,196,175)'>等级：%d</par> )###""\n", to_d(SYS_LEVEL(exp)));
        for(const auto jobStr: jobf::jobName(m_smChar.value().job)){
            xmlStr += str_printf(u8R"###( <par color='RGB(231,231,189)'>职业：%s</par> )###""\n", to_cstr(jobStr));
        }
        xmlStr += str_printf(u8R"###( </layout> )###""\n");

        const LayoutBoard charBoard
        {{
            .lineWidth = 200,
            .initXML = to_cstr(xmlStr),
            .font
            {
                .id = 1,
                .size = 15,
            },
        }};

        const int drawBoardX = 120;
        const int drawBoardY = 260 - charBoard.h();
        const int drawBoardMargin = 15;

        g_glDevice->fillRectangle(colorf::RGBA(  0,   0,   0, 128), drawBoardX - drawBoardMargin, drawBoardY - drawBoardMargin, charBoard.w() + drawBoardMargin * 2, charBoard.h() + drawBoardMargin * 2, 5);
        g_glDevice->drawRectangle(colorf::RGBA(231, 231, 189, 128), drawBoardX - drawBoardMargin, drawBoardY - drawBoardMargin, charBoard.w() + drawBoardMargin * 2, charBoard.h() + drawBoardMargin * 2, 5);
        charBoard.draw({.x=drawBoardX, .y=drawBoardY});
    }
}

uint32_t ProcessSelectChar::charFrameCount() const
{
    if(m_smChar.has_value() && !m_smChar.value().name.empty()){
        const bool gender = m_smChar.value().gender;
        const int firstJob = jobf::firstJob(m_smChar.value().job);

        fflassert(jobf::jobValid(firstJob), firstJob);
        fflassert(m_charAni >= 0);
        fflassert(m_charAni <  4);

        // motion:
        // 0 : stand, inactive
        // 1 : on select
        // 2 : stand, active
        // 3 : on deselect
        // 4 : on create char in ProcessCreateChar, not used here

        // (job, gender, motion) -> frameCount
        const static std::map<std::tuple<int, bool, int>, uint32_t> s_frameCount
        {
            #include "selectcharframecount.inc"
        };

        if(auto p = s_frameCount.find({firstJob, gender, m_charAni}); p != s_frameCount.end()){
            return p->second;
        }
    }
    return 0;
}

std::optional<uint32_t> ProcessSelectChar::charGfxBaseID() const
{
    if(m_smChar.has_value() && !m_smChar.value().name.empty()){
        const bool gender = m_smChar.value().gender;
        const auto jobIndexList = jobf::jobGfxIndex(m_smChar.value().job);

        fflassert(!jobIndexList.empty());
        fflassert(m_charAni >= 0);
        fflassert(m_charAni <  4);

        // 14     : max =  2   shadow
        // 13     : max =  2   magic
        // 10 - 12: max =  8   job
        // 09     : max =  2   gender: male as true
        // 05 - 08: max = 16   motion
        // 00 - 04: max = 32   frame

        return 0
            + (to_u32(jobIndexList.front()) << 10)
            + (to_u32(gender             ) <<  9)
            + (to_u32(m_charAni          ) <<  5);
    }
    return {};
}

void ProcessSelectChar::drawChar() const
{
    if(m_smChar.has_value() && !m_smChar.value().name.empty()){
        const auto frameCount = charFrameCount();
        if(frameCount <= 0){
            return;
        }

        const auto gfxIDOpt = charGfxBaseID();
        if(!gfxIDOpt.has_value()){
            return;
        }

        const uint32_t shadowMask = to_u32(1) << 14;
        const uint32_t  magicMask = to_u32(1) << 13;
        const uint32_t frameIndex = gfxIDOpt.value() + absFrame() % frameCount;

        const auto fnDrawTexture = [](uint32_t texIndex, bool alpha = false) -> bool
        {
            constexpr int drawX = 430;
            constexpr int drawY = 300;

            if(const auto [texPtr, dx, dy] = g_selectCharDB->retrieve(texIndex); texPtr){
                GLDeviceHelper::EnableTextureModColor enableModColor(texPtr, colorf::WHITE + colorf::A_SHF(alpha ? 150 : 255));
                g_glDevice->drawTexture(texPtr, drawX + dx, drawY + dy);
                return true;
            }
            return false;
        };

        if(fnDrawTexture(frameIndex)){
            fnDrawTexture(frameIndex | shadowMask, true);
            fnDrawTexture(frameIndex);
            fnDrawTexture(frameIndex | magicMask);
        }
    }
}

void ProcessSelectChar::switchCharGfx()
{
    // switch the char animation to show all gfx
    // since for mir2x I only allow 1 char per account, so there is no select/deselect

    if(!hasChar()){
        return;
    }

    const auto frameCount = charFrameCount();
    if(frameCount <= 0){
        return;
    }

    const auto absFrameIndex = absFrame();
    if(absFrameIndex % frameCount){
        return;
    }

    // can switch now
    // but if current frame already did, we skip

    if(m_charAniSwitchFrame == absFrameIndex){
        return;
    }

    m_charAniSwitchFrame = absFrameIndex;
    switch(m_charAni){
        case 0:
            {
                if(mathf::rand<int>(0, 1) == 0){
                    m_charAni = 1;
                }
                break;
            }
        case 1:
            {
                m_charAni = 2;
                break;
            }
        case 2:
            {
                if(mathf::rand<int>(0, 1) == 0){
                    m_charAni = 3;
                }
                break;
            }
        case 3:
            {
                m_charAni = 0;
                break;
            }
        default:
            {
                throw fflreach();
            }
    }

    if(m_charAni == 1){
        const int offGender = to_d(m_smChar.value().gender);
        const auto jobIndexList = jobf::jobGfxIndex(m_smChar.value().job);

        fflassert(!jobIndexList.empty());
        const int offJob = jobIndexList.front();

        const uint32_t seffID = UINT32_C(0X00010000) // base
            | (to_u32(offGender) << 4)               //
            | (to_u32(offJob   ) << 8)               //
            | (to_u32(1        ) << 0);              // 0 for create, 1 for select
        g_audioDevice->playSoundEffect(g_seffDB->retrieve(seffID));
    }
}

// ===== processselectcharnet.cpp =====
#include "uidf.hpp"
#include "dbcomid.hpp"

extern Client *g_client;
extern ClientArgParser *g_clientArgParser;

void ProcessSelectChar::on_SM_QUERYCHAROK(const uint8_t *buf, size_t)
{
    m_smChar = ServerMsg::conv<SMQueryCharOK>(buf);
    fflassert(m_smChar.value().name.size > 0);
    m_notifyBoard.clear();

    if(m_smChar.value().name.empty()){
        m_notifyBoard.addMessage(u8"请先创建游戏角色");
    }
    else if(g_clientArgParser->autoLogin){
        onStart();
    }
}

void ProcessSelectChar::on_SM_QUERYCHARERROR(const uint8_t *buf, size_t)
{
    const auto smQCE = ServerMsg::conv<SMQueryCharError>(buf);
    switch(smQCE.error){
        case QUERYCHARERR_NOCHAR:
            {
                m_smChar.emplace();
                m_smChar.value().name.clear();

                m_notifyBoard.addMessage(u8"请先创建游戏角色");
                break;
            }
        case QUERYCHARERR_NOLOGIN:
        default:
            {
                throw fflvalue(smQCE.error);
            }
    }
}

void ProcessSelectChar::on_SM_DELETECHAROK(const uint8_t *, size_t)
{
    m_smChar.emplace();
    m_smChar.value().name.clear();
    m_notifyBoard.addMessage(u8"删除角色成功");
}

void ProcessSelectChar::on_SM_DELETECHARERROR(const uint8_t *buf, size_t)
{
    const auto smDCE = ServerMsg::conv<SMDeleteCharError>(buf);
    switch(smDCE.error){
        case DELCHARERR_BADPASSWORD:
            {
                m_notifyBoard.addMessage(u8"密码错误");
                return;
            }
        case DELCHARERR_NOCHAR:
            {
                m_notifyBoard.addMessage(u8"没有角色可以删除");
                return;
            }
        case DELCHARERR_DBERROR:
            {
                m_notifyBoard.addMessage(u8"删除角色失败，请稍后重试");
                return;
            }
        default:
            {
                throw fflreach();
            }
    }
}

void ProcessSelectChar::on_SM_ONLINEOK(const uint8_t *buf, size_t)
{
    const auto smOOK = ServerMsg::conv<SMOnlineOK>(buf);
    fflassert(uidf::isPlayer(smOOK.uid), smOOK.uid, uidf::getUIDString(smOOK.uid));
    fflassert(DBCOM_MAPRECORD(uidf::getMapID(smOOK.mapUID)), smOOK.mapUID);

    g_client->setOnlineOK(smOOK);
    g_client->requestProcess(PROCESSID_RUN);
}

void ProcessSelectChar::on_SM_ONLINEERROR(const uint8_t *buf, size_t)
{
    const auto smOE = ServerMsg::conv<SMOnlineError>(buf);
    switch(smOE.error){
        case ONLINEERR_NOCHAR:
            {
                m_notifyBoard.addMessage(u8"先创建角色以运行游戏");
                return;
            }
        case ONLINEERR_MULTIONLINE:
            {
                m_notifyBoard.addMessage(u8"请勿频繁登录");
                return;
            }
        default:
            {
                throw fflvalue(smOE.error);
            }
    }
}

// ===== processcreatechar.cpp =====
#include "mathf.hpp"
#include "imeboard.hpp"

extern Client *g_client;
extern GLDevice *g_glDevice;
extern AudioDevice *g_audioDevice;
extern IMEBoard *g_imeBoard;
extern BGMusicDB *g_bgmDB;
extern SoundEffectDB *g_seffDB;
extern PNGTexDB *g_progUseDB;
extern PNGTexOffDB *g_selectCharDB;
extern ClientArgParser *g_clientArgParser;

ProcessCreateChar::ProcessCreateChar()
    : Process()
    , m_warrior{{.x = 339, .y = 539, .texIDList{.off = 0X0D000030, .on = 0X0D000031, .down = 0X0D000032}, .onTrigger = [this](Widget *, int){ m_job = JOB_WARRIOR; }}}
    , m_wizard {{.x = 381, .y = 539, .texIDList{.off = 0X0D000040, .on = 0X0D000041, .down = 0X0D000042}, .onTrigger = [this](Widget *, int){ m_job = JOB_WIZARD ; }}}
    , m_taoist {{.x = 424, .y = 539, .texIDList{.off = 0X0D000050, .on = 0X0D000051, .down = 0X0D000052}, .onTrigger = [this](Widget *, int){ m_job = JOB_TAOIST ; }}}
    , m_submit {{.x = 512, .y = 549, .texIDList{.off = 0X0D000010, .on = 0X0D000011, .down = 0X0D000012}, .onTrigger = [this](Widget *, int){ onSubmit();          }}}
    , m_exit   {{.x = 554, .y = 549, .texIDList{.off = 0X0D000020, .on = 0X0D000021, .down = 0X0D000022}, .onTrigger = [this](Widget *, int){ onExit  ();          }}}

    , m_nameBox
      {{
          .x = 355,
          .y = 520,

          .w = 85,
          .h = 15,

          .enableIME = IME_SYSTEM,
          .font
          {
              .id = 1,
              .size = 12,
          },

          .onCR = [this]
          {
              onSubmit();
          },
      }}

    , m_notifyBoard
      {{
          .dir = DIR_UPLEFT,
          .x = 0,
          .y = 0,
          .width = 0,
          .font
          {
              .id = 1,
              .size = 15,
              .color = colorf::YELLOW + colorf::A_SHF(255),
          },
          .showTime = 5000,
          .entryLimit = 10,
          .align = ItemAlign::CENTER,
      }}
{
    g_audioDevice->playBGM(g_bgmDB->retrieve(0X00040001));
    g_imeBoard->dropFocus();
}

ProcessCreateChar::~ProcessCreateChar()
{
    g_audioDevice->stopBGM();
    g_audioDevice->stopSoundEffect();
}

void ProcessCreateChar::update(double fUpdateTime)
{
    m_aniTime += fUpdateTime;
    m_notifyBoard.update(fUpdateTime);
    g_imeBoard->update(fUpdateTime);

    if(const uint32_t frameCount = charFrameCount(m_job, m_activeGender); frameCount > 0){
        if(const auto currAbsFrame = absFrame(); ((currAbsFrame % frameCount) == 0) && (m_lastStartAbsFrame != currAbsFrame)){
            playMagicSoundEffect();
            m_lastStartAbsFrame = currAbsFrame;
        }
    }
}

void ProcessCreateChar::draw() const
{
    GLDeviceHelper::RenderNewFrame newFrame;
    if(auto texPtr = g_progUseDB->retrieve(0X0D000000)){
        g_glDevice->drawTexture(texPtr, 0, 0);
    }

    drawChar( true, 193, 215);
    drawChar(false, 495, 220);

    if(auto texPtr = g_progUseDB->retrieve(0X0D000002)){
        GLDeviceHelper::EnableTextureModColor enableModColor(texPtr, colorf::RGBA(255, 255, 255, 150));
        g_glDevice->drawTexture(texPtr, 268, 555);
    }

    g_glDevice->fillRectangle(colorf::RGBA(  0,   0,   0, 255), 355, 520, 90, 15);
    m_nameBox.drawRoot({});
    g_glDevice->drawRectangle(colorf::RGBA(231, 231, 189, 100), 355, 520, 90, 15);

    if(auto texPtr = g_progUseDB->retrieve(0X0D000001)){
        g_glDevice->drawTexture(texPtr, 320, 500);
    }

    m_warrior.drawRoot({});
    m_wizard .drawRoot({});
    m_taoist .drawRoot({});

    m_submit.drawRoot({});
    m_exit  .drawRoot({});

    g_imeBoard->drawRoot({});

    const int notifX = (800 - m_notifyBoard.w()) / 2;
    const int notifY = (600 - m_notifyBoard. h()) / 2;
    const int margin = 15;

    if(!m_notifyBoard.empty()){
        g_glDevice->fillRectangle(colorf::RGBA(0, 0,   0, 128), notifX - margin, notifY - margin, m_notifyBoard.w() + margin * 2, m_notifyBoard.h() + margin * 2, 8);
        g_glDevice->drawRectangle(colorf::RGBA(0, 0, 255, 128), notifX - margin, notifY - margin, m_notifyBoard.w() + margin * 2, m_notifyBoard.h() + margin * 2, 8);
    }
    m_notifyBoard.draw({.dir=DIR_UPLEFT, .x=notifX, .y=notifY});
}

void ProcessCreateChar::processEvent(const MirEvent &event)
{
    bool tookEvent = false;

    tookEvent |= g_imeBoard->processEventRoot(event, !tookEvent, {});
    tookEvent |= m_warrior  .processEventRoot(event, !tookEvent, {});
    tookEvent |= m_wizard   .processEventRoot(event, !tookEvent, {});
    tookEvent |= m_taoist   .processEventRoot(event, !tookEvent, {});
    tookEvent |= m_submit   .processEventRoot(event, !tookEvent, {});
    tookEvent |= m_exit     .processEventRoot(event, !tookEvent, {});
    tookEvent |= m_nameBox  .processEventRoot(event, !tookEvent, {});

    if(!tookEvent){
        switch(event.type){
        case MIR_EVENT_KEY_DOWN:
            {
                switch(event.key.key){
                    case MIRK_TAB:
                        {
                            m_nameBox.setFocus(true);
                            break;
                        }
                    default:
                        {
                            break;
                        }
                }
                break;
            }
            case MIR_EVENT_MOUSE_BUTTON_DOWN:
                {
                    const auto [px, py] = GLDeviceHelper::getMousePLoc();
                    if(mathf::pointInRectangle(px, py, 200, 290, 90, 260)){
                        m_activeGender = true;
                    }
                    else if(mathf::pointInRectangle(px, py, 510, 310, 90, 260)){
                        m_activeGender = false;
                    }

                    m_aniTime = 0.0;
                    m_lastStartAbsFrame = UINT32_MAX; // force playing sound effect when switching chars
                    break;
                }
            default:
                {
                    break;
                }
        }
    }
}

uint32_t ProcessCreateChar::charFrameCount(int job, bool gender)
{
    fflassert(job >= JOB_BEGIN);
    fflassert(job <  JOB_END  );

    const static std::map<std::tuple<int, bool, int>, uint32_t> s_frameCount
    {
        #include "selectcharframecount.inc"
    };

    if(auto p = s_frameCount.find({job, gender, 4}); p != s_frameCount.end()){
        return p->second;
    }
    return 0;
}

uint32_t ProcessCreateChar::charGfxBaseID(int job, bool gender)
{
    fflassert(job >= JOB_BEGIN);
    fflassert(job <  JOB_END  );

    // 14     : max =  2   shadow
    // 13     : max =  2   magic
    // 10 - 12: max =  8   job
    // 09     : max =  2   gender: male as true
    // 05 - 08: max = 16   motion
    // 00 - 04: max = 32   frame

    return 0
        + (to_u32(job - JOB_BEGIN) << 10)
        + (to_u32(gender         ) <<  9)
        + (to_u32(4              ) <<  5);
}

void ProcessCreateChar::onSubmit()
{
    CMCreateChar cmCC;
    std::memset(&cmCC, 0, sizeof(cmCC));
    const auto nameStr = m_nameBox.getRawString();

    if(nameStr.empty() || nameStr.size() >= cmCC.name.capacity()){
        m_notifyBoard.addMessage(u8"无效的角色名");
    }
    else{
        cmCC.name.assign(nameStr);
        cmCC.job = m_job;
        cmCC.gender = m_activeGender;
        g_client->send({CM_CREATECHAR, cmCC});
        m_notifyBoard.addMessage(u8"提交中");
    }
}

void ProcessCreateChar::onExit()
{
    g_client->requestProcess(PROCESSID_SELECTCHAR);
}

void ProcessCreateChar::setGUIActive(bool active)
{
    m_warrior.setActive(active);
    m_wizard .setActive(active);
    m_taoist .setActive(active);
    m_submit .setActive(active);
    m_exit   .setActive(active);
    m_nameBox.setActive(active);
}

void ProcessCreateChar::drawChar(bool gender, int drawX, int drawY) const
{
    const uint32_t frameCount = charFrameCount(m_job, gender);
    if(frameCount <= 0){
        return;
    }

    const uint32_t shadowMask = to_u32(1) << 14;
    const uint32_t  magicMask = to_u32(1) << 13;

    const bool active = (gender == m_activeGender);
    const uint32_t frameIndex = charGfxBaseID(m_job, gender) + (active ? (absFrame() % frameCount) : 0);

    // TODO hack code
    // seems for female warrior offset-y is mis-aligned
    if(gender == false && m_job == JOB_WARRIOR){
        drawY += 40;
    }

    const auto fnDrawTexture = [drawX, drawY, active](uint32_t texIndex, bool alpha = false) -> bool
    {
        if(const auto [texPtr, dx, dy] = g_selectCharDB->retrieve(texIndex); texPtr){
            const auto modColor = [active, alpha]() -> uint32_t
            {
                if(active){
                    return colorf::RGBA(255, 255, 255, alpha ? 150 : 255);
                }
                else{
                    return colorf::RGBA(128, 128, 128, alpha ? 150 : 255);
                }
            }();

            GLDeviceHelper::EnableTextureModColor enableModColor(texPtr, modColor);
            g_glDevice->drawTexture(texPtr, drawX + dx, drawY + dy);
            return true;
        }
        return false;
    };

    if(fnDrawTexture(frameIndex)){
        fnDrawTexture(frameIndex | shadowMask, true);
        fnDrawTexture(frameIndex);
        fnDrawTexture(frameIndex | magicMask);
    }
}

void ProcessCreateChar::playMagicSoundEffect()
{
    const int offGender = to_d(m_activeGender);
    const int offJob = m_job - JOB_BEGIN;

    const uint32_t seffID = UINT32_C(0X00010000) // base
        | (to_u32(offGender) << 4)               //
        | (to_u32(offJob   ) << 8)               //
        | (to_u32(0        ) << 0);              // 0 for create, 1 for select

    g_audioDevice->playSoundEffect(g_seffDB->retrieve(seffID));
}

// ===== processcreatecharnet.cpp =====

extern Client *g_client;
void ProcessCreateChar::on_SM_CREATECHAROK(const uint8_t *, size_t)
{
    g_client->requestProcess(PROCESSID_SELECTCHAR);
}

void ProcessCreateChar::on_SM_CREATECHARERROR(const uint8_t *buf, size_t)
{
    const auto smCCE = ServerMsg::conv<SMCreateCharError>(buf);
    switch(smCCE.error){
        case CRTCHARERR_BADNAME:
            {
                setGUIActive(true);
                m_notifyBoard.addMessage(u8"无效的角色名");
                break;
            }
        case CRTCHARERR_BADGENDER:
            {
                setGUIActive(true);
                m_notifyBoard.addMessage(u8"无效的角色性别");
                break;
            }
        case CRTCHARERR_BADJOB:
            {
                setGUIActive(true);
                m_notifyBoard.addMessage(u8"无效的角色职业");
                break;
            }
        case CRTCHARERR_CHAREXIST:
            {
                setGUIActive(true);
                m_notifyBoard.addMessage(u8"一个账号只能创建一个角色");
                break;
            }
        case CRTCHARERR_NAMEEXIST:
            {
                setGUIActive(true);
                m_notifyBoard.addMessage(u8"角色名已被使用");
                break;
            }
        default:
            {
                throw fflreach();
            }
    }
}

// ===== processchangepassword.cpp =====

#include "gui_core.hpp"

extern Client *g_client;
extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

ProcessChangePassword::ProcessChangePassword()
	: Process()
    , m_LBID           {{.label = u8"账号"    , .font{.size = 15}}}
    , m_LBPwd          {{.label = u8"密码"    , .font{.size = 15}}}
    , m_LBNewPwd       {{.label = u8"新密码"  , .font{.size = 15}}}
    , m_LBNewPwdConfirm{{.label = u8"确认密码", .font{.size = 15}}}
    , m_boxID
      {{
          .dir = DIR_LEFT,

          .x = m_x + 129 + 6, // offset + start of box in gfx + offset for input char
          .y = m_y +  79,

          .w = 186,
          .h =  28,

          .font
          {
              .id = 2,
              .size = 15,
          },

          .onTab = [this]
          {
              m_boxID           .setFocus(false);
              m_boxPwd          .setFocus(true );
              m_boxNewPwd       .setFocus(true );
              m_boxNewPwdConfirm.setFocus(false);
          },

          .onCR = [this]
          {
              doPostPasswordChange();
          },
      }}

    , m_boxPwd
      {{
          .dir = DIR_LEFT,

          .x = m_x + 129 + 6,
          .y = m_y + 126,

          .w = 186,
          .h =  28,

          .font
          {
              .id = 2,
              .size = 15,
          },

          .onTab = [this]
          {
              m_boxID           .setFocus(false);
              m_boxPwd          .setFocus(false);
              m_boxNewPwd       .setFocus(false);
              m_boxNewPwdConfirm.setFocus(true );
          },

          .onCR = [this]
          {
              doPostPasswordChange();
          },
      }}

    , m_boxNewPwd
      {{
          .dir = DIR_LEFT,

          .x = m_x + 129 + 6,
          .y = m_y + 173,

          .w = 186,
          .h =  28,

          .font
          {
              .id = 2,
              .size = 15,
          },

          .onTab = [this]
          {
              m_boxID           .setFocus(true );
              m_boxPwd          .setFocus(false);
              m_boxNewPwd       .setFocus(false);
              m_boxNewPwdConfirm.setFocus(false);
          },

          .onCR = [this]
          {
              doPostPasswordChange();
          },
      }}

    , m_boxNewPwdConfirm
      {{
          .dir = DIR_LEFT,

          .x = m_x + 129 + 6,
          .y = m_y + 220,

          .w = 186,
          .h =  28,

          .font
          {
              .id = 2,
              .size = 15,
          },

          .onTab = [this]
          {
              m_boxID           .setFocus(true );
              m_boxPwd          .setFocus(false);
              m_boxNewPwd       .setFocus(false);
              m_boxNewPwdConfirm.setFocus(false);
          },

          .onCR = [this]
          {
              doPostPasswordChange();
          },
      }}

    , m_LBCheckID           {{.font{.id = 0, .size = 15, .color = colorf::RGBA(0xFF, 0X00, 0X00, 0XFF)}}}
    , m_LBCheckPwd          {{.font{.id = 0, .size = 15, .color = colorf::RGBA(0xFF, 0X00, 0X00, 0XFF)}}}
    , m_LBCheckNewPwd       {{.font{.id = 0, .size = 15, .color = colorf::RGBA(0xFF, 0X00, 0X00, 0XFF)}}}
    , m_LBCheckNewPwdConfirm{{.font{.id = 0, .size = 15, .color = colorf::RGBA(0xFF, 0X00, 0X00, 0XFF)}}}

    , m_submit
      {{
          .x = m_x + 189,
          .y = m_y + 254,

          .texIDList
          {
              .on   = 0X0800000B,
              .down = 0X0800000C,
          },

          .onTrigger = [this](Widget *, int)
          {
              doPostPasswordChange();
          },

          .radioMode = true,
      }}

    , m_quit
      {{
          .x = m_x + 400,
          .y = m_y + 288,

          .texIDList
          {
              .on   = 0X0000001C,
              .down = 0X0000001D,
          },

          .onTrigger = [this](Widget *, int)
          {
              doExit();
          },

          .radioMode = true,
      }}

    , m_infoStr
      {{
          .dir = DIR_NONE,
          .x = 400,
          .y = 190,

          .font
          {
              .id = 1,
              .size = 15,
              .color = colorf::YELLOW_A255,
          },
      }}
{
    m_boxID.setFocus(true);
    m_boxPwd.setFocus(false);
    m_boxNewPwd.setFocus(false);
    m_boxNewPwdConfirm.setFocus(false);
}

void ProcessChangePassword::update(double fUpdateTime)
{
    m_boxID           .update(fUpdateTime);
    m_boxPwd          .update(fUpdateTime);
    m_boxNewPwd       .update(fUpdateTime);
    m_boxNewPwdConfirm.update(fUpdateTime);
}

void ProcessChangePassword::draw() const
{
    const GLDeviceHelper::RenderNewFrame newFrame;
    g_glDevice->drawTexture(g_progUseDB->retrieve(0X00000003), 0, 75);
    g_glDevice->drawTexture(g_progUseDB->retrieve(0X00000004), 0, 75, 0, 0, 800, 450);

    m_boxID.drawRoot({});
    m_boxPwd.drawRoot({});
    m_boxNewPwd.drawRoot({});
    m_boxNewPwdConfirm.drawRoot({});
    g_glDevice->drawTexture(g_progUseDB->retrieve(0X0A000001), m_x, m_y);

    const auto fnDrawInput = [](int x, int y, int dx, auto &title, auto &check)
    {
        //           (x, y)
        // +-------+  +-------------+  +-------+
        // |       |  |             |  |       |
        // | title |  x anhong      |  | check |
        // |       |  |             |  |       |
        // +-------+  +-------------+  +-------+
        //      -->|  |<--  186  -->|  |<--
        //          dx               dx

        title.draw({.dir=DIR_RIGHT, .x{x - dx      }, .y=y});
        check.draw({.dir=DIR_LEFT , .x{x + dx + 186}, .y=y});
    };

    fnDrawInput(m_x + 129, m_y +  79, 10, m_LBID           , m_LBCheckID           );
    fnDrawInput(m_x + 129, m_y + 126, 10, m_LBPwd          , m_LBCheckPwd          );
    fnDrawInput(m_x + 129, m_y + 173, 10, m_LBNewPwd       , m_LBCheckNewPwd       );
    fnDrawInput(m_x + 129, m_y + 220, 10, m_LBNewPwdConfirm, m_LBCheckNewPwdConfirm);

    m_submit.drawRoot({});
    m_quit.drawRoot({});

    if(hasInfo()){
        g_glDevice->fillRectangle(colorf::BLUE + colorf::A_SHF(32), 0, 75, 800, 450);
        m_infoStr.drawRoot({});
    }
}

void ProcessChangePassword::processEvent(const MirEvent &event)
{
    if(m_quit.processEventRoot(event, true, {})){
        return;
    }

    if(hasInfo()){
        g_glDevice->flushEvent(MIR_EVENT_KEY_DOWN);
        return;
    }

    if(m_submit.processEventRoot(event, true, {})){
        return;
    }

    switch(event.type){
        case MIR_EVENT_KEY_DOWN:
            {
                switch(event.key.key){
                    case MIRK_TAB:
                        {
                            Widget * boxPtrList[]
                            {
                                &m_boxID,
                                &m_boxPwd,
                                &m_boxNewPwd,
                                &m_boxNewPwdConfirm,
                            };

                            for(size_t i = 0; i < std::extent_v<decltype(boxPtrList)>; ++i){
                                if(boxPtrList[i]->focus()){
                                    for(size_t j = 0; j < std::extent_v<decltype(boxPtrList)>; ++j){
                                        boxPtrList[j]->setFocus(j == ((i + 1) % std::extent_v<decltype(boxPtrList)>));
                                    }
                                    break;
                                }
                            }
                            return;
                        }
                    default:
                        {
                            break;
                        }
                }
            }
        default:
            {
                break;
            }
    }

    // widget idbox and pwdbox are not independent from each other
    // tab in one box will grant focus to another

    m_boxID           .processEventRoot(event, true, {});
    m_boxPwd          .processEventRoot(event, true, {});
    m_boxNewPwd       .processEventRoot(event, true, {});
    m_boxNewPwdConfirm.processEventRoot(event, true, {});

    localCheck();
}

void ProcessChangePassword::doExit()
{
    g_client->requestProcess(PROCESSID_LOGIN);
}

void ProcessChangePassword::doPostPasswordChange()
{
    const auto idStr = m_boxID.getRawString();
    const auto pwdStr = m_boxPwd.getPasswordString();
    const auto pwdNewStr = m_boxNewPwd.getPasswordString();
    const auto pwdNewConfirmStr = m_boxNewPwdConfirm.getPasswordString();

    if(!idstrf::isEmail(idStr.c_str())){
        setInfoStr(u8"无效账号", 2);
        clearInput();
        return;
    }

    if(!idstrf::isPassword(pwdStr.c_str())){
        setInfoStr(u8"无效密码", 2);
        m_boxPwd.clear();
        m_boxNewPwd.clear();
        m_boxNewPwdConfirm.clear();
        return;
    }

    if(!idstrf::isPassword(pwdNewStr.c_str())){
        setInfoStr(u8"无效新密码", 2);
        m_boxNewPwd.clear();
        m_boxNewPwdConfirm.clear();
        return;
    }

    if(pwdNewStr != pwdNewConfirmStr){
        setInfoStr(u8"新密码两次输入不一致", 2);
        m_boxNewPwd.clear();
        m_boxNewPwdConfirm.clear();
        return;
    }

    if(pwdStr == pwdNewStr){
        setInfoStr(u8"新旧密码相同", 2);
        m_boxNewPwd.clear();
        m_boxNewPwdConfirm.clear();
        return;
    }

    setInfoStr(u8"提交中");

    CMChangePassword cmCP;
    std::memset(&cmCP, 0, sizeof(cmCP));

    cmCP.id.assign(idStr);
    cmCP.password.assign(pwdStr);
    cmCP.passwordNew.assign(pwdNewStr);
    g_client->send({CM_CHANGEPASSWORD, cmCP});
}

void ProcessChangePassword::localCheck()
{
    const auto idStr = m_boxID.getRawString();
    const auto pwdStr = m_boxPwd.getPasswordString();
    const auto pwdNewStr = m_boxNewPwd.getPasswordString();
    const auto pwdNewConfirmStr = m_boxNewPwdConfirm.getPasswordString();

    const auto fnCheckInput = [](const std::string &s, auto &check, bool good)
    {
        if(s.empty()){
            check.setText(u8"");
        }
        else if(good){
            check.loadXML(to_cstr(str_printf(u8"<par><t color=\"green\">√</t></par>").c_str()));
        }
        else{
            check.loadXML(to_cstr(str_printf(u8"<par><t color=\"red\">×</t></par>").c_str()));
        }
    };

    fnCheckInput(idStr, m_LBCheckID, idstrf::isEmail(idStr.c_str()));
    fnCheckInput(pwdStr, m_LBCheckPwd, idstrf::isPassword(pwdStr.c_str()));
    fnCheckInput(pwdNewStr, m_LBCheckNewPwd, idstrf::isPassword(pwdNewStr.c_str()));
    fnCheckInput(pwdNewConfirmStr, m_LBCheckNewPwdConfirm, idstrf::isPassword(pwdNewConfirmStr.c_str()) && pwdNewStr == pwdNewConfirmStr);
}

void ProcessChangePassword::clearInput()
{
    m_boxID.clear();
    m_boxPwd.clear();
    m_boxNewPwd.clear();
    m_boxNewPwdConfirm.clear();
}

bool ProcessChangePassword::hasInfo() const
{
    if(m_infoStr.empty()){
        return false;
    }

    if(m_infoStrSec == 0){
        return true;
    }
    return m_infoStrTimer.diff_sec() < m_infoStrSec;
}

void ProcessChangePassword::setInfoStr(const char8_t *s)
{
    m_infoStr.setText(str_haschar(s) ? s : u8"");
    m_infoStrSec = 0;
}

void ProcessChangePassword::setInfoStr(const char8_t *s, uint32_t sec)
{
    m_infoStr.setText(str_haschar(s) ? s : u8"");
    m_infoStrSec = sec;
    m_infoStrTimer.reset();
}

// ===== processchangepasswordnet.cpp =====

void ProcessChangePassword::on_SM_CHANGEPASSWORDOK(const uint8_t *, size_t)
{
    setInfoStr(u8"修改密码成功", 2);
    m_boxID.setFocus(false);
    m_boxPwd.setFocus(false);
    m_boxNewPwd.setFocus(false);
    m_boxNewPwdConfirm.setFocus(false);
}

void ProcessChangePassword::on_SM_CHANGEPASSWORDERROR(const uint8_t *buf, size_t)
{
    const auto smCAE = ServerMsg::conv<SMChangePasswordError>(buf);
    switch(smCAE.error){
        case CHGPWDERR_BADACCOUNT:
            {
                setInfoStr(u8"无效的账号", 2);
                clearInput();

                m_boxID.setFocus(true);
                m_boxPwd.setFocus(false);
                m_boxNewPwd.setFocus(false);
                m_boxNewPwdConfirm.setFocus(false);
                return;
            }
        case CHGPWDERR_BADPASSWORD:
            {
                setInfoStr(u8"无效的密码", 2);
                clearInput();

                m_boxID.setFocus(true);
                m_boxPwd.setFocus(false);
                m_boxNewPwd.setFocus(false);
                m_boxNewPwdConfirm.setFocus(false);
                return;
            }
        case CHGPWDERR_BADNEWPASSWORD:
            {
                setInfoStr(u8"无效的新密码", 2);
                clearInput();

                m_boxID.setFocus(true);
                m_boxPwd.setFocus(false);
                m_boxNewPwd.setFocus(false);
                m_boxNewPwdConfirm.setFocus(false);
                return;
            }
        case CHGPWDERR_BADACCOUNTPASSWORD:
            {
                setInfoStr(u8"错误的账号或密码", 2);
                clearInput();

                m_boxID.setFocus(true);
                m_boxPwd.setFocus(false);
                m_boxNewPwd.setFocus(false);
                m_boxNewPwdConfirm.setFocus(false);
                return;
            }
        default:
            {
                throw fflreach();
            }
    }
}
