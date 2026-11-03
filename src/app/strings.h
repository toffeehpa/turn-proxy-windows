#pragma once

#include <initializer_list>
#include <string>
#include <string_view>

#include "core/connection_state.h"
#include "core/freeturn_uri.h"
#include "core/profile.h"

namespace ft::ui {

enum class S {
    AppName,
    StateDisconnected,
    StateStarting,
    StateConnecting,
    StateConnected,
    StateCaptcha,
    StateReconnecting,
    StateStopping,
    StateError,
    MenuConnect,
    MenuDisconnect,
    MenuStreams,
    MenuProfiles,
    MenuNoProfiles,
    MenuEdit,
    MenuClone,
    MenuCopyLink,
    MenuDelete,
    MenuCopyAddress,
    MenuImport,
    MenuSettings,
    MenuOpenLog,
    MenuCheckUpdates,
    MenuAbout,
    MenuExit,
    StatusLine,
    NotifyConnected,
    NotifyCaptcha,
    NotifyError,
    NotifyReconnecting,
    NotifyImported,
    AlreadyRunning,
    ConfirmDelete,
    DeleteActive,
    InvalidProfile,
    ExperimentalDisabled,
    ImportFailed,
    ImportFileFailed,
    ClipboardEmpty,
    SaveFailed,
    BrokenProfiles,
    AboutTitle,
    AboutApp,
    AboutCore,
    DlgClose,
    UntrustedPipe,
    EditApplyLater,
    DlgImportTitle,
    DlgProfileTitle,
    DlgLink,
    DlgPaste,
    DlgFromFile,
    DlgName,
    DlgProvider,
    DlgPeer,
    DlgLinks,
    DlgListen,
    DlgMode,
    DlgTransport,
    DlgStreams,
    DlgObf,
    DlgObfKey,
    DlgClientId,
    DlgManualCaptcha,
    DlgBond,
    DlgSave,
    DlgCancel,
    ProviderVk,
    ProviderDirect,
    ProviderSfu,
    ModeUdp,
    ModeTcp,
    ObfNone,
    DlgSettingsTitle,
    DlgConnectOnStart,
    DlgAutoReconnect,
    DlgVerboseLog,
    DlgExperimentalSfu,
    DlgOpenData,
    DlgGroupStartup,
    DlgGroupLog,
    DlgGroupExperimental,
    FilterJson,
    Count,
};

// Язык по языку интерфейса Windows: русский или английский.
void init_language();
const wchar_t* tr(S id);
// Подстановка {} по порядку: std::format для wchar_t тут избыточен.
std::wstring trf(S id, std::initializer_list<std::wstring_view> args);

std::wstring state_text(ConnectionState s);
std::wstring issue_text(ProfileIssue issue);
std::wstring uri_error_text(UriError e);

} // namespace ft::ui
