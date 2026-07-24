// Stub tinyfiledialogs for Windows build
// File dialogs are disabled - returns nullptr (no file selected)
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

static inline const char * tinyfd_openFileDialog(
    const char * const /*aTitle*/,
    const char * const /*aDefaultPathAndFile*/,
    const int /*aNumOfFilterPatterns*/,
    const char * const * const /*aFilterPatterns*/,
    const char * const /*aSingleFilterDescription*/,
    const int /*aAllowMultipleSelects*/)
{
    return nullptr;
}

static inline const char * tinyfd_selectFolderDialog(
    const char * const /*aTitle*/,
    const char * const /*aDefaultPath*/)
{
    return nullptr;
}

#ifdef __cplusplus
}
#endif
