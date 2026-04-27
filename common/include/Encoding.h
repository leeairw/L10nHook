#pragma once

#include <string>

namespace Encoding {

bool MultiByteToUtf16(unsigned int codePage, const char* text, std::wstring& output);
bool Utf16ToMultiByte(unsigned int codePage, const wchar_t* text, std::string& output);

bool GbkToUtf16(const char* text, std::wstring& output);
bool GbkToUtf16(const std::string& text, std::wstring& output);
bool Utf16ToGbk(const wchar_t* text, std::string& output);
bool Utf16ToGbk(const std::wstring& text, std::string& output);

bool Utf8ToUtf16(const char* text, std::wstring& output);
bool Utf8ToUtf16(const std::string& text, std::wstring& output);
bool Utf16ToUtf8(const wchar_t* text, std::string& output);
bool Utf16ToUtf8(const std::wstring& text, std::string& output);

bool GbkToUtf8(const char* text, std::string& output);
bool GbkToUtf8(const std::string& text, std::string& output);
bool Utf8ToGbk(const char* text, std::string& output);
bool Utf8ToGbk(const std::string& text, std::string& output);

} // namespace Encoding
