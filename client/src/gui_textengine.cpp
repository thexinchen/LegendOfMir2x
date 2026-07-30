#include "gui_textengine.hpp"

// ===== xmlparagraphleaf.cpp =====
#include <utf8.h>
#include <tinyxml2.h>

#include "log.hpp"
#include "xmlf.hpp"
#include "utf8f.hpp"
#include "colorf.hpp"
#include "totype.hpp"
#include "bevent.hpp"

extern Log *g_mir2xLog;

XMLParagraphLeaf::XMLParagraphLeaf(tinyxml2::XMLNode *pNode)
    : m_node([pNode]()
      {
          if(pNode == nullptr){
              throw fflpanic("invalid argument: (nullptr)");
          }

          if(!xmlf::checkValidLeaf(pNode)){
              throw fflpanic("invalid argument: not a valid XMParagraph leaf");
          }
          return pNode;
      }())
    , m_type([this]()
      {
          if(xmlf::checkTextLeaf(xmlNode())){
              if(!utf8::is_valid(xmlNode()->Value(), xmlNode()->Value() + std::strlen(xmlNode()->Value()))){
                  throw fflpanic("not a utf8 string: {}", xmlNode()->Value());
              }
              return LEAF_UTF8STR;
          }

          if(xmlf::checkEmojiLeaf(xmlNode())){
              return LEAF_EMOJI;
          }

          if(xmlf::checkImageLeaf(xmlNode())){
              return LEAF_IMAGE;
          }

          throw fflpanic("invalid argument: node type not recognized");
      }())
    , m_u64Key([this]() -> uint64_t
      {
          switch(m_type){
              case LEAF_EMOJI:
                  {
                      int emojiId = 0;
                      if((m_node->ToElement()->QueryIntAttribute("id", &emojiId) != tinyxml2::XML_SUCCESS) &&
                         (m_node->ToElement()->QueryIntAttribute("Id", &emojiId) != tinyxml2::XML_SUCCESS) &&
                         (m_node->ToElement()->QueryIntAttribute("ID", &emojiId) != tinyxml2::XML_SUCCESS)){

                          // no id attribute
                          // use default emojiId zero
                      }
                      return emojiId;
                  }
              default:
                  {
                      return 0;
                  }
          }
      }())
    , m_event(BEVENT_OFF)
{
    if(type() == LEAF_UTF8STR){
        m_utf8CharOff = utf8f::buildUTF8Off(utf8Text());
        if(auto par = m_node->Parent(); par && par->ToElement()){
            std::string tagName = par->ToElement()->Name();
            std::transform(tagName.begin(), tagName.end(), tagName.begin(), [](unsigned char c)
            {
                return std::tolower(c);
            });

            if(tagName == "event"){
                std::unordered_map<std::string, std::string> attrList;
                for(auto attrPtr = par->ToElement()->FirstAttribute(); attrPtr; attrPtr = attrPtr->Next()){
                    std::string attrName = attrPtr->Name();
                    std::transform(attrName.begin(), attrName.end(), attrName.begin(), [](unsigned char c)
                    {
                        return std::tolower(c);
                    });
                    attrList.emplace(std::move(attrName), attrPtr->Value());
                }
                m_attrListOpt = std::move(attrList);
            }
        }
    }

    // pre-save for color() and bgColor(), these two get called very frequently in draw()
    // colorf::string2RGBA() is expensive and shouldn't get called in tight loop

    if(const auto colorStr = xmlf::findAttribute(xmlNode(), "color", true)){
        m_fontColor = colorf::string2RGBA(colorStr);
    }

    if(const auto bgColorStr = xmlf::findAttribute(xmlNode(), "bgcolor", true)){
        m_fontBGColor = colorf::string2RGBA(bgColorStr);
    }
}

int XMLParagraphLeaf::markEvent(int event)
{
    switch(event){
        case BEVENT_ON:
        case BEVENT_OFF:
        case BEVENT_DOWN:
            {
                std::swap(m_event, event);
                return event;
            }
        default:
            {
                throw fflpanic("invalid event: {}", event);
            }
    }
}

uint32_t XMLParagraphLeaf::peekUTF8Code(int leafOff) const
{
    if(leafOff >= length()){
        throw fflpanic("provided LeafOff exceeds leaf length: {}", length());
    }

    if(type() != LEAF_UTF8STR){
        throw fflpanic("try peek utf8 code from a leaf with type: {}", type());
    }

    return utf8f::str2code(xmlNode()->Value() + utf8CharOff()[leafOff]);
}

std::optional<uint32_t> XMLParagraphLeaf::color() const
{
    if(m_fontColor.has_value()){
        return m_fontColor;
    }

    if(hasEvent()){
        switch(m_event){
            case BEVENT_ON  : return colorf::GREEN   + colorf::A_SHF(255);
            case BEVENT_OFF : return colorf::YELLOW  + colorf::A_SHF(255);
            case BEVENT_DOWN: return colorf::MAGENTA + colorf::A_SHF(255);
            default: throw fflpanic("invalid leaf event: {}", m_event);
        }
    }
    return {};
}

std::optional<uint32_t> XMLParagraphLeaf::bgColor() const
{
    return m_fontBGColor;
}

std::optional<uint8_t> XMLParagraphLeaf::font() const
{
    if(const auto fontStr = xmlf::findAttribute(xmlNode(), "font", true)){
        try{
            size_t pos = 0;
            if(const auto val = std::stoi(fontStr, &pos); val >= 0 && val < 256 && pos == std::strlen(fontStr)){
                return to_u8(val);
            }
        }
        catch(...){}
        throw fflpanic("invalid font index, not an uint8_t: {}", fontStr);
    }
    return {};
}

std::optional<uint8_t> XMLParagraphLeaf::fontSize() const
{
    if(const auto sizeStr = xmlf::findAttribute(xmlNode(), "size", true)){
        try{
            size_t pos = 0;
            if(const auto val = std::stoi(sizeStr, &pos); val >= 0 && val < 256 && pos == std::strlen(sizeStr)){
                return to_u8(val);
            }
        }
        catch(...){}
        throw fflpanic("invalid font size, not an uint8_t: {}", sizeStr);
    }
    return {};
}

std::optional<uint8_t> XMLParagraphLeaf::fontStyle() const
{
    if(const auto fontStyleStr = xmlf::findAttribute(xmlNode(), "style", true)){
        try{
            size_t pos = 0;
            if(const auto val = std::stoi(fontStyleStr, &pos); val >= 0 && val < 256 && pos == std::strlen(fontStyleStr)){
                return to_u8(val);
            }
        }
        catch(...){}
        throw fflpanic("invalid font style, not an uint8_t: {}", fontStyleStr);
    }
    return {};
}

std::optional<bool> XMLParagraphLeaf::wrap() const
{
    if(const auto wrapStr = xmlf::findAttribute(xmlNode(), "wrap", true)){
        return to_parsedbool(wrapStr);
    }
    return {};
}

std::tuple<tinyxml2::XMLNode *, tinyxml2::XMLNode *> XMLParagraphLeaf::split(int cursor, tinyxml2::XMLDocument &doc1, tinyxml2::XMLDocument &doc2)
{
    fflassert(type() == LEAF_UTF8STR, type());
    fflassert(cursor > 0 && cursor < length(), cursor, length()); // we don't support empty leaf node

    tinyxml2::XMLNode *node1 = nullptr;
    tinyxml2::XMLNode *node2 = nullptr;

    const auto text1 = std::string(utf8Text() , m_utf8CharOff.at(cursor));
    const auto text2 = std::string(utf8Text() + m_utf8CharOff.at(cursor));

    if(m_node->GetDocument() == &doc1){
        node1 = m_node;
    }
    else{
        node1 = m_node->DeepClone(&doc1);
    }

    if(m_node->GetDocument() == &doc2){
        node2 = m_node;
    }
    else{
        node2 = m_node->DeepClone(&doc2);
    }

    node1->SetValue(text1.c_str());
    node2->SetValue(text2.c_str());

    if(node1 == m_node || node2 == m_node){
        m_utf8CharOff = utf8f::buildUTF8Off(utf8Text());
    }

    return {node1, node2};
}

// ===== xmlparagraph.cpp =====
#include <tuple>
#include <stdexcept>

#include "strf.hpp"

extern Log *g_mir2xLog;
XMLParagraph *XMLParagraph::split(int leafIndex, int cursorLoc)
{
    fflassert(leafValid(leafIndex));
    fflassert(cursorLoc >= 0 && cursorLoc <= leaf(leafIndex).length());

    XMLParagraph *fromPar = this;
    XMLParagraph *  toPar = new XMLParagraph{};

    const auto fnMoveFrontLeaf = [fromPar, toPar]()
    {
        auto node = fromPar->m_leafList.front().xmlNode()->DeepClone(toPar->m_xmlDocument.get());
        toPar->m_xmlDocument->FirstChild()->InsertEndChild(node);
        toPar->m_leafList.emplace_back(node);

        fromPar->m_xmlDocument->FirstChild()->DeleteChild(fromPar->m_leafList.front().xmlNode());
        fromPar->m_leafList.pop_front();
    };

    for(int i = 0; i < leafIndex; ++i){
        fnMoveFrontLeaf();
    }

    if(cursorLoc == 0){
        // do nothing
    }
    else if(cursorLoc == fromPar->m_leafList.front().length()){
        fnMoveFrontLeaf();
    }
    else{
        auto [node1, node2] = fromPar->m_leafList.front().split(cursorLoc, *toPar->m_xmlDocument, *fromPar->m_xmlDocument);
        toPar->m_xmlDocument->FirstChild()->InsertEndChild(node1);
        toPar->m_leafList.emplace_back(node1);
    }

    return toPar;
}

void XMLParagraph::deleteLeaf(int leafIndex)
{
    auto node   = leaf(leafIndex).xmlNode();
    auto parent = node->Parent();

    while(parent && (parent->FirstChild() == parent->LastChild()) && (parent != m_xmlDocument->FirstChild())){
        node   = parent;
        parent = parent->Parent();
    }

    fflassert(parent);
    parent->DeleteChild(node);
    m_leafList.erase(m_leafList.begin() + leafIndex);
}

size_t XMLParagraph::insertUTF8String(int leafIndex, int leafOff, const char *utf8String)
{
    if(leaf(leafIndex).type() != LEAF_UTF8STR){
        throw fflpanic("the {}-th leaf is not a XMLText", leafIndex);
    }

    if(leafOff < 0 || leafOff > to_d(leaf(leafIndex).utf8CharOff().size())){
        throw fflpanic("leaf offset {} is not a valid insert offset", leafOff);
    }

    if(utf8String == nullptr){
        throw fflpanic("null utf8 text string");
    }

    if(std::strlen(utf8String) == 0){
        return 0;
    }

    if(!utf8::is_valid(utf8String, utf8String + std::strlen(utf8String))){
        throw fflpanic("invalid utf-8 string: {}", utf8String);
    }

    auto &utf8OffRef = leaf(leafIndex).utf8CharOff();
    const auto [oldLength, newValue] = [leafIndex, leafOff, utf8String, &utf8OffRef, this]() -> std::tuple<int, std::string>
    {
        // after we call XMLNode::SetValue(), the oldValue pointer get updated
        // use lambda here on purpose to prevent oldValue pointer get exposed to outiler scope

        const auto oldValue  = leaf(leafIndex).xmlNode()->Value();
        const auto oldLength = to_d(std::strlen(oldValue));

        if(leafOff >= to_d(utf8OffRef.size())){
            return {oldLength, std::string(oldValue) + utf8String};
        }
        return {oldLength, (std::string(oldValue, oldValue + utf8OffRef[leafOff]) + utf8String) + std::string(oldValue + utf8OffRef[leafOff])};
    }();
    m_leafList[leafIndex].xmlNode()->SetValue(newValue.c_str());

    auto addedValueOff = utf8f::buildUTF8Off(utf8String);
    if(leafOff > 0){
        const auto firstOffDiff = [leafOff, oldLength, &utf8OffRef]() -> int
        {
            if(leafOff < to_d(utf8OffRef.size())){
                return utf8OffRef[leafOff];
            }
            return oldLength;
        }();

        for(auto &elemRef: addedValueOff){
            elemRef += firstOffDiff;
        }
    }

    if(leafOff < to_d(utf8OffRef.size())){
        const int addedLength = std::strlen(utf8String);
        for(int i = leafOff; i < to_d(utf8OffRef.size()); ++i){
            utf8OffRef[i] += addedLength;
        }
    }

    utf8OffRef.insert(utf8OffRef.begin() + leafOff, addedValueOff.begin(), addedValueOff.end());
    return addedValueOff.size();
}

void XMLParagraph::deleteUTF8Char(int leafIndex, int leafOff, int tokenCount)
{
    if(leaf(leafIndex).type() != LEAF_UTF8STR){
        throw fflpanic("the {}-th leaf is not a XMLText", leafIndex);
    }

    if(!leafOffValid(leafIndex, leafOff)){
        throw fflpanic("invalid leafOff: {}", leafOff);
    }

    if(tokenCount == 0){
        return;
    }

    if((leafOff + tokenCount - 1) >= leaf(leafIndex).length()){
        throw fflpanic("the {}-th leaf has only {} tokens", leafIndex, leaf(leafIndex).length());
    }

    if(tokenCount == leaf(leafIndex).length()){
        deleteLeaf(leafIndex);
        return;
    }

    const int charOff = leaf(leafIndex).utf8CharOff()[leafOff];
    const int charLen = [this, leafIndex, charOff, leafOff, tokenCount]() -> int
    {
        if(leafOff + tokenCount < to_d(leaf(leafIndex).utf8CharOff().size())){
            return leaf(leafIndex).utf8CharOff()[leafOff + tokenCount] - charOff;
        }
        return to_d(std::strlen(leaf(leafIndex).xmlNode()->Value())) - charOff;
    }();

    const auto newValue = std::string(leaf(leafIndex).xmlNode()->Value()).erase(charOff, charLen);
    leaf(leafIndex).xmlNode()->SetValue(newValue.c_str());

    for(int i = leafOff; i + tokenCount < to_d(leaf(leafIndex).utf8CharOff().size()); ++i){
        leaf(leafIndex).utf8CharOff()[i] = leaf(leafIndex).utf8CharOff()[i + tokenCount] - charLen;
    }
    leaf(leafIndex).utf8CharOff().resize(leaf(leafIndex).utf8CharOff().size() - tokenCount);
}

void XMLParagraph::deleteToken(int leafIndex, int leafOff, int tokenCount)
{
    if(!leafOffValid(leafIndex, leafOff)){
        throw fflpanic("invalid leaf off: ({}, {})", leafIndex, leafOff);
    }

    if(tokenCount == 0){
        return;
    }

    const bool hasEnoughToken = [leafIndex, leafOff, tokenCount, this]()
    {
        int tokenLeft = leaf(leafIndex).length() - leafOff;
        if(tokenLeft >= tokenCount){
            return true;
        }

        for(int i = leafIndex + 1; i < leafCount(); ++i){
            tokenLeft += leaf(i).length();
            if(tokenLeft >= tokenCount){
                return true;
            }
        }
        return false;
    }();

    if(!hasEnoughToken){
        throw fflpanic("insufficient token to remove");
    }

    int currLeaf     = leafIndex;
    int currLeafOff  = leafOff;
    int deletedToken = 0;

    while(deletedToken < tokenCount){
        switch(leaf(currLeaf).type()){
            case LEAF_UTF8STR:
                {
                    const auto leafLength = leaf(currLeaf).length();
                    const auto needDelete = std::min<int>(leafLength - currLeafOff, tokenCount - deletedToken);

                    deleteUTF8Char(currLeaf, currLeafOff, needDelete);
                    deletedToken += needDelete;

                    if(needDelete != leafLength){
                        currLeaf++; // current leaf is not fully deleted
                    }

                    currLeafOff = 0;
                    break;
                }
            case LEAF_EMOJI:
            case LEAF_IMAGE:
                {
                    deleteLeaf(currLeaf);
                    deletedToken += 1;
                    currLeafOff = 0;
                    break;
                }
            default:
                {
                    throw fflpanic("invalid leaf type: {}", leaf(currLeaf).type());
                }
        }
    }
}

std::tuple<int, int, int> XMLParagraph::prevLeafOff(int leafIndex, int leafOff, int) const
{
    if(leafOff >= to_d(leaf(leafIndex).utf8CharOff().size())){
        throw fflpanic("the {}-th leaf has only {} tokens", leafIndex, leaf(leafIndex).utf8CharOff().size());
    }

    return {0, 0, 0};
}

std::tuple<int, int, int> XMLParagraph::nextLeafOff(int leafIndex, int leafOff, int tokenCount) const
{
    if(leafOff >= leaf(leafIndex).length()){
        throw fflpanic("the {}-th leaf has only {} tokens", leafIndex, leaf(leafIndex).length());
    }

    int nCurrLeaf      = leafIndex;
    int nCurrLeafOff   = leafOff;
    int nAdvancedToken = 0;

    while((nAdvancedToken < tokenCount) && (nCurrLeaf < leafCount())){
        switch(auto nType = leaf(nCurrLeaf).type()){
            case LEAF_EMOJI:
            case LEAF_IMAGE:
            case LEAF_UTF8STR:
                {
                    int nCurrTokenLeft = leaf(nCurrLeaf).length() - nCurrLeafOff - 1;
                    if(nCurrTokenLeft >= tokenCount - nAdvancedToken){
                        return {nCurrLeaf, nCurrLeafOff + (tokenCount - nAdvancedToken), tokenCount};
                    }

                    // current leaf is not enough
                    // but this is the last leaf, have to stop
                    if(nCurrLeaf == (leafCount() - 1)){
                        return {nCurrLeaf, leaf(nCurrLeaf).length() - 1, nAdvancedToken + nCurrTokenLeft};
                    }

                    // take all the rest
                    // go to next leaf and take the first token
                    nCurrLeaf++;
                    nCurrLeafOff = 0;
                    nAdvancedToken += (nCurrTokenLeft + 1);
                    break;
                }
            default:
                {
                    throw fflpanic("invalid leaf type: {}", nType);
                }
        }
    }
    return {nCurrLeaf, nCurrLeafOff, nAdvancedToken};
}

void XMLParagraph::join(const XMLParagraph &input, bool append)
{
    if(std::addressof(input) == this){
        throw fflpanic("cannot join XMLParagraph with itself");
    }

    if(input.m_xmlDocument->FirstChild() == nullptr){
        return;
    }

    if(append){
        for(auto node = input.m_xmlDocument->FirstChild()->FirstChild(); node; node = node->NextSibling()){
            const auto cloneNode = node->DeepClone(m_xmlDocument->GetDocument());
            m_xmlDocument->FirstChild()->InsertEndChild(cloneNode);

            for(auto leafNode = xmlf::getNodeFirstLeaf(cloneNode); leafNode; leafNode = xmlf::getNextLeaf(leafNode, cloneNode)){
                if(xmlf::checkValidLeaf(leafNode)){
                    m_leafList.emplace_back(leafNode);
                }
            }
        }
    }
    else{
        for(auto node = input.m_xmlDocument->FirstChild()->LastChild(); node; node = node->PreviousSibling()){
            const auto cloneNode = node->DeepClone(m_xmlDocument->GetDocument());
            m_xmlDocument->FirstChild()->InsertFirstChild(cloneNode);

            for(auto leafNode = xmlf::getNodeLastLeaf(cloneNode); leafNode; leafNode = xmlf::getPreviousLeaf(leafNode, cloneNode)){
                if(xmlf::checkValidLeaf(leafNode)){
                    m_leafList.emplace_front(leafNode);
                }
            }
        }
    }
}

void XMLParagraph::loadXML(const char *xmlString)
{
    if(xmlString == nullptr){
        throw fflpanic("null xml string");
    }

    if(!utf8::is_valid(xmlString, xmlString + std::strlen(xmlString))){
        throw fflpanic("xml is not a valid utf8 string: {}", xmlString);
    }

    tinyxml2::XMLDocument xmlDoc(true, tinyxml2::PEDANTIC_WHITESPACE);
    if(xmlDoc.Parse(xmlString) != tinyxml2::XML_SUCCESS){
        throw fflpanic("tinyxml2::XMLDocument::Parse() failed: {}", xmlString);
    }

    loadXMLNode(xmlDoc.RootElement());
}

void XMLParagraph::loadXMLNode(const tinyxml2::XMLNode *node)
{
    if(!node){
        throw fflpanic("null node pointer");
    }

    if(!node->ToElement()){
        throw fflpanic("given node is not an element");
    }

    bool parXML = false;
    for(const char *cstr: {"par", "Par", "PAR"}){
        if(to_sv(node->Value()) == cstr){
            parXML = true;
            break;
        }
    }

    if(!parXML){
        throw fflpanic("not a paragraph node");
    }

    m_xmlDocument->Clear();
    if(auto pNew = node->DeepClone(m_xmlDocument.get()); pNew){
        m_xmlDocument->InsertEndChild(pNew);
    }
    else{
        throw fflpanic("copy paragraph node failed");
    }

    m_leafList.clear();
    if(auto parNode = m_xmlDocument->FirstChild(); !parNode->NoChildren()){
        for(auto nodePtr = xmlf::getNodeFirstLeaf(parNode); nodePtr; nodePtr = xmlf::getNextLeaf(nodePtr, parNode)){
            if(xmlf::checkValidLeaf(nodePtr)){
                m_leafList.emplace_back(nodePtr);
            }
        }
    }
}

size_t XMLParagraph::insertLeafXML(int loc, const char *xmlString)
{
    if(loc < 0 || loc > leafCount() || !xmlString){
        throw fflpanic("invalid argument: loc = {}, xmlString = {:p}", loc, to_cvptr(xmlString));
    }

    if(loc == 0){
        return insertXMLAtFront(xmlString);
    }
    return insertXMLAfter(leaf(loc - 1).xmlNode(), xmlString);
}

size_t XMLParagraph::insertXMLAtFront(const char *xmlString)
{
    if(!xmlString){
        throw fflpanic("invalid argument: xmlString = {:p}", to_cvptr(xmlString));
    }

    // to insert XML as leaves, we requires to insert as a new paragraph leaf
    // for emoji and image we have specified tag <emoji>, <image>, but for text we don't have

    tinyxml2::XMLDocument xmlDoc(true, tinyxml2::PEDANTIC_WHITESPACE);
    if(xmlDoc.Parse(xmlString) != tinyxml2::XML_SUCCESS){
        throw fflpanic("tinyxml2::XMLDocument::Parse() failed: {}", xmlString);
    }

    auto xmlRoot = xmlDoc.RootElement();
    if(!xmlRoot){
        throw fflpanic("no root element");
    }

    bool parXML = false;
    for(const char *cstr: {"par", "Par", "PAR"}){
        if(to_sv(xmlRoot->Value()) == cstr){
            parXML = true;
            break;
        }
    }

    if(!parXML){
        throw fflpanic("xmlString is not a paragraph");
    }

    size_t addedLeafCount = 0;
    for(auto p = xmlRoot->LastChild(); p; p = p->PreviousSibling()){
        if(auto cloneNode = p->DeepClone(m_xmlDocument.get())){
            if(m_xmlDocument->FirstChild()->InsertFirstChild(cloneNode) != cloneNode){
                throw fflpanic("insert node failed");
            }
            addedLeafCount++;
        }
        else{
            throw fflpanic("deep clone node failed");
        }
    }

    // TODO rebuild everything
    // this is not necessary, optimize later

    m_leafList.clear();
    if(auto parNode = m_xmlDocument->FirstChild(); !parNode->NoChildren()){
        for(auto nodePtr = xmlf::getNodeFirstLeaf(parNode); nodePtr; nodePtr = xmlf::getNextLeaf(nodePtr, parNode)){
            if(xmlf::checkValidLeaf(nodePtr)){
                m_leafList.emplace_back(nodePtr);
            }
        }
    }

    size_t addedCount = 0;
    for(size_t i = 0; i < addedLeafCount; ++i){
        addedCount += leaf(i).length();
    }

    return addedCount;
}

size_t XMLParagraph::insertXMLAfter(tinyxml2::XMLNode *after, const char *xmlString)
{
    if(!(after && xmlString)){
        throw fflpanic("invalid argument: after = {:p}, xmlString = {:p}", to_cvptr(after), to_cvptr(xmlString));
    }

    if(after->GetDocument() != m_xmlDocument.get()){
        throw fflpanic("can't find after node in current XMLDocument");
    }

    // to insert XML as leaves, we requires to insert as a new paragraph leaf
    // for emoji and image we have specified tag <emoji>, <image>, but for text we don't have

    tinyxml2::XMLDocument xmlDoc(true, tinyxml2::PEDANTIC_WHITESPACE);
    if(xmlDoc.Parse(xmlString) != tinyxml2::XML_SUCCESS){
        throw fflpanic("tinyxml2::XMLDocument::Parse() failed: {}", xmlString);
    }

    auto xmlRoot = xmlDoc.RootElement();
    if(!xmlRoot){
        throw fflpanic("no root element");
    }

    bool parXML = false;
    for(const char *cstr: {"par", "Par", "PAR"}){
        if(to_sv(xmlRoot->Value()) == cstr){
            parXML = true;
            break;
        }
    }

    if(!parXML){
        throw fflpanic("xmlString is not a paragraph");
    }

    size_t addedLeafCount = 0;
    for(auto p = xmlRoot->LastChild(); p; p = p->PreviousSibling()){
        if(auto cloneNode = p->DeepClone(m_xmlDocument.get())){
            if(after->Parent()->InsertAfterChild(after, cloneNode) != cloneNode){
                throw fflpanic("insert node failed");
            }
            addedLeafCount++;
        }
        else{
            throw fflpanic("deep clone node failed");
        }
    }

    // TODO rebuild everything
    // this is not necessary, optimize later

    m_leafList.clear();
    if(auto parNode = m_xmlDocument->FirstChild(); !parNode->NoChildren()){
        for(auto nodePtr = xmlf::getNodeFirstLeaf(parNode); nodePtr; nodePtr = xmlf::getNextLeaf(nodePtr, parNode)){
            if(xmlf::checkValidLeaf(nodePtr)){
                m_leafList.emplace_back(nodePtr);
            }
        }
    }

    int afterLoc = -1;
    size_t addedCount = 0;

    for(int i = 0; i < leafCount(); ++i){
        if(leaf(i).xmlNode() == after){
            afterLoc = i;
            break;
        }
    }

    for(int i = afterLoc + 1; i < leafCount() && addedLeafCount > 0; ++i, --addedLeafCount){
        addedCount += leaf(i).length();
    }

    return addedCount;
}

void XMLParagraph::deleteToken(int leafIndex, int leafOff)
{
    switch(leaf(leafIndex).type()){
        case LEAF_UTF8STR:
            {
                deleteUTF8Char(leafIndex, leafOff, 1);
                return;
            }
        case LEAF_EMOJI:
        case LEAF_IMAGE:
            {
                deleteLeaf(leafIndex);
                return;
            }
        default:
            {
                throw fflpanic("invalid leaf type: {}", leaf(leafIndex).type());
            }
    }
}

std::string XMLParagraph::getRawString() const
{
    std::string rawString;
    for(const auto &leaf: m_leafList){
        switch(leaf.type()){
            case LEAF_UTF8STR:
                {
                    rawString += leaf.xmlNode()->Value();
                    break;
                }
            case LEAF_EMOJI:
            case LEAF_IMAGE:
                {
                    break;
                }
            default:
                {
                    throw fflpanic("invalid leaf node type");
                }
        }
    }
    return rawString;
}

size_t XMLParagraph::tokenCount() const
{
    size_t count = 0;
    for(int i = 0; i < leafCount(); ++i){
        count += leaf(i).length();
    }
    return count;
}

// ===== xmltypeset.cpp =====
#include <cinttypes>
#include "lalign.hpp"
#include "fflerror.hpp"
#include "mathf.hpp"
#include "gui_font.hpp"
#include "gldevice.hpp"
#include "gui_texture.hpp"
#include "clientargparser.hpp"

extern Log *g_mir2xLog;
extern FontexDB *g_fontexDB;
extern GLDevice *g_glDevice;
extern EmojiDB *g_emojiDB;
extern ClientArgParser *g_clientArgParser;

void XMLTypeset::setTokenBoxWordSpace(int argLine)
{
    if(!lineValid(argLine)){
        throw fflpanic("invalid line: {}", argLine);
    }

    const int extraW1 = m_wordSpace / 2;
    const int extraW2 = m_wordSpace - extraW1;

    for(auto &tk: m_lineList[argLine].content){
        tk.box.state.w1 += extraW1;
        tk.box.state.w2 += extraW2;
    }

    if(!m_lineList[argLine].content.empty()){
        m_lineList[argLine].content[0]    .box.state.w1 = 0;
        m_lineList[argLine].content.back().box.state.w2 = 0;
    }
}

// get line full width, count everything inside
// assume:
//      token W/W1/W2 has been set
// return:
//      full line width
int XMLTypeset::LineFullWidth(int argLine) const
{
    if(!lineValid(argLine)){
        throw fflpanic("invalid line: {}", argLine);
    }

    int fullWidth = 0;
    for(int nIndex = 0; nIndex < lineTokenCount(argLine); ++nIndex){
        auto pToken = getToken(nIndex, argLine);
        fullWidth += (pToken->box.state.w1 + pToken->box.info.w + pToken->box.state.w2);
    }
    return fullWidth;
}

// calculate line width without W1/W2
// assume:
//      tokens are in current line
//      tokens have W initialized
// return:
//      total token width, plus word space optionally
//      negative if error
// this function is used before we padding all tokens, to estimate how many pixels we need for current line
int XMLTypeset::LineRawWidth(int argLine, bool bWithWordSpace) const
{
    if(!lineValid(argLine)){
        throw fflpanic("invalid line: {}", argLine);
    }

    switch(lineTokenCount(argLine)){
        case 0:
            {
                return 0;
            }
        case 1:
            {
                return getToken(0, argLine)->box.info.w;
            }
        default:
            {
                // for more than one tokens
                // we need to check word spacing

                int nWidth = 0;
                for(int nX = 0; nX < lineTokenCount(argLine); ++nX){
                    nWidth += getToken(nX, argLine)->box.info.w;
                    if(bWithWordSpace){
                        nWidth += GetTokenWordSpace(nX, argLine);
                    }
                }

                if(bWithWordSpace){
                    int nWordSpaceFirst = GetTokenWordSpace(0, argLine);
                    int nWordSpaceLast  = GetTokenWordSpace(lineTokenCount(argLine) - 1, argLine);

                    nWidth -= (nWordSpaceFirst / 2);
                    nWidth -= (nWordSpaceLast - nWordSpaceLast / 2);
                }

                return nWidth;
            }
    }
}

bool XMLTypeset::addRawTokenLine(int argLine, const std::vector<TOKEN> &tokenLine)
{
    if(!lineValid(argLine)){
        throw fflpanic("invalid line: {}", argLine);
    }

    if(tokenLine.empty()){
        throw fflpanic("empty token line");
    }

    if(m_lineWidth <= 0){
        m_lineList[argLine].content.insert(m_lineList[argLine].content.end(), tokenLine.begin(), tokenLine.end());
        return true;
    }

    // if we have a defined width but too small
    // need to accept but give warnings

    const auto rawExtraWidth = [&tokenLine]() -> int
    {
        int result = 0;
        for(const auto &token: tokenLine){
            result += token.box.info.w;
        }
        return result;
    }();

    if((lineTokenCount(argLine) == 0) && (m_lineWidth < to_d(rawExtraWidth + (tokenLine.size() - 1) * m_wordSpace))){
        g_mir2xLog->addLog(LOGTYPE_WARNING, "XMLTypeset width is too small to hold the token line: lineWidth = %d", m_lineWidth);
        m_lineList[argLine].content.insert(m_lineList[argLine].content.end(), tokenLine.begin(), tokenLine.end());
        return true;
    }

    if(m_lineWidth < LineRawWidth(argLine, false) + rawExtraWidth + to_d(lineTokenCount(argLine) + tokenLine.size() - 1) * m_wordSpace){
        return false;
    }

    m_lineList[argLine].content.insert(m_lineList[argLine].content.end(), tokenLine.begin(), tokenLine.end());
    return true;
}

void XMLTypeset::LinePadding(int argLine)
{
    if(!lineValid(argLine)){
        throw fflpanic("invalid line: {}", argLine);
    }
}

void XMLTypeset::LineDistributedPadding(int)
{
}

// do justify padding for current line
// assume:
//      1. alreayd put all tokens into current line
//      2. token has Box.W ready
void XMLTypeset::LineJustifyPadding(int argLine)
{
    if(!lineValid(argLine)){
        throw fflpanic("invalid line: {}", argLine);
    }

    if(lineAlign() != LALIGN_JUSTIFY){
        throw fflpanic("do line justify-padding while board align is configured as: {}", lineAlign());
    }

    switch(lineTokenCount(argLine)){
        case 0:
            {
                throw fflpanic("empty line detected: {}", argLine);
            }
        case 1:
            {
                return;
            }
        default:
            {
                break;
            }
    }

    if(MaxLineWidth() == 0){
        throw fflpanic("do line justify-padding while board is configured as infinite single line mode");
    }

    // we allow to exceeds the line limitation..
    // when there is a huge token inderted to current line, but only for this exception

    if((LineRawWidth(argLine, true) > MaxLineWidth()) && (lineTokenCount(argLine) > 1)){
        throw fflpanic("line raw width exceeds the fixed max line width: {}", MaxLineWidth());
    }

    const auto fnLeafPadding = [this, y = argLine](const auto &fnCheckToken) -> int
    {
        while(LineFullWidth(y) < MaxLineWidth()){
            int currDWidth = MaxLineWidth() - LineFullWidth(y);
            int doneDWidth = currDWidth;

            for(int x = 0; x < lineTokenCount(y); ++x){
                if(!fnCheckToken(x, y)){
                    continue;
                }

                auto tokenPtr = getToken(x, y);

                // pick the smaller side to keep w1/w2 balanced
                // if the picked side is edge-frozen (w1 of first token, w2 of last), fall back to the other side
                // otherwise a 2-token line has both sides frozen and can never be padded

                const bool preferW1 = tokenPtr->box.state.w1 <= tokenPtr->box.state.w2;
                const bool canW1 = (x != 0);
                const bool canW2 = (x != lineTokenCount(y) - 1);

                int16_t *growPtr = nullptr;

                if     ( preferW1 ? canW1 : canW2){ growPtr =  preferW1 ? &tokenPtr->box.state.w1 : &tokenPtr->box.state.w2; }
                else if(!preferW1 ? canW1 : canW2){ growPtr = !preferW1 ? &tokenPtr->box.state.w1 : &tokenPtr->box.state.w2; }

                if(growPtr){
                    (*growPtr)++;
                    doneDWidth--;

                    if(doneDWidth == 0){
                        return MaxLineWidth();
                    }
                }
            }

            // can't help
            // we go through the whole line but no width changed

            if(currDWidth == doneDWidth){
                return LineFullWidth(y);
            }
        }
        return MaxLineWidth();
    };

    // first round
    // padding image and emoji only, limited to 10%

    if(fnLeafPadding([this](int x, int y) -> bool
    {
        const auto tokenPtr = getToken(x, y);
        switch(m_paragraph->leaf(tokenPtr->leaf).type()){
            case LEAF_IMAGE:
            case LEAF_EMOJI: return tokenPtr->box.state.w1 + tokenPtr->box.state.w2 < tokenPtr->box.info.w / 5;
            default        : return false;
        }
    }) == MaxLineWidth()){
        return;
    }

    // second round
    // padding utf8 text but only do blank chars

    if(fnLeafPadding([this](int x, int y) -> bool
    {
        return blankToken(x, y);
    }) == MaxLineWidth()){
        return;
    }

    // last round
    // padding ut8 chars

    if(fnLeafPadding([this](int x, int y) -> bool
    {
        return m_paragraph->leaf(getToken(x, y)->leaf).type() == LEAF_UTF8STR;
    }) == MaxLineWidth()){
        return;
    }

    // final round
    // for lines have only emoji/image, all previous padding cannot help

    if(fnLeafPadding([this](int, int) -> bool
    {
        return true;
    }) == MaxLineWidth()){
        return;
    }

    g_mir2xLog->addLog(LOGTYPE_WARNING, "can't do justify padding to width: %d", MaxLineWidth());
}

void XMLTypeset::resetOneLine(int argLine, bool crEnd)
{
    if(!lineValid(argLine)){
        throw fflpanic("invalid line: {}", argLine);
    }

    // some tokens may have non-zero W1/W2 when reach here
    // need to reset them all

    const auto wordSpace = [this]() -> std::array<int, 2>
    {
        switch(lineAlign()){
            case LALIGN_LEFT:
            case LALIGN_JUSTIFY:
            case LALIGN_RIGHT:
            case LALIGN_CENTER:
                {
                    return
                    {
                        (m_wordSpace + 0) / 2,
                        (m_wordSpace + 1) / 2,
                    };
                }
            case LALIGN_DISTRIBUTED:
                {
                    return {0, 0};
                }
            default:
                {
                    throw fflpanic("invalid line align: {}", lineAlign());
                }
        }
    }();

    for(int i = 0, tokenCnt = lineTokenCount(argLine); i < tokenCnt; ++i){
        int left = 0;
        int right = 0;

        auto tkp = getToken(i, argLine);
        fflassert(tkp);

        switch(m_paragraph->leaf(tkp->leaf).type()){
            case LEAF_UTF8STR:
                {
                    g_fontexDB->retrieve(tkp->utf8char.key, &left, &right);
                    break;
                }
            default:
                {
                    break;
                }
        }

        tkp->box.state.w1 = (i     == 0       ) ? 0 : (left  + wordSpace[0]);
        tkp->box.state.w2 = (i + 1 == tokenCnt) ? 0 : (right + wordSpace[1]);
    }

    switch(lineAlign()){
        case LALIGN_JUSTIFY:
            {
                if(!crEnd){
                    LineJustifyPadding(argLine);
                }
                break;
            }
        case LALIGN_DISTRIBUTED:
            {
                LineDistributedPadding(argLine);
                break;
            }
        default:
            {
                break;
            }
    }

    setLineTokenStartX(argLine);
    setLineTokenStartY(argLine);
}

void XMLTypeset::setLineTokenStartX(int argLine)
{
    if(!lineValid(argLine)){
        throw fflpanic("invalid line: {}", argLine);
    }

    int nLineStartX = 0;
    switch(lineAlign()){
        case LALIGN_RIGHT:
            {
                nLineStartX = MaxLineWidth() - LineFullWidth(argLine);
                break;
            }
        case LALIGN_CENTER:
            {
                nLineStartX = (MaxLineWidth() - LineFullWidth(argLine)) / 2;
                break;
            }
    }

    int nCurrX = nLineStartX;
    for(auto &rstToken: m_lineList[argLine].content){
        nCurrX += rstToken.box.state.w1;
        rstToken.box.state.x = nCurrX;

        nCurrX += rstToken.box.info.w;
        nCurrX += rstToken.box.state.w2;
    }
}

// get max H2 for an interval
// assume:
//      W1/W/W2/X of tokens in argLine is initialized
// return:
//      max H2, or negative if there is no tokens for that interval
int XMLTypeset::LineIntervalMaxH2(int argLine, int nIntervalStartX, int nIntervalWidth) const
{
    //    This function only take care of argLine
    //    has nothing to do with (n-1)th or (n+1)th Line
    //
    // ---------+----+----+-------------+-------  <-BaseLine
    // |        |    |    |             |
    // |        | W2 | W1 |             |
    // |        |    +----+-------------+
    // |        |    |
    // +--------+----+
    //             |    |
    //             |    |
    //             |    |
    //        +----+----+-----+
    //        | W1 |    | W2  |
    //        |  ->|    |<-   |
    //            Interval
    //
    //  Here we take the padding space as part of the token.
    //  If not, token in (n + 1)-th line may go through this space
    //
    //  but token in (n + 1)-th line only count for the interval
    //  W1/W2 won't count for here, as shown

    if(!lineValid(argLine)){
        throw fflpanic("invalid line: {}", argLine);
    }

    if(nIntervalWidth == 0){
        throw fflpanic("zero-length interval provided");
    }

    std::optional<uint32_t> fontInfoOpt;
    int fontDescent = -1;
    int maxH2 = -1;

    for(int nIndex = 0; nIndex < lineTokenCount(argLine); ++nIndex){
        auto pToken = getToken(nIndex, argLine);
        int nW  = pToken->box.info .w;
        int nX  = pToken->box.state.x;
        int nW1 = pToken->box.state.w1;
        int nW2 = pToken->box.state.w2;
        int nH2 = pToken->box.state.h2;

        if(!m_compactLine && m_paragraph->leaf(pToken->leaf).type() == LEAF_UTF8STR){
            const uint32_t fontInfo = utf8f::fontInfoFromU64Key(pToken->utf8char.key);
            if(fontInfoOpt.has_value() && fontInfoOpt.value() == fontInfo){
                // token uses same font as last one
                // reuse the cached info
            }
            else{
                const auto [fontIndex, fontSize, _u1, _u2] = utf8f::extractU64Key(pToken->utf8char.key);
                fontInfoOpt = fontInfo;
                fontDescent = std::max<int>(0, -g_fontexDB->fontDescent(fontIndex, fontSize));
            }
            nH2 = std::max<int>(nH2, fontDescent);
        }

        if(mathf::intervalOverlap<int>(nX - nW1, nW1 + nW + nW2, nIntervalStartX, nIntervalWidth)){
            maxH2 = std::max<int>(maxH2, nH2);
        }
    }

    // maybe the line is not long enough to cover interval
    // in this case we return -1
    return maxH2;
}

// get the minmal (compact) possible start Y of a token in nth line
// assume:
//      (n-1)th line is valid if exists
// return:
//      possible minimal Y for current box
//
// for nth line we try to calculate possible minmal Y for each token then
// take the max-min as the line start Y, here we don't permit tokens in nth
// line go through upper than (n - 1)th baseLine
int XMLTypeset::LineTokenBestY(int argLine, int nTokenX, int nTokenWidth, int nTokenHeight) const
{
    if(!lineValid(argLine)){
        throw fflpanic("invalid line: {}", argLine);
    }

    if(nTokenWidth <= 0 || nTokenHeight <= 0){
        throw fflpanic("invalid token size: width = {}, height = {}", nTokenWidth, nTokenHeight);
    }

    if(argLine == 0){
        return nTokenHeight - 1;
    }

    if(int nMaxH2 = LineIntervalMaxH2(argLine - 1, nTokenX, nTokenWidth); nMaxH2 >= 0){
        return m_lineList[argLine - 1].startY + to_d(nMaxH2) + m_lineSpace + nTokenHeight;
    }

    // negative nMaxH2 means the (n - 1)th line is not long enough
    // directly let the token touch the (n - 1)th baseline
    //
    //           +----------+
    //           |          |
    // +-------+ |          |
    // | Words | |   Emoji  |  Words
    // +-------+-+----------+------------------
    //           |          |   +----------+
    //           +----------+   |          |
    //                          |   TB0    |
    //                +-------+ |          |
    //                | Words | |  Emoji   |  Words
    //                +-------+-+----------+-------
    //                          |          |
    //                          +----------+
    return m_lineList[argLine - 1].startY + m_lineSpace + nTokenHeight;
}

// get start Y of current line
// assume:
//      1. (nth - 1) line is valid
//      2. argLine is padded already, StartX, W/W1/W2 are OK now
int XMLTypeset::LineNewStartY(int argLine)
{
    //           +----------+
    //           |          |
    // +-------+ |          |
    // | Words | |   Emoji  |  Words
    // +-------+-+----------+------------------
    //           |          |   +----------+
    //           +----------+   |          |
    //                          |          |
    //                +-------+ |          |
    //                | Words | |   Emoji  |  Words
    //                +-------+-+----------+-------
    //                          |          |
    //                          +----------+
    //
    // for argLine we use W only, but for argLine - 1, we use W1 + W + W2
    // reason is clear since W1/2 of argLine won't show, then use them are
    // wasteful for space
    //
    //          +---+-----+--+---+-------+
    //          |   |     |  |   |       |
    //          | 1 |  W  |2 | 1 |   W   |...
    //          |   |     |  |   |       |
    //          +---+-----+--+---+-------+
    //
    //                   +--------+
    //                   |        |
    //                   |   W    |
    //                   |        |
    //

    if(!lineValid(argLine)){
        throw fflpanic("invalid line: {}", argLine);
    }

    if(argLine == 0){
        return LineMaxHk(0, 1) - 1; // return -1 if all tokens have H1 == 0
    }

    if(!CanThrough()){
        return lineReachMaxY(argLine - 1) + m_lineSpace + LineMaxHk(argLine, 1);
    }

    if(lineTokenCount(argLine) == 0){
        throw fflpanic("empty line detected: {}", argLine);
    }

    std::optional<uint32_t> fontInfoOpt;
    int fontAscent = -1;
    int nCurrentY = -1;

    for(int nIndex = 0; nIndex < lineTokenCount(argLine); ++nIndex){
        auto pToken = getToken(nIndex, argLine);
        int nX  = pToken->box.state.x;
        int nW  = pToken->box.info .w;
        int nH1 = pToken->box.state.h1;

        if(!m_compactLine && m_paragraph->leaf(pToken->leaf).type() == LEAF_UTF8STR){
            const uint32_t fontInfo = utf8f::fontInfoFromU64Key(pToken->utf8char.key);
            if(fontInfoOpt.has_value() && fontInfoOpt.value() == fontInfo){
                // token uses same font as last one
                // reuse the cached info
            }
            else{
                const auto [fontIndex, fontSize, _u1, _u2] = utf8f::extractU64Key(pToken->utf8char.key);
                fontInfoOpt = fontInfo;
                fontAscent = g_fontexDB->fontAscent(fontIndex, fontSize);
            }
            nH1 = std::max<int>(nH1, fontAscent);
        }

        // LineTokenBestY() already take m_lineSpace into consideration
        nCurrentY = std::max<int>(nCurrentY, LineTokenBestY(argLine, nX, nW, nH1));
    }

    // not done yet, think about the following situation
    // if we only check boxes in nth line we may make (n - 1)th line go cross
    //
    // +-----------+ +----+
    // |           | |    |
    // +-----------+-+    + ------ (n - 1)th
    //        +----+ |    |
    //        |    | |    |
    //  +--+  |    | |    |
    //  |  |  |    | |    |
    // -+--+--+----+ |    | ------ nth
    //               +----+

    return std::max<int>(nCurrentY, lineReachMaxY(argLine - 1) + 1);
}

void XMLTypeset::setLineTokenStartY(int argLine)
{
    if(!lineValid(argLine)){
        throw fflpanic("invalid line: {}", argLine);
    }

    m_lineList[argLine].startY = LineNewStartY(argLine);
    for(int nIndex = 0; nIndex < lineTokenCount(argLine); ++nIndex){
        auto pToken = getToken(nIndex, argLine);
        pToken->box.state.y = m_lineList[argLine].startY - pToken->box.state.h1 + 1;
    }
}

void XMLTypeset::checkDefaultFontEx() const
{
    const uint64_t u64key = utf8f::buildU64Key(m_font, m_fontSize, 0, utf8f::str2code("0"));
    if(!g_fontexDB->retrieve(u64key)){
        throw fflpanic("invalid default font: font = {}, fontsize = {}", m_font, m_fontSize);
    }
}

TOKEN XMLTypeset::buildUTF8Token(int leafIndex, uint8_t font, uint8_t fontSize, uint8_t fontStyle, uint32_t codePoint) const
{
    TOKEN token;
    std::memset(&(token), 0, sizeof(token));
    auto u64Key = utf8f::buildU64Key(font, fontSize, fontStyle, codePoint);

    int left = 0;
    int right = 0;
    int ascent = 0;

    token.leaf = leafIndex;
    if(auto texPtr = g_fontexDB->retrieve(u64Key, &left, &right, &ascent)){
        const auto [boxW, boxH] = GLDeviceHelper::getTextureSize(texPtr);
        token.box.info.w   = boxW;
        token.box.info.h   = boxH;
        token.box.state.w1 = left;
        token.box.state.w2 = right;
        token.box.state.h1 = std::max<int>(0, ascent);
        token.box.state.h2 = std::max<int>(0, boxH - ascent);
        token.utf8char.key = u64Key;
        return token;
    }

    u64Key = utf8f::buildU64Key(m_font, m_fontSize, 0, codePoint);
    if(!g_fontexDB->retrieve(u64Key)){
        throw fflpanic("can't find texture for UTF8: {:X}", codePoint);
    }

    u64Key = utf8f::buildU64Key(m_font, m_fontSize, fontStyle, utf8f::str2code("a"));
    if(!g_fontexDB->retrieve(u64Key)){
        throw fflpanic("invalid font style: {:X}", fontStyle);
    }

    // invalid font given
    // use system default font, don't fail it

    u64Key = utf8f::buildU64Key(m_font, m_fontSize, fontStyle, codePoint);
    if(auto texPtr = g_fontexDB->retrieve(u64Key)){
        g_mir2xLog->addLog(LOGTYPE_WARNING, "Fallback to default font: font: %d -> %d, fontsize: %d -> %d", font, m_font, fontSize, m_fontSize);
        const auto [boxW, boxH] = GLDeviceHelper::getTextureSize(texPtr);
        token.box.info.w   = boxW;
        token.box.info.h   = boxH;
        token.box.state.h1 = token.box.info.h;
        token.box.state.h2 = 0;
        token.utf8char.key = u64Key;
        return token;
    }
    throw fflpanic("fallback to default font failed: font: {} -> {}, fontsize: {} -> {}", font, m_font, fontSize, m_fontSize);
}

TOKEN XMLTypeset::buildEmojiToken(int leafIndex, uint32_t emoji) const
{
    TOKEN token;
    std::memset(&(token), 0, sizeof(token));
    token.leaf = leafIndex;

    int tokenW     = -1;
    int tokenH     = -1;
    int h1         = -1;
    int fps        = -1;
    int frameCount = -1;

    if(((emoji << 8) >> 8) != emoji){
        throw fflpanic("invalid emoji key: {}", emoji);
    }

    emoji <<= 8;
    if(g_emojiDB->retrieve(emoji, 0, 0, &tokenW, &tokenH, &h1, &fps, &frameCount)){
        token.box.info.w       = tokenW;
        token.box.info.h       = tokenH;
        token.box.state.h1     = h1;
        token.box.state.h2     = tokenH - h1;
        token.emoji.key     = emoji;
        token.emoji.fps        = fps;
        token.emoji.frameCount = frameCount;
    }
    return token;
}

TOKEN XMLTypeset::createToken(int leafIndex, int leafOff) const
{
    switch(auto &lf = m_paragraph->leaf(leafIndex); lf.type()){
        case LEAF_UTF8STR:
            {
                const auto font      = lf.font()     .value_or(m_font);
                const auto fontSize  = lf.fontSize() .value_or(m_fontSize);
                const auto fontStyle = lf.fontStyle().value_or(m_fontStyle);
                return buildUTF8Token(leafIndex, font, fontSize, fontStyle, lf.peekUTF8Code(leafOff));
            }
        case LEAF_EMOJI:
            {
                return buildEmojiToken(leafIndex, lf.emojiU32Key());
            }
        case LEAF_IMAGE:
            {
                throw fflpanic("not supported yet");
            }
        default:
            {
                throw fflpanic("invalid type: {}", lf.type());
            }
    }
}

std::vector<TOKEN> XMLTypeset::createTokenLine(int leafIndex, int leafOff, int &maxH1, int &maxH2, std::vector<TOKEN> *tokenListPtr) const
{
    std::vector<TOKEN> tokenList;
    if(tokenListPtr){
        std::swap(tokenList, *tokenListPtr);
    }

    tokenList.clear();
    if(m_paragraph->leaf(leafIndex).wrap().value_or(true)){
        tokenList.push_back(createToken(leafIndex, leafOff));
        maxH1 = std::max<int>(maxH1, tokenList.back().box.state.h1);
        maxH2 = std::max<int>(maxH2, tokenList.back().box.state.h2);
    }
    else{
        if(leafOff != 0){
            throw fflpanic("pick up token in middle of non-wrap leaf");
        }

        if(m_paragraph->leaf(leafIndex).type() != LEAF_UTF8STR){
            throw fflpanic("non-text leaf node has wrap attribute disabled");
        }

        for(int i = 0; i < m_paragraph->leaf(leafIndex).length(); ++i){
            tokenList.push_back(createToken(leafIndex, i));
            maxH1 = std::max<int>(maxH1, tokenList.back().box.state.h1);
            maxH2 = std::max<int>(maxH2, tokenList.back().box.state.h2);
        }
    }
    return tokenList;
}

// rebuild the board from token (x, y)
// assumen:
//      (0, 0) ~ prevTokenLoc(x, y) are valid
// output:
//      valid board
// notice:
// this function may get called after any XML update, the (x, y) in XMLTypeset may be
// invalid but as long as it's valid in XMLParagraph it's well-defined
void XMLTypeset::buildTypeset(int x, int y)
{
    for(int line = 0; line < y; ++line){
        if(lineEmpty(line)){
            throw fflpanic("line {} is empty", line);
        }
    }

    int leafIndex = 0;
    int leafOff = 0;

    if(x || y){
        if(const auto [startLeaf, startLeafOff] = leafLocInXMLParagraph(x, y); !m_paragraph->leaf(startLeaf).wrap().value_or(true)){
            std::tie(x, y) = prevTokenLoc(x, y, startLeafOff);
        }
    }

    if(x || y){
        const auto [prevX, prevY] = prevTokenLoc(x, y);
        const auto [prevLeaf, prevLeafOff] = leafLocInXMLParagraph(prevX, prevY);

        int advanced = 0;
        std::tie(leafIndex, leafOff, advanced) = m_paragraph->nextLeafOff(prevLeaf, prevLeafOff, 1);

        if(advanced == 0){
            // only prev location is valid
            // from (x, y) all token should be removed
            // leave maxH1/maxH2 as it is, there may be descrepency
            m_leafInfoList.resize(prevLeaf + 1);

            m_lineList.resize(prevY + 1);
            m_lineList[prevY].startY = 0;
            m_lineList[prevY].content.resize(prevX + 1);

            resetOneLine(prevY, true);
            resetBoardPixelRegion();
            return;
        }
    }

    // we start to push token from (leafIndex, leafOff)
    // if current it's a utf8String and not start from the beginning, we should keep the leaf record
    m_leafInfoList.resize(leafIndex + to_d(leafOff > 0));

    m_lineList.resize(y + 1);
    m_lineList[y].startY = 0;
    m_lineList[y].content.resize(x);

    int currLine = y;
    int advanced = -1;

    std::vector<TOKEN> tokenList;
    for(; (advanced < 0) || (advanced == to_d(tokenList.size())); std::tie(leafIndex, leafOff, advanced) = m_paragraph->nextLeafOff(leafIndex, leafOff, tokenList.size())){
        // if not start from offset 0, we use cached maxH1/maxH2
        // this can cause some mismatch because [0, offset) may contain token with maxH1/maxH2 smaller than the cached value
        //
        // since a leaf use same font/size/style
        // the descrepency is acceptable, leave it as it is for now.
        int firstMaxH1 = 0;
        int firstMaxH2 = 0;

        int &maxH1 = (leafOff == 0) ? firstMaxH1 : m_leafInfoList.at(leafIndex).maxH1;
        int &maxH2 = (leafOff == 0) ? firstMaxH2 : m_leafInfoList.at(leafIndex).maxH2;

        tokenList = createTokenLine(leafIndex, leafOff, maxH1, maxH2, &tokenList);
        if(addRawTokenLine(currLine, tokenList)){
            if(leafOff == 0){
                m_leafInfoList.push_back(LeafInfo
                {
                    .tokenX = to_d(m_lineList[currLine].content.size() - tokenList.size()),
                    .tokenY = currLine,

                    .maxH1 = firstMaxH1,
                    .maxH2 = firstMaxH2,
                });
            }
            continue;
        }

        resetOneLine(currLine, false);

        currLine++;
        m_lineList.resize(currLine + 1);

        if(!addRawTokenLine(currLine, tokenList)){
            throw fflpanic("insert token to a new line failed: line = {}", currLine);
        }

        if(leafOff == 0){
            m_leafInfoList.push_back(LeafInfo
            {
                .tokenX = to_d(m_lineList[currLine].content.size() - tokenList.size()),
                .tokenY = currLine,

                .maxH1 = firstMaxH1,
                .maxH2 = firstMaxH2,
            });
        }
    }

    resetOneLine(currLine, true);
    resetBoardPixelRegion();
}

std::tuple<int, int> XMLTypeset::leafLocInXMLParagraph(int tokenX, int tokenY) const
{
    if(!tokenLocValid(tokenX, tokenY)){
        throw fflpanic("invalid token location: ({}, {})", tokenX, tokenY);
    }

    const int leafIndex = getToken(tokenX, tokenY)->leaf;
    const auto [startTokenX, startTokenY] = leafTokenLoc(leafIndex);

    if(getToken(startTokenX, startTokenY)->leaf != leafIndex){
        throw fflpanic("invalid start token location");
    }

    if(tokenY == startTokenY){
        return {leafIndex, tokenX - startTokenX};
    }

    int tokenOff = lineTokenCount(startTokenY) - startTokenX;
    for(int line = startTokenY + 1; line < tokenY; ++line){
        tokenOff += lineTokenCount(line);
    }

    tokenOff += tokenX;
    return {leafIndex, tokenOff};
}

void XMLTypeset::resetBoardPixelRegion()
{
    if(lineCount() == 0){
        m_px = 0;
        m_py = 0;
        m_pw = 0;
        m_ph = 0;
        return;
    }

    int maxPX = 0;
    int maxPY = 0;
    int minPX = INT_MAX;
    int minPY = INT_MAX;

    for(int argLine = 0; lineValid(argLine); ++argLine){
        if(!lineTokenCount(argLine)){
            throw fflpanic("found empty line in XMLTypeset: line = {}", argLine);
        }

        maxPX = std::max<int>(maxPX, lineReachMaxX(argLine));
        maxPY = std::max<int>(maxPY, lineReachMaxY(argLine));
        minPX = std::min<int>(minPX, lineReachMinX(argLine));
        minPY = std::min<int>(minPY, lineReachMinY(argLine));
    }

    m_px = minPX;
    m_py = minPY;
    m_pw = maxPX + 1 - minPX;
    m_ph = maxPY + 1 - minPY;
}

std::tuple<int, int> XMLTypeset::prevTokenLoc(int argX, int argY, int tokenCount, bool strict) const
{
    fflassert(tokenLocValid(argX, argY), argX, argY);
    fflassert(tokenCount >= 0, tokenCount);

    while(tokenCount > argX){ // need to cross to previous line
        if(argY == 0){
            if(strict){
                throw fflpanic("try to find {} token{} before ({}, {})", tokenCount, (tokenCount > 1) ? "s" : "", argX, argY);
            }
            return {0, 0};
        }

        tokenCount -= (argX + 1);
        argY--;
        argX = lineTokenCount(argY) - 1;
    }

    return {argX - tokenCount, argY};
}

std::tuple<int, int> XMLTypeset::nextTokenLoc(int argX, int argY, int tokenCount, bool strict) const
{
    fflassert(tokenLocValid(argX, argY), argX, argY);
    fflassert(tokenCount >= 0, tokenCount);

    for(int maxMoves = lineTokenCount(argY) - argX - 1; tokenCount > maxMoves;){ // maxMoves: max number of moves without goto next line
        if(argY + 1 >= lineCount()){
            if(strict){
                throw fflpanic("try to find {} token{} location after ({}, {})", tokenCount, (tokenCount > 1) ? "s" : "", argX, argY);
            }
            return lastTokenLoc();
        }

        tokenCount -= (maxMoves + 1); // cross to next line
        argX = 0;
        argY++;
        maxMoves = lineTokenCount(argY) - 1;
    }

    return {argX + tokenCount, argY};
}

std::tuple<int, int> XMLTypeset::prevCursorLoc(int argX, int argY, bool allowDupCursorLoc, bool strict) const
{
    return prevCursorLoc(argX, argY, 1, allowDupCursorLoc, strict);
}

std::tuple<int, int> XMLTypeset::nextCursorLoc(int argX, int argY, bool allowDupCursorLoc, bool strict) const
{
    return nextCursorLoc(argX, argY, 1, allowDupCursorLoc, strict);
}

std::tuple<int, int> XMLTypeset::prevCursorLoc(int argX, int argY, int hopCount, bool allowDupCursorLoc, bool strict) const
{
    fflassert(cursorLocValid(argX, argY), argX, argY);
    fflassert(hopCount >= 0, hopCount);

    while(hopCount > argX){ // need to cross to previous line
        if(argY == 0){
            if(strict){
                throw fflpanic("try to find {} cursor location{} before ({}, {})", hopCount, (hopCount > 1) ? "s" : "", argX, argY);
            }
            return {0, 0};
        }

        hopCount -= (argX + 1);
        argY--;
        argX = lineTokenCount(argY) - (allowDupCursorLoc ? 0 : 1);
    }

    return {argX - hopCount, argY};
}

std::tuple<int, int> XMLTypeset::nextCursorLoc(int argX, int argY, int hopCount, bool allowDupCursorLoc, bool strict) const
{
    fflassert(cursorLocValid(argX, argY), argX, argY);
    fflassert(hopCount >= 0, hopCount);

    // if typeset is empty
    // need special handling otherwise lineTokenCount(0) throws

    if(empty()){
        if(hopCount > 0 && strict){
            throw fflpanic("try to find {} cursor location{} after ({}, {})", hopCount, (hopCount > 1) ? "s" : "", argX, argY);
        }
        else{
            return {0, 0};
        }
    }

    const auto fnMaxCursorMoves = [this, allowDupCursorLoc](int cursorX, int cursorY)
    {
        if(allowDupCursorLoc || (cursorY + 1 >= lineCount())){
            return lineTokenCount(cursorY) - cursorX;
        }
        else{
            return lineTokenCount(cursorY) - cursorX - 1;
        }
    };

    // if we disable allowDupCursorLoc, and current line is not the last line
    // then during moving we don't allow cursor to reach line's right-most boundary cursor location
    //
    // but the initial place (argX, argY) may be already at this cursor location
    // then we need to canonicalize the initial location by move it to next line's left-most cursor location, otherwise fnMaxCursorMoves returns -1

    if(!allowDupCursorLoc && (argX == lineTokenCount(argY))){
        if(hopCount == 0){
            return {argX, argY}; // don't canonicalize if it's an NOP
        }
        else if(argY + 1 < lineCount()){
            argX = 0;
            argY++;
        }
        else if(strict){
            throw fflpanic("try to find {} cursor location{} after ({}, {})", hopCount, (hopCount > 1) ? "s" : "", argX, argY);
        }
        else{
            return lastCursorLoc();
        }
    }

    for(int maxCursorMoves = fnMaxCursorMoves(argX, argY); hopCount > maxCursorMoves;){ // maxCursorMoves: max number of moves without goto next line
        if(argY + 1 >= lineCount()){
            if(strict){
                throw fflpanic("try to find {} cursor location{} after ({}, {})", hopCount, (hopCount > 1) ? "s" : "", argX, argY);
            }
            return lastCursorLoc();
        }

        hopCount -= (maxCursorMoves + 1); // cross to next line
        argX = 0;
        argY++;
        maxCursorMoves = fnMaxCursorMoves(argX, argY);
    }

    return {argX + hopCount, argY};
}

std::optional<std::tuple<int, int>> XMLTypeset::tokenLocBeforeCursor(int argX, int argY) const
{
    fflassert(cursorLocValid(argX, argY), argX, argY);
    if(argX == 0 && argY == 0){
        return std::nullopt;
    }
    else if(argX == 0){
        return std::make_tuple(lineTokenCount(argY - 1) - 1, argY - 1);
    }
    else{
        return std::make_tuple(argX - 1, argY);
    }
}

void XMLTypeset::deleteToken(int x, int y, int tokenCount)
{
    if(!tokenLocValid(x, y)){
        throw fflpanic("invalid token location: ({}, {})", x, y);
    }

    if(tokenCount <= 0){
        return;
    }

    int newX = x;
    int newY = y;

    for(int i = 0; i < tokenCount; ++i){
        if(!tokenLocValid(newX, newY)){
            break;
        }

        if(newX == 0 && newY == 0){
            break;
        }

        std::tie(newX, newY) = prevTokenLoc(newX, newY);
    }

    const auto [leafIndex, leafOff] = leafLocInXMLParagraph(x, y);
    m_paragraph->deleteToken(leafIndex, leafOff, tokenCount);

    if(m_paragraph->empty()){
        clear();
    }
    else{
        buildTypeset(newX, newY);
    }
}

size_t XMLTypeset::insertUTF8String(int x, int y, const char *text)
{
    if(!cursorLocValid(x, y)){
        throw fflpanic("invalid cursor location: ({}, {})", x, y);
    }

    const std::string xmlText = xmlf::toParString("%s", text);
    if(m_paragraph->empty()){
        m_paragraph->loadXML(xmlText.c_str());
        if(m_paragraph->leafCount() > 0){
            buildTypeset(0, 0);
        }
        return m_paragraph->tokenCount();
    }

    if((y + 1 < lineCount()) && (x == lineTokenCount(y))){
        y++;
        x = 0;
    }

    // XMLParagraph doesn't have cursor
    // need to parse here

    if(x == 0 && y == 0){
        size_t addedCount = 0;
        if(m_paragraph->leaf(0).type() != LEAF_UTF8STR){
            addedCount = m_paragraph->insertLeafXML(0, xmlText.c_str());
        }
        else{
            addedCount = m_paragraph->insertUTF8String(0, 0, text);
        }
        buildTypeset(0, 0);
        return addedCount;
    }

    // we are appending at the last location
    // can't call prevTokenLoc() because this tokenLoc is invalid

    if((y == lineCount() - 1) && (x == lineTokenCount(y))){
        size_t addedCount = 0;
        if(m_paragraph->backLeaf().type() != LEAF_UTF8STR){
            addedCount = m_paragraph->insertLeafXML(leafCount(), xmlText.c_str());
        }
        else{
            addedCount = m_paragraph->insertUTF8String(leafCount() - 1, m_paragraph->backLeaf().utf8CharOff().size(), text);
        }
        buildTypeset(0, 0);
        return addedCount;
    }

    const auto [prevTX, prevTY] = prevTokenLoc(x, y);
    const auto currLeaf = getToken(x, y)->leaf;
    const auto prevLeaf = getToken(prevTX, prevTY)->leaf;

    size_t addedCount = 0;
    if(prevLeaf == currLeaf){
        if(m_paragraph->leaf(prevLeaf).type() != LEAF_UTF8STR){
            throw fflpanic("only UTF8 leaf can have length great than 1");
        }

        const auto [leafIndex, leafOff] = leafLocInXMLParagraph(x, y);
        addedCount = m_paragraph->insertUTF8String(currLeaf, leafOff, text);
    }
    else{
        if(m_paragraph->leaf(prevLeaf).type() == LEAF_UTF8STR){
            addedCount = m_paragraph->insertUTF8String(prevLeaf, m_paragraph->leaf(prevLeaf).utf8CharOff().size(), text);
        }
        else if(m_paragraph->leaf(currLeaf).type() == LEAF_UTF8STR){
            addedCount = m_paragraph->insertUTF8String(currLeaf, 0, text);
        }
        else{
            addedCount = m_paragraph->insertLeafXML(currLeaf, xmlText.c_str());
        }
    }

    buildTypeset(0, 0);
    return addedCount;
}

XMLTypeset *XMLTypeset::split(int cursorX, int cursorY)
{
    fflassert(cursorLocValid(cursorX, cursorY));
    auto newTpset = new XMLTypeset
    {
        MaxLineWidth(),
        m_lineAlign,
        CanThrough(),
        m_compactLine,

        m_font,
        m_fontSize,
        m_fontStyle,
        m_fontColor,
        m_fontBGColor,
        m_imageMaskColor,

        m_lineSpace,
        m_wordSpace,
    };

    if(m_paragraph->empty()){
        return newTpset;
    }

    const auto [leafIndex, cursorInLeaf] = [cursorX, cursorY, this]() -> std::tuple<int, int>
    {
        if(std::tie(cursorX, cursorY) == lastCursorLoc()){
            return {m_paragraph->leafCount() - 1, m_paragraph->leaf(m_paragraph->leafCount() - 1).length()};
        }
        else if(cursorX == lineTokenCount(cursorY)){
            const auto [leaf, off] = leafLocInXMLParagraph(cursorX - 1, cursorY);
            return {leaf, off + 1};
        }
        else{
            return leafLocInXMLParagraph(cursorX, cursorY);
        }
    }();

    const bool copyFullLine = (cursorX == lineTokenCount(cursorY));
    const bool copyLeafTLoc = (cursorInLeaf > 0);

    newTpset->m_lineList    .assign(m_lineList    .begin(), m_lineList    .begin() + cursorY   + (copyFullLine ? 1 : 0));
    newTpset->m_leafInfoList.assign(m_leafInfoList.begin(), m_leafInfoList.begin() + leafIndex + (copyLeafTLoc ? 1 : 0));
    newTpset->m_paragraph.reset(m_paragraph->split(leafIndex, cursorInLeaf));

    m_lineList.clear();
    m_leafInfoList.clear();

    if(!newTpset->empty()){
        if(copyFullLine){
            newTpset->buildTypeset(0, cursorY);
        }
        else{
            newTpset->buildTypeset(0, std::max<int>(cursorY - 1, 0)); // split point can be in middle of first line
        }
    }

    if(empty()){
        resetBoardPixelRegion();
    }
    else{
        buildTypeset(0, 0);
    }

    return newTpset;
}

void XMLTypeset::join(const XMLTypeset &input, bool append)
{
    if(std::addressof(input) == this){
        throw fflpanic("cannot join XMLTypeset with itself");
    }

    if(input.empty()){
        return;
    }

    const int startLine = (append && !empty()) ? (lineCount() - 1) : 0;
    m_paragraph->join(*input.m_paragraph, append);
    buildTypeset(0, startLine);
}

void XMLTypeset::drawImGui(ImDrawList *drawList, const int dstX, const int dstY) const
{
    fflassert(drawList);

    uint32_t foreground = 0;
    uint32_t background = 0;
    int lastLeaf = -1;
    for(int line = 0; line < lineCount(); ++line){
        for(int token = 0; token < lineTokenCount(line); ++token){
            const auto tokenPtr = getToken(token, line);
            const auto &leaf = m_paragraph->leaf(tokenPtr->leaf);
            const auto &leafInfo = m_leafInfoList.at(tokenPtr->leaf);

            if(lastLeaf != tokenPtr->leaf){
                foreground = leaf.color().value_or(color());
                background = leaf.bgColor().value_or(bgColor());
                lastLeaf = tokenPtr->leaf;
            }

            if(colorf::A(background)){
                const int x = tokenPtr->box.state.x - tokenPtr->box.state.w1;
                const int y = tokenPtr->box.state.y + tokenPtr->box.state.h1 - leafInfo.maxH1;
                const int w = tokenPtr->box.info.w + tokenPtr->box.state.w1 + tokenPtr->box.state.w2;
                const int h = leafInfo.maxH1 + leafInfo.maxH2;
                drawList->AddRectFilled(
                    {to_f(dstX + x), to_f(dstY + y)},
                    {to_f(dstX + x + w), to_f(dstY + y + h)},
                    background);
            }

            const int boxX = tokenPtr->box.state.x;
            const int boxY = tokenPtr->box.state.y;
            const int boxW = tokenPtr->box.info.w;
            const int boxH = tokenPtr->box.info.h;
            switch(leaf.type()){
                case LEAF_UTF8STR:
                    {
                        if(const auto texture = g_fontexDB->retrieve(tokenPtr->utf8char.key); texture){
                            drawList->AddImage(
                                texture,
                                {to_f(dstX + boxX), to_f(dstY + boxY)},
                                {to_f(dstX + boxX + boxW), to_f(dstY + boxY + boxH)},
                                {0.0f, 0.0f},
                                {to_f(boxW) / texture.w, to_f(boxH) / texture.h},
                                foreground);
                        }
                        break;
                    }
                case LEAF_IMAGE:
                    {
                        break;
                    }
                case LEAF_EMOJI:
                    {
                        const auto emojiKey = tokenPtr->emoji.frameCount && tokenPtr->emoji.fps
                                            ? (tokenPtr->emoji.key & 0XFFFFFF00) + tokenPtr->emoji.frame % tokenPtr->emoji.frameCount
                                            : tokenPtr->emoji.key & 0XFFFFFF00;
                        int textureX = 0;
                        int textureY = 0;
                        if(const auto texture = g_emojiDB->retrieve(emojiKey, &textureX, &textureY, 0, 0, 0, 0, 0); texture){
                            drawList->AddImage(
                                texture,
                                {to_f(dstX + boxX), to_f(dstY + boxY)},
                                {to_f(dstX + boxX + boxW), to_f(dstY + boxY + boxH)},
                                {to_f(textureX) / texture.w, to_f(textureY) / texture.h},
                                {to_f(textureX + boxW) / texture.w, to_f(textureY + boxH) / texture.h},
                                m_imageMaskColor);
                        }
                        break;
                    }
                default:
                    {
                        throw fflpanic("invalid leaf type: {}", leaf.type());
                    }
            }
        }
    }
}

void XMLTypeset::update(double fMS)
{
    for(int leafIndex = 0; leafIndex < m_paragraph->leafCount(); ++leafIndex){
        if(m_paragraph->leaf(leafIndex).type() == LEAF_EMOJI){
            const auto [x, y] = leafTokenLoc(leafIndex);
            if(auto pToken= getToken(x, y); pToken->emoji.fps != 0){
                double fPeroidMS = 1000.0 / pToken->emoji.fps;
                double fCurrTick = fMS + pToken->emoji.tick;

                auto nAdvancedFrame  = to_u8(fCurrTick / fPeroidMS);
                pToken->emoji.tick   = to_u8(fCurrTick - nAdvancedFrame * fPeroidMS);
                pToken->emoji.frame += nAdvancedFrame;
            }
        }
    }
}

std::string XMLTypeset::getText() const
{
    std::string plainString;
    for(int i = 0; i < m_paragraph->leafCount(); ++i){
        switch(auto leafType = m_paragraph->leaf(i).type()){
            case LEAF_UTF8STR:
                {
                    plainString += m_paragraph->leaf(i).utf8Text();
                    break;
                }
            case LEAF_IMAGE:
                {
                    plainString += str_printf("\\image{0x%016llx}", to_llu(m_paragraph->leaf(i).imageU64Key()));
                    break;
                }
            case LEAF_EMOJI:
                {
                    plainString += str_printf("\\emoji{0x%016llx}", to_llu(m_paragraph->leaf(i).emojiU32Key()));
                    break;
                }
            default:
                {
                    throw fflpanic("invalid leaf type: {}", leafType);
                }
        }
    }
    return plainString;
}

int XMLTypeset::GetTokenWordSpace(int nX, int nY) const
{
    if(!tokenLocValid(nX, nY)){
        throw fflpanic("invalid token location: ({}, {})", nX, nY);
    }
    return m_wordSpace;
}

int XMLTypeset::lineReachMaxX(int argLine) const
{
    if(!lineValid(argLine)){
        throw fflpanic("invalid line: {}", argLine);
    }

    if(lineTokenCount(argLine) == 0){
        throw fflpanic("invalie empty line: {}", argLine);
    }

    const auto tokPtr = GetLineBackToken(argLine);

    if(tokPtr->box.info.w <= 0){
        throw fflpanic("invalid token width: {}", tokPtr->box.info.w);
    }

    return tokPtr->box.state.x + tokPtr->box.info.w - 1;
}

int XMLTypeset::lineReachMaxY(int argLine) const
{
    if(!lineValid(argLine)){
        throw fflpanic("invalid line: {}", argLine);
    }
    return m_lineList[argLine].startY + LineMaxHk(argLine, 2);
}

int XMLTypeset::lineReachMinX(int argLine) const
{
    if(!lineValid(argLine)){
        throw fflpanic("invalid line: {}", argLine);
    }

    if(lineTokenCount(argLine) == 0){
        throw fflpanic("invalie empty line: {}", argLine);
    }

    return getToken(0, argLine)->box.state.x;
}

int XMLTypeset::lineReachMinY(int argLine) const
{
    if(!lineValid(argLine)){
        throw fflpanic("invalid line: {}", argLine);
    }
    return m_lineList[argLine].startY - LineMaxHk(argLine, 1) + 1;
}

int XMLTypeset::LineMaxHk(int argLine, int k) const
{
    // nHk = 1: get MaxH1
    // nHk = 2: get MaxH2

    if(!lineValid(argLine)){
        throw fflpanic("invalid line: {}", argLine);
    }

    if(k != 1 && k != 2){
        throw fflpanic("invalid argument k: {}", k);
    }

    std::optional<uint32_t> fontInfoOpt;
    int fontHeightMetric = 0;
    int currMaxHk = 0;

    for(int nIndex = 0; nIndex < lineTokenCount(argLine); ++nIndex){
        const auto tokenPtr = getToken(nIndex, argLine);
        /* */ auto tokenHk  = (k == 1) ? tokenPtr->box.state.h1 : tokenPtr->box.state.h2;

        if(!m_compactLine && m_paragraph->leaf(tokenPtr->leaf).type() == LEAF_UTF8STR){
            const uint32_t fontInfo = utf8f::fontInfoFromU64Key(tokenPtr->utf8char.key);
            if(fontInfoOpt.has_value() && fontInfoOpt.value() == fontInfo){
                // token uses same font as last one
                // reuse the cached info
            }
            else{
                const auto [fontIndex, fontSize, _u1, _u2] = utf8f::extractU64Key(tokenPtr->utf8char.key);
                fontInfoOpt = fontInfo;

                if(k == 1){ fontHeightMetric =                   g_fontexDB->fontAscent (fontIndex, fontSize) ; }
                else      { fontHeightMetric = std::max<int>(0, -g_fontexDB->fontDescent(fontIndex, fontSize)); }
            }
            tokenHk = std::max<int>(tokenHk, fontHeightMetric);
        }

        currMaxHk = std::max<int>(currMaxHk, tokenHk);
    }
    return currMaxHk;
}

int XMLTypeset::lineAlign() const
{
    if(MaxLineWidth() == 0){
        return LALIGN_LEFT;
    }
    return m_lineAlign;
}

std::tuple<int, int> XMLTypeset::locCursor(int xOffPixel, int yOffPixel) const
{
    if(empty()){
        return {0, 0};
    }

    const auto cursorY = [this, yOffPixel]() -> int
    {
        const auto pLine = std::lower_bound(m_lineList.begin(), m_lineList.end(), yOffPixel, [](const auto &parmX, const auto &parmY)
        {
            return parmX.startY < parmY;
        });

        if(pLine == m_lineList.end()){
            return lineCount() - 1;
        }
        return to_d(std::distance(m_lineList.begin(), pLine));
    }();

    if(lineEmpty(cursorY)){
        return {0, cursorY};
    }

    const auto cursorX = [cursorY, xOffPixel, this]() -> int
    {
        const auto iter_begin = m_lineList.at(cursorY).content.begin();
        const auto iter_end   = m_lineList.at(cursorY).content.end();

        const auto pBox = std::lower_bound(iter_begin, iter_end, xOffPixel, [](const auto &parmX, const auto &parmY)
        {
            return parmX.box.state.x < parmY;
        });
        return std::distance(m_lineList.at(cursorY).content.begin(), pBox);
    }();

    return {cursorX, cursorY};
}

int XMLTypeset::cursorLoc2Off(int argCursorX, int argCursorY) const
{
    fflassert(cursorLocValid(argCursorX, argCursorY), argCursorX, argCursorY);
    int off = 0;
    for(int line = 0; line < argCursorY; ++line){
        off += lineTokenCount(line);
    }
    return off + argCursorX;
}

std::tuple<int, int> XMLTypeset::cursorOff2Loc(int argCursorOff) const
{
    if(empty()){
        fflassert(argCursorOff == 0, argCursorOff);
        return firstCursorLoc();
    }

    int line = 0;
    int tokenLeft = argCursorOff;

    for(; line < lineCount(); ++line){
        if(tokenLeft <= lineTokenCount(line)){
            return {tokenLeft, line};
        }
        else{
            tokenLeft -= lineTokenCount(line);
        }
    }

    throw fflvalue(argCursorOff);
}

bool XMLTypeset::locInToken(int xOffPixel, int yOffPixel, const TOKEN *pToken, bool withPadding)
{
    if(!pToken){
        throw fflpanic("null token pointer");
    }

    const int startX = pToken->box.state.x - (withPadding ? pToken->box.state.w1 : 0);
    const int startY = pToken->box.state.y;
    const int boxW   = pToken->box.info.w + (withPadding ? (pToken->box.state.w1 + pToken->box.state.w2) : 0);
    const int boxH   = pToken->box.info.h;

    return mathf::pointInRectangle(xOffPixel, yOffPixel, startX, startY, boxW, boxH);
}

std::tuple<int, int> XMLTypeset::locToken(int xOffPixel, int yOffPixel, bool withPadding) const
{
    if(yOffPixel < 0 || yOffPixel >= ph()){
        return {-1, -1};
    }

    const auto [cursorX, cursorY] = locCursor(xOffPixel, yOffPixel);
    if(!cursorLocValid(cursorX, cursorY)){
        throw fflpanic("invalid cursor location: ({}, {})", cursorX, cursorY);
    }

    for(const int tokenX: {cursorX - 1, cursorX, cursorX + 1}){
        if(!tokenLocValid(tokenX, cursorY)){
            continue;
        }

        if(locInToken(xOffPixel, yOffPixel, getToken(tokenX, cursorY), withPadding)){
            return {tokenX, cursorY};
        }
    }
    return {-1, -1};
}

bool XMLTypeset::blankToken(int x, int y) const
{
    if(!tokenLocValid(x, y)){
        throw fflpanic("invalid token location: x = {}, y = {}", x, y);
    }

    const auto tokenPtr = getToken(x, y);
    const auto &leaf = m_paragraph->leaf(tokenPtr->leaf);

    const auto fnCheckBlank = [](uint64_t u64Key)
    {
        const auto [fontIndex, fontSize, _, codePoint] = utf8f::extractU64Key(u64Key);
        return g_fontexDB->isTransparant(fontIndex, fontSize, codePoint);
    };

    return (leaf.type() == LEAF_UTF8STR) && fnCheckBlank(tokenPtr->utf8char.key);
}

void XMLTypeset::setLineWidth(int lineWidth)
{
    m_lineWidth = lineWidth;
    updateGfx();
}

std::tuple<int, int> XMLTypeset::getDefaultFontHk() const
{
    return
    {
                          g_fontexDB->fontAscent (m_font, m_fontSize),
        std::max<int>(0, -g_fontexDB->fontDescent(m_font, m_fontSize)),
    };
}

std::tuple<int, int> XMLTypeset::getTokenCursorHk(int tokenX, int tokenY) const
{
    // in compactLine mode, the ascent/descent is not reserved for utf8 chars
    // if still uses ascent/descent for cursor height, the cursor may go cross gfx of previous/next line tokens

    const auto tkptr = getToken(tokenX, tokenY);
    if(!m_compactLine && (m_paragraph->leaf(tkptr->leaf).type() == LEAF_UTF8STR)){
        const auto [fontIndex, fontSize, _u1, _u2] = utf8f::extractU64Key(tkptr->utf8char.key);
        return
        {
                              g_fontexDB->fontAscent (fontIndex, fontSize),
            std::max<int>(0, -g_fontexDB->fontDescent(fontIndex, fontSize)),
        };
    }

    return m_leafInfoList.at(tkptr->leaf).maxHk();
}

int XMLTypeset::getDefaultFontHeight() const
{
    const auto [fontH1, fontH2] = getDefaultFontHk();
    return fontH1 + fontH2;
}
