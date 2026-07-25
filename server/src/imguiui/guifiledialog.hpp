#pragma once

#include <string>
#include <vector>
#include <filesystem>

class GUIFileDialog
{
    public:
        enum class Mode
        {
            File,
            Directory,
        };

    private:
        struct Entry
        {
            std::filesystem::path path;
            bool isDirectory = false;
        };

    private:
        std::string m_id;
        std::string m_title;
        std::string m_extension;
        std::filesystem::path m_currentDirectory;
        std::vector<Entry> m_entries;
        int m_selectedEntry = -1;
        bool m_openRequested = false;
        Mode m_mode = Mode::File;

    public:
        explicit GUIFileDialog(const char *id)
            : m_id(id)
        {}

    public:
        void open(const char *title, const char *initialPath, Mode mode, const char *extension = nullptr);
        bool draw(std::string &selectedPath, bool *cancelled = nullptr);

    private:
        void refresh();
        void enterDirectory(const std::filesystem::path &);
};
