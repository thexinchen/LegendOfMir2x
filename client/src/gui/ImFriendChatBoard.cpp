#include "ImFriendChatBoard.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "cblog.hpp"
#include "client.hpp"
#include "fflerror.hpp"
#include "gldevice.hpp"
#include "gui_font.hpp"
#include "gui_texture.hpp"
#include "hero.hpp"
#include "ImInputStringBoard.hpp"
#include "processrun.hpp"
#include "strf.hpp"
#include "sysconst.hpp"
#include "totype.hpp"

extern Client *g_client;
extern PNGTexDB *g_progUseDB;
extern FontexDB *g_fontexDB;
extern GLDevice *g_glDevice;

namespace
{
    constexpr int pageChat = 0;
    constexpr int pagePreview = 1;
    constexpr int pageFriend = 2;
    constexpr int pageSearch = 3;
    constexpr int pageCreateGroup = 4;

    constexpr float minBoardW = 451.0f;
    constexpr float minBoardH = 464.0f;
    constexpr float borderTop = 54.0f;
    constexpr float borderBottom = 10.0f;
    constexpr float borderLeft = 13.0f;
    constexpr float borderRight = 38.0f;

    void drawTexture(ImDrawList *list, const GLTexID texture, const ImVec2 pos, const ImVec2 size = {})
    {
        if(texture){
            const ImVec2 actualSize = size.x > 0 && size.y > 0 ? size : ImVec2 {to_f(texture.w), to_f(texture.h)};
            list->AddImage(texture, pos, {pos.x + actualSize.x, pos.y + actualSize.y});
        }
    }

    ImVec2 drawText(ImDrawList *list, const ImVec2 pos, const std::string &text, const uint8_t size = 12, const ImU32 color = IM_COL32_WHITE)
    {
        if(const auto texture = g_fontexDB->retrieve(1, size, 0, text.c_str()); texture){
            list->AddImage(texture, pos, {pos.x + texture.w, pos.y + texture.h}, {0, 0}, {1, 1}, color);
            return {to_f(texture.w), to_f(texture.h)};
        }
        return {};
    }

    std::string plainText(const std::string &xml)
    {
        std::string result;
        bool inTag = false;
        for(const char ch: xml){
            if(ch == '<'){
                inTag = true;
            }
            else if(ch == '>'){
                inTag = false;
            }
            else if(!inTag && ch != '\r' && ch != '\n'){
                result.push_back(ch);
            }
        }
        return result;
    }

    std::string messageText(const SDChatMessage &message)
    {
        try{
            return plainText(cerealf::deserialize<std::string>(message.message));
        }
        catch(...){
            return "[无效消息]";
        }
    }

    std::optional<std::string> xmlAttribute(const std::string &xml, const std::string_view name)
    {
        const std::string prefix = std::string(name) + "=\"";
        const auto begin = xml.find(prefix);
        if(begin == std::string::npos){
            return {};
        }
        const auto valueBegin = begin + prefix.size();
        const auto end = xml.find('"', valueBegin);
        return end == std::string::npos ? std::optional<std::string> {} : std::make_optional(xml.substr(valueBegin, end - valueBegin));
    }

    void drawNineSlice(
            ImDrawList *list,
            const GLTexID texture,
            const ImVec2 pos,
            const ImVec2 size,
            const float left,
            const float top,
            const float centerW,
            const float centerH,
            const ImU32 tint = IM_COL32_WHITE)
    {
        if(!texture){
            return;
        }
        const float right = texture.w - left - centerW;
        const float bottom = texture.h - top - centerH;
        const std::array<float, 4> sx {{0, left, left + centerW, to_f(texture.w)}};
        const std::array<float, 4> sy {{0, top, top + centerH, to_f(texture.h)}};
        const std::array<float, 4> dx {{pos.x, pos.x + left, pos.x + size.x - right, pos.x + size.x}};
        const std::array<float, 4> dy {{pos.y, pos.y + top, pos.y + size.y - bottom, pos.y + size.y}};
        for(int y = 0; y < 3; ++y){
            for(int x = 0; x < 3; ++x){
                const float srcW = sx[x + 1] - sx[x];
                const float srcH = sy[y + 1] - sy[y];
                const float dstW = dx[x + 1] - dx[x];
                const float dstH = dy[y + 1] - dy[y];
                if(srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0){
                    continue;
                }
                for(float tileY = 0; tileY < dstH; tileY += srcH){
                    const float tileH = std::min(srcH, dstH - tileY);
                    for(float tileX = 0; tileX < dstW; tileX += srcW){
                        const float tileW = std::min(srcW, dstW - tileX);
                        list->AddImage(
                            texture,
                            {dx[x] + tileX, dy[y] + tileY},
                            {dx[x] + tileX + tileW, dy[y] + tileY + tileH},
                            {sx[x] / texture.w, sy[y] / texture.h},
                            {(sx[x] + tileW) / texture.w, (sy[y] + tileH) / texture.h},
                            tint);
                    }
                }
            }
        }
    }

    bool textureButton(ImDrawList *list, const char *id, const ImVec2 pos, const uint32_t offID, const uint32_t downID)
    {
        const auto off = g_progUseDB->retrieve(offID);
        if(!off){
            return false;
        }
        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton(id, {to_f(off.w), to_f(off.h)});
        const auto down = g_progUseDB->retrieve(downID);
        drawTexture(list, ImGui::IsItemActive() && down ? down : off, pos);
        return clicked;
    }

    GLTexID peerAvatar(const SDChatPeer &peer)
    {
        if(peer.group()){
            return g_progUseDB->retrieve(0X00001300);
        }
        if(peer.id == SYS_CHATDBID_SYSTEM || peer.special()){
            return g_progUseDB->retrieve(0X00001100);
        }
        if(const auto player = peer.player()){
            return g_progUseDB->retrieve(Hero::faceGfxID(player->gender, player->job));
        }
        return g_progUseDB->retrieve(0X010007CF);
    }
}

struct ImFriendChatBoard::Impl
{
    struct Conversation
    {
        SDChatPeerID cpid;
        size_t unread = 0;
        std::vector<SDChatMessage> messages;
        std::string preview;
    };

    ProcessRun *processRun = nullptr;
    SDFriendList friends;
    std::list<SDChatPeer> cachedPeers;
    std::list<Conversation> conversations;
    std::unordered_map<uint64_t, SDChatMessage> cachedMessages;
    std::unordered_map<uint64_t, SDChatMessage> pendingMessages;

    SDChatPeer selectedPeer;
    int page = pagePreview;
    int previousPage = pagePreview;
    ImVec2 boardSize {minBoardW, minBoardH};
    std::array<float, 5> scroll {};
    std::array<float, 5> scrollReach {};

    std::array<char, 4096> chatInput {};
    std::array<char, 128> searchInput {};
    std::string lastSearch;
    std::vector<SDChatPeer> searchResults;
    bool searchShowCandidates = false;
    std::unordered_set<uint64_t> selectedGroupPeers;

    uint64_t nextPendingID = 1;
    std::optional<uint64_t> referMessageID;
    std::string referPreview;
    bool dragging = false;
    int resizeEdge = -1;
};

ImFriendChatBoard::ImFriendChatBoard(const int x, const int y, ProcessRun *processRun)
    : ImBoard("##friend-chat-board")
    , m_impl(std::make_unique<Impl>())
{
    m_impl->processRun = fflcheck(processRun);
    m_impl->cachedPeers =
    {
        SDChatPeer {.id = SYS_CHATDBID_SYSTEM, .name = "系统助手"},
        SDChatPeer {.id = SYS_CHATDBID_GROUP, .name = "群管理助手"},
        processRun->getMyHeroChatPeer(),
    };
    moveTo(to_f(x), to_f(y));
}

ImFriendChatBoard::~ImFriendChatBoard() = default;

void ImFriendChatBoard::draw() const
{
    if(!show()){
        return;
    }

    if(beginWindow(m_impl->boardSize, false)){
        const auto pos = ImGui::GetWindowPos();
        auto *list = ImGui::GetWindowDrawList();
        drawNineSlice(list, g_progUseDB->retrieve(0X00000810), pos, m_impl->boardSize, 0, 0, 510, 187, IM_COL32(160,160,160,255));
        drawNineSlice(list, g_progUseDB->retrieve(0X00000800), pos, m_impl->boardSize, 55, 95, 230, 250);

        const ImVec2 contentMin {pos.x + borderLeft + 4, pos.y + borderTop + 4};
        const ImVec2 contentMax {pos.x + m_impl->boardSize.x - borderRight - 4, pos.y + m_impl->boardSize.y - borderBottom - 4};
        const float contentW = contentMax.x - contentMin.x;
        const float contentH = contentMax.y - contentMin.y;

        std::string titleStorage;
        const char *title = "【聊天记录】";
        if(m_impl->page == pageChat){
            const auto &peer = m_impl->selectedPeer;
            if(peer.name.empty()){
                title = "好友名称";
            }
            else if(peer.group() || peer.special() || findFriend(peer.cpid())){
                title = peer.name.c_str();
            }
            else if(peer.player() && peer.id == m_impl->processRun->getMyHeroDBID()){
                titleStorage = str_printf("自己 %s", peer.name.c_str());
                title = titleStorage.c_str();
            }
            else{
                titleStorage = str_printf("陌生人 %s", peer.name.c_str());
                title = titleStorage.c_str();
            }
        }
        else if(m_impl->page == pageFriend){ title = "【好友列表】"; }
        else if(m_impl->page == pageSearch){ title = "【查找用户】"; }
        else if(m_impl->page == pageCreateGroup){ title = "【创建群聊】"; }
        if(const auto titleTexture = g_fontexDB->retrieve(1, 14, 0, title); titleTexture){
            drawTexture(list, titleTexture, {pos.x + 45 + (m_impl->boardSize.x - 235 - titleTexture.w) * 0.5f, pos.y + 22});
        }

        float controlX = pos.x + m_impl->boardSize.x - 42;
        const auto control = [&](const char *id, const uint32_t off, const uint32_t down, const auto &fn)
        {
            const auto texture = g_progUseDB->retrieve(off);
            if(!texture){
                return;
            }
            controlX -= texture.w;
            if(textureButton(list, id, {controlX, pos.y + 20}, off, down)){
                fn();
            }
            controlX -= 2;
        };
        if(m_impl->page == pageChat){
            control("##friend-back-preview", 0X000008F0, 0X000008F1, [&]{ m_impl->previousPage = m_impl->page; m_impl->page = pagePreview; });
            control("##friend-chat-unused1", 0X00000023, 0X00000024, []{});
            control("##friend-chat-unused2", 0X000008B0, 0X000008B1, []{});
            control("##friend-chat-unused3", 0X00000590, 0X00000591, []{});
        }
        else if(m_impl->page == pagePreview){
            control("##friend-open-list", 0X00000160, 0X00000161, [&]{ m_impl->previousPage = m_impl->page; m_impl->page = pageFriend; });
        }
        else if(m_impl->page == pageFriend){
            control("##friend-list-back", 0X000008F0, 0X000008F1, [&]{ m_impl->page = pagePreview; });
            control("##friend-list-search", 0X00000900, 0X00000901, [&]{ m_impl->page = pageSearch; });
            control("##friend-list-group", 0X00000170, 0X00000171, [&]
            {
                m_impl->selectedGroupPeers.clear();
                m_impl->page = pageCreateGroup;
            });
        }
        else if(m_impl->page == pageSearch){
            control("##friend-search-back", 0X000008F0, 0X000008F1, [&]{ m_impl->page = pagePreview; });
        }
        else if(m_impl->page == pageCreateGroup){
            control("##friend-group-back", 0X000008F0, 0X000008F1, [&]{ m_impl->page = pagePreview; });
            control("##friend-group-create", 0X00000910, 0X00000911, [&]{ createSelectedGroup(); });
            control("##friend-group-invert", 0X00000860, 0X00000861, [&]
            {
                for(const auto &peer: m_impl->friends){
                    const auto id = peer.cpid().asU64();
                    if(m_impl->selectedGroupPeers.contains(id)){ m_impl->selectedGroupPeers.erase(id); }
                    else{ m_impl->selectedGroupPeers.insert(id); }
                }
            });
        }

        list->PushClipRect(contentMin, contentMax, true);
        auto &scroll = m_impl->scroll.at(m_impl->page);
        const auto wheelScroll = [&](const float totalHeight)
        {
            const float reach = std::max(0.0f, totalHeight - contentH);
            m_impl->scrollReach.at(m_impl->page) = reach;
            if(ImGui::IsMouseHoveringRect(contentMin, contentMax) && ImGui::GetIO().MouseWheel != 0.0f){
                scroll = std::clamp(scroll - ImGui::GetIO().MouseWheel * 45.0f, 0.0f, reach);
            }
            scroll = std::clamp(scroll, 0.0f, reach);
        };

        if(m_impl->page == pagePreview){
            constexpr float rowH = 58;
            int row = 0;
            for(auto &conversation: m_impl->conversations){
                const auto peer = findPeer(conversation.cpid);
                if(!peer){
                    continue;
                }
                const ImVec2 rowPos {contentMin.x, contentMin.y + row * rowH - scroll};
                ImGui::PushID(to_d(conversation.cpid.asU64()));
                ImGui::SetCursorScreenPos(rowPos);
                ImGui::InvisibleButton("##friend-preview-row", {contentW, rowH});
                const bool hovered = ImGui::IsItemHovered();
                if(ImGui::IsItemClicked()){
                    openChat(*peer, true);
                }
                ImGui::PopID();
                list->AddRect(rowPos, {rowPos.x + contentW, rowPos.y + rowH}, IM_COL32(231,231,189, hovered ? 64 : 32));
                if(hovered){
                    list->AddRectFilled(rowPos, {rowPos.x + contentW, rowPos.y + rowH}, IM_COL32(231,231,189,64));
                }
                drawTexture(list, peerAvatar(*peer), {rowPos.x + 4, rowPos.y + 4}, {42, 50});
                drawText(list, {rowPos.x + 52, rowPos.y + 7}, peer->name, 14);
                drawText(list, {rowPos.x + 52, rowPos.y + 30}, conversation.preview, 12, IM_COL32(128,128,128,255));
                ++row;
            }
            wheelScroll(row * rowH);
        }
        else if(m_impl->page == pageFriend || m_impl->page == pageCreateGroup){
            std::vector<const SDChatPeer *> peers;
            if(m_impl->page == pageFriend){
                if(const auto system = findPeer(SDChatPeerID(CPR_SPECIAL, SYS_CHATDBID_SYSTEM))){ peers.push_back(system); }
                const auto self = m_impl->processRun->getMyHeroChatPeer();
                m_impl->cachedPeers.remove_if([&](const SDChatPeer &peer){ return peer.cpid() == self.cpid(); });
                m_impl->cachedPeers.push_back(self);
                peers.push_back(std::addressof(m_impl->cachedPeers.back()));
            }
            for(const auto &peer: m_impl->friends){
                peers.push_back(std::addressof(peer));
            }

            constexpr float rowH = 52;
            for(size_t i = 0; i < peers.size(); ++i){
                const auto peer = peers[i];
                const ImVec2 rowPos {contentMin.x, contentMin.y + i * rowH - scroll};
                ImGui::PushID(to_d(peer->cpid().asU64()));
                ImGui::SetCursorScreenPos(rowPos);
                ImGui::InvisibleButton("##friend-list-row", {contentW, rowH});
                const bool hovered = ImGui::IsItemHovered();
                if(ImGui::IsItemClicked()){
                    if(m_impl->page == pageCreateGroup){
                        const auto id = peer->cpid().asU64();
                        if(m_impl->selectedGroupPeers.contains(id)){ m_impl->selectedGroupPeers.erase(id); }
                        else{ m_impl->selectedGroupPeers.insert(id); }
                    }
                    else{
                        openChat(*peer, true);
                    }
                }
                ImGui::PopID();
                list->AddRect(rowPos, {rowPos.x + contentW, rowPos.y + rowH}, IM_COL32(231,231,189, hovered ? 64 : 32));
                if(hovered){
                    list->AddRectFilled(rowPos, {rowPos.x + contentW, rowPos.y + rowH}, IM_COL32(231,231,189,64));
                }
                drawTexture(list, peerAvatar(*peer), {rowPos.x + 4, rowPos.y + 4}, {40, 44});
                drawText(list, {rowPos.x + 52, rowPos.y + 18}, peer->name, 14);
                if(m_impl->page == pageCreateGroup){
                    const ImVec2 boxPos {rowPos.x + contentW - 28, rowPos.y + 17};
                    list->AddRect(boxPos, {boxPos.x + 16, boxPos.y + 16}, IM_COL32(231,231,189,128));
                    if(m_impl->selectedGroupPeers.contains(peer->cpid().asU64())){
                        drawTexture(list, g_progUseDB->retrieve(0X00000480), boxPos);
                    }
                }
            }
            wheelScroll(peers.size() * rowH);
        }
        else if(m_impl->page == pageSearch){
            constexpr float inputH = 30.0f;
            constexpr float clearGap = 10.0f;
            const float inputW = contentW - 60.0f;
            const auto inputFrame = g_progUseDB->retrieve(0X00000460);
            drawNineSlice(list, inputFrame, contentMin, {inputW, inputH}, 3, 3, inputFrame ? inputFrame.w - 6.0f : 0.0f, 2);
            drawTexture(list, g_progUseDB->retrieve(0X00001200), {contentMin.x + 8, contentMin.y + 5}, {20, 20});

            ImGui::SetCursorScreenPos({contentMin.x + 33, contentMin.y + 3});
            ImGui::SetNextItemWidth(inputW - 36);
            const bool enterPressed = ImGui::InputText(
                "##friend-search-input",
                m_impl->searchInput.data(),
                m_impl->searchInput.size(),
                ImGuiInputTextFlags_EnterReturnsTrue);
            const bool inputChanged = ImGui::IsItemEdited();
            if(m_impl->searchInput.front() == '\0' && !ImGui::IsItemActive()){
                drawText(list, {contentMin.x + 33, contentMin.y + 8}, "输入用户ID或角色名", 14, IM_COL32(128,128,128,255));
            }
            if(inputChanged){
                const std::string query = m_impl->searchInput.data();
                m_impl->lastSearch = query;
                m_impl->searchResults.clear();
                m_impl->searchShowCandidates = false;
                if(!query.empty()){
                    CMQueryChatPeerList message {};
                    message.input.assign(query);
                    g_client->send({CM_QUERYCHATPEERLIST, message}, [query, this](const uint8_t headCode, const uint8_t *data, const size_t size)
                    {
                        if(headCode == SM_OK && query == m_impl->lastSearch){
                            m_impl->searchResults = cerealf::deserialize<SDChatPeerList>(data, size);
                        }
                        else if(headCode != SM_OK){
                            throw fflpanic("query failed in server");
                        }
                    });
                }
            }
            if(enterPressed && m_impl->searchInput.front() != '\0'){
                m_impl->searchShowCandidates = true;
            }

            const auto clearText = g_fontexDB->retrieve(1, 15, 0, "清空");
            const ImVec2 clearPos {contentMin.x + inputW + clearGap, contentMin.y + 7};
            ImGui::SetCursorScreenPos({clearPos.x, contentMin.y});
            if(ImGui::InvisibleButton("##friend-search-clear", {contentMax.x - clearPos.x, inputH})){
                m_impl->searchInput.fill(0);
                m_impl->lastSearch.clear();
                m_impl->searchResults.clear();
                m_impl->searchShowCandidates = false;
            }
            if(clearText){
                list->AddImage(clearText, clearPos, {clearPos.x + clearText.w, clearPos.y + clearText.h});
            }

            const float rowH = m_impl->searchShowCandidates ? 52.0f : 30.0f;
            for(size_t i = 0; i < m_impl->searchResults.size(); ++i){
                const auto &peer = m_impl->searchResults[i];
                const ImVec2 rowPos {contentMin.x, contentMin.y + inputH + i * rowH - scroll};
                ImGui::PushID(to_d(peer.cpid().asU64()));
                ImGui::SetCursorScreenPos(rowPos);
                ImGui::InvisibleButton("##friend-search-row", {contentW, rowH});
                const bool hovered = ImGui::IsItemHovered();
                const bool clicked = ImGui::IsItemClicked();
                ImGui::PopID();
                list->AddRectFilled(
                    rowPos,
                    {rowPos.x + contentW, rowPos.y + rowH},
                    hovered ? IM_COL32(231,231,189,64) : IM_COL32(128,128,128,64));
                list->AddRect(rowPos, {rowPos.x + contentW, rowPos.y + rowH}, IM_COL32(231,231,189, hovered ? 64 : 32));

                if(m_impl->searchShowCandidates){
                    drawTexture(list, peerAvatar(peer), {rowPos.x + 4, rowPos.y + 4}, {40, 44});
                    drawText(list, {rowPos.x + 52, rowPos.y + 10}, str_printf("%s（%llu）", peer.name.c_str(), to_llu(peer.id)), 14);
                    if(peer.id != m_impl->processRun->getMyHeroDBID()){
                        ImGui::PushID(to_d(peer.cpid().asU64()));
                        ImGui::SetCursorScreenPos({rowPos.x + contentW - 48, rowPos.y + 17});
                        if(ImGui::SmallButton("添加")){
                            requestAddFriend(peer, true);
                        }
                        ImGui::PopID();
                    }
                }
                else{
                    drawTexture(list, g_progUseDB->retrieve(0X00001200), {rowPos.x + 8, rowPos.y + 5}, {20, 20});
                    const auto query = m_impl->lastSearch;
                    const bool byID = query == std::to_string(peer.id);
                    std::string prefix;
                    std::string match;
                    std::string suffix;
                    if(byID){
                        prefix = peer.name + "（";
                        match = std::to_string(peer.id);
                        suffix = "）";
                    }
                    else if(const auto matchPos = peer.name.find(query); matchPos != std::string::npos){
                        prefix = peer.name.substr(0, matchPos);
                        match = query;
                        suffix = peer.name.substr(matchPos + query.size()) + str_printf("（%llu）", to_llu(peer.id));
                    }
                    else{
                        prefix = str_printf("%s（%llu）", peer.name.c_str(), to_llu(peer.id));
                    }
                    ImVec2 labelPos {rowPos.x + 33, rowPos.y + 8};
                    labelPos.x += drawText(list, labelPos, prefix, 14).x;
                    labelPos.x += drawText(list, labelPos, match, 14, IM_COL32(255,0,0,255)).x;
                    drawText(list, labelPos, suffix, 14);
                    if(clicked){
                        const auto selectedInput = byID ? std::to_string(peer.id) : peer.name;
                        std::snprintf(m_impl->searchInput.data(), m_impl->searchInput.size(), "%s", selectedInput.c_str());
                        m_impl->searchShowCandidates = true;
                        scroll = 0.0f;
                    }
                }
            }
            wheelScroll(inputH + m_impl->searchResults.size() * rowH);
        }
        else if(m_impl->page == pageChat){
            const float inputH = 82;
            const ImVec2 messageMax {contentMax.x, contentMax.y - inputH - 4};
            float y = contentMin.y - scroll;
            const auto drawMessage = [&](const SDChatMessage &message, const bool pending)
            {
                const bool mine = message.from == m_impl->processRun->getMyHeroChatPeer().cpid();
                const auto sender = findPeer(message.from);
                const auto text = messageText(message);
                const auto xml = [&]() -> std::string
                {
                    try{ return cerealf::deserialize<std::string>(message.message); }
                    catch(...){ return {}; }
                }();
                const bool friendRequest = xml.find(SYS_AFRESP) != std::string::npos;
                const float bubbleW = std::clamp(40.0f + to_f(text.size()) * 6.0f, 80.0f, contentW - 70.0f);
                const float bubbleH = 48.0f + (message.refer ? 20.0f : 0.0f) + (friendRequest ? 100.0f : 0.0f);
                const float avatarX = mine ? contentMax.x - 42 : contentMin.x;
                const float bubbleX = mine ? avatarX - 8 - bubbleW : avatarX + 50;
                drawTexture(list, sender ? peerAvatar(*sender) : g_progUseDB->retrieve(0X010007CF), {avatarX, y}, {42,42});
                const ImU32 bubbleColor = mine
                                        ? (pending ? IM_COL32(128,128,128,128) : IM_COL32(0,128,0,128))
                                        : IM_COL32(255,0,0,128);
                list->AddRectFilled({bubbleX, y}, {bubbleX + bubbleW, y + bubbleH}, bubbleColor, 4);
                if(!mine && sender){
                    drawText(list, {bubbleX + 8, y + 3}, sender->name, 10);
                }
                drawText(list, {bubbleX + 8, y + (mine ? 15.0f : 19.0f)}, text, 12);
                float extraY = y + 44;
                if(message.refer){
                    std::string reference = str_printf("引用消息 #%llu", to_llu(message.refer.value()));
                    if(const auto cached = m_impl->cachedMessages.find(message.refer.value()); cached != m_impl->cachedMessages.end()){
                        reference = "引用：" + messageText(cached->second);
                    }
                    drawText(list, {bubbleX + 8, extraY}, reference, 10, IM_COL32(200,200,200,255));
                    extraY += 20;
                }

                ImGui::PushID(std::addressof(message));
                ImGui::SetCursorScreenPos({bubbleX, y});
                ImGui::InvisibleButton("##friend-message-bubble", {bubbleW, bubbleH}, ImGuiButtonFlags_MouseButtonRight);
                if(ImGui::IsItemClicked(ImGuiMouseButton_Right) && message.seq){
                    ImGui::OpenPopup("##friend-message-menu");
                }
                if(ImGui::BeginPopup("##friend-message-menu")){
                    if(ImGui::MenuItem("引用") && message.seq){
                        m_impl->referMessageID = message.seq->id;
                        m_impl->referPreview = (sender ? sender->name : "[未知]") + std::string("：") + text.substr(0, 150);
                    }
                    if(ImGui::MenuItem("复制")){
                        g_client->Clipboard(text);
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();

                if(friendRequest){
                    const auto cpidText = xmlAttribute(xml, "cpid");
                    const auto action = [&](const char *id, const char *label, const bool accept, const bool addFriend, const bool block)
                    {
                        ImGui::PushID(id);
                        ImGui::SetCursorScreenPos({bubbleX + 8, extraY});
                        if(ImGui::SmallButton(label) && cpidText){
                            queryPeer(SDChatPeerID(std::stoull(cpidText.value())), [accept, addFriend, block, this](const SDChatPeer *peer, bool)
                            {
                                if(!peer){
                                    return;
                                }
                                if(accept){
                                    requestAcceptFriend(*peer);
                                    if(addFriend && !findFriend(peer->cpid())){
                                        requestAddFriend(*peer, false);
                                    }
                                }
                                else{
                                    requestRejectFriend(*peer);
                                    if(block){
                                        requestBlockPlayer(*peer);
                                    }
                                }
                            });
                        }
                        extraY += 22;
                        ImGui::PopID();
                    };
                    if(xml.find("accept=\"\"") != std::string::npos){
                        action("accept", "同意", true, false, false);
                    }
                    if(xml.find("addfriend=\"\"") != std::string::npos){
                        action("accept-add", "同意并添加对方为好友", true, true, false);
                    }
                    if(xml.find("reject=\"\"") != std::string::npos){
                        action("reject", "拒绝", false, false, false);
                    }
                    if(xml.find("block=\"\"") != std::string::npos){
                        action("reject-block", "拒绝并加入黑名单", false, false, true);
                    }
                }
                y += bubbleH + 10;
            };
            if(auto conversation = std::find_if(m_impl->conversations.begin(), m_impl->conversations.end(), [&](const Impl::Conversation &item)
            {
                return item.cpid == m_impl->selectedPeer.cpid();
            }); conversation != m_impl->conversations.end()){
                for(const auto &message: conversation->messages){
                    drawMessage(message, false);
                }
            }
            for(const auto &[_, message]: m_impl->pendingMessages){
                if(message.to == m_impl->selectedPeer.cpid()){
                    drawMessage(message, true);
                }
            }

            const auto &peer = m_impl->selectedPeer;
            const bool needOps = !peer.special()
                              && !peer.group()
                              && !(peer.player() && peer.id == m_impl->processRun->getMyHeroDBID())
                              && !findFriend(peer.cpid());
            if(needOps){
                const ImVec2 opsMin {contentMin.x, y};
                const ImVec2 opsMax {contentMax.x, y + 54};
                list->AddRectFilled(opsMin, opsMax, IM_COL32(231,231,189,64), 4);
                drawText(list, {opsMin.x + 8, opsMin.y + 6}, "对方不是你的好友，你可以添加对方为好友，或者屏蔽对方的消息。", 12);
                ImGui::SetCursorScreenPos({opsMin.x + 8, opsMin.y + 27});
                if(ImGui::SmallButton("添加##friend-chat-add")){
                    requestAddFriend(peer, false);
                }
                ImGui::SameLine();
                ImGui::SmallButton("屏蔽##friend-chat-block");
                y += 64;
            }

            const float messageHeight = y + scroll - contentMin.y;
            const float messageReach = std::max(0.0f, messageHeight - (messageMax.y - contentMin.y));
            m_impl->scrollReach.at(m_impl->page) = messageReach;
            if(ImGui::IsMouseHoveringRect(contentMin, messageMax) && ImGui::GetIO().MouseWheel != 0.0f){
                scroll = std::clamp(scroll - ImGui::GetIO().MouseWheel * 45.0f, 0.0f, messageReach);
            }
            scroll = std::clamp(scroll, 0.0f, messageReach);

            list->AddLine({contentMin.x, contentMax.y - inputH}, {contentMax.x, contentMax.y - inputH}, IM_COL32(231,231,189,96));
            float inputY = contentMax.y - inputH + 4;
            float actualInputH = inputH - 8;
            if(m_impl->referMessageID){
                list->AddRectFilled({contentMin.x, inputY}, {contentMax.x, inputY + 20}, IM_COL32(64,64,64,180));
                drawText(list, {contentMin.x + 5, inputY + 3}, m_impl->referPreview, 10);
                ImGui::SetCursorScreenPos({contentMax.x - 22, inputY + 1});
                if(ImGui::SmallButton("×##friend-clear-reference")){
                    m_impl->referMessageID.reset();
                    m_impl->referPreview.clear();
                }
                inputY += 22;
                actualInputH -= 22;
            }
            ImGui::SetCursorScreenPos({contentMin.x, inputY});
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0,0,0,80));
            ImGui::InputTextMultiline(
                "##friend-chat-input",
                m_impl->chatInput.data(),
                m_impl->chatInput.size(),
                {contentW, actualInputH},
                ImGuiInputTextFlags_CtrlEnterForNewLine);
            const bool send = ImGui::IsItemActive()
                           && ImGui::IsKeyPressed(ImGuiKey_Enter, false)
                           && !ImGui::GetIO().KeyShift
                           && !ImGui::GetIO().KeyCtrl;
            ImGui::PopStyleColor();
            if(send){
                sendCurrentMessage();
            }
        }
        list->PopClipRect();

        {
            const float barX = pos.x + m_impl->boardSize.x - 30;
            const float barY = pos.y + 70;
            const float barH = m_impl->boardSize.y - 140;
            const auto sliderTexture = g_progUseDB->retrieve(0X00000089);
            const auto reach = m_impl->scrollReach.at(m_impl->page);
            ImGui::SetCursorScreenPos({barX - 10, barY - 12});
            ImGui::InvisibleButton("##friend-scroll-slider", {20, barH + 24});
            if(reach > 0.0f && (ImGui::IsItemClicked() || ImGui::IsItemActive())){
                const float value = std::clamp((ImGui::GetIO().MousePos.y - barY) / barH, 0.0f, 1.0f);
                m_impl->scroll.at(m_impl->page) = value * reach;
            }
            const float value = reach > 0.0f ? m_impl->scroll.at(m_impl->page) / reach : 0.0f;
            drawTexture(
                list,
                sliderTexture,
                {barX - 10, barY + value * barH - 12});
        }

        if(textureButton(list, "##friend-close", {pos.x + m_impl->boardSize.x - 38, pos.y + m_impl->boardSize.y - 40}, 0X0000001C, 0X0000001D)){
            setShow(false);
        }

        const auto mouse = ImGui::GetIO().MousePos;
        const float localX = mouse.x - pos.x;
        const float localY = mouse.y - pos.y;
        const bool left = localX < 12;
        const bool right = localX >= m_impl->boardSize.x - 10;
        const bool top = localY < 10;
        const bool bottom = localY >= m_impl->boardSize.y - 10;
        const int hoverEdge = top ? (left ? 0 : right ? 2 : 1)
                            : bottom ? (left ? 5 : right ? 7 : 6)
                            : left ? 3 : right ? 4 : -1;
        if((hoverEdge >= 0 || m_impl->resizeEdge >= 0)){
            list->AddRectFilled(pos, {pos.x + m_impl->boardSize.x, pos.y + 10}, IM_COL32(231,231,189,64));
            list->AddRectFilled({pos.x, pos.y + m_impl->boardSize.y - 10}, {pos.x + m_impl->boardSize.x, pos.y + m_impl->boardSize.y}, IM_COL32(231,231,189,64));
            list->AddRectFilled(pos, {pos.x + 12, pos.y + m_impl->boardSize.y}, IM_COL32(231,231,189,64));
            list->AddRectFilled({pos.x + m_impl->boardSize.x - 10, pos.y}, {pos.x + m_impl->boardSize.x, pos.y + m_impl->boardSize.y}, IM_COL32(231,231,189,64));
        }
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()){
            if(hoverEdge >= 0){ m_impl->resizeEdge = hoverEdge; }
            else{ m_impl->dragging = true; }
        }
        if(!ImGui::IsMouseDown(ImGuiMouseButton_Left)){
            m_impl->resizeEdge = -1;
            m_impl->dragging = false;
        }
        const auto delta = ImGui::GetIO().MouseDelta;
        if(m_impl->dragging){
            moveTo(pos.x + delta.x, pos.y + delta.y);
        }
        else if(m_impl->resizeEdge >= 0){
            ImVec2 newPos = pos;
            ImVec2 newSize = m_impl->boardSize;
            if(m_impl->resizeEdge == 0 || m_impl->resizeEdge == 3 || m_impl->resizeEdge == 5){
                const float applied = std::min(delta.x, newSize.x - minBoardW);
                newPos.x += applied;
                newSize.x -= applied;
            }
            if(m_impl->resizeEdge == 2 || m_impl->resizeEdge == 4 || m_impl->resizeEdge == 7){
                newSize.x = std::max(minBoardW, newSize.x + delta.x);
            }
            if(m_impl->resizeEdge <= 2){
                const float applied = std::min(delta.y, newSize.y - minBoardH);
                newPos.y += applied;
                newSize.y -= applied;
            }
            if(m_impl->resizeEdge >= 5){
                newSize.y = std::max(minBoardH, newSize.y + delta.y);
            }
            m_impl->boardSize = newSize;
            moveTo(newPos.x, newPos.y);
        }
    }
    endWindow();
}

bool ImFriendChatBoard::processEvent(const MirEvent &event) const
{
    if(!show()){
        return false;
    }
    if(event.type == MIR_EVENT_KEY_DOWN && event.key.key == MIRK_ESCAPE){
        setShow(false);
        return true;
    }
    return ImBoard::processEvent(event);
}

const SDChatPeer *ImFriendChatBoard::findFriend(const SDChatPeerID &cpid) const
{
    if(const auto p = std::find_if(m_impl->friends.begin(), m_impl->friends.end(), [&](const SDChatPeer &peer){ return peer.cpid() == cpid; }); p != m_impl->friends.end()){
        return std::addressof(*p);
    }
    return nullptr;
}

const SDChatPeer *ImFriendChatBoard::findPeer(const SDChatPeerID &cpid) const
{
    if(const auto peer = findFriend(cpid)){
        return peer;
    }
    if(const auto p = std::find_if(m_impl->cachedPeers.begin(), m_impl->cachedPeers.end(), [&](const SDChatPeer &peer){ return peer.cpid() == cpid; }); p != m_impl->cachedPeers.end()){
        return std::addressof(*p);
    }
    return nullptr;
}

void ImFriendChatBoard::setFriendList(const SDFriendList &friends)
{
    m_impl->friends = friends;
    for(const auto &peer: friends){
        if(!findPeer(peer.cpid())){
            m_impl->cachedPeers.push_back(peer);
        }
    }
}

void ImFriendChatBoard::openChat(const SDChatPeer &peer, const bool requestLatest) const
{
    m_impl->selectedPeer = peer;
    m_impl->previousPage = m_impl->page;
    m_impl->page = pageChat;
    if(requestLatest){
        m_impl->processRun->requestLatestChatMessage({peer.cpid().asU64()}, 50, true, true);
    }
}

void ImFriendChatBoard::queryPeer(const SDChatPeerID &cpid, std::function<void(const SDChatPeer *, bool)> callback) const
{
    if(const auto peer = cpid.group() ? nullptr : findPeer(cpid)){
        callback(peer, false);
        return;
    }
    CMQueryChatPeerList message {};
    message.input.assign(std::to_string(cpid.id()));
    g_client->send({CM_QUERYCHATPEERLIST, message}, [cpid, callback = std::move(callback), this](const uint8_t headCode, const uint8_t *data, const size_t size)
    {
        if(headCode != SM_OK){
            callback(nullptr, true);
            return;
        }
        for(const auto &peer: cerealf::deserialize<SDChatPeerList>(data, size)){
            if(peer.cpid() == cpid){
                m_impl->cachedPeers.push_back(peer);
                callback(std::addressof(m_impl->cachedPeers.back()), true);
                return;
            }
        }
        callback(nullptr, true);
    });
}

void ImFriendChatBoard::queryMessage(const uint64_t messageID, std::function<void(const SDChatMessage *, bool)> callback) const
{
    if(const auto cached = m_impl->cachedMessages.find(messageID); cached != m_impl->cachedMessages.end()){
        callback(std::addressof(cached->second), false);
        return;
    }
    CMQueryChatMessage message {};
    message.msgid = messageID;
    g_client->send({CM_QUERYCHATMESSAGE, message}, [messageID, callback = std::move(callback), this](const uint8_t headCode, const uint8_t *data, const size_t size)
    {
        if(headCode != SM_OK){
            callback(nullptr, true);
            return;
        }
        const auto queried = cerealf::deserialize<SDChatMessage>(data, size);
        const auto [iter, _] = m_impl->cachedMessages.insert_or_assign(messageID, queried);
        callback(std::addressof(iter->second), true);
    });
}

void ImFriendChatBoard::addMessage(const std::optional<uint64_t> localPendingID, const SDChatMessage &message)
{
    fflassert(message.seq.has_value());
    m_impl->cachedMessages.insert_or_assign(message.seq->id, message);
    if(message.refer && !m_impl->cachedMessages.contains(message.refer.value())){
        queryMessage(message.refer.value(), [](const SDChatMessage *, bool){});
    }
    if(localPendingID){
        m_impl->pendingMessages.erase(localPendingID.value());
    }
    const auto self = m_impl->processRun->getMyHeroChatPeer().cpid();
    const auto peerCPID = message.from == self ? message.to
                        : message.to == self ? message.from
                        : message.to.group() ? message.to
                        : SDChatPeerID {};
    if(!peerCPID){
        throw fflpanic("received invalid chat message");
    }

    auto conversation = std::find_if(m_impl->conversations.begin(), m_impl->conversations.end(), [&](const Impl::Conversation &item){ return item.cpid == peerCPID; });
    if(conversation == m_impl->conversations.end()){
        m_impl->conversations.push_front({.cpid = peerCPID});
    }
    else if(conversation != m_impl->conversations.begin()){
        m_impl->conversations.splice(m_impl->conversations.begin(), m_impl->conversations, conversation);
    }
    conversation = m_impl->conversations.begin();
    if(std::find_if(conversation->messages.begin(), conversation->messages.end(), [&](const SDChatMessage &item)
    {
        return item.seq && item.seq->id == message.seq->id;
    }) == conversation->messages.end()){
        conversation->messages.push_back(message);
        std::sort(conversation->messages.begin(), conversation->messages.end(), [](const SDChatMessage &left, const SDChatMessage &right)
        {
            return std::tie(left.seq->timestamp, left.seq->id) < std::tie(right.seq->timestamp, right.seq->id);
        });
        conversation->preview = messageText(message);
        conversation->unread++;
    }
    if(!findPeer(peerCPID)){
        queryPeer(peerCPID, [](const SDChatPeer *, bool){});
    }
}

void ImFriendChatBoard::sendCurrentMessage() const
{
    std::string text = m_impl->chatInput.data();
    while(!text.empty() && (text.back() == '\n' || text.back() == '\r')){
        text.pop_back();
    }
    if(text.empty() || m_impl->selectedPeer.empty()){
        return;
    }
    const auto xml = str_printf("<layout><par>%s</par></layout>", text.c_str());
    const SDChatMessage message
    {
        .refer = m_impl->referMessageID,
        .from = m_impl->processRun->getMyHeroChatPeer().cpid(),
        .to = m_impl->selectedPeer.cpid(),
        .message = cerealf::serialize(xml),
    };
    const uint64_t pendingID = m_impl->nextPendingID++;
    m_impl->pendingMessages.emplace(pendingID, message);
    m_impl->chatInput.fill(0);
    m_impl->referMessageID.reset();
    m_impl->referPreview.clear();

    CMChatMessageHeader header {};
    header.toCPID = message.to.asU64();
    header.hasRef = to_boolint(message.refer.has_value());
    header.refID = message.refer.value_or(0);
    std::string buffer(as_sv(header));
    buffer.append(message.message);
    g_client->send({CM_CHATMESSAGE, buffer}, [pendingID, this](const uint8_t headCode, const uint8_t *data, const size_t size)
    {
        if(headCode != SM_OK){
            m_impl->pendingMessages.erase(pendingID);
            throw fflpanic("failed to send message");
        }
        if(const auto pending = m_impl->pendingMessages.find(pendingID); pending != m_impl->pendingMessages.end()){
            auto sent = pending->second;
            m_impl->pendingMessages.erase(pending);
            sent.seq = cerealf::deserialize<SDChatMessageDBSeq>(data, size);
            const_cast<ImFriendChatBoard *>(this)->addMessage({}, sent);
        }
    });
}

void ImFriendChatBoard::requestAddFriend(const SDChatPeer &peer, const bool switchToPreview) const
{
    CMAddFriend message {};
    message.cpid = peer.cpid().asU64();
    g_client->send({CM_ADDFRIEND, message}, [peer, switchToPreview, this](const uint8_t headCode, const uint8_t *data, const size_t size)
    {
        if(headCode != SM_OK){
            m_impl->processRun->addCBParLog(u8"<par bgcolor=\"rgb(0x00, 0x80, 0x00)\">无效的请求。</par>");
            return;
        }
        switch(cerealf::deserialize<SDAddFriendNotif>(data, size).notif){
            case AF_ACCEPTED:
                const_cast<ImFriendChatBoard *>(this)->onAddFriendAccepted(peer);
                if(switchToPreview){ m_impl->page = pagePreview; }
                break;
            case AF_REJECTED: const_cast<ImFriendChatBoard *>(this)->onAddFriendRejected(peer); break;
            case AF_EXIST: m_impl->processRun->addCBLog(CBLOG_SYS, u8"重复添加好友%s", to_cstr(peer.name)); break;
            case AF_PENDING: m_impl->processRun->addCBLog(CBLOG_SYS, u8"等待%s处理你的好友验证", to_cstr(peer.name)); break;
            case AF_BLOCKED: m_impl->processRun->addCBLog(CBLOG_SYS, u8"你已经被%s加入了黑名单", to_cstr(peer.name)); break;
            default: break;
        }
    });
}

void ImFriendChatBoard::requestAcceptFriend(const SDChatPeer &peer) const
{
    CMAcceptAddFriend message {};
    message.cpid = peer.cpid().asU64();
    g_client->send({CM_ACCEPTADDFRIEND, message}, [peer, this](const uint8_t headCode, const uint8_t *, const size_t)
    {
        m_impl->processRun->addCBLog(headCode == SM_OK ? CBLOG_SYS : CBLOG_ERR, headCode == SM_OK ? u8"你已经通过%s的好友申请" : u8"无效的请求", to_cstr(peer.name));
    });
}

void ImFriendChatBoard::requestRejectFriend(const SDChatPeer &peer) const
{
    CMRejectAddFriend message {};
    message.cpid = peer.cpid().asU64();
    g_client->send({CM_REJECTADDFRIEND, message}, [peer, this](const uint8_t headCode, const uint8_t *, const size_t)
    {
        m_impl->processRun->addCBLog(headCode == SM_OK ? CBLOG_SYS : CBLOG_ERR, headCode == SM_OK ? u8"你已经拒绝%s的好友申请" : u8"无效的请求", to_cstr(peer.name));
    });
}

void ImFriendChatBoard::requestBlockPlayer(const SDChatPeer &peer) const
{
    CMBlockPlayer message {};
    message.cpid = peer.cpid().asU64();
    g_client->send({CM_BLOCKPLAYER, message}, [peer, this](const uint8_t headCode, const uint8_t *, const size_t)
    {
        m_impl->processRun->addCBLog(headCode == SM_OK ? CBLOG_SYS : CBLOG_ERR, headCode == SM_OK ? u8"你已经拉黑%s" : u8"无效的拉黑请求", to_cstr(peer.name));
    });
}

void ImFriendChatBoard::createSelectedGroup() const
{
    if(m_impl->selectedGroupPeers.empty()){
        return;
    }
    std::vector<uint32_t> ids;
    for(const auto encoded: m_impl->selectedGroupPeers){
        ids.push_back(SDChatPeerID(encoded).id());
    }
    if(ids.size() > CMCreateChatGroup().list.capacity()){
        throw fflpanic("selected too many friends, max {}", CMCreateChatGroup().list.capacity());
    }
    m_impl->processRun->getInputStringBoard()->waitInput(u8"<layout><par>请输入你要建立的群名称</par></layout>", false, [ids = std::move(ids), this](std::u8string name)
    {
        if(name.empty()){
            m_impl->processRun->addCBLog(CBLOG_ERR, u8"无效输入:%s", to_cstr(name));
            return;
        }
        CMCreateChatGroup message {};
        message.name.assign(name);
        message.list.assign(ids.begin(), ids.end());
        g_client->send({CM_CREATECHATGROUP, message}, [this](const uint8_t headCode, const uint8_t *data, const size_t size)
        {
            if(headCode == SM_OK){
                const_cast<ImFriendChatBoard *>(this)->addGroup(cerealf::deserialize<SDChatPeer>(data, size));
            }
        });
    });
}

void ImFriendChatBoard::addGroup(const SDChatPeer &peer)
{
    if(findPeer(peer.cpid())){
        return;
    }
    m_impl->friends.push_back(peer);
    m_impl->cachedPeers.push_back(peer);
    m_impl->conversations.push_front({.cpid = peer.cpid(), .preview = "你已经加入了群聊，现在就可以聊天了。"});
}

void ImFriendChatBoard::onAddFriendAccepted(const SDChatPeer &peer)
{
    if(!findFriend(peer.cpid())){
        m_impl->friends.push_back(peer);
        m_impl->cachedPeers.push_back(peer);
        m_impl->conversations.push_front({.cpid = peer.cpid(), .preview = str_printf("%s已经通过你的好友申请，现在可以开始聊天了。", peer.name.c_str())});
        m_impl->processRun->addCBLog(CBLOG_SYS, u8"%s已经通过了你的好友请求", to_cstr(peer.name));
    }
}

void ImFriendChatBoard::onAddFriendRejected(const SDChatPeer &peer)
{
    m_impl->processRun->addCBLog(CBLOG_SYS, u8"%s已经拒绝了你的好友请求", to_cstr(peer.name));
}
