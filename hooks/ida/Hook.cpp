#include "Hook.h"

#include <Windows.h>

#include "DetoursHook.h"
#include "Encoding.h"
#include "Logger.h"
#include "ModuleUtils.h"
#include "QStringBridge.h"
#include "TranslationManager.h"

#include <cstddef>
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

bool IsCompatibleQtModule(const wchar_t* moduleName, const std::wstring& qtCoreModuleName) {
    if (moduleName == nullptr) {
        return false;
    }
    if (qtCoreModuleName.find(L"Qt6") != std::wstring::npos) {
        return wcsstr(moduleName, L"Qt6") != nullptr;
    }
    if (qtCoreModuleName.find(L"Qt5") != std::wstring::npos) {
        return wcsstr(moduleName, L"Qt5") != nullptr;
    }
    return false;
}

std::wstring DetectQtCoreModuleName() {
    if (ModuleUtils::LoadLibraryFromProcessDirectory(L"Qt6Core.dll") != nullptr) {
        return L"Qt6Core.dll";
    }
    if (ModuleUtils::LoadLibraryFromProcessDirectory(L"Qt5Core.dll") != nullptr) {
        return L"Qt5Core.dll";
    }
    return {};
}

struct HookExportCandidate {
    const wchar_t* moduleName;
    const char* functionName;
};

constexpr std::size_t kMaxHookCandidateCount = 4;

struct HookFunctionInfo {
    void** original;
    void* detour;
    HookExportCandidate candidates[kMaxHookCandidateCount];
};

bool RegisterHookFunction(const HookFunctionInfo& hook, const std::wstring& qtCoreModuleName) {
    if (hook.original == nullptr || hook.detour == nullptr) {
        Logger::Write(L"[Hook] 注册候选 Hook 参数无效。original=%p detour=%p", hook.original, hook.detour);
        return false;
    }

    *hook.original = nullptr;
    for (const HookExportCandidate& candidate : hook.candidates) {
        if (candidate.moduleName == nullptr || candidate.functionName == nullptr) {
            continue;
        }
        if (!IsCompatibleQtModule(candidate.moduleName, qtCoreModuleName)) {
            continue;
        }

        HMODULE module = ModuleUtils::LoadLibraryFromProcessDirectory(candidate.moduleName);
        if (module == nullptr) {
            continue;
        }

        void* address = reinterpret_cast<void*>(GetProcAddress(module, candidate.functionName));
        if (address == nullptr) {
            continue;
        }

        *hook.original = address;
        if (!detoursHook_.Register(hook.original, hook.detour)) {
            Logger::Write(L"[Hook] 注册候选 Hook 失败。module=%s function=%s address=%p", candidate.moduleName, U82W(candidate.functionName), address);
            *hook.original = nullptr;
            return false;
        }

        Logger::Write(L"[Hook] 注册候选 Hook 成功。module=%s function=%s address=%p", candidate.moduleName, U82W(candidate.functionName), address);
        return true;
    }

    Logger::Write(L"[Hook] 未找到候选 Hook 导出。function=%s", U82W(hook.candidates[0].functionName));
    return false;
}

const HookFunctionInfo kHookFunctions[] = {
    {
        reinterpret_cast<void**>(&OriginalDrawText3),
        reinterpret_cast<void*>(HookedDrawText3),
        {
            {L"Qt5Gui.dll", "?drawText@QPainter@QT@@QEAAXAEBVQPointF@2@AEBVQString@2@HH@Z"},
            {L"Qt6Gui.dll", "?drawText@QPainter@QT@@QEAAXAEBVQPointF@2@AEBVQString@2@HH@Z"},
            {L"Qt5Gui.dll", "?drawText@QPainter@@QEAAXAEBVQPointF@@AEBVQString@@HH@Z"},
            {L"Qt6Gui.dll", "?drawText@QPainter@@QEAAXAEBVQPointF@@AEBVQString@@HH@Z"},
        },
    },
    {
        reinterpret_cast<void**>(&OriginalDrawText4),
        reinterpret_cast<void*>(HookedDrawText4),
        {
            {L"Qt5Gui.dll", "?drawText@QPainter@QT@@QEAAXAEBVQRect@2@HAEBVQString@2@PEAV32@@Z"},
            {L"Qt6Gui.dll", "?drawText@QPainter@QT@@QEAAXAEBVQRect@2@HAEBVQString@2@PEAV32@@Z"},
            {L"Qt5Gui.dll", "?drawText@QPainter@@QEAAXAEBVQRect@@HAEBVQString@@PEAV2@@Z"},
            {L"Qt6Gui.dll", "?drawText@QPainter@@QEAAXAEBVQRect@@HAEBVQString@@PEAV2@@Z"},
        },
    },
    {
        reinterpret_cast<void**>(&OriginalDrawText5),
        reinterpret_cast<void*>(HookedDrawText5),
        {
            {L"Qt5Gui.dll", "?drawText@QPainter@QT@@QEAAXAEBVQRectF@2@AEBVQString@2@AEBVQTextOption@2@@Z"},
            {L"Qt6Gui.dll", "?drawText@QPainter@QT@@QEAAXAEBVQRectF@2@AEBVQString@2@AEBVQTextOption@2@@Z"},
            {L"Qt5Gui.dll", "?drawText@QPainter@@QEAAXAEBVQRectF@@AEBVQString@@AEBVQTextOption@@@Z"},
            {L"Qt6Gui.dll", "?drawText@QPainter@@QEAAXAEBVQRectF@@AEBVQString@@AEBVQTextOption@@@Z"},
        },
    },
    {
        reinterpret_cast<void**>(&OriginalDrawText6),
        reinterpret_cast<void*>(HookedDrawText6),
        {
            {L"Qt5Gui.dll", "?drawText@QPainter@QT@@QEAAXAEBVQRectF@2@HAEBVQString@2@PEAV32@@Z"},
            {L"Qt6Gui.dll", "?drawText@QPainter@QT@@QEAAXAEBVQRectF@2@HAEBVQString@2@PEAV32@@Z"},
            {L"Qt5Gui.dll", "?drawText@QPainter@@QEAAXAEBVQRectF@@HAEBVQString@@PEAV2@@Z"},
            {L"Qt6Gui.dll", "?drawText@QPainter@@QEAAXAEBVQRectF@@HAEBVQString@@PEAV2@@Z"},
        },
    },
    {
        reinterpret_cast<void**>(&OriginalQLabelSetText),
        reinterpret_cast<void*>(HookedQLabelSetText),
        {
            {L"Qt5Widgets.dll", "?setText@QLabel@QT@@QEAAXAEBVQString@2@@Z"},
            {L"Qt6Widgets.dll", "?setText@QLabel@QT@@QEAAXAEBVQString@2@@Z"},
            {L"Qt5Widgets.dll", "?setText@QLabel@@QEAAXAEBVQString@@@Z"},
            {L"Qt6Widgets.dll", "?setText@QLabel@@QEAAXAEBVQString@@@Z"},
        },
    },
    {
        reinterpret_cast<void**>(&OriginalQWindowSetTitle),
        reinterpret_cast<void*>(HookedQWindowSetTitle),
        {
            {L"Qt5Gui.dll", "?setTitle@QWindow@QT@@QEAAXAEBVQString@2@@Z"},
            {L"Qt6Gui.dll", "?setTitle@QWindow@QT@@QEAAXAEBVQString@2@@Z"},
            {L"Qt5Gui.dll", "?setTitle@QWindow@@QEAAXAEBVQString@@@Z"},
            {L"Qt6Gui.dll", "?setTitle@QWindow@@QEAAXAEBVQString@@@Z"},
        },
    },
    {
        reinterpret_cast<void**>(&OriginalQFileDialogGetOpenFileName),
        reinterpret_cast<void*>(HookedQFileDialogGetOpenFileName),
        {
			{L"Qt5Widgets.dll", "?getOpenFileName@QFileDialog@QT@@SA?AVQString@2@PEAVQWidget@2@AEBV32@11PEAV32@V?$QFlags@W4Option@QFileDialog@QT@@@2@@Z"},
            {L"Qt6Widgets.dll", "?getOpenFileName@QFileDialog@QT@@SA?AVQString@2@PEAVQWidget@2@AEBV32@11PEAV32@V?$QFlags@W4Option@QFileDialog@QT@@@2@@Z"},
        },
    },
    {
        reinterpret_cast<void**>(&OriginalQFileDialogGetExistingDirectory),
        reinterpret_cast<void*>(HookedQFileDialogGetExistingDirectory),
        {
            {L"Qt5Widgets.dll", "?getExistingDirectory@QFileDialog@QT@@SA?AVQString@2@PEAVQWidget@2@AEBV32@1V?$QFlags@W4Option@QFileDialog@QT@@@2@@Z"},
            {L"Qt6Widgets.dll", "?getExistingDirectory@QFileDialog@QT@@SA?AVQString@2@PEAVQWidget@2@AEBV32@1V?$QFlags@W4Option@QFileDialog@QT@@@2@@Z"},
        },
    },
    {
        reinterpret_cast<void**>(&OriginalQFontMetricsSize),
        reinterpret_cast<void*>(HookedFontMetricsSize),
        {
            {L"Qt5Gui.dll", "?size@QFontMetrics@@QEBA?AVQSize@@HAEBVQString@@HPEAH@Z"},
            {L"Qt6Gui.dll", "?size@QFontMetrics@@QEBA?AVQSize@@HAEBVQString@@HPEAH@Z"},
            {L"Qt6Gui.dll", "?size@QFontMetrics@QT@@QEBA?AVQSize@2@HAEBVQString@2@HPEAH@Z"},
            {L"Qt5Gui.dll", "?size@QFontMetrics@QT@@QEBA?AVQSize@2@HAEBVQString@2@HPEAH@Z"},
        },
    },
};

bool Hook::Initialize() {

    const std::wstring qtCoreModuleName = DetectQtCoreModuleName();
    if (qtCoreModuleName.empty()) {
        Logger::Write(L"[Hook] 未在程序目录找到 Qt6Core.dll 或 Qt5Core.dll");
        return false;
    }
    Logger::Write(L"[Hook] 已选择 QtCore 模块。module=%s", qtCoreModuleName.c_str());

    bool ok = true;
    for (const HookFunctionInfo& hook : kHookFunctions) {
        ok = RegisterHookFunction(hook, qtCoreModuleName) && ok;
    }
    if (!ok) {
        Logger::Write(u8"[Hook] 注册 Detours Hook 失败");
        detoursHook_.Release();
        return false;
    }

    QStringBridge::Config qstringBridgeConfig;
    qstringBridgeConfig.module_name = qtCoreModuleName;
    qstringBridgeConfig.load_if_missing = false;
    if (!qstringBridge_.Initialize(qstringBridgeConfig)) {
        Logger::Write(u8"[Hook] 初始化 QStringBridge 失败");
        detoursHook_.Release();
        return false;
    }

    TranslationManagerConfig translationConfig;
    translationConfig.filterChineseSourceWrites = true;
    TranslationManager::Configure(translationConfig);
    if (!TranslationManager::Initialize(L"Dictionaries/translations.txt")) {
        Logger::Write(L"[Hook] 初始化翻译字典失败。dictionary=Dictionaries/translations.txt");
        detoursHook_.Release();
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
