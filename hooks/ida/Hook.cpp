#include "Hook.h"

#include <Windows.h>

#include "DetoursHook.h"
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

static DetoursHook detoursHook_;     // DetoursHook 对象
static QStringBridge qstringBridge_; // QString 桥接对象

static QStringBridge::ScopedQString QStringTranslation(const void* text, bool writeUntranslated = true);

bool Hook::Initialize() {

    bool ok = true;
    ok = detoursHook_.Register(L"Qt6Gui.dll", "?drawText@QPainter@QT@@QEAAXAEBVQPointF@2@AEBVQString@2@HH@Z", reinterpret_cast<void**>(&OriginalDrawText3), reinterpret_cast<void*>(HookedDrawText3)) && ok;
    ok = detoursHook_.Register(L"Qt6Gui.dll", "?drawText@QPainter@QT@@QEAAXAEBVQRect@2@HAEBVQString@2@PEAV32@@Z", reinterpret_cast<void**>(&OriginalDrawText4), reinterpret_cast<void*>(HookedDrawText4)) && ok;
    ok = detoursHook_.Register(L"Qt6Gui.dll", "?drawText@QPainter@QT@@QEAAXAEBVQRectF@2@AEBVQString@2@AEBVQTextOption@2@@Z", reinterpret_cast<void**>(&OriginalDrawText5), reinterpret_cast<void*>(HookedDrawText5)) && ok;
    ok = detoursHook_.Register(L"Qt6Gui.dll", "?drawText@QPainter@QT@@QEAAXAEBVQRectF@2@HAEBVQString@2@PEAV32@@Z", reinterpret_cast<void**>(&OriginalDrawText6), reinterpret_cast<void*>(HookedDrawText6)) && ok;
    ok = detoursHook_.Register(L"Qt6Widgets.dll", "?setText@QLabel@QT@@QEAAXAEBVQString@2@@Z", reinterpret_cast<void**>(&OriginalQLabelSetText), reinterpret_cast<void*>(HookedQLabelSetText)) && ok;
    ok = detoursHook_.Register(L"Qt6Gui.dll", "?setTitle@QWindow@QT@@QEAAXAEBVQString@2@@Z", reinterpret_cast<void**>(&OriginalQWindowSetTitle), reinterpret_cast<void*>(HookedQWindowSetTitle)) && ok;
    ok = detoursHook_.Register(L"Qt6Widgets.dll", "?getOpenFileName@QFileDialog@QT@@SA?AVQString@2@PEAVQWidget@2@AEBV32@11PEAV32@V?$QFlags@W4Option@QFileDialog@QT@@@2@@Z", reinterpret_cast<void**>(&OriginalQFileDialogGetOpenFileName), reinterpret_cast<void*>(HookedQFileDialogGetOpenFileName)) && ok;
    ok = detoursHook_.Register(L"Qt6Widgets.dll", "?getExistingDirectory@QFileDialog@QT@@SA?AVQString@2@PEAVQWidget@2@AEBV32@1V?$QFlags@W4Option@QFileDialog@QT@@@2@@Z", reinterpret_cast<void**>(&OriginalQFileDialogGetExistingDirectory), reinterpret_cast<void*>(HookedQFileDialogGetExistingDirectory)) && ok;
    ok = detoursHook_.Register(L"Qt6Gui.dll", "?size@QFontMetrics@QT@@QEBA?AVQSize@2@HAEBVQString@2@HPEAH@Z", reinterpret_cast<void**>(&OriginalQFontMetricsSize), reinterpret_cast<void*>(HookedFontMetricsSize)) && ok;
    if (!ok) {
        Logger::Write(u8"[Hook] 注册 Detours Hook 失败");
        detoursHook_.Release();
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

    if (!detoursHook_.Commit()) {
        Logger::Write(L"[Hook] 安装 Detours Hook 失败。");
        OriginalDrawText3 = nullptr;
        OriginalDrawText4 = nullptr;
        OriginalDrawText5 = nullptr;
        OriginalDrawText6 = nullptr;
        OriginalQLabelSetText = nullptr;
        OriginalQWindowSetTitle = nullptr;
        OriginalQFileDialogGetOpenFileName = nullptr;
        OriginalQFileDialogGetExistingDirectory = nullptr;
        OriginalQFontMetricsSize = nullptr;
        detoursHook_.Release();
        qstringBridge_.Reset();
        TranslationManager::Clear();
        return false;
    }

    return true;
}

void Hook::Uninitialize() {
    detoursHook_.Release();

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
    Logger::Write(L"[Hook] 已卸载");
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

void* HookedQFileDialogGetOpenFileName(
    void* outQString,
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

void* HookedQFileDialogGetExistingDirectory(
	void* outQString,
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
