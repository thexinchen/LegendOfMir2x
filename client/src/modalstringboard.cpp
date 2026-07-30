#include "modalstringboard.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include "colorf.hpp"
#include "fflerror.hpp"
#include "gldevice.hpp"
#include "gui_textengine.hpp"
#include "gui_texture.hpp"
#include "lalign.hpp"
#include "tinyxml2.h"
#include "totype.hpp"

extern PNGTexDB *g_progUseDB;
extern GLDevice *g_glDevice;

struct ModalStringBoard::Impl
{
    struct Paragraph
    {
        int startY = 0;
        std::unique_ptr<XMLTypeset> typeset;
    };

    int width = 0;
    int height = 0;
    std::vector<Paragraph> paragraphs;

    void loadXML(const char *xmlString)
    {
        width = 0;
        height = 0;
        paragraphs.clear();

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

            int paragraphWidth = 300;
            node->QueryIntAttribute("lineWidth", &paragraphWidth);

            int align = LALIGN_JUSTIFY;
            if(const auto value = node->Attribute("align")){
                if     (to_sv(value) == "left"       ){ align = LALIGN_LEFT;        }
                else if(to_sv(value) == "right"      ){ align = LALIGN_RIGHT;       }
                else if(to_sv(value) == "center"     ){ align = LALIGN_CENTER;      }
                else if(to_sv(value) == "justify"    ){ align = LALIGN_JUSTIFY;     }
                else if(to_sv(value) == "distributed"){ align = LALIGN_DISTRIBUTED; }
            }

            int font = 1;
            int fontSize = 12;
            int lineSpace = 0;
            int wordSpace = 0;
            bool canThrough = false;
            bool compactLine = false;
            node->QueryIntAttribute("font", &font);
            node->QueryIntAttribute("size", &fontSize);
            node->QueryIntAttribute("lineSpace", &lineSpace);
            node->QueryIntAttribute("wordSpace", &wordSpace);
            node->QueryBoolAttribute("canThrough", &canThrough);
            node->QueryBoolAttribute("compactLine", &compactLine);

            uint32_t foreground = colorf::WHITE_A255;
            uint32_t background = 0;
            if(const auto value = node->Attribute("color")){
                foreground = colorf::string2RGBA(value);
            }
            if(const auto value = node->Attribute("bgcolor")){
                background = colorf::string2RGBA(value);
            }

            auto typeset = std::make_unique<XMLTypeset>(
                fflcheck(paragraphWidth, paragraphWidth >= 0),
                align,
                canThrough,
                compactLine,
                to_u8(fflcheck(font, font >= 0 && font < 255)),
                to_u8(fflcheck(fontSize, fontSize >= 0 && fontSize < 255)),
                0,
                foreground,
                background,
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

ModalStringBoard::ModalStringBoard()
    : m_impl(std::make_unique<Impl>())
{}

ModalStringBoard::~ModalStringBoard() = default;

void ModalStringBoard::loadXML(std::u8string xmlString)
{
    if(xmlString == m_xmlString){
        return;
    }
    m_xmlString = std::move(xmlString);
    m_impl->loadXML(to_cstr(m_xmlString));
}

void ModalStringBoard::drawScreen(const bool drainEvents) const
{
    if(drainEvents){
        MirEvent event;
        while(g_glDevice->pollEvent(&event)){
            continue;
        }
    }

    GLDeviceHelper::RenderNewFrame frame;
    const auto texture = g_progUseDB->retrieve(0X07000000);
    if(!texture){
        return;
    }

    constexpr int fixedTop = 84;
    constexpr int sourceUpperHeight = 180;
    constexpr int fixedBottom = 40;
    constexpr int minUpperHeight = 220;
    const int upperHeight = std::max(minUpperHeight, fixedTop + m_impl->height + 60);
    const int panelHeight = upperHeight + fixedBottom;
    const int panelX = (g_glDevice->getRendererWidth() - texture.w) / 2;
    const int panelY = (g_glDevice->getRendererHeight() - panelHeight) / 2;

    auto drawList = ImGui::GetBackgroundDrawList();
    drawList->AddImage(
        texture,
        {to_f(panelX), to_f(panelY)},
        {to_f(panelX + texture.w), to_f(panelY + fixedTop)},
        {0.0f, 0.0f},
        {1.0f, to_f(fixedTop) / texture.h});
    drawList->AddImage(
        texture,
        {to_f(panelX), to_f(panelY + fixedTop)},
        {to_f(panelX + texture.w), to_f(panelY + upperHeight)},
        {0.0f, to_f(fixedTop) / texture.h},
        {1.0f, to_f(sourceUpperHeight) / texture.h});
    drawList->AddImage(
        texture,
        {to_f(panelX), to_f(panelY + upperHeight)},
        {to_f(panelX + texture.w), to_f(panelY + panelHeight)},
        {0.0f, to_f(texture.h - fixedBottom) / texture.h},
        {1.0f, 1.0f});

    const int textX = panelX + (texture.w - m_impl->width) / 2;
    const int textY = panelY + fixedTop + (upperHeight - fixedTop - m_impl->height) / 2;
    for(const auto &paragraph: m_impl->paragraphs){
        paragraph.typeset->drawImGui(drawList, textX, textY + paragraph.startY);
    }
}
