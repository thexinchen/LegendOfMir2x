#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "mirevent.hpp"
#include "ImMiniMapBoard.hpp"
#include "ImHorseBoard.hpp"
#include "gui/ImSkillBoard.hpp"
#include "gui/ImAcutionBoard.hpp"
#include "ImGuildBoard.hpp"
#include "gui/ImNPCChatBoard.hpp"
#include "gui/ImFriendChatBoard.hpp"
#include "gui/ImPurchaseBoard.hpp"
#include "ImTeamStateBoard.hpp"
#include "ImInventoryBoard.hpp"
#include "ImQuestStateBoard.hpp"
#include "ImPlayerStateBoard.hpp"
#include "ImInputStringBoard.hpp"
#include "gui/ImRuntimeConfigBoard.hpp"
#include "ImSecuredItemListBoard.hpp"

class ProcessRun;
class XMLTypeset;

enum
{
    CBLOG_DEF = 0,
    CBLOG_SYS,
    CBLOG_DBG,
    CBLOG_ERR,
};

class ImMainUI final
{
    private:
        struct LogLine
        {
            std::string text;
            uint32_t color = 0XFFFFFFFF;
            std::string xml;
            mutable int layoutWidth = -1;
            mutable std::shared_ptr<XMLTypeset> layout;
        };

        struct BlinkState
        {
            double elapsedMS = 0.0;
            double remainingMS = 0.0;
            bool active = false;
        };

    private:
        ProcessRun *m_processRun = nullptr;

    private:
        ImNPCChatBoard m_NPCChatBoard;
        ImFriendChatBoard m_friendChatBoard;
        ImHorseBoard m_horseBoard;
        ImSkillBoard m_skillBoard;
        ImGuildBoard m_guildBoard;
        ImMiniMapBoard m_miniMapBoard;
        ImAcutionBoard m_acutionBoard;
        ImPurchaseBoard m_purchaseBoard;
        ImTeamStateBoard m_teamStateBoard;
        ImInventoryBoard m_inventoryBoard;
        ImQuestStateBoard m_questStateBoard;
        ImPlayerStateBoard m_playerStateBoard;
        ImInputStringBoard m_inputStringBoard;
        ImRuntimeConfigBoard m_runtimeConfigBoard;
        ImSecuredItemListBoard m_securedItemListBoard;

        mutable bool m_expand = false;
        mutable bool m_minimize = false;
        mutable bool m_quickAccessShown = false;
        mutable bool m_focusCommand = false;
        mutable bool m_quickDragging = false;
        mutable bool m_acMagic = false;
        mutable bool m_dcMagic = false;

        mutable float m_quickX = 0.0f;
        mutable float m_quickY = -1.0f;
        mutable float m_logScroll = 1.0f;
        mutable std::array<char, 512> m_command {};

        double m_accuTimeMS = 0.0;
        std::deque<LogLine> m_logList;
        mutable std::unordered_map<std::string, BlinkState> m_buttonBlink;

    public:
        explicit ImMainUI(ProcessRun *);

    public:
        void draw() const;
        void update(double);
        bool processEvent(const MirEvent &);

    public:
        void flipBoard(std::string_view);

        auto getInputStringBoard(this auto &&self) { return std::addressof(self.m_inputStringBoard); }
        auto getQuestStateBoard(this auto &&self) { return std::addressof(self.m_questStateBoard); }
        auto getTeamStateBoard(this auto &&self) { return std::addressof(self.m_teamStateBoard); }
        auto getSecuredItemListBoard(this auto &&self) { return std::addressof(self.m_securedItemListBoard); }
        auto getInventoryBoard(this auto &&self) { return std::addressof(self.m_inventoryBoard); }
        auto getMiniMapBoard(this auto &&self) { return std::addressof(self.m_miniMapBoard); }
        auto getNPCChatBoard(this auto &&self) { return std::addressof(self.m_NPCChatBoard); }
        auto getFriendChatBoard(this auto &&self) { return std::addressof(self.m_friendChatBoard); }
        auto getSkillBoard(this auto &&self) { return std::addressof(self.m_skillBoard); }
        auto getPurchaseBoard(this auto &&self) { return std::addressof(self.m_purchaseBoard); }
        auto getRuntimeConfigBoard(this auto &&self) { return std::addressof(self.m_runtimeConfigBoard); }

    public:
        void addXMLLog(const char *);
        void addParLog(const char *);
        void addLog(int, const char *);
        void startButtonBlink(std::string_view, double = 0.0);

    public:
        int shiftHeight() const;

    private:
        void drawHUD() const;
        void drawQuickAccess() const;
        void drawSkillAndBuffHUD() const;
        void submitCommand() const;
        void consumeQuickSlot(int) const;
        bool blinkVisible(std::string_view) const;
        void stopButtonBlink(std::string_view) const;
        bool processHUDEvent(const MirEvent &);
        void afterResize();
};
