#pragma once
#include "gui_core.hpp"
#include "ImMainUI.hpp"
#include "minimapboard.hpp"
#include "ImHorseBoard.hpp"
#include "gui/skillboard.hpp"
#include "gui/ImAcutionBoard.hpp"
#include "ImGuildBoard.hpp"
#include "gui/npcchatboard.hpp"
#include "gui/friendchatboard.hpp"
#include "gui/purchaseboard.hpp"
#include "teamstateboard.hpp"
#include "inventoryboard.hpp"
#include "queststateboard.hpp"
#include "playerstateboard.hpp"
#include "ImInputStringBoard.hpp"
#include "gui/runtimeconfigboard.hpp"
#include "secureditemlistboard.hpp"

class ProcessRun;
class GUIManager: public Widget
{
    private:
        ProcessRun *m_processRun;

    private:
        NPCChatBoard m_NPCChatBoard;
        ImMainUI m_mainUI;

    private:
        FriendChatBoard m_friendChatBoard;

    private:
        ImHorseBoard m_horseBoard;
        SkillBoard m_skillBoard;
        ImGuildBoard m_guildBoard;
        MiniMapBoard m_miniMapBoard;
        ImAcutionBoard m_acutionBoard;
        PurchaseBoard m_purchaseBoard;
        TeamStateBoard m_teamStateBoard;
        InventoryBoard m_inventoryBoard;
        QuestStateBoard m_questStateBoard;
        PlayerStateBoard m_playerStateBoard;
        ImInputStringBoard m_inputStringBoard;
        RuntimeConfigBoard m_runtimeConfigBoard;
        SecuredItemListBoard m_securedItemListBoard;

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

    public:
        void updateDefault(double) override;

    public:
        void drawDefault(Widget::ROIMap) const override;

    public:
        bool processEventDefault(const MirEvent &, bool, Widget::ROIMap) override;

    public:
        Widget *getWidget(const std::string_view &);

    public:
        void flipBoard(std::string_view);

    public:
        const Widget *getWidget(const std::string_view &name) const
        {
            return const_cast<GUIManager *>(this)->getWidget(name);
        }

    private:
        void afterResizeDefault() override;
};
