#include "Encoding.h"

#include <Windows.h>

#include <cstring>
#include <limits>

namespace {

constexpr unsigned int GbkCodePage = 936;

DWORD MultiByteFlags(unsigned int codePage) {
    return codePage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0;
}

DWORD Utf16Flags(unsigned int codePage) {
    return codePage == CP_UTF8 ? WC_ERR_INVALID_CHARS : 0;
}

} // namespace

namespace Encoding {

bool MultiByteToUtf16(unsigned int codePage, const char* text, std::wstring& output) {
    output.clear();
    if (text == nullptr) {
        return false;
    }

    const size_t rawLength = std::strlen(text);
    if (rawLength > static_cast<size_t>((std::numeric_limits<int>::max)())) {
        return false;
    }

    const int length = static_cast<int>(rawLength);
    if (length == 0) {
        return true;
    }

    const int size = MultiByteToWideChar(
        codePage,
        MultiByteFlags(codePage),
        text,
        length,
        nullptr,
        0
    );
    if (size <= 0) {
        return false;
    }

    output.resize(static_cast<size_t>(size));
    const int written = MultiByteToWideChar(
        codePage,
        MultiByteFlags(codePage),
        text,
        length,
        output.data(),
        size
    );
    return written == size;
}

bool Utf16ToMultiByte(unsigned int codePage, const wchar_t* text, std::string& output) {
    output.clear();
    if (text == nullptr) {
        return false;
    }

    const size_t rawLength = wcslen(text);
    if (rawLength > static_cast<size_t>((std::numeric_limits<int>::max)())) {
        return false;
    }

    const int length = static_cast<int>(rawLength);
    if (length == 0) {
        return true;
    }

    BOOL usedDefaultChar = FALSE;
    BOOL* usedDefaultCharPtr = codePage == CP_UTF8 ? nullptr : &usedDefaultChar;
    const int size = WideCharToMultiByte(
        codePage,
        Utf16Flags(codePage),
        text,
        length,
        nullptr,
        0,
        nullptr,
        usedDefaultCharPtr
    );
    if (size <= 0) {
        return false;
    }

    output.resize(static_cast<size_t>(size));
    usedDefaultChar = FALSE;
    const int written = WideCharToMultiByte(
        codePage,
        Utf16Flags(codePage),
        text,
        length,
        output.data(),
        size,
        nullptr,
        usedDefaultCharPtr
    );
    return written == size && !usedDefaultChar;
}

bool GbkToUtf16(const char* text, std::wstring& output) {
    return MultiByteToUtf16(GbkCodePage, text, output);
}

bool GbkToUtf16(const std::string& text, std::wstring& output) {
    return GbkToUtf16(text.c_str(), output);
}

bool Utf16ToGbk(const wchar_t* text, std::string& output) {
    return Utf16ToMultiByte(GbkCodePage, text, output);
}

bool Utf16ToGbk(const std::wstring& text, std::string& output) {
    return Utf16ToGbk(text.c_str(), output);
}

bool Utf8ToUtf16(const char* text, std::wstring& output) {
    return MultiByteToUtf16(CP_UTF8, text, output);
}

bool Utf8ToUtf16(const std::string& text, std::wstring& output) {
    return Utf8ToUtf16(text.c_str(), output);
}

bool Utf16ToUtf8(const wchar_t* text, std::string& output) {
    return Utf16ToMultiByte(CP_UTF8, text, output);
}

bool Utf16ToUtf8(const std::wstring& text, std::string& output) {
    return Utf16ToUtf8(text.c_str(), output);
}

bool GbkToUtf8(const char* text, std::string& output) {
    std::wstring utf16;
    if (!GbkToUtf16(text, utf16)) {
        output.clear();
        return false;
    }
    return Utf16ToUtf8(utf16, output);
}

bool GbkToUtf8(const std::string& text, std::string& output) {
    return GbkToUtf8(text.c_str(), output);
}

bool Utf8ToGbk(const char* text, std::string& output) {
    std::wstring utf16;
    if (!Utf8ToUtf16(text, utf16)) {
        output.clear();
        return false;
    }
    return Utf16ToGbk(utf16, output);
}

bool Utf8ToGbk(const std::string& text, std::string& output) {
    return Utf8ToGbk(text.c_str(), output);
}

} // namespace Encoding
