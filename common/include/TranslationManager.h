#pragma once

struct TranslationManagerConfig {
    bool filterChineseSourceWrites = false;
};

class TranslationManager {
public:
    TranslationManager() = delete;

    static void Configure(const TranslationManagerConfig& config);
    static bool Initialize(const wchar_t* dictionaryName);
    static void Clear();

    static wchar_t* Translate(const wchar_t* sourceText, bool writeUntranslated);
};
