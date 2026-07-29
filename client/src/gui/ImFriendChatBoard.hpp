#pragma once

#include <optional>

#include "ImBoard.hpp"
#include "friendchatboard.hpp"

class ImFriendChatBoard final: public ImBoard
{
    private:
        mutable FriendChatBoard m_content;

    public:
        ImFriendChatBoard(int, int, ProcessRun *);

    public:
        void draw() const override;
        void update(double) override;
        bool processEvent(const MirEvent &) const override;

    public:
        void setShow(bool) const;
        void flipShow() const;

    public:
        void setFriendList(const SDFriendList &friendList) { m_content.setFriendList(friendList); }
        void addMessage(std::optional<uint64_t> localID, const SDChatMessage &message) { m_content.addMessage(localID, message); }
        void addGroup(const SDChatPeer &peer) { m_content.addGroup(peer); }
        void onAddFriendAccepted(const SDChatPeer &peer) { m_content.onAddFriendAccepted(peer); }
        void onAddFriendRejected(const SDChatPeer &peer) { m_content.onAddFriendRejected(peer); }
};
