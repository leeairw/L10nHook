#include "Hook.h"

#include <Windows.h>
#include <detours.h>

#include "AddressResolver.h"
#include "Logger.h"
#include "QStringBridge.h"
#include "TranslationManager.h"

#include <cwchar>
#include <string>
#include <string_view>

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
QLabelSetText_t OriginalQLabelSetText = nullptr;
void HookedQLabelSetText(void* pThis, const void* text);

using QWindowSetTitle_t = void (*)(void* pThis, const void* text);
QWindowSetTitle_t OriginalQWindowSetTitle = nullptr;
void HookedQWindowSetTitle(void* pThis, const void* text);

using QFileDialogGetOpenFileName_t = void* (*)(void* outQString, void* parent, const void* caption, const void* dir, const void* filter, void* selectedFilter, unsigned int options);
QFileDialogGetOpenFileName_t OriginalQFileDialogGetOpenFileName = nullptr;
void* HookedQFileDialogGetOpenFileName(void* outQString, void* parent, const void* caption, const void* dir, const void* filter, void* selectedFilter, unsigned int options);

using QFileDialogGetExistingDirectory_t = void* (*)(void* outQString, void* parent, const void* caption, const void* dir, unsigned int options);
QFileDialogGetExistingDirectory_t OriginalQFileDialogGetExistingDirectory = nullptr;
void* HookedQFileDialogGetExistingDirectory(void* outQString, void* parent, const void* caption, const void* dir, unsigned int options);

using QFontMetricsSize_t = void* (*)(void* pThis, void* result, int flags, void* text, int tabStops, int* tabArray);
QFontMetricsSize_t OriginalQFontMetricsSize = nullptr;
void* HookedFontMetricsSize(void* pThis, void* result, int flags, void* text, int tabStops, int* tabArray);

static QStringBridge qstringBridge_; // QString 桥接对象

static QStringBridge::ScopedQString QStringTranslation(const void* text, bool writeUntranslated = true);

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
    
    //std::string PatternDrawText3 = "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 48 89 7C 24 ?? 41 56 48 81 EC ?? ?? ?? ?? 48 8B 19 49 8B F9 41 8B E8 48 8B F2 4C 8B F1 48 83 BB ?? ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 49 83 79 ?? ?? 0F 84 ?? ?? ?? ?? 48 8B 4B ?? 48 83 C1 ?? E8 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 48 83 BB ?? ?? ?? ?? ?? 75 ?? 48 8B 53 ?? 48 8B CB E8 ?? ?? ?? ?? 0F 57 C0 48 8D 4C 24 ?? 0F 57 C9 48 8B D6 0F 11 44 24 ?? 0F 11 8C 24 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 48 8B 4B ?? 48 8D 44 24 ?? 48 8B B4 24 ?? ?? ?? ?? 33 D2 4C 89 74 24 ?? 48 85 F6 89 54 24 ?? 44 8B C5 48 0F 44 C2 48 89 54 24 ?? 89 54 24 ?? 48 83 C1 ?? 48 89 44 24 ?? 48 8D 54 24 ?? 45 33 C9 48 89 7C 24 ?? E8 ?? ?? ?? ?? 48 85 F6 74 ?? 48 8D 54 24 ?? 48 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 0F 10 00 0F 11 06 4C 8D 9C 24 ?? ?? ?? ?? 49 8B 5B ?? 49 8B 6B ?? 49 8B 73 ?? 49 8B 7B ?? 49 8B E3 41 5E C3";
    //void* address = AddressResolver::FindPattern(L"Qt6Gui.dll", PatternDrawText3.c_str());
    //Logger::Write(u8"模式解析地址：%p", address);

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

    QStringBridge::Config qstringBridgeConfig;
    qstringBridgeConfig.module_name = L"Qt6Core.dll";
    if (!qstringBridge_.Initialize(qstringBridgeConfig)) {
        Logger::Write(u8"[Hook] 初始化 QStringBridge 失败");
        return false;
    }

    TranslationManagerConfig translationConfig;
    translationConfig.filterChineseSourceWrites = true;
    TranslationManager::Configure(translationConfig);
    if (!TranslationManager::Initialize(L"Dictionaries/translations.txt")) {
        Logger::Write(L"[Hook] 初始化翻译字典失败。dictionary=Dictionaries/translations.txt");
        qstringBridge_.Reset();
        return false;
    }
    Logger::Write(L"[Hook] 已初始化翻译字典。dictionary=Dictionaries/translations.txt");

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
        qstringBridge_.Reset();
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
    qstringBridge_.Reset();
    TranslationManager::Clear();
    Logger::Write(L"[Hook] 已卸载 QPainter::drawText hook。");
}


void HookedDrawText3(void* pThis, void* pointF, const void* text, int textFlags, int justificationPadding) {
    try {
        auto translated = QStringTranslation(text);
        OriginalDrawText3(pThis, pointF, translated ? translated.Get() : text, textFlags, justificationPadding);
    } catch (...) {
        OriginalDrawText3(pThis, pointF, text, textFlags, justificationPadding);
    }
}

void HookedDrawText4(void* pThis, void* rect, int flags, const void* text, void* boundingRect) {
    try {
        auto translated = QStringTranslation(text);
        OriginalDrawText4(pThis, rect, flags, translated ? translated.Get() : text, boundingRect);
    } catch (...) {
        OriginalDrawText4(pThis, rect, flags, text, boundingRect);
    }
}

void HookedDrawText5(void* pThis, void* rectangle, const void* text, void* option) {
    try {
        auto translated = QStringTranslation(text);
        OriginalDrawText5(pThis, rectangle, translated ? translated.Get() : text, option);
    } catch (...) {
        OriginalDrawText5(pThis, rectangle, text, option);
    }
}

void HookedDrawText6(void* pThis, void* rectangle, int flags, const void* text, void* boundingRect) {
    try {
        auto translated = QStringTranslation(text);
        OriginalDrawText6(pThis, rectangle, flags, translated ? translated.Get() : text, boundingRect);
    } catch (...) {
        OriginalDrawText6(pThis, rectangle, flags, text, boundingRect);
    }
}

void HookedQLabelSetText(void* pThis, const void* text) {
    try {
        auto translated = QStringTranslation(text);
        OriginalQLabelSetText(pThis, translated ? translated.Get() : text);
    } catch (...) {
        OriginalQLabelSetText(pThis, text);
    }
}

void HookedQWindowSetTitle(void* pThis, const void* text) {
    try {
        auto translated = QStringTranslation(text);
        OriginalQWindowSetTitle(pThis, translated ? translated.Get() : text);
    } catch (...) {
        OriginalQWindowSetTitle(pThis, text);
    }
}

void* HookedQFileDialogGetOpenFileName(void* outQString,
                                       void* parent,
                                       const void* caption,
                                       const void* dir,
                                       const void* filter,
                                       void* selectedFilter,
                                       unsigned int options) {
    try {
        auto translatedCaption = QStringTranslation(caption);
        auto translatedFilter = QStringTranslation(filter);
        return OriginalQFileDialogGetOpenFileName(
            outQString,
            parent,
            translatedCaption ? translatedCaption.Get() : caption,
            dir,
            translatedFilter ? translatedFilter.Get() : filter,
            selectedFilter,
            options
        );
    } catch (...) {
        return OriginalQFileDialogGetOpenFileName(outQString, parent, caption, dir, filter, selectedFilter, options);
    }
}

void* HookedQFileDialogGetExistingDirectory(void* outQString,
                                            void* parent,
                                            const void* caption,
                                            const void* dir,
                                            unsigned int options) {
    try {
        auto translatedCaption = QStringTranslation(caption);
        return OriginalQFileDialogGetExistingDirectory(
            outQString,
            parent,
            translatedCaption ? translatedCaption.Get() : caption,
            dir,
            options
        );
    } catch (...) {
        return OriginalQFileDialogGetExistingDirectory(outQString, parent, caption, dir, options);
    }
}

void* HookedFontMetricsSize(void* pThis, void* result, int flags, void* text, int tabStops, int* tabArray) {
    try {
        auto translated = QStringTranslation(text, false);
        return OriginalQFontMetricsSize(
            pThis,
            result,
            flags,
            const_cast<void*>(translated ? translated.Get() : text),
            tabStops,
            tabArray
        );
    } catch (...) {
        return OriginalQFontMetricsSize(pThis, result, flags, text, tabStops, tabArray);
    }
}

static QStringBridge::ScopedQString QStringTranslation(const void* text, bool writeUntranslated) {
    if (text == nullptr) {
        return {};
    }

    int length = 0;
    const wchar_t* value = qstringBridge_.Extract(text, length);
    if (value == nullptr || length <= 0) {
        return {};
    }

    const std::wstring_view translated = TranslationManager::Translate(
        std::wstring_view(value, static_cast<std::size_t>(length)),
        writeUntranslated
    );
    if (translated.empty()) {
        return {};
    }

    return qstringBridge_.CreateScoped(translated);
}
