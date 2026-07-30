#pragma once

#include <functional>
#include <memory>
#include <optional>

#include "ImBoard.hpp"
#include "serdesmsg.hpp"

class ProcessRun;
class ImFriendChatBoard final: public ImBoard
{
    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;

    public:
        ImFriendChatBoard(int, int, ProcessRun *);
        ~ImFriendChatBoard() override;

    public:
        void draw() const override;
        bool processEvent(const MirEvent &) const override;

    public:
        void setFriendList(const SDFriendList &);
        void addMessage(std::optional<uint64_t>, const SDChatMessage &);
        void addGroup(const SDChatPeer &);
        void onAddFriendAccepted(const SDChatPeer &);
        void onAddFriendRejected(const SDChatPeer &);

    private:
        const SDChatPeer *findFriend(const SDChatPeerID &) const;
        const SDChatPeer *findPeer(const SDChatPeerID &) const;
        void openChat(const SDChatPeer &, bool) const;
        void queryPeer(const SDChatPeerID &, std::function<void(const SDChatPeer *, bool)>) const;
        void queryMessage(uint64_t, std::function<void(const SDChatMessage *, bool)>) const;
        void requestAddFriend(const SDChatPeer &, bool) const;
        void requestAcceptFriend(const SDChatPeer &) const;
        void requestRejectFriend(const SDChatPeer &) const;
        void requestBlockPlayer(const SDChatPeer &) const;
        void sendCurrentMessage() const;
        void createSelectedGroup() const;
};
