#include "ImNPCChatBoard.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "clientargparser.hpp"
#include "colorf.hpp"
#include "fflerror.hpp"
#include "gldevice.hpp"
#include "gui_font.hpp"
#include "gui_textengine.hpp"
#include "gui_texture.hpp"
#include "lalign.hpp"
#include "processrun.hpp"
#include "sysconst.hpp"
#include "tinyxml2.h"
#include "totype.hpp"
#include "uidf.hpp"

extern PNGTexDB *g_progUseDB;
extern FontexDB *g_fontexDB;
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

struct ImNPCChatBoard::Impl
{
    struct Paragraph
    {
        int startY = 0;
        std::unique_ptr<XMLTypeset> typeset;
    };

    int lineWidth = 0;
    int width = 0;
    int height = 0;
    std::vector<Paragraph> paragraphs;

    void clear()
    {
        width = 0;
        height = 0;
        paragraphs.clear();
    }

    void loadXML(const char *xmlString)
    {
        clear();

        tinyxml2::XMLDocument document(true, tinyxml2::PEDANTIC_WHITESPACE);
        if(document.Parse(xmlString) != tinyxml2::XML_SUCCESS){
            throw fflpanic("parse xml failed: {}", xmlString);
        }

        const auto root = document.RootElement();
        if(!root || (to_sv(root->Name()) != "layout" && to_sv(root->Name()) != "Layout" && to_sv(root->Name()) != "LAYOUT")){
            throw fflpanic("string is not layout xml");
        }

        for(auto node = root->FirstChildElement(); node; node = node->NextSiblingElement()){
            if(to_sv(node->Name()) != "par" && to_sv(node->Name()) != "Par" && to_sv(node->Name()) != "PAR"){
                continue;
            }

            int paragraphWidth = lineWidth;
            node->QueryIntAttribute("lineWidth", &paragraphWidth);

            int align = LALIGN_JUSTIFY;
            if(const auto value = node->Attribute("align")){
                if     (to_sv(value) == "left"       ){ align = LALIGN_LEFT;        }
                else if(to_sv(value) == "right"      ){ align = LALIGN_RIGHT;       }
                else if(to_sv(value) == "center"     ){ align = LALIGN_CENTER;      }
                else if(to_sv(value) == "justify"    ){ align = LALIGN_JUSTIFY;     }
                else if(to_sv(value) == "distributed"){ align = LALIGN_DISTRIBUTED; }
            }

            bool canThrough = false;
            bool compactLine = false;
            node->QueryBoolAttribute("canThrough", &canThrough);
            node->QueryBoolAttribute("compactLine", &compactLine);

            int font = 11;
            if(const auto value = node->Attribute("font")){
                try{
                    font = std::stoi(value);
                }
                catch(...){
                    font = g_fontexDB->findFontName(value);
                }
            }
            if(!g_fontexDB->hasFont(font)){
                font = 0;
            }

            int fontSize = 15;
            node->QueryIntAttribute("size", &fontSize);

            uint32_t fontColor = colorf::WHITE_A255;
            uint32_t fontBGColor = 0;
            if(const auto value = node->Attribute("color")){
                fontColor = colorf::string2RGBA(value);
            }
            if(const auto value = node->Attribute("bgcolor")){
                fontBGColor = colorf::string2RGBA(value);
            }

            int lineSpace = 0;
            int wordSpace = 0;
            node->QueryIntAttribute("lineSpace", &lineSpace);
            node->QueryIntAttribute("wordSpace", &wordSpace);

            auto typeset = std::make_unique<XMLTypeset>(
                fflcheck(paragraphWidth, paragraphWidth >= 0),
                align,
                canThrough,
                compactLine,
                to_u8(font),
                to_u8(fflcheck(fontSize, fontSize >= 0 && fontSize < 255)),
                0,
                fontColor,
                fontBGColor,
                colorf::WHITE_A255,
                fflcheck(lineSpace, lineSpace >= 0),
                fflcheck(wordSpace, wordSpace >= 0));
            typeset->loadXMLNode(node);

            const int startY = paragraphs.empty()
                             ? 0
                             : paragraphs.back().startY
                                 + std::max(
                                     paragraphs.back().typeset->ph(),
                                     paragraphs.back().typeset->getDefaultFontHeight());
            width = std::max(width, typeset->pw());
            height = startY + std::max(typeset->ph(), typeset->getDefaultFontHeight());
            paragraphs.push_back({startY, std::move(typeset)});
        }
    }
};

ImNPCChatBoard::ImNPCChatBoard(ProcessRun *processRun)
    : ImBoard("##npc-chat-board")
    , m_impl(std::make_unique<Impl>())
    , m_processRun(fflcheck(processRun))
{
    moveTo(0.0f, 0.0f);
}

ImNPCChatBoard::~ImNPCChatBoard() = default;

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
                        : to_dround(pos.x + (frameSize.x - m_impl->width) * 0.5f);
        const int chatY = to_dround(pos.y + (frameSize.y - m_impl->height) * 0.5f);
        for(size_t i = 0; i < m_impl->paragraphs.size(); ++i){
            const auto &paragraph = m_impl->paragraphs[i];
            const int paragraphY = chatY + paragraph.startY;
            const int paragraphH = std::max(paragraph.typeset->ph(), paragraph.typeset->getDefaultFontHeight());

            ImGui::PushID(to_d(i));
            ImGui::SetCursorScreenPos({to_f(chatX), to_f(paragraphY)});
            const bool paragraphPressed = ImGui::InvisibleButton(
                "##npc-chat-paragraph",
                {to_f(std::max(m_impl->lineWidth, paragraph.typeset->pw())), to_f(paragraphH)});

            int activeLeaf = -1;
            if(ImGui::IsItemHovered()){
                const auto mouse = ImGui::GetIO().MousePos;
                const auto [tokenX, tokenY] = paragraph.typeset->locToken(
                    to_dround(mouse.x) - chatX,
                    to_dround(mouse.y) - paragraphY,
                    true);
                if(paragraph.typeset->tokenLocValid(tokenX, tokenY)){
                    const int leaf = paragraph.typeset->getToken(tokenX, tokenY)->leaf;
                    if(paragraph.typeset->leafEvent(leaf)){
                        activeLeaf = leaf;
                    }
                }
            }

            paragraph.typeset->clearEvent(activeLeaf);
            if(activeLeaf >= 0){
                paragraph.typeset->markLeafEvent(
                    activeLeaf,
                    ImGui::IsItemActive() ? BEVENT_DOWN : BEVENT_ON);
                if(paragraphPressed){
                    const auto attributes = paragraph.typeset->leafEvent(activeLeaf);
                    const auto findAttribute = [attributes](const char *name, const char *fallback) -> const char *
                    {
                        if(const auto p = attributes->find(name); p != attributes->end()){
                            return p->second.c_str();
                        }
                        return fallback;
                    };
                    if(const auto id = findAttribute("id", nullptr)){
                        const auto close = findAttribute("close", nullptr);
                        onClickEvent(
                            findAttribute("path", m_eventPath.c_str()),
                            id,
                            findAttribute("args", nullptr),
                            close ? to_parsedbool(close) : false);
                    }
                }
            }
            paragraph.typeset->drawImGui(ImGui::GetWindowDrawList(), chatX, paragraphY);
            ImGui::PopID();
        }

        if(closeButton({pos.x + frameSize.x - 40, pos.y + frameSize.y - 43})){
            setShow(false);
        }
    }
    endWindow();
}

void ImNPCChatBoard::update(const double milliseconds)
{
    for(const auto &paragraph: m_impl->paragraphs){
        paragraph.typeset->update(milliseconds);
    }
}

bool ImNPCChatBoard::processEvent(const MirEvent &event) const
{
    if(!show()){
        return false;
    }
    return ImBoard::processEvent(event);
}

void ImNPCChatBoard::loadXML(const uint64_t uid, const char *eventPath, const char *xmlString)
{
    fflassert(uidf::isNPChar(uid), uidf::getUIDString(uid));
    fflassert(str_haschar(eventPath));
    fflassert(str_haschar(xmlString));

    m_npcUID = uid;
    m_eventPath = eventPath;
    m_impl->clear();

    const int boardWidth = std::max(g_glDevice->getRendererWidth() / 3, 300);
    if(const auto face = g_progUseDB->retrieve(getNPCFaceKey())){
        m_impl->lineWidth = boardWidth - margin * 3 - face.w;
    }
    else{
        m_impl->lineWidth = boardWidth - margin * 2;
    }
    m_impl->loadXML(xmlString);
}

ImVec2 ImNPCChatBoard::boardSize() const
{
    if(const auto face = g_progUseDB->retrieve(getNPCFaceKey())){
        return
        {
            to_f(margin * 3 + face.w + m_impl->width),
            to_f(margin * 2 + std::max(face.h, m_impl->height)),
        };
    }
    return
    {
        to_f(margin * 2 + m_impl->width),
        to_f(margin * 2 + m_impl->height),
    };
}

void ImNPCChatBoard::onClickEvent(const char *path, const char *id, const char *args, const bool autoClose) const
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
