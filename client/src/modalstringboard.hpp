#pragma once

#include <memory>
#include <string>

class ModalStringBoard
{
    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
        std::u8string m_xmlString;

    public:
        ModalStringBoard();
        ~ModalStringBoard();

    public:
        void loadXML(std::u8string);
        void drawScreen(bool) const;
};
