#include "TranslationManager.h"

#include "StringUtils.h"

#include <Windows.h>

#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

std::mutex g_dictionaryMutex;
std::unordered_map<std::wstring, std::wstring> g_dictionary;
TranslationManagerConfig g_config;
std::wstring g_dictionaryPath;
thread_local std::wstring g_translationBuffer;

constexpr wchar_t kWildcardToken[] = L"{*}";
constexpr size_t kWildcardTokenLength = 3;

struct WildcardEntry {
    std::wstring source;
    std::wstring translated;
    std::vector<std::wstring> sourceParts;
    bool translatedHasWildcard = false;
};

std::vector<WildcardEntry> g_wildcardDictionary;

bool Utf8ToWide(const std::string& utf8, std::wstring& wide) {
    if (utf8.empty()) {
        wide.clear();
        return true;
    }

    const int size = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        utf8.data(),
        static_cast<int>(utf8.size()),
        nullptr,
        0
    );
    if (size <= 0) {
        return false;
    }

    wide.resize(static_cast<size_t>(size));
    const int written = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        utf8.data(),
        static_cast<int>(utf8.size()),
        wide.data(),
        size
    );
    return written == size;
}

bool WideToUtf8(const std::wstring& wide, std::string& utf8) {
    if (wide.empty()) {
        utf8.clear();
        return true;
    }

    const int size = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        wide.data(),
        static_cast<int>(wide.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (size <= 0) {
        return false;
    }

    utf8.resize(static_cast<size_t>(size));
    const int written = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        wide.data(),
        static_cast<int>(wide.size()),
        utf8.data(),
        size,
        nullptr,
        nullptr
    );
    return written == size;
}

void RemoveUtf8Bom(std::string& text) {
    constexpr unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == bom[0] &&
        static_cast<unsigned char>(text[1]) == bom[1] &&
        static_cast<unsigned char>(text[2]) == bom[2]) {
        text.erase(0, 3);
    }
}

void RemoveTrailingCarriageReturn(std::string& text) {
    if (!text.empty() && text.back() == '\r') {
        text.pop_back();
    }
}

bool HasTranslatedText(const std::wstring& sourceText, const std::wstring& translatedText) {
    return !translatedText.empty() && translatedText != sourceText;
}

bool IsChineseCharacter(wchar_t ch) {
    return (ch >= 0x3400 && ch <= 0x4DBF) ||
           (ch >= 0x4E00 && ch <= 0x9FFF) ||
           (ch >= 0xF900 && ch <= 0xFAFF);
}

bool ContainsChineseText(const std::wstring& text) {
    for (const wchar_t ch : text) {
        if (IsChineseCharacter(ch)) {
            return true;
        }
    }
    return false;
}

bool CanWriteUntranslatedSource(const std::wstring& sourceText) {
    if (g_config.filterChineseSourceWrites && ContainsChineseText(sourceText)) {
        return false;
    }
    return true;
}

bool HasWildcard(const std::wstring& text) {
    return text.find(kWildcardToken) != std::wstring::npos;
}

std::vector<std::wstring> SplitWildcardPattern(const std::wstring& pattern) {
    std::vector<std::wstring> parts;
    size_t offset = 0;

    while (true) {
        const size_t token = pattern.find(kWildcardToken, offset);
        if (token == std::wstring::npos) {
            parts.push_back(pattern.substr(offset));
            return parts;
        }

        parts.push_back(pattern.substr(offset, token - offset));
        offset = token + kWildcardTokenLength;
    }
}

bool MatchWildcardPattern(
    const std::vector<std::wstring>& parts,
    const std::wstring& text,
    std::vector<std::wstring>& captures
) {
    captures.clear();
    if (parts.empty() || text.compare(0, parts.front().size(), parts.front()) != 0) {
        return false;
    }

    size_t textOffset = parts.front().size();
    const size_t wildcardCount = parts.size() - 1;

    for (size_t i = 0; i < wildcardCount; ++i) {
        const std::wstring& nextPart = parts[i + 1];

        if (i + 1 == wildcardCount) {
            if (nextPart.empty()) {
                captures.push_back(text.substr(textOffset));
                textOffset = text.size();
                continue;
            }

            if (text.size() < nextPart.size()) {
                return false;
            }

            const size_t suffixOffset = text.size() - nextPart.size();
            if (suffixOffset < textOffset ||
                text.compare(suffixOffset, nextPart.size(), nextPart) != 0) {
                return false;
            }

            captures.push_back(text.substr(textOffset, suffixOffset - textOffset));
            textOffset = text.size();
            continue;
        }

        if (nextPart.empty()) {
            captures.emplace_back();
            continue;
        }

        const size_t nextOffset = text.find(nextPart, textOffset);
        if (nextOffset == std::wstring::npos) {
            return false;
        }

        captures.push_back(text.substr(textOffset, nextOffset - textOffset));
        textOffset = nextOffset + nextPart.size();
    }

    return textOffset == text.size();
}

std::wstring ApplyWildcardCaptures(
    const std::wstring& translatedText,
    const std::vector<std::wstring>& captures
) {
    if (!HasWildcard(translatedText)) {
        return translatedText;
    }

    std::wstring result;
    size_t offset = 0;
    size_t captureIndex = 0;

    while (true) {
        const size_t token = translatedText.find(kWildcardToken, offset);
        if (token == std::wstring::npos) {
            result += translatedText.substr(offset);
            return result;
        }

        result += translatedText.substr(offset, token - offset);
        if (captureIndex < captures.size()) {
            result += captures[captureIndex];
        }

        ++captureIndex;
        offset = token + kWildcardTokenLength;
    }
}

bool NeedsLeadingNewline(const wchar_t* filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return false;
    }

    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size <= 0) {
        return false;
    }

    file.seekg(size - 1, std::ios::beg);
    char last = '\0';
    file.get(last);
    return last != '\n';
}

bool AppendUntranslatedLocked(const std::wstring& sourceText) {
    if (sourceText.empty() || g_dictionaryPath.empty()) {
        return false;
    }

    std::string sourceUtf8;
    const std::wstring escapedSource = StringUtils::Escape(sourceText);
    if (!WideToUtf8(escapedSource, sourceUtf8)) {
        return false;
    }

    const bool needsLeadingNewline = NeedsLeadingNewline(g_dictionaryPath.c_str());
    std::ofstream file(g_dictionaryPath.c_str(), std::ios::binary | std::ios::app);
    if (!file) {
        return false;
    }

    if (needsLeadingNewline) {
        file.put('\n');
    }
    file.write(sourceUtf8.data(), static_cast<std::streamsize>(sourceUtf8.size()));
    file.put('\t');
    file.write(sourceUtf8.data(), static_cast<std::streamsize>(sourceUtf8.size()));
    file.write("\r\n", 2);
    return static_cast<bool>(file);
}

bool WriteDictionaryFile(
    const wchar_t* filePath,
    const std::vector<std::wstring>& order,
    const std::unordered_map<std::wstring, std::wstring>& dictionary
) {
    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }

    for (const std::wstring& source : order) {
        const auto it = dictionary.find(source);
        if (it == dictionary.end()) {
            continue;
        }

        std::string sourceUtf8;
        std::string translatedUtf8;
        if (!WideToUtf8(StringUtils::Escape(source), sourceUtf8) ||
            !WideToUtf8(StringUtils::Escape(it->second), translatedUtf8)) {
            return false;
        }

        file.write(sourceUtf8.data(), static_cast<std::streamsize>(sourceUtf8.size()));
        file.put('\t');
        file.write(translatedUtf8.data(), static_cast<std::streamsize>(translatedUtf8.size()));
        file.write("\r\n", 2);
    }

    return static_cast<bool>(file);
}

bool BuildDictionaryPath(const wchar_t* dictionaryName, std::wstring& dictionaryPath) {
    dictionaryPath.clear();
    if (dictionaryName == nullptr || dictionaryName[0] == L'\0') {
        return false;
    }

    wchar_t modulePath[MAX_PATH] = {};
    HMODULE selfModule = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&TranslationManager::Initialize),
                            &selfModule)) {
        return false;
    }

    const DWORD length = GetModuleFileNameW(selfModule, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return false;
    }

    dictionaryPath.assign(modulePath, length);
    const size_t slash = dictionaryPath.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        dictionaryPath.resize(slash + 1);
    } else {
        dictionaryPath.clear();
    }
    dictionaryPath += dictionaryName;
    return true;
}

bool EnsureParentDirectoryExists(const std::wstring& filePath) {
    const size_t slash = filePath.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return true;
    }

    const std::wstring directory = filePath.substr(0, slash);
    if (directory.empty() || directory.size() >= MAX_PATH) {
        return !directory.empty();
    }

    wchar_t normalized[MAX_PATH] = {};
    wcscpy_s(normalized, directory.c_str());
    for (wchar_t* p = normalized; *p; ++p) {
        if (*p == L'/') {
            *p = L'\\';
        }
    }

    for (wchar_t* p = normalized; *p; ++p) {
        if (*p != L'\\') {
            continue;
        }

        if (p == normalized || *(p - 1) == L':') {
            continue;
        }

        *p = L'\0';
        CreateDirectoryW(normalized, nullptr);
        *p = L'\\';
    }

    return CreateDirectoryW(normalized, nullptr) ||
           GetLastError() == ERROR_ALREADY_EXISTS;
}

} // namespace

void TranslationManager::Configure(const TranslationManagerConfig& config) {
    std::lock_guard<std::mutex> lock(g_dictionaryMutex);
    g_config = config;
}

bool TranslationManager::Initialize(const wchar_t* dictionaryName) {
    std::wstring dictionaryPath;
    if (!BuildDictionaryPath(dictionaryName, dictionaryPath)) {
        return false;
    }
    if (!EnsureParentDirectoryExists(dictionaryPath)) {
        return false;
    }

    std::ifstream file(dictionaryPath.c_str(), std::ios::binary);
    if (!file) {
        std::ofstream createFile(dictionaryPath.c_str(), std::ios::binary | std::ios::app);
        if (!createFile) {
            return false;
        }
        createFile.close();
        file.open(dictionaryPath.c_str(), std::ios::binary);
        if (!file) {
            return false;
        }
    }

    std::unordered_map<std::wstring, std::wstring> loaded;
    std::vector<std::wstring> order;
    std::string line;
    bool isFirstLine = true;

    while (std::getline(file, line)) {
        RemoveTrailingCarriageReturn(line);
        if (isFirstLine) {
            RemoveUtf8Bom(line);
            isFirstLine = false;
        }

        if (line.empty()) {
            continue;
        }

        const size_t tab = line.find('\t');
        if (tab == std::string::npos) {
            continue;
        }

        std::wstring source;
        std::wstring translated;
        if (!Utf8ToWide(line.substr(0, tab), source) ||
            !Utf8ToWide(line.substr(tab + 1), translated)) {
            return false;
        }
        source = StringUtils::Unescape(source);
        translated = StringUtils::Unescape(translated);

        if (source.empty()) {
            continue;
        }

        const auto existing = loaded.find(source);
        if (existing == loaded.end()) {
            loaded.emplace(source, translated);
            order.push_back(source);
            continue;
        }

        if (!HasTranslatedText(source, existing->second) &&
            HasTranslatedText(source, translated)) {
            existing->second = translated;
        }
    }

    if (!WriteDictionaryFile(dictionaryPath.c_str(), order, loaded)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_dictionaryMutex);
    g_dictionary = std::move(loaded);
    g_wildcardDictionary.clear();
    for (const std::wstring& source : order) {
        const auto it = g_dictionary.find(source);
        if (it != g_dictionary.end() &&
            HasWildcard(source) &&
            HasTranslatedText(source, it->second)) {
            g_wildcardDictionary.push_back({
                source,
                it->second,
                SplitWildcardPattern(source),
                HasWildcard(it->second)
            });
        }
    }
    g_dictionaryPath = std::move(dictionaryPath);
    g_translationBuffer.clear();
    return true;
}

void TranslationManager::Clear() {
    std::lock_guard<std::mutex> lock(g_dictionaryMutex);
    g_dictionary.clear();
    g_wildcardDictionary.clear();
    g_dictionaryPath.clear();
    g_translationBuffer.clear();
}

wchar_t* TranslationManager::Translate(const wchar_t* sourceText, bool writeUntranslated) {
    if (sourceText == nullptr) {
        return nullptr;
    }

    const std::wstring source(sourceText);
    std::lock_guard<std::mutex> lock(g_dictionaryMutex);
    const auto it = g_dictionary.find(source);
    if (it != g_dictionary.end() && HasTranslatedText(source, it->second)) {
        g_translationBuffer = it->second;
        return g_translationBuffer.data();
    }

    std::vector<std::wstring> captures;
    for (const auto& wildcardEntry : g_wildcardDictionary) {
        if (MatchWildcardPattern(wildcardEntry.sourceParts, source, captures)) {
            g_translationBuffer = wildcardEntry.translatedHasWildcard
                ? ApplyWildcardCaptures(wildcardEntry.translated, captures)
                : wildcardEntry.translated;
            return g_translationBuffer.data();
        }
    }

    if (it == g_dictionary.end() && writeUntranslated) {
        if (CanWriteUntranslatedSource(source) && AppendUntranslatedLocked(source)) {
            g_dictionary.emplace(source, source);
        }
    }

    return nullptr;
}
