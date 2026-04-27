#include "Hook.h"

#include <Windows.h>
#include <detours.h>

#include "AddressResolver.h"
#include "Logger.h"
#include "TranslationManager.h"

#include <cstddef>
#include <cwchar>
#include <cstring>
#include <string>

using QPainterDrawText3_t = void (*)(void* pThis, void* pointF, const void* text, int textFlags, int justificationPadding);
using QPainterDrawText4_t = void (*)(void* pThis, void* rect, int flags, const void* text, void* boundingRect);
using QPainterDrawText5_t = void (*)(void* pThis, void* rectangle, const void* text, void* option);
using QPainterDrawText6_t = void (*)(void* pThis, void* rectangle, int flags, const void* text, void* boundingRect);

QPainterDrawText3_t OriginalDrawText3 = nullptr;
QPainterDrawText4_t OriginalDrawText4 = nullptr;
QPainterDrawText5_t OriginalDrawText5 = nullptr;
QPainterDrawText6_t OriginalDrawText6 = nullptr;

void HookedDrawText3(void* pThis, void* pointF, const void* text, int textFlags, int justificationPadding);
void HookedDrawText4(void* pThis, void* rect, int flags, const void* text, void* boundingRect);
void HookedDrawText5(void* pThis, void* rectangle, const void* text, void* option);
void HookedDrawText6(void* pThis, void* rectangle, int flags, const void* text, void* boundingRect);


using QLabelSetText_t = void (*)(void* pThis, const void* text);
using QWindowSetTitle_t = void (*)(void* pThis, const void* text);
using QFileDialogGetOpenFileName_t = void* (*)(void* outQString, void* parent, const void* caption, const void* dir, const void* filter, void* selectedFilter, unsigned int options);
using QFileDialogGetExistingDirectory_t = void* (*)(void* outQString, void* parent, const void* caption, const void* dir, unsigned int options);

QLabelSetText_t OriginalQLabelSetText = nullptr;
QWindowSetTitle_t OriginalQWindowSetTitle = nullptr;
QFileDialogGetOpenFileName_t OriginalQFileDialogGetOpenFileName = nullptr;
QFileDialogGetExistingDirectory_t OriginalQFileDialogGetExistingDirectory = nullptr;

void HookedQLabelSetText(void* pThis, const void* text);
void HookedQWindowSetTitle(void* pThis, const void* text);
void* HookedQFileDialogGetOpenFileName(void* outQString, void* parent, const void* caption, const void* dir, const void* filter, void* selectedFilter, unsigned int options);
void* HookedQFileDialogGetExistingDirectory(void* outQString, void* parent, const void* caption, const void* dir, unsigned int options);


using QFontMetricsSize_t = void* (*)(void* pThis, void* result, int flags, void* text, int tabStops, int* tabArray);
QFontMetricsSize_t OriginalQFontMetricsSize = nullptr;
void* HookedFontMetricsSize(void* pThis, void* result, int flags, void* text, int tabStops, int* tabArray);

using QStringUtf16_t = const wchar_t* (*)(void* pThis);
using QStringSize_t = long long (*)(void* pThis);
using QStringCtor_t = void* (*)(void* pThis, const wchar_t* text, long long length);
using QStringDtor_t = void (*)(void* pThis);
QStringUtf16_t QStringUtf16 = nullptr;
QStringSize_t QStringSize = nullptr;
QStringCtor_t QStringCtor = nullptr;
QStringDtor_t QStringDtor = nullptr;

bool TranslationReady = false;
constexpr size_t kQStringObjectSize = 24;

const wchar_t* ExtractQStringText(void* qString, long long& length);
class QStringMemory;
bool CreateQStringObject(const wchar_t* text, long long length, QStringMemory& qString);
bool CreateTranslatedQStringObject(const void* text, bool writeUntranslated, QStringMemory& qString);

class QStringMemory {
public:
    QStringMemory() = default;
    QStringMemory(const QStringMemory&) = delete;
    QStringMemory& operator=(const QStringMemory&) = delete;

    ~QStringMemory() {
        Destroy();
    }

    void* Data() {
        return memory_;
    }

    void MarkConstructed() {
        constructed_ = true;
    }

    void Destroy() {
        if (constructed_ && QStringDtor != nullptr) {
            QStringDtor(memory_);
        }
        constructed_ = false;
    }

private:
    alignas(std::max_align_t) unsigned char memory_[kQStringObjectSize] = {};
    bool constructed_ = false;
};

bool Hook::Initialize() {

    OriginalDrawText3 = reinterpret_cast<QPainterDrawText3_t>(AddressResolver::GetFunctionAddress(L"Qt6Gui.dll", "?drawText@QPainter@QT@@QEAAXAEBVQPointF@2@AEBVQString@2@HH@Z"));
    OriginalDrawText4 = reinterpret_cast<QPainterDrawText4_t>(AddressResolver::GetFunctionAddress(L"Qt6Gui.dll", "?drawText@QPainter@QT@@QEAAXAEBVQRect@2@HAEBVQString@2@PEAV32@@Z"));
    OriginalDrawText5 = reinterpret_cast<QPainterDrawText5_t>(AddressResolver::GetFunctionAddress(L"Qt6Gui.dll", "?drawText@QPainter@QT@@QEAAXAEBVQRectF@2@AEBVQString@2@AEBVQTextOption@2@@Z"));
    OriginalDrawText6 = reinterpret_cast<QPainterDrawText6_t>(AddressResolver::GetFunctionAddress(L"Qt6Gui.dll", "?drawText@QPainter@QT@@QEAAXAEBVQRectF@2@HAEBVQString@2@PEAV32@@Z"));
    OriginalQLabelSetText = reinterpret_cast<QLabelSetText_t>(AddressResolver::GetFunctionAddress(L"Qt6Widgets.dll", "?setText@QLabel@QT@@QEAAXAEBVQString@2@@Z"));
    OriginalQWindowSetTitle = reinterpret_cast<QWindowSetTitle_t>(AddressResolver::GetFunctionAddress(L"Qt6Gui.dll", "?setTitle@QWindow@QT@@QEAAXAEBVQString@2@@Z"));
    OriginalQFileDialogGetOpenFileName = reinterpret_cast<QFileDialogGetOpenFileName_t>(AddressResolver::GetFunctionAddress(L"Qt6Widgets.dll", "?getOpenFileName@QFileDialog@QT@@SA?AVQString@2@PEAVQWidget@2@AEBV32@11PEAV32@V?$QFlags@W4Option@QFileDialog@QT@@@2@@Z"));
    OriginalQFileDialogGetExistingDirectory = reinterpret_cast<QFileDialogGetExistingDirectory_t>(AddressResolver::GetFunctionAddress(L"Qt6Widgets.dll", "?getExistingDirectory@QFileDialog@QT@@SA?AVQString@2@PEAVQWidget@2@AEBV32@1V?$QFlags@W4Option@QFileDialog@QT@@@2@@Z"));
    OriginalQFontMetricsSize = reinterpret_cast<QFontMetricsSize_t>(AddressResolver::GetFunctionAddress(L"Qt6Gui.dll", "?size@QFontMetrics@QT@@QEBA?AVQSize@2@HAEBVQString@2@HPEAH@Z"));
    QStringUtf16 = reinterpret_cast<QStringUtf16_t>(AddressResolver::GetFunctionAddress(L"Qt6Core.dll", "?utf16@QString@QT@@QEBAPEBGXZ"));
    QStringSize = reinterpret_cast<QStringSize_t>(AddressResolver::GetFunctionAddress(L"Qt6Core.dll", "?size@QString@QT@@QEBA_JXZ"));
    QStringCtor = reinterpret_cast<QStringCtor_t>(AddressResolver::GetFunctionAddress(L"Qt6Core.dll", "??0QString@QT@@QEAA@PEBVQChar@1@_J@Z"));
    QStringDtor = reinterpret_cast<QStringDtor_t>(AddressResolver::GetFunctionAddress(L"Qt6Core.dll", "??1QString@QT@@QEAA@XZ"));
    
    //std::string PatternDrawText3 = "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 48 89 7C 24 ?? 41 56 48 81 EC ?? ?? ?? ?? 48 8B 19 49 8B F9 41 8B E8 48 8B F2 4C 8B F1 48 83 BB ?? ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 49 83 79 ?? ?? 0F 84 ?? ?? ?? ?? 48 8B 4B ?? 48 83 C1 ?? E8 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 48 83 BB ?? ?? ?? ?? ?? 75 ?? 48 8B 53 ?? 48 8B CB E8 ?? ?? ?? ?? 0F 57 C0 48 8D 4C 24 ?? 0F 57 C9 48 8B D6 0F 11 44 24 ?? 0F 11 8C 24 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 48 8B 4B ?? 48 8D 44 24 ?? 48 8B B4 24 ?? ?? ?? ?? 33 D2 4C 89 74 24 ?? 48 85 F6 89 54 24 ?? 44 8B C5 48 0F 44 C2 48 89 54 24 ?? 89 54 24 ?? 48 83 C1 ?? 48 89 44 24 ?? 48 8D 54 24 ?? 45 33 C9 48 89 7C 24 ?? E8 ?? ?? ?? ?? 48 85 F6 74 ?? 48 8D 54 24 ?? 48 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 0F 10 00 0F 11 06 4C 8D 9C 24 ?? ?? ?? ?? 49 8B 5B ?? 49 8B 6B ?? 49 8B 73 ?? 49 8B 7B ?? 49 8B E3 41 5E C3";
    //void* address = AddressResolver::FindPattern(L"Qt6Gui.dll", PatternDrawText3.c_str());
    //Logger::Write(u8"模式解析地址：%p", address);

    if (QStringUtf16 == nullptr || QStringSize == nullptr) {
        Logger::Write(u8"[Hook] 解析 QString 文本提取函数失败。utf16=%p size=%p", reinterpret_cast<void*>(QStringUtf16), reinterpret_cast<void*>(QStringSize));
        return false;
    }
    if (QStringCtor == nullptr || QStringDtor == nullptr) {
        Logger::Write(u8"[Hook] 解析 QString 构造/析构函数失败。ctor=%p dtor=%p", reinterpret_cast<void*>(QStringCtor), reinterpret_cast<void*>(QStringDtor));
        return false;
    }

    if (OriginalDrawText3 == nullptr || OriginalDrawText4 == nullptr ||
        OriginalDrawText5 == nullptr || OriginalDrawText6 == nullptr) {
        Logger::Write(u8"[Hook] 解析 QPainter::drawText 重载失败。dt3=%p dt4=%p dt5=%p dt6=%p",
                                reinterpret_cast<void*>(OriginalDrawText3),
                                reinterpret_cast<void*>(OriginalDrawText4),
                                reinterpret_cast<void*>(OriginalDrawText5),
                                reinterpret_cast<void*>(OriginalDrawText6));
        return false;
    }
    if (OriginalQFontMetricsSize == nullptr) {
        Logger::Write(u8"[Hook] 解析 QFontMetrics::size 失败");
        return false;
    }
    if (OriginalQLabelSetText == nullptr || OriginalQWindowSetTitle == nullptr) {
        Logger::Write(u8"[Hook] 解析 setText/setTitle 失败。label=%p window=%p", reinterpret_cast<void*>(OriginalQLabelSetText), reinterpret_cast<void*>(OriginalQWindowSetTitle));
        return false;
    }
    if (OriginalQFileDialogGetOpenFileName == nullptr ||
        OriginalQFileDialogGetExistingDirectory == nullptr) {
        Logger::Write(u8"[Hook] 解析 QFileDialog 静态函数失败。openFileName=%p existingDirectory=%p", reinterpret_cast<void*>(OriginalQFileDialogGetOpenFileName), reinterpret_cast<void*>(OriginalQFileDialogGetExistingDirectory));
        return false;
    }


    TranslationManagerConfig translationConfig;
    translationConfig.filterChineseSourceWrites = true;
    TranslationManager::Configure(translationConfig);
    TranslationReady = TranslationManager::Initialize(L"Dictionaries/translations.txt");
    Logger::Write(L"[Hook] translation dictionary=Dictionaries/translations.txt ready=%d", TranslationReady ? 1 : 0);

    DetourRestoreAfterWith();

    LONG error = DetourTransactionBegin();
    if (error == NO_ERROR) {
        error = DetourUpdateThread(GetCurrentThread());
    }
    if (error == NO_ERROR) {
        error = DetourAttach(reinterpret_cast<void**>(&OriginalDrawText3), reinterpret_cast<void*>(HookedDrawText3));
    }
    if (error == NO_ERROR) {
        error = DetourAttach(reinterpret_cast<void**>(&OriginalDrawText4), reinterpret_cast<void*>(HookedDrawText4));
    }
    if (error == NO_ERROR) {
        error = DetourAttach(reinterpret_cast<void**>(&OriginalDrawText5), reinterpret_cast<void*>(HookedDrawText5));
    }
    if (error == NO_ERROR) {
        error = DetourAttach(reinterpret_cast<void**>(&OriginalDrawText6), reinterpret_cast<void*>(HookedDrawText6));
    }
    if (error == NO_ERROR) {
        error = DetourAttach(reinterpret_cast<void**>(&OriginalQFontMetricsSize), reinterpret_cast<void*>(HookedFontMetricsSize));
    }
    if (error == NO_ERROR) {
        error = DetourAttach(reinterpret_cast<void**>(&OriginalQLabelSetText), reinterpret_cast<void*>(HookedQLabelSetText));
    }
    if (error == NO_ERROR) {
        error = DetourAttach(reinterpret_cast<void**>(&OriginalQWindowSetTitle), reinterpret_cast<void*>(HookedQWindowSetTitle));
    }
    if (error == NO_ERROR) {
        error = DetourAttach(reinterpret_cast<void**>(&OriginalQFileDialogGetOpenFileName), reinterpret_cast<void*>(HookedQFileDialogGetOpenFileName));
    }
    if (error == NO_ERROR) {
        error = DetourAttach(reinterpret_cast<void**>(&OriginalQFileDialogGetExistingDirectory), reinterpret_cast<void*>(HookedQFileDialogGetExistingDirectory));
    }
    if (error == NO_ERROR) {
        error = DetourTransactionCommit();
    } else {
        DetourTransactionAbort();
    }

    if (error != NO_ERROR) {
        Logger::Write(L"[Hook] 安装 QPainter::drawText hook 失败。error=%ld", error);
        OriginalDrawText3 = nullptr;
        OriginalDrawText4 = nullptr;
        OriginalDrawText5 = nullptr;
        OriginalDrawText6 = nullptr;
        OriginalQLabelSetText = nullptr;
        OriginalQWindowSetTitle = nullptr;
        OriginalQFileDialogGetOpenFileName = nullptr;
        OriginalQFileDialogGetExistingDirectory = nullptr;
        OriginalQFontMetricsSize = nullptr;
        QStringUtf16 = nullptr;
        QStringSize = nullptr;
        QStringCtor = nullptr;
        QStringDtor = nullptr;
        TranslationReady = false;
        TranslationManager::Clear();
        return false;
    }

    Logger::Write(L"[Hook] 已安装 Hook: drawText3=%p drawText4=%p drawText5=%p drawText6=%p labelSetText=%p windowSetTitle=%p fileOpen=%p existingDir=%p fontMetricsSize=%p",
                        reinterpret_cast<void*>(OriginalDrawText3),
                        reinterpret_cast<void*>(OriginalDrawText4),
                        reinterpret_cast<void*>(OriginalDrawText5),
                        reinterpret_cast<void*>(OriginalDrawText6),
                        reinterpret_cast<void*>(OriginalQLabelSetText),
                        reinterpret_cast<void*>(OriginalQWindowSetTitle),
                        reinterpret_cast<void*>(OriginalQFileDialogGetOpenFileName),
                        reinterpret_cast<void*>(OriginalQFileDialogGetExistingDirectory),
                        reinterpret_cast<void*>(OriginalQFontMetricsSize));
    return true;
}

void Hook::Uninitialize() {
    LONG error = DetourTransactionBegin();
    if (error == NO_ERROR) {
        error = DetourUpdateThread(GetCurrentThread());
    }
    if (error == NO_ERROR) {
        error = DetourDetach(reinterpret_cast<void**>(&OriginalDrawText3), reinterpret_cast<void*>(HookedDrawText3));
    }
    if (error == NO_ERROR) {
        error = DetourDetach(reinterpret_cast<void**>(&OriginalDrawText4), reinterpret_cast<void*>(HookedDrawText4));
    }
    if (error == NO_ERROR) {
        error = DetourDetach(reinterpret_cast<void**>(&OriginalDrawText5), reinterpret_cast<void*>(HookedDrawText5));
    }
    if (error == NO_ERROR) {
        error = DetourDetach(reinterpret_cast<void**>(&OriginalDrawText6), reinterpret_cast<void*>(HookedDrawText6));
    }
    if (error == NO_ERROR) {
        error = DetourDetach(reinterpret_cast<void**>(&OriginalQFontMetricsSize), reinterpret_cast<void*>(HookedFontMetricsSize));
    }
    if (error == NO_ERROR) {
        error = DetourDetach(reinterpret_cast<void**>(&OriginalQLabelSetText), reinterpret_cast<void*>(HookedQLabelSetText));
    }
    if (error == NO_ERROR) {
        error = DetourDetach(reinterpret_cast<void**>(&OriginalQWindowSetTitle), reinterpret_cast<void*>(HookedQWindowSetTitle));
    }
    if (error == NO_ERROR) {
        error = DetourDetach(reinterpret_cast<void**>(&OriginalQFileDialogGetOpenFileName), reinterpret_cast<void*>(HookedQFileDialogGetOpenFileName));
    }
    if (error == NO_ERROR) {
        error = DetourDetach(reinterpret_cast<void**>(&OriginalQFileDialogGetExistingDirectory), reinterpret_cast<void*>(HookedQFileDialogGetExistingDirectory));
    }
    if (error == NO_ERROR) {
        error = DetourTransactionCommit();
    } else {
        DetourTransactionAbort();
    }

    if (error != NO_ERROR) {
        Logger::Write(L"[Hook] 卸载 QPainter::drawText hook 失败。error=%ld", error);
        return;
    }

    OriginalDrawText3 = nullptr;
    OriginalDrawText4 = nullptr;
    OriginalDrawText5 = nullptr;
    OriginalDrawText6 = nullptr;
    OriginalQLabelSetText = nullptr;
    OriginalQWindowSetTitle = nullptr;
    OriginalQFileDialogGetOpenFileName = nullptr;
    OriginalQFileDialogGetExistingDirectory = nullptr;
    OriginalQFontMetricsSize = nullptr;
    QStringUtf16 = nullptr;
    QStringSize = nullptr;
    QStringCtor = nullptr;
    QStringDtor = nullptr;
    TranslationReady = false;
    TranslationManager::Clear();
    Logger::Write(L"[Hook] 已卸载 QPainter::drawText hook。");
}


void HookedDrawText3(void* pThis, void* pointF, const void* text, int textFlags, int justificationPadding) {
    QStringMemory translatedQString;
    if (CreateTranslatedQStringObject(text, true, translatedQString)) {
        OriginalDrawText3(pThis, pointF, translatedQString.Data(), textFlags, justificationPadding);
        return;
    }

    OriginalDrawText3(pThis, pointF, text, textFlags, justificationPadding);
}

void HookedDrawText4(void* pThis, void* rect, int flags, const void* text, void* boundingRect) {
    QStringMemory translatedQString;
    if (CreateTranslatedQStringObject(text, true, translatedQString)) {
        OriginalDrawText4(pThis, rect, flags, translatedQString.Data(), boundingRect);
        return;
    }

    OriginalDrawText4(pThis, rect, flags, text, boundingRect);
}

void HookedDrawText5(void* pThis, void* rectangle, const void* text, void* option) {
    QStringMemory translatedQString;
    if (CreateTranslatedQStringObject(text, true, translatedQString)) {
        OriginalDrawText5(pThis, rectangle, translatedQString.Data(), option);
        return;
    }

    OriginalDrawText5(pThis, rectangle, text, option);
}

void HookedDrawText6(void* pThis, void* rectangle, int flags, const void* text, void* boundingRect) {
    QStringMemory translatedQString;
    if (CreateTranslatedQStringObject(text, true, translatedQString)) {
        OriginalDrawText6(pThis, rectangle, flags, translatedQString.Data(), boundingRect);
        return;
    }

    OriginalDrawText6(pThis, rectangle, flags, text, boundingRect);
}

void HookedQLabelSetText(void* pThis, const void* text) {
    QStringMemory translatedQString;
    if (CreateTranslatedQStringObject(text, true, translatedQString)) {
        OriginalQLabelSetText(pThis, translatedQString.Data());
        return;
    }

    OriginalQLabelSetText(pThis, text);
}

void HookedQWindowSetTitle(void* pThis, const void* text) {
    QStringMemory translatedQString;
    if (CreateTranslatedQStringObject(text, true, translatedQString)) {
        OriginalQWindowSetTitle(pThis, translatedQString.Data());
        return;
    }

    OriginalQWindowSetTitle(pThis, text);
}

void* HookedQFileDialogGetOpenFileName(void* outQString,
                                       void* parent,
                                       const void* caption,
                                       const void* dir,
                                       const void* filter,
                                       void* selectedFilter,
                                       unsigned int options) {
    QStringMemory translatedCaption;
    QStringMemory translatedFilter;
    const bool hasTranslatedCaption = CreateTranslatedQStringObject(caption, true, translatedCaption);
    const bool hasTranslatedFilter = CreateTranslatedQStringObject(filter, true, translatedFilter);

    void* result = OriginalQFileDialogGetOpenFileName(
        outQString,
        parent,
        hasTranslatedCaption ? translatedCaption.Data() : caption,
        dir,
        hasTranslatedFilter ? translatedFilter.Data() : filter,
        selectedFilter,
        options
    );

    return result;
}

void* HookedQFileDialogGetExistingDirectory(void* outQString,
                                            void* parent,
                                            const void* caption,
                                            const void* dir,
                                            unsigned int options) {
    QStringMemory translatedCaption;
    const bool hasTranslatedCaption = CreateTranslatedQStringObject(caption, true, translatedCaption);
    void* result = OriginalQFileDialogGetExistingDirectory(
        outQString,
        parent,
        hasTranslatedCaption ? translatedCaption.Data() : caption,
        dir,
        options
    );

    return result;
}

void* HookedFontMetricsSize(void* pThis, void* result, int flags, void* text, int tabStops, int* tabArray) {
    long long length = 0;
    const wchar_t* value = ExtractQStringText(text, length);
    if (value != nullptr && TranslationReady) {
        std::wstring source(value, static_cast<size_t>(length));
        wchar_t* translated = source.empty() ? nullptr : TranslationManager::Translate(source.c_str(), false);
        if (translated != nullptr) {
            QStringMemory translatedQString;
            if (CreateQStringObject(
                translated,
                static_cast<long long>(std::wcslen(translated)),
                translatedQString
            )) {
                void* sizeResult = OriginalQFontMetricsSize(pThis, result, flags, translatedQString.Data(), tabStops, tabArray);
                //Logger::Write(L"[Hook] QFontMetrics::size text=%.*s translated=%s",
                //                    static_cast<int>(length),
                //                    value,
                //                    translated);
                return sizeResult;
            }

            Logger::Write(L"[Hook] 创建 QFontMetrics::size 翻译 QString 失败，回退原始文本。");
        }
    }

    return OriginalQFontMetricsSize(pThis, result, flags, text, tabStops, tabArray);
}

const wchar_t* ExtractQStringText(void* qString, long long& length) {
    length = 0;
    if (qString == nullptr || QStringUtf16 == nullptr || QStringSize == nullptr) {
        return nullptr;
    }

    length = QStringSize(qString);
    if (length <= 0) {
        length = 0;
        return L"";
    }

    return QStringUtf16(qString);
}

bool CreateQStringObject(const wchar_t* text, long long length, QStringMemory& qString) {
    if (text == nullptr || length < 0 || QStringCtor == nullptr) {
        return false;
    }

    std::memset(qString.Data(), 0, kQStringObjectSize);
    QStringCtor(qString.Data(), text, length);
    qString.MarkConstructed();
    return true;
}

bool CreateTranslatedQStringObject(const void* text, bool writeUntranslated, QStringMemory& qString) {
    long long length = 0;
    const wchar_t* value = ExtractQStringText(const_cast<void*>(text), length);
    if (value == nullptr || !TranslationReady || length <= 0) {
        return false;
    }

    const std::wstring source(value, static_cast<size_t>(length));
    wchar_t* translated = TranslationManager::Translate(source.c_str(), writeUntranslated);
    if (translated == nullptr) {
        return false;
    }

    //Logger::Write(L"[Hook] QString translate text=%.*s translated=%s length=%lld",
    //                    static_cast<int>(length),
    //                    value,
    //                    translated,
    //                    length);
    return CreateQStringObject(translated, static_cast<long long>(std::wcslen(translated)), qString);
}
