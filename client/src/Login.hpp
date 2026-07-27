#pragma once

// ===== processlogo.hpp =====
#include "mirevent.hpp"
#include "process.hpp"

class ProcessLogo: public Process
{
    private:
        const double m_timeR1 = 0.3;
        const double m_timeR2 = 0.3;

    private:
        const double m_fullTime = 5000.0;

    private:
        double m_totalTime = 0.0;

    public:
        ProcessLogo(): Process() {}

    public:
        virtual ~ProcessLogo() = default;

    public:
        int id() const override
        {
            return PROCESSID_LOGO;
        }

    public:
        void draw() const override;
        void update(double) override;
        void processEvent(const MirEvent &) override;

    private:
        double colorRatio() const;
};

// ===== processsync.hpp =====
#include "gui_widgets.hpp"

class ProcessSync: public Process
{
    private:
        int m_ratio = 0;

    private:
        Widget m_canvas;

    private:
        ImageBoard m_barFull;
        GfxCropBoard m_bar;

    private:
        ImageBoard m_bgImg;

    private:
        TextBoard m_barText;

    public:
        ProcessSync();

    public:
        ~ProcessSync() = default;

    public:
        int id() const override
        {
            return PROCESSID_SYRC;
        }

    public:
        void draw() const override;
        void update(double) override;
        void processEvent(const MirEvent &) override;
};

// ===== processlogin.hpp =====
#include <cstdint>

#include "strf.hpp"
#include "totype.hpp"
#include "message.hpp"
#include "messagestackboard.hpp"

class ProcessLogin: public Process
{
    private:
        Widget m_canvas;

    private:
        TritexButton m_button1;
        TritexButton m_button2;
        TritexButton m_button3;
        TritexButton m_button4;

    private:
        InputLine   m_idBox;
        PasswordBox m_passwordBox;

    private:
        TextBoard m_buildSignature;

    private:
        GfxShapeBoard m_notifyBoardBg;
        MessageStackBoard m_notifyBoard;

    public:
        ProcessLogin();
        virtual ~ProcessLogin() = default;

    public:
        int id() const override
        {
            return PROCESSID_LOGIN;
        }

    public:
        void draw() const override;
        void update(double) override;
        void processEvent(const MirEvent &) override;

    private:
        void doExit();
        void doLogin();
        void doCreateAccount();
        void doChangePassword();

    private:
        void sendLogin(const std::string &, const std::string &);

    public:
        void on_SM_LOGINOK   (const uint8_t *, size_t);
        void on_SM_LOGINERROR(const uint8_t *, size_t);
};

// ===== processcreateaccount.hpp =====
#include <optional>
#include "raiitimer.hpp"

class ProcessCreateAccount: public Process
{
    private:
        constexpr static int m_x = 180;
        constexpr static int m_y = 145;

    private:
        LabelBoard m_LBID;
        LabelBoard m_LBPwd;
        LabelBoard m_LBPwdConfirm;

    private:
        InputLine   m_boxID;
        PasswordBox m_boxPwd;
        PasswordBox m_boxPwdConfirm;

    private:
        LabelBoard m_LBCheckID;
        LabelBoard m_LBCheckPwd;
        LabelBoard m_LBCheckPwdConfirm;

    private:
        TritexButton m_submit;
        TritexButton m_quit;

    private:
        LabelBoard m_infoStr;
        uint32_t   m_infoStrSec;
        hres_timer m_infoStrTimer;

    public:
        ProcessCreateAccount();
        virtual ~ProcessCreateAccount() = default;

    public:
        int id() const override
        {
            return PROCESSID_CREATEACCOUNT;
        }

    public:
        void draw() const override;
        void update(double) override;
        void processEvent(const MirEvent &) override;

    private:
        void doPostAccount();
        void doExit();

    private:
        void localCheck();

    private:
        void clearInput();

    private:
        bool hasInfo() const;
        void setInfoStr(const char8_t *);
        void setInfoStr(const char8_t *, uint32_t);

    public:
        void on_SM_CREATEACCOUNTOK   (const uint8_t *, size_t);
        void on_SM_CREATEACCOUNTERROR(const uint8_t *, size_t);
};

// ===== processselectchar.hpp =====
#include "servermsg.hpp"
#include "inputstringboard.hpp"

class ProcessSelectChar: public Process
{
    private:
        std::optional<SMQueryCharOK> m_smChar;

    private:
        int m_charAni = 0;
        double m_charAniTime = 0.0;
        uint32_t m_charAniSwitchFrame = 0;

    private:
        Widget m_canvas;

    private:
        TritexButton m_start;
        TritexButton m_create;
        TritexButton m_delete;
        TritexButton m_exit;

    private:
        GfxShapeBoard m_notifyBoardBg;
        MessageStackBoard m_notifyBoard;

    private:
        InputStringBoard m_deleteInput;

    public:
        ProcessSelectChar();

    public:
        ~ProcessSelectChar() override;

    public:
        int id() const override
        {
            return PROCESSID_SELECTCHAR;
        }

    public:
        void draw() const override;
        void update(double) override;
        void processEvent(const MirEvent &) override;

    private:
        void drawChar() const;
        void drawCharName() const;

    private:
        void switchCharGfx();

    private:
        uint32_t charFrameCount() const;
        std::optional<uint32_t> charGfxBaseID() const;

    private:
        uint32_t absFrame() const
        {
            return to_u32(std::lround(m_charAniTime / 200.0));
        }

    private:
        bool hasChar() const
        {
            return m_smChar.has_value() && !m_smChar.value().name.empty();
        }

    private:
        void onStart();
        void onCreate();
        void onDelete();
        void onExit();

    public:
        void on_SM_QUERYCHAROK    (const uint8_t *, size_t);
        void on_SM_QUERYCHARERROR (const uint8_t *, size_t);
        void on_SM_DELETECHAROK   (const uint8_t *, size_t);
        void on_SM_DELETECHARERROR(const uint8_t *, size_t);
        void on_SM_ONLINEOK       (const uint8_t *, size_t);
        void on_SM_ONLINEERROR    (const uint8_t *, size_t);
};

// ===== processcreatechar.hpp =====

class ProcessCreateChar: public Process
{
    private:
        int m_job = JOB_WARRIOR;
        bool m_activeGender = true;

    private:
        TritexButton m_warrior;
        TritexButton m_wizard;
        TritexButton m_taoist;

    private:
        TritexButton m_submit;
        TritexButton m_exit;

    private:
        InputLine m_nameBox;

    private:
        MessageStackBoard m_notifyBoard;

    private:
        double m_aniTime = 0.0;
        uint32_t m_lastStartAbsFrame = 0;

    public:
        ProcessCreateChar();

    public:
        ~ProcessCreateChar() override;

    public:
        int id() const override
        {
            return PROCESSID_CREATECHAR;
        }

    private:
        uint32_t absFrame() const
        {
            return to_u32(std::lround(m_aniTime / 200.0));
        }

    private:
        static uint32_t charGfxBaseID(int, bool);
        static uint32_t charFrameCount(int, bool);

    public:
        void draw() const override;
        void update(double) override;
        void processEvent(const MirEvent &) override;

    private:
        void drawChar(bool, int, int) const;

    private:
        void onSubmit();
        void onExit();

    private:
        void setGUIActive(bool);

    public:
        void on_SM_CREATECHAROK   (const uint8_t *, size_t);
        void on_SM_CREATECHARERROR(const uint8_t *, size_t);

    private:
        void playMagicSoundEffect();
};

// ===== processchangepassword.hpp =====
#include "gui_core.hpp"

class ProcessChangePassword: public Process
{
    private:
        constexpr static int m_x = 180;
        constexpr static int m_y = 145;

    private:
        LabelBoard m_LBID;
        LabelBoard m_LBPwd;
        LabelBoard m_LBNewPwd;
        LabelBoard m_LBNewPwdConfirm;

    private:
        InputLine   m_boxID;
        PasswordBox m_boxPwd;
        PasswordBox m_boxNewPwd;
        PasswordBox m_boxNewPwdConfirm;

    private:
        LabelBoard m_LBCheckID;
        LabelBoard m_LBCheckPwd;
        LabelBoard m_LBCheckNewPwd;
        LabelBoard m_LBCheckNewPwdConfirm;

    private:
        TritexButton m_submit;
        TritexButton m_quit;

    private:
        LabelBoard m_infoStr;
        uint32_t   m_infoStrSec;
        hres_timer m_infoStrTimer;

    public:
        ProcessChangePassword();
        virtual ~ProcessChangePassword() = default;

    public:
        int id() const override
        {
            return PROCESSID_CHANGEPASSWORD;
        }

    public:
        void draw() const override;
        void update(double) override;
        void processEvent(const MirEvent &) override;

    private:
        void doPostPasswordChange();
        void doExit();

    private:
        void localCheck();

    private:
        void clearInput();

    private:
        bool hasInfo() const;
        void setInfoStr(const char8_t *);
        void setInfoStr(const char8_t *, uint32_t);

    public:
        void on_SM_CHANGEPASSWORDOK   (const uint8_t *, size_t);
        void on_SM_CHANGEPASSWORDERROR(const uint8_t *, size_t);
};
