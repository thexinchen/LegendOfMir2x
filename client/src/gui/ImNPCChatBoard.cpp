#include "ImNPCChatBoard.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>

#include "clientargparser.hpp"
#include "fflerror.hpp"
#include "gldevice.hpp"
#include "gui_texture.hpp"
#include "processrun.hpp"
#include "sysconst.hpp"
#include "totype.hpp"
#include "uidf.hpp"

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;
extern ClientArgParser *g_clientArgParser;

namespace
{
    void drawCompositeFrame(ImDrawList *drawList, const ImVec2 pos, const ImVec2 size)
    {
        const auto upper = g_progUseDB->retrieve(0X00000051);
        const auto lower = g_progUseDB->retrieve(0X00000053);
        if(!(upper && lower)){
            return;
        }

        constexpr int centerX = 40;
        constexpr int centerY = 50;
        constexpr int centerW = 300;
        constexpr int centerH = 110;

        const int sourceW = std::max(upper.w, lower.w);
        const int sourceH = upper.h + lower.h;
        const int rightW = sourceW - centerX - centerW;
        const int bottomH = sourceH - centerY - centerH;
        fflassert(rightW >= 0 && bottomH >= 0);

        const std::array<float, 4> sourceX
        {{
            0.0f,
            to_f(centerX),
            to_f(centerX + centerW),
            to_f(sourceW),
        }};
        const std::array<float, 4> sourceY
        {{
            0.0f,
            to_f(centerY),
            to_f(centerY + centerH),
            to_f(sourceH),
        }};
        const std::array<float, 4> targetX
        {{
            pos.x,
            pos.x + centerX,
            pos.x + size.x - rightW,
            pos.x + size.x,
        }};
        const std::array<float, 4> targetY
        {{
            pos.y,
            pos.y + centerY,
            pos.y + size.y - bottomH,
            pos.y + size.y,
        }};

        const auto drawPart = [drawList, upper, lower](
                const float sx0,
                const float sy0,
                const float sx1,
                const float sy1,
                const ImVec2 dst0,
                const ImVec2 dst1)
        {
            if(sx1 <= sx0 || sy1 <= sy0 || dst1.x <= dst0.x || dst1.y <= dst0.y){
                return;
            }

            const std::array<std::pair<GLTexID, float>, 2> textures
            {{
                {upper, 0.0f},
                {lower, to_f(upper.h)},
            }};
            for(const auto &[texture, textureY]: textures){
                const float partY0 = std::max(sy0, textureY);
                const float partY1 = std::min(sy1, textureY + texture.h);
                if(partY1 <= partY0){
                    continue;
                }

                const float ratio0 = (partY0 - sy0) / (sy1 - sy0);
                const float ratio1 = (partY1 - sy0) / (sy1 - sy0);
                drawList->AddImage(
                    texture,
                    {dst0.x, dst0.y + (dst1.y - dst0.y) * ratio0},
                    {dst1.x, dst0.y + (dst1.y - dst0.y) * ratio1},
                    {sx0 / texture.w, (partY0 - textureY) / texture.h},
                    {sx1 / texture.w, (partY1 - textureY) / texture.h});
            }
        };

        for(size_t y = 0; y < 3; ++y){
            for(size_t x = 0; x < 3; ++x){
                drawPart(
                    sourceX[x],
                    sourceY[y],
                    sourceX[x + 1],
                    sourceY[y + 1],
                    {targetX[x], targetY[y]},
                    {targetX[x + 1], targetY[y + 1]});
            }
        }
    }

    bool closeButton(const ImVec2 pos)
    {
        const auto hover = g_progUseDB->retrieve(0X0000001C);
        if(!hover){
            return false;
        }

        ImGui::SetCursorScreenPos(pos);
        const bool clicked = ImGui::InvisibleButton("##npc-chat-close", {to_f(hover.w), to_f(hover.h)});
        if(ImGui::IsItemHovered() || ImGui::IsItemActive()){
            const auto down = g_progUseDB->retrieve(0X0000001D);
            const auto shown = ImGui::IsItemActive() && down ? down : hover;
            ImGui::GetWindowDrawList()->AddImage(shown, pos, {pos.x + shown.w, pos.y + shown.h});
        }
        return clicked;
    }
}

ImNPCChatBoard::ImNPCChatBoard(ProcessRun *processRun)
    : ImBoard("##npc-chat-board")
    , m_processRun(fflcheck(processRun))
    , m_chatBoard
      {{
          .lineAlign = LALIGN_JUSTIFY,
          .onClickText = [this](const std::unordered_map<std::string, std::string> &attributes, const int event)
          {
              if(event != BEVENT_RELEASE){
                  return;
              }

              if(const auto id = LayoutBoard::findAttrValue(attributes, "id", nullptr)){
                  const auto close = LayoutBoard::findAttrValue(attributes, "close", nullptr);
                  onClickEvent(
                      LayoutBoard::findAttrValue(attributes, "path", m_eventPath.c_str()),
                      id,
                      LayoutBoard::findAttrValue(attributes, "args", nullptr),
                      close ? to_parsedbool(close) : false);
              }
          },
      }}
{
    moveTo(0.0f, 0.0f);
}

void ImNPCChatBoard::draw() const
{
    if(!show()){
        return;
    }

    const auto frameSize = boardSize();
    moveTo(0.0f, 0.0f);
    if(beginWindow(frameSize)){
        const ImVec2 pos = ImGui::GetWindowPos();
        drawCompositeFrame(ImGui::GetBackgroundDrawList(), pos, frameSize);

        const auto face = g_progUseDB->retrieve(getNPCFaceKey());
        if(face){
            ImGui::GetBackgroundDrawList()->AddImage(
                face,
                {pos.x + margin, pos.y + margin},
                {pos.x + margin + face.w, pos.y + margin + face.h});
        }

        const int chatX = face
                        ? to_dround(pos.x) + margin * 2 + face.w
                        : to_dround(pos.x + (frameSize.x - m_chatBoard.w()) * 0.5f);
        const int chatY = to_dround(pos.y + (frameSize.y - m_chatBoard.h()) * 0.5f);
        m_chatBoard.moveTo(chatX, chatY);
        m_chatBoard.drawRoot({});

        if(closeButton({pos.x + frameSize.x - 40, pos.y + frameSize.y - 43})){
            setShow(false);
        }
    }
    endWindow();
}

bool ImNPCChatBoard::processEvent(const MirEvent &event) const
{
    if(!show()){
        return false;
    }
    const bool textTookEvent = m_chatBoard.processEventRoot(event, true, {});
    return textTookEvent || ImBoard::processEvent(event);
}

void ImNPCChatBoard::loadXML(const uint64_t uid, const char *eventPath, const char *xmlString)
{
    fflassert(uidf::isNPChar(uid), uidf::getUIDString(uid));
    fflassert(str_haschar(eventPath));
    fflassert(str_haschar(xmlString));

    m_npcUID = uid;
    m_eventPath = eventPath;
    m_chatBoard.clear();

    const int boardWidth = std::max(g_glDevice->getRendererWidth() / 3, 300);
    if(const auto face = g_progUseDB->retrieve(getNPCFaceKey())){
        m_chatBoard.setLineWidth(boardWidth - margin * 3 - face.w);
    }
    else{
        m_chatBoard.setLineWidth(boardWidth - margin * 2);
    }
    m_chatBoard.loadXML(xmlString);
}

ImVec2 ImNPCChatBoard::boardSize() const
{
    if(const auto face = g_progUseDB->retrieve(getNPCFaceKey())){
        return
        {
            to_f(margin * 3 + face.w + m_chatBoard.w()),
            to_f(margin * 2 + std::max(face.h, m_chatBoard.h())),
        };
    }
    return
    {
        to_f(margin * 2 + m_chatBoard.w()),
        to_f(margin * 2 + m_chatBoard.h()),
    };
}

void ImNPCChatBoard::onClickEvent(const char *path, const char *id, const char *args, const bool autoClose)
{
    if(g_clientArgParser->debugClickEvent){
        m_processRun->addCBLog(CBLOG_SYS, u8"clickEvent: path = %s, id = %s, args = %s", to_cstr(path), to_cstr(id), to_cstr(args));
    }

    fflassert(str_haschar(id));
    m_processRun->sendNPCEvent(m_npcUID, path, id, args ? std::make_optional<std::string>(args) : std::nullopt);
    if(autoClose){
        setShow(false);
    }
}

uint32_t ImNPCChatBoard::getNPCFaceKey() const
{
    return uidf::isNPChar(m_npcUID)
         ? 0X50000000 | uidf::getNPCID(m_npcUID)
         : SYS_U32NIL;
}

