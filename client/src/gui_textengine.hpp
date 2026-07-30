#pragma once
#include "totype.hpp"

// ===== token.hpp =====
// +-------------+------(*: X, Y)
// |             |
// |             v
// |           W1      W       W2
// |     --->|   |<--------->|   |<--
// +-------> +---*-----------+---+  --------
//           |   |  .    .   |   |  ^    ^
//           |   |           |   |  |    |
//           |   |  /\_/\    |   |  |    |
//           |   |=( 0w0 )=  |   |  | H1 | H
//           |   |  )   (  //|   |  |    |
//           |   | (__ __)// |   |  v    |
//           +---+   .       +---+  -    |
//               |    .      |        H2 v
//               +-----------+      --------
//                                  ^
//                                  |

#include <cstdint>
#include <cstddef>

struct TOKEN
{
    int leaf;
    int selectID;

    struct _TokenBox
    {
        struct _TokenBoxInfo
        {
            // general static information
            // keep unchanged after token initialization

            uint16_t w;
            uint16_t h;
        }info;

        struct _TokenBoxState
        {
            // we put mutable attributes here
            // should be valid after token board layout done

            int16_t x;
            int16_t y;
            int16_t w1;
            int16_t w2;
            int16_t h1;
            int16_t h2;
        }state;
    }box;

    union
    {
        struct
        {
            uint64_t key;
        }utf8char;

        struct
        {
            uint32_t key;

            uint8_t fps;
            uint8_t frameCount;

            uint8_t tick;
            uint8_t frame;
        }emoji;

        struct
        {
            uint64_t key;
        }image;
    };
};

// ===== xmlparagraphleaf.hpp =====
#include <cctype>
#include <vector>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tinyxml2.h>
#include "strf.hpp"
#include "fflerror.hpp"

constexpr int LEAF_UTF8STR = 0;
constexpr int LEAF_IMAGE   = 1;
constexpr int LEAF_EMOJI   = 2;

class XMLParagraphLeaf
{
    private:
        friend class XMLParapragh;

    private:
        tinyxml2::XMLNode * m_node;

    private:
        int m_type;

    private:
        uint64_t m_u64Key;

    private:
        std::vector<int> m_utf8CharOff;
        std::optional<std::unordered_map<std::string, std::string>> m_attrListOpt;

    private:
        std::optional<uint32_t> m_fontColor;
        std::optional<uint32_t> m_fontBGColor;

    private:
        int m_event;

    public:
        explicit XMLParagraphLeaf(tinyxml2::XMLNode *);

    public:
        int type() const
        {
            return m_type;
        }

        tinyxml2::XMLNode *xmlNode(this auto && self)
        {
            return self.m_node;
        }

        auto & utf8CharOff(this auto && self)
        {
            if(self.type() != LEAF_UTF8STR){
                throw fflpanic("leaf is not an utf8 string");
            }

            if(self.m_utf8CharOff.empty()){
                throw fflpanic("utf8 token off doesn't initialized");
            }

            return self.m_utf8CharOff;
        }

        int length() const
        {
            if(type() == LEAF_UTF8STR){
                return to_d(utf8CharOff().size());
            }
            return 1;
        }

        const char *utf8Text() const
        {
            if(type() != LEAF_UTF8STR){
                return nullptr;
            }
            return xmlNode()->Value();
        }

        uint64_t imageU64Key() const
        {
            if(type() != LEAF_IMAGE){
                throw fflpanic("leaf is not an image");
            }
            return m_u64Key;
        }

        uint32_t emojiU32Key() const
        {
            if(type() != LEAF_EMOJI){
                throw fflpanic("leaf is not an emoji");
            }
            return m_u64Key;
        }

        uint32_t peekUTF8Code(int) const;

    public:
        int markEvent(int);

    public:
        std::optional<bool> wrap() const;

    public:
        std::optional<uint32_t>   color() const;
        std::optional<uint32_t> bgColor() const;

    public:
        std::optional<uint8_t> font()      const;
        std::optional<uint8_t> fontSize()  const;
        std::optional<uint8_t> fontStyle() const;

    public:
        template<typename T> T *leafData() const
        {
            return reinterpret_cast<T *>(m_node->GetUserData());
        }

    public:
        const std::unordered_map<std::string, std::string> *hasEvent() const
        {
            return m_attrListOpt.has_value() ? std::addressof(m_attrListOpt.value()) : nullptr;
        }

    public:
        std::tuple<tinyxml2::XMLNode *, tinyxml2::XMLNode *> split(int, tinyxml2::XMLDocument &, tinyxml2::XMLDocument &);
};

// ===== xmlparagraph.hpp =====
#include <deque>

#include "xmlf.hpp"
#include "utf8f.hpp"

class XMLParagraph
{
    private:
        std::unique_ptr<tinyxml2::XMLDocument> m_xmlDocument; // leaf node refers to it

    private:
        std::deque<XMLParagraphLeaf> m_leafList;

    public:
        XMLParagraph(const char *xmlString = nullptr)
            : m_xmlDocument(std::make_unique<tinyxml2::XMLDocument>(true, tinyxml2::PEDANTIC_WHITESPACE))
        {
            loadXML(xmlString ? xmlString : "<par/>");
        }

    public:
        ~XMLParagraph() = default;

    public:
        bool empty() const
        {
            return leafCount() == 0;
        }

    public:
        int leafCount() const
        {
            return to_d(m_leafList.size());
        }

    public:
        bool leafValid(int leafIndex) const
        {
            return leafIndex >= 0 && leafIndex < leafCount();
        }

    public:
        auto & leaf(this auto && self, int leafIndex)
        {
            if(!self.leafValid(leafIndex)){
                throw fflpanic("invalid leaf index: {}", leafIndex);
            }
            return self.m_leafList[leafIndex];
        }

    public:
        bool leafOffValid(int leafIndex, int leafOff) const
        {
            if(!leafValid(leafIndex)){
                return false;
            }
            return leafOff >= 0 && leafOff < leaf(leafIndex).length();
        }

    public:
        auto & backLeaf(this auto && self)
        {
            if(self.m_leafList.empty()){
                throw fflpanic("no leaf");
            }
            return self.m_leafList.back();
        }

    public:
        XMLParagraph *split(int, int);
        void join(const XMLParagraph &, bool);

    public:
        void loadXML(const char *);
        void loadXMLNode(const tinyxml2::XMLNode *);

    private:
        size_t insertXMLAtFront(                     const char *);
        size_t insertXMLAfter  (tinyxml2::XMLNode *, const char *);

    public:
        size_t insertLeafXML(int, const char *);

    public:
        size_t insertUTF8String(int, int, const char *);

    public:
        tinyxml2::XMLNode *CloneLeaf(tinyxml2::XMLDocument *pDoc, int leafIndex) const
        {
            return leaf(leafIndex).xmlNode()->DeepClone(pDoc);
        }

    public:
        const tinyxml2::XMLNode *getXMLNode() const
        {
            return m_xmlDocument->RootElement();
        }

    public:
        std::string getXML() const
        {
            tinyxml2::XMLPrinter printer;
            m_xmlDocument->Accept(&printer);

            std::string result = printer.CStr();
            while(result.ends_with('\n')){
                result.pop_back();
            }
            return result;
        }

    public:
        void deleteToken(int, int, int);
        void deleteToken(int, int);

    public:
        void deleteLeaf(int);
        void deleteUTF8Char(int, int, int);

    public:
        std::tuple<int, int, int> prevLeafOff(int, int, int) const;
        std::tuple<int, int, int> nextLeafOff(int, int, int) const;

    public:
        void clear()
        {
            loadXML("<par/>");
        }

    public:
        std::string getRawString() const;

    public:
        size_t tokenCount() const;
};

// ===== xmltypeset.hpp =====
#include <climits>
#include <tuple>
#include "lalign.hpp"
#include "colorf.hpp"
#include "bevent.hpp"
#include "gui_core.hpp" // Widget::VarXXX

struct ImDrawList;

class XMLTypeset // means XMLParagraph typeset
{
    private:
        struct contentLine
        {
            int startY; // Y-axis coordinate reached by all tokens' H1, representing the bottom line of H1 pixels
                        // If all tokens are with H1 == 0, startY is the Y-axis coordinate above starting line of all tokens
            std::deque<TOKEN> content;
        };

        struct LeafInfo
        {
            int tokenX = 0;
            int tokenY = 0;
            int  maxH1 = 0;
            int  maxH2 = 0;

            std::tuple<int, int> tokenLoc() const noexcept
            {
                return {tokenX, tokenY};
            }

            std::tuple<int, int> maxHk() const noexcept
            {
                return {maxH1, maxH2};
            }
        };

    private:
        int m_lineWidth;

    private:
        const int m_lineAlign;

    private:
        const bool m_canThrough;
        const bool m_compactLine;

    private:
        int m_wordSpace;
        int m_lineSpace;

    private:
        uint8_t m_font;
        uint8_t m_fontSize;
        uint8_t m_fontStyle;

        Widget::VarU32 m_fontColor;
        Widget::VarU32 m_fontBGColor;
        Widget::VarU32 m_imageMaskColor;

    private:
        int m_px = 0;
        int m_py = 0;
        int m_pw = 0;
        int m_ph = 0;

    private:
        std::unique_ptr<XMLParagraph> m_paragraph;

    private:
        std::deque<contentLine> m_lineList;

    private:
        std::deque<LeafInfo> m_leafInfoList;

    public:
        XMLTypeset(
                int maxLineWidth,

                int  lineAlign  = LALIGN_LEFT,
                bool canThrough = true,
                bool compactLine = false,

                uint8_t defaultFont      = 0,
                uint8_t defaultFontSize  = 8,
                uint8_t defaultFontStyle = 0,

                Widget::VarU32 defaultFontColor      = colorf::WHITE_A255,
                Widget::VarU32 defaultFontBGColor    = 0U,
                Widget::VarU32 defaultImageMaskColor = colorf::WHITE_A255,

                int lineSpace = 0,
                int wordSpace = 0)

            : m_lineWidth(maxLineWidth)
            , m_lineAlign(lineAlign)
            , m_canThrough(canThrough)
            , m_compactLine(compactLine)
            , m_wordSpace(wordSpace)
            , m_lineSpace(lineSpace)

            , m_font(defaultFont)
            , m_fontSize(defaultFontSize)
            , m_fontStyle(defaultFontStyle)

            , m_fontColor(std::move(defaultFontColor))
            , m_fontBGColor(std::move(defaultFontBGColor))
            , m_imageMaskColor(std::move(defaultImageMaskColor))
            , m_paragraph(std::make_unique<XMLParagraph>())
        {
            checkDefaultFontEx();
        }

    public:
        ~XMLTypeset() = default;

    public:
        bool empty() const
        {
            return m_paragraph->empty();
        }

        bool lineEmpty(int argLine) const
        {
            fflassert(lineValid(argLine), argLine);
            return m_lineList.at(argLine).content.empty();
        }

    public:
        void loadXML(const char *xmlString)
        {
            clear();
            m_paragraph->loadXML(xmlString);
            updateGfx();
        }

        void loadXMLNode(const tinyxml2::XMLNode *node)
        {
            clear();
            m_paragraph->loadXMLNode(node);
            updateGfx();
        }

    public:
        void clear() // release everything
        {
            m_px = 0;
            m_py = 0;
            m_pw = 0;
            m_ph = 0;
            m_lineList.clear();
            m_paragraph->clear();
        }

        void updateGfx() // build without reload xml
        {
            if(m_paragraph->leafCount() > 0){
                buildTypeset(0, 0);
            }
            else{
                m_ph = getDefaultFontHeight();
            }
        }

    private:
        void resetBoardPixelRegion();

    public:
        bool lineValid(int line) const
        {
            return line >= 0 && line < lineCount();
        }

        int lineCount() const
        {
            return to_d(m_lineList.size());
        }

        int lineTokenCount(int argLine) const
        {
            fflassert(lineValid(argLine), argLine);
            return to_d(m_lineList[argLine].content.size());
        }

        int lineStartY(int argLine) const
        {
            fflassert(lineValid(argLine), argLine);
            return m_lineList[argLine].startY;
        }

    public:
        std::tuple<int, int> prevTokenLoc(int, int, int = 1, bool = true) const;
        std::tuple<int, int> nextTokenLoc(int, int, int = 1, bool = true) const;

    public:
        std::tuple<int, int> prevCursorLoc(int, int,      bool, bool = true) const;
        std::tuple<int, int> nextCursorLoc(int, int,      bool, bool = true) const;
        std::tuple<int, int> prevCursorLoc(int, int, int, bool, bool = true) const;
        std::tuple<int, int> nextCursorLoc(int, int, int, bool, bool = true) const;

    public:
        std::optional<std::tuple<int, int>> tokenLocBeforeCursor(int, int) const; // use for deleteToken(cursorLoc)

    public:
        static bool locInToken(int, int, const TOKEN *, bool withPadding);

    public:
        std::tuple<int, int> locToken(int, int, bool withPadding) const;

    public:
        std::tuple<int, int> locCursor(int, int) const;

    public:
        std::tuple<int, int> firstTokenLoc() const
        {
            if(empty()){
                throw fflpanic("empty typeset");
            }
            return {0, 0};
        }

        std::tuple<int, int> lastTokenLoc() const
        {
            if(empty()){
                throw fflpanic("empty board");
            }
            return {lineTokenCount(lineCount() - 1) - 1, lineCount() - 1};
        }

    public:
        std::tuple<int, int> firstCursorLoc() const
        {
            if(empty()){
                return {0, 0};
            }
            return {0, 0};
        }

        std::tuple<int, int> lastCursorLoc() const
        {
            if(empty()){
                return {0, 0};
            }
            return {lineTokenCount(lineCount() - 1), lineCount() - 1};
        }

    public:
        std::tuple<int, int> leafTokenLoc(int leafIndex) const
        {
            if(leafValid(leafIndex)){
                return m_leafInfoList.at(leafIndex).tokenLoc();
            }
            throw fflpanic("invalid leaf: {}", leafIndex);
        }

    public:
        bool tokenLocValid(int argX, int argY) const
        {
            return lineValid(argY) && (argX >= 0) && (argX < lineTokenCount(argY));
        }

        bool cursorLocValid(int argX, int argY) const
        {
            if(empty()){
                return argX == 0 && argY == 0;
            }
            return lineValid(argY) && (argX >= 0) && (argX <= lineTokenCount(argY));
        }

    public:
        int cursorLoc2Off(int, int) const; // actually returns how many tokens in front of cursor
        std::tuple<int, int> cursorOff2Loc(int) const;

    public:
        void update(double);

    public:
        void InsertXML(int, int, const char *);

    public:
        size_t insertUTF8String(int, int, const char *);

    public:
        XMLTypeset *split(int, int);
        void join(const XMLTypeset &, bool);

    public:
        void deleteToken(int, int, int); // deleteToken(tokenLoc)

    public:
        int leafCount() const
        {
            return m_paragraph->leafCount();
        }

        bool leafValid(int leafIndex) const
        {
            return leafIndex >= 0 && leafIndex < leafCount();
        }

    public:
        void clearEvent(int currLeaf = -1)
        {
            for(int leafIndex = 0; leafIndex < m_paragraph->leafCount(); ++leafIndex){
                if(leafIndex != currLeaf){
                    m_paragraph->leaf(leafIndex).markEvent(BEVENT_OFF);
                }
            }
        }

        int markLeafEvent(int leafIndex, int event)
        {
            return m_paragraph->leaf(leafIndex).markEvent(event);
        }

    public:
        void draw(Widget::ROIMap) const;
        void drawImGui(ImDrawList *, int, int) const;

    public:
        void setFont(uint8_t font)
        {
            m_font = font;
        }

        void setFontSize(uint8_t fontSize)
        {
            m_fontSize = fontSize;
        }

        void setFontStyle(uint8_t fontStyle)
        {
            m_fontStyle = fontStyle;
        }

        void setFontColor(Widget::VarU32 fontColor)
        {
            m_fontColor = std::move(fontColor);
        }

        void setFontBGColor(Widget::VarU32 fontBGColor)
        {
            m_fontBGColor = std::move(fontBGColor);
        }

        void setImageMaskColor(Widget::VarU32 imageMaskColor)
        {
            m_imageMaskColor = std::move(imageMaskColor);
        }

    public:
        std::string getXML() const
        {
            return m_paragraph->getXML();
        }

    public:
        std::string getText() const;

    public:
        const tinyxml2::XMLNode *getXMLNode() const
        {
            return m_paragraph->getXMLNode();
        }

    public:
        const auto leafEvent(int leafID) const
        {
            return m_paragraph->leaf(leafID).hasEvent();
        }

    private:
        void checkDefaultFontEx() const;

    private:
        void resetOneLine(int, bool);

    private:
        bool addRawTokenLine(int, const std::vector<TOKEN> &);

    private:
        void setTokenBoxWordSpace(int);

    private:
        void setLineTokenStartX(int);
        void setLineTokenStartY(int);

    private:
        int LineRawWidth(int, bool) const;

    private:
        int LineFullWidth(int) const;

    public:
        auto getToken(this auto && self, int argX, int argY)
        {
            if(!self.tokenLocValid(argX, argY)){
                throw fflpanic("invalid token location: ({}, {})", argX, argY);
            }
            return std::addressof(self.m_lineList[argY].content[argX]);
        }

    public:
        auto GetLineBackToken(this auto && self, int argLine)
        {
            if(!self.lineValid(argLine)){
                throw fflpanic("invalid line: {}", argLine);
            }

            if(self.lineTokenCount(argLine) == 0){
                throw fflpanic("invalie empty line: {}", argLine);
            }

            return self.getToken(self.lineTokenCount(argLine) - 1, argLine);
        }

    public:
        auto GetBackToken(this auto && self)
        {
            if(self.lineCount() == 0){
                throw fflpanic("empty board");
            }

            if(self.lineTokenCount(self.lineCount() - 1) == 0){
                throw fflpanic("invalie empty line: {}", self.lineCount() - 1);
            }

            return self.GetLineBackToken(self.lineCount() - 1);
        }

    private:
        int GetTokenWordSpace(int, int) const;

    private:
        bool AppendToken(int, const TOKEN &);
        void LinePadding(int);

    private:
        TOKEN buildUTF8Token(int, uint8_t, uint8_t, uint8_t, uint32_t) const;
        TOKEN buildEmojiToken(int, uint32_t) const;

    private:
        std::tuple<int, int> leafLocInXMLParagraph(int, int) const;

    private:
        TOKEN createToken(int, int) const;
        std::vector<TOKEN> createTokenLine(int, int, int &, int &, std::vector<TOKEN> * = nullptr) const;

    private:
        void buildTypeset(int, int);

    private:
        int lineReachMaxX(int) const;
        int lineReachMaxY(int) const;
        int lineReachMinX(int) const;
        int lineReachMinY(int) const;

    public:
        int px() const
        {
            return m_px;
        }

        int py() const
        {
            return m_py;
        }

        int pw() const
        {
            return m_pw;
        }

        int ph() const
        {
            return m_ph;
        }

    public:
        int LineMaxHk(int, int) const;

    private:
        void LineJustifyPadding(int);
        void LineDistributedPadding(int);

    public:
        int lineAlign() const;

    public:
        int MaxLineWidth() const
        {
            return m_lineWidth;
        }

        bool CanThrough() const
        {
            return m_canThrough;
        }

    private:
        int LineNewStartY(int);
        int LineTokenBestY(int, int, int, int) const;
        int LineIntervalMaxH2(int, int, int) const;

    public:
        uint32_t color() const
        {
            return Widget::evalU32(m_fontColor, nullptr, this);
        }

        uint32_t bgColor() const
        {
            return Widget::evalU32(m_fontBGColor, nullptr, this);
        }

    public:
        std::string getRawString() const
        {
            return m_paragraph->getRawString();
        }

        void setLineWidth(int);

    public:
        bool blankToken(int, int) const;

    public:
        std::tuple<int, int> getDefaultFontHk() const;
        std::tuple<int, int> getTokenCursorHk(int, int) const;
        int getDefaultFontHeight() const;
};
