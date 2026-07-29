#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>

#include "mirevent.hpp"
#include "process.hpp"
#include "servermsg.hpp"
#include "sysconst.hpp"

namespace imlogin
{
    template<size_t N>
    struct TextBuffer
    {
        mutable std::array<char, N + 1> data {};

        std::string str() const
        {
            return data.data();
        }

        void clear() const
        {
            data.fill('\0');
        }
    };

    class Notice
    {
        private:
            struct Entry
            {
                std::string text;
                double remainingMS = 5000.0;
            };

        private:
            std::deque<Entry> m_entries;
            size_t m_limit = 10;

        public:
            explicit Notice(size_t limit = 10): m_limit(limit) {}
            void update(double);
            void draw() const;
            void show(std::string, double = 5000.0);
            void clear();
    };

    class Status
    {
        private:
            std::string m_text;
            double m_remainingMS = 0.0;
            bool m_persistent = false;

        public:
            void update(double);
            void show(std::string, double = 0.0);
            bool active() const;
            const std::string &text() const;
    };
}

class ProcessLogo final: public Process
{
    private:
        double m_totalTime = 0.0;

    public:
        int id() const override { return PROCESSID_LOGO; }
        void draw() const override;
        void update(double) override;
        void processEvent(const MirEvent &) override;
};

class ProcessSync final: public Process
{
    private:
        int m_ratio = 0;

    public:
        int id() const override { return PROCESSID_SYRC; }
        void draw() const override;
        void update(double) override;
        void processEvent(const MirEvent &) override;
};

class ProcessLogin final: public Process
{
    private:
        imlogin::TextBuffer<SYS_IDSIZE> m_id;
        imlogin::TextBuffer<SYS_PWDSIZE> m_password;
        mutable imlogin::Notice m_notice;

    public:
        ProcessLogin();
        int id() const override { return PROCESSID_LOGIN; }
        void draw() const override;
        void update(double) override;
        void processEvent(const MirEvent &) override;

    private:
        void login() const;
        void sendLogin(const std::string &, const std::string &) const;

    public:
        void on_SM_LOGINOK(const uint8_t *, size_t);
        void on_SM_LOGINERROR(const uint8_t *, size_t);
};

class ProcessCreateAccount final: public Process
{
    private:
        imlogin::TextBuffer<SYS_IDSIZE> m_id;
        imlogin::TextBuffer<SYS_PWDSIZE> m_password;
        imlogin::TextBuffer<SYS_PWDSIZE> m_confirm;
        mutable imlogin::Notice m_notice;
        mutable imlogin::Status m_status;

    public:
        int id() const override { return PROCESSID_CREATEACCOUNT; }
        void draw() const override;
        void update(double) override;
        void processEvent(const MirEvent &) override;

    private:
        void submit() const;
        void clear() const;

    public:
        void on_SM_CREATEACCOUNTOK(const uint8_t *, size_t);
        void on_SM_CREATEACCOUNTERROR(const uint8_t *, size_t);
};

class ProcessSelectChar final: public Process
{
    private:
        std::optional<SMQueryCharOK> m_character;
        mutable imlogin::Notice m_notice {1};
        mutable bool m_openDeletePopup = false;
        imlogin::TextBuffer<SYS_PWDSIZE> m_deletePassword;
        int m_charAni = 0;
        double m_charAniTime = 0.0;
        uint32_t m_charAniSwitchFrame = 0;

    public:
        ProcessSelectChar();
        ~ProcessSelectChar() override;
        int id() const override { return PROCESSID_SELECTCHAR; }
        void draw() const override;
        void update(double) override;
        void processEvent(const MirEvent &) override;

    private:
        bool hasCharacter() const;
        uint32_t absoluteFrame() const;
        uint32_t characterFrameCount() const;
        std::optional<uint32_t> characterGfxBaseID() const;
        void drawCharacter() const;
        void drawCharacterInfo() const;
        void switchCharacterGfx();
        void start() const;
        void create() const;
        void deleteCharacter() const;

    public:
        void on_SM_QUERYCHAROK(const uint8_t *, size_t);
        void on_SM_QUERYCHARERROR(const uint8_t *, size_t);
        void on_SM_DELETECHAROK(const uint8_t *, size_t);
        void on_SM_DELETECHARERROR(const uint8_t *, size_t);
        void on_SM_ONLINEOK(const uint8_t *, size_t);
        void on_SM_ONLINEERROR(const uint8_t *, size_t);
};

class ProcessCreateChar final: public Process
{
    private:
        mutable int m_job = JOB_WARRIOR;
        mutable bool m_male = true;
        imlogin::TextBuffer<SYS_NAMESIZE> m_name;
        mutable imlogin::Notice m_notice;
        mutable double m_animationTime = 0.0;
        mutable uint32_t m_lastStartFrame = 0;

    public:
        ProcessCreateChar();
        ~ProcessCreateChar() override;
        int id() const override { return PROCESSID_CREATECHAR; }
        void draw() const override;
        void update(double) override;
        void processEvent(const MirEvent &) override;

    private:
        void submit() const;
        uint32_t absoluteFrame() const;
        static uint32_t characterGfxBaseID(int, bool);
        static uint32_t characterFrameCount(int, bool);
        void drawCharacter(bool, int, int) const;
        void playMagicSoundEffect() const;

    public:
        void on_SM_CREATECHAROK(const uint8_t *, size_t);
        void on_SM_CREATECHARERROR(const uint8_t *, size_t);
};

class ProcessChangePassword final: public Process
{
    private:
        imlogin::TextBuffer<SYS_IDSIZE> m_id;
        imlogin::TextBuffer<SYS_PWDSIZE> m_password;
        imlogin::TextBuffer<SYS_PWDSIZE> m_newPassword;
        imlogin::TextBuffer<SYS_PWDSIZE> m_confirm;
        mutable imlogin::Notice m_notice;
        mutable imlogin::Status m_status;

    public:
        int id() const override { return PROCESSID_CHANGEPASSWORD; }
        void draw() const override;
        void update(double) override;
        void processEvent(const MirEvent &) override;

    private:
        void submit() const;
        void clear() const;

    public:
        void on_SM_CHANGEPASSWORDOK(const uint8_t *, size_t);
        void on_SM_CHANGEPASSWORDERROR(const uint8_t *, size_t);
};
