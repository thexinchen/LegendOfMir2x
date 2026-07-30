#pragma once
#include "ImMainUI.hpp"
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
class GUIManager
{
    private:
        ProcessRun *m_processRun;

    private:
        ImNPCChatBoard m_NPCChatBoard;
        ImMainUI m_mainUI;

    private:
        ImFriendChatBoard m_friendChatBoard;

    private:
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

    public:
        GUIManager(ProcessRun *);

    public:
        auto getMainUI(this auto &&self)
        {
            return std::addressof(self.m_mainUI);
        }

        auto getInputStringBoard(this auto &&self)
        {
            return std::addressof(self.m_inputStringBoard);
        }

        auto getQuestStateBoard(this auto &&self)
        {
            return std::addressof(self.m_questStateBoard);
        }

        auto getTeamStateBoard(this auto &&self)
        {
            return std::addressof(self.m_teamStateBoard);
        }

        auto getSecuredItemListBoard(this auto &&self)
        {
            return std::addressof(self.m_securedItemListBoard);
        }

        auto getInventoryBoard(this auto &&self)
        {
            return std::addressof(self.m_inventoryBoard);
        }

        auto getMiniMapBoard(this auto &&self)
        {
            return std::addressof(self.m_miniMapBoard);
        }

        auto getNPCChatBoard(this auto &&self)
        {
            return std::addressof(self.m_NPCChatBoard);
        }

        auto getFriendChatBoard(this auto &&self)
        {
            return std::addressof(self.m_friendChatBoard);
        }

        auto getSkillBoard(this auto &&self)
        {
            return std::addressof(self.m_skillBoard);
        }

        auto getPurchaseBoard(this auto &&self)
        {
            return std::addressof(self.m_purchaseBoard);
        }

        auto getRuntimeConfigBoard(this auto &&self)
        {
            return std::addressof(self.m_runtimeConfigBoard);
        }

    public:
        void update(double);

    public:
        void draw() const;

    public:
        bool processEvent(const MirEvent &, bool = true);

    public:
        void flipBoard(std::string_view);

    private:
        void afterResize();
};
