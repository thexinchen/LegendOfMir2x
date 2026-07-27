#pragma once

// ===== merged from npcchatboard/npcchatorigframe.hpp =====
#include "gui_core.hpp"
#include "gui_widgets.hpp"

class NPCChatOrigFrame: public Widget
{
    private:
        ImageBoard m_up;
        ImageBoard m_down;

    public:
        NPCChatOrigFrame(
                Widget::VarDir = DIR_UPLEFT,
                Widget::VarInt = 0,
                Widget::VarInt = 0,

                Widget * = nullptr,
                bool     = false);
};

// ===== merged from npcchatboard/npcchatframe.hpp =====
#include "gui_core.hpp"

class NPCChatFrame: public Widget
{
    private:
        NPCChatOrigFrame m_frame;

    private:
        GfxResizeBoard m_board;

    public:
        NPCChatFrame(
                Widget::VarDir,
                Widget::VarInt,
                Widget::VarInt,

                Widget::VarSizeOpt,
                Widget::VarSizeOpt,

                Widget * = nullptr,
                bool     = false);
};

// ===== merged from npcchatboard/npcchatboard.hpp =====
#include <cstdint>
#include "uidf.hpp"
#include "gui_core.hpp"
#include "layoutboard.hpp"

class ProcessRun;
class NPCChatBoard: public Widget
{
    private:
        const int m_margin = 35;

    private:
        uint64_t m_npcUID = 0;
        std::string m_eventPath;

    private:
        ProcessRun *m_processRun;

    private:
        NPCChatFrame m_bg;

    private:
        ImageBoard m_face;
        LayoutBoard m_chatBoard;
        TritexButton m_buttonClose;

    public:
        NPCChatBoard(
                Widget::VarDir,
                Widget::VarInt,
                Widget::VarInt,

                ProcessRun *,

                Widget * = nullptr,
                bool     = false);

    public:
        void loadXML(uint64_t, const char *, const char *);

    private:
        void onClickEvent(const char *, const char *, const char *, bool);

    private:
        uint32_t getNPCFaceKey() const
        {
            if(uidf::isNPChar(m_npcUID)){
                return 0X50000000 | uidf::getNPCID(m_npcUID);
            }
            else{
                return SYS_U32NIL;
            }
        }
};
