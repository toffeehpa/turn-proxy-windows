#include "app/strings.h"

#include <windows.h>

#include <array>

namespace ft::ui {

namespace {

struct Pair {
    const wchar_t* ru;
    const wchar_t* en;
};

constexpr std::array<Pair, static_cast<std::size_t>(S::Count)> kTable = {{
    {L"FreeTurn", L"FreeTurn"},
    {L"Отключено", L"Disconnected"},
    {L"Запуск", L"Starting"},
    {L"Подключение", L"Connecting"},
    {L"Подключено", L"Connected"},
    {L"Нужна капча", L"Captcha required"},
    {L"Переподключение", L"Reconnecting"},
    {L"Остановка", L"Stopping"},
    {L"Ошибка", L"Error"},
    {L"Подключить", L"Connect"},
    {L"Отключить", L"Disconnect"},
    {L"Потоки: {}/{}", L"Streams: {}/{}"},
    {L"Профили", L"Profiles"},
    {L"Нет профилей", L"No profiles"},
    {L"Изменить...", L"Edit..."},
    {L"Клонировать", L"Clone"},
    {L"Копировать ссылку freeturn://", L"Copy freeturn:// link"},
    {L"Удалить", L"Delete"},
    {L"Копировать локальный адрес", L"Copy local address"},
    {L"Импорт профиля...", L"Import profile..."},
    {L"Настройки", L"Settings"},
    {L"Открыть лог", L"Open log"},
    {L"Проверить обновления", L"Check for updates"},
    {L"О программе", L"About"},
    {L"Выход", L"Exit"},
    {L"{}: {}", L"{}: {}"},
    {L"Подключено. Направьте WireGuard или Xray на {}", L"Connected. Point WireGuard or Xray to {}"},
    {L"Пройдите капчу в открывшемся браузере", L"Solve the captcha in the opened browser"},
    {L"Ошибка подключения: {}", L"Connection error: {}"},
    {L"Связь потеряна, попытка {} из {}", L"Connection lost, attempt {} of {}"},
    {L"Профиль {} добавлен", L"Profile {} added"},
    {L"FreeTurn уже запущен - меню в области уведомлений",
     L"FreeTurn is already running - see the notification area"},
    {L"Удалить профиль {}?", L"Delete profile {}?"},
    {L"Профиль сейчас подключён. Сначала отключитесь.", L"The profile is connected. Disconnect first."},
    {L"Профиль заполнен не полностью: {}", L"The profile is incomplete: {}"},
    {L"SFU - экспериментальный режим. Включите его в настройках.",
     L"SFU is experimental. Enable it in Settings."},
    {L"Не удалось разобрать ссылку: {}", L"Cannot parse the link: {}"},
    {L"Не удалось прочитать файл: {}", L"Cannot read the file: {}"},
    {L"В буфере обмена нет ссылки freeturn://", L"The clipboard has no freeturn:// link"},
    {L"Не удалось сохранить: {}", L"Cannot save: {}"},
    {L"Не прочитаны файлы профилей: {}", L"Cannot read profile files: {}"},
    {L"О программе FreeTurn", L"About FreeTurn"},
    {L"FreeTurn {} - клиент FreeTurn для Windows\nCopyright (C) 2026 toffeehpa\n"
     L"https://github.com/toffeehpa/turn-proxy-windows",
     L"FreeTurn {} - FreeTurn client for Windows\nCopyright (C) 2026 toffeehpa\n"
     L"https://github.com/toffeehpa/turn-proxy-windows"},
    {L"FreeTurn {} - ядро: туннель для WireGuard и прокси через звонки VK.\n"
     L"Copyright (C) 2025-2026 samosvalishe\nCopyright (C) 2026 "
     L"toffeehpa\nhttps://github.com/toffeehpa/free-turn-proxy",
     L"FreeTurn {} - core: tunnel for WireGuard and proxies over VK calls.\n"
     L"Copyright (C) 2025-2026 samosvalishe\nCopyright (C) 2026 "
     L"toffeehpa\nhttps://github.com/toffeehpa/free-turn-proxy"},
    {L"Закрыть", L"Close"},
    {L"Канал FreeTurn занят процессом другого пользователя - команда не отправлена",
     L"The FreeTurn pipe belongs to another user - the command was not sent"},
    {L"Изменения применятся при следующем подключении", L"Changes apply on the next connect"},
    {L"Импорт профиля", L"Import profile"},
    {L"Профиль", L"Profile"},
    {L"Ссылка:", L"Link:"},
    {L"Вставить", L"Paste"},
    {L"Из файла...", L"From file..."},
    {L"Имя:", L"Name:"},
    {L"Провайдер:", L"Provider:"},
    {L"Сервер:", L"Server:"},
    {L"Ссылки на звонки, по одной в строке:", L"Call links, one per line:"},
    {L"Локальный адрес:", L"Local address:"},
    {L"Режим:", L"Mode:"},
    {L"Транспорт:", L"Transport:"},
    {L"Потоки: {}", L"Streams: {}"},
    {L"Рекомендуется 8–12", L"8–12 recommended"},
    {L"Обфускация:", L"Obfuscation:"},
    {L"Ключ обфускации:", L"Obfuscation key:"},
    {L"Пейсинг:", L"Pacing:"},
    {L"мс", L"ms"},
    {L"0 - без пейсинга", L"0 - no pacing"},
    {L"Client ID:", L"Client ID:"},
    {L"Ручная капча", L"Manual captcha"},
    {L"Bond (только TCP)", L"Bond (TCP only)"},
    {L"Сохранить", L"Save"},
    {L"Отмена", L"Cancel"},
    {L"Дополнительно >>", L"Advanced >>"},
    {L"<< Дополнительно", L"<< Advanced"},
    {L"vk - TURN-реле звонков", L"vk - call TURN relay"},
    {L"direct - напрямую на сервер", L"direct - straight to the server"},
    {L"sfu - SFU звонка (experimental)", L"sfu - call SFU (experimental)"},
    {L"udp - WireGuard / AmneziaWG", L"udp - WireGuard / AmneziaWG"},
    {L"tcp - Xray / VLESS", L"tcp - Xray / VLESS"},
    {L"none - без обфускации", L"none - no obfuscation"},
    {L"Настройки", L"Settings"},
    {L"Подключаться при запуске", L"Connect on start"},
    {L"Переподключаться при обрыве", L"Reconnect on failure"},
    {L"Подробный лог ядра", L"Verbose core log"},
    {L"SFU-канал звонка", L"Call SFU channel"},
    {L"Открыть папку данных", L"Open data folder"},
    {L"Запуск и подключение", L"Startup and connection"},
    {L"Журнал", L"Log"},
    {L"Экспериментальные функции", L"Experimental features"},
    {L"Профиль JSON (*.json)", L"Profile JSON (*.json)"},
}};

bool g_russian = false;

} // namespace

void init_language() {
    g_russian = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN;
}

const wchar_t* tr(S id) {
    const auto& p = kTable[static_cast<std::size_t>(id)];
    return g_russian ? p.ru : p.en;
}

std::wstring trf(S id, std::initializer_list<std::wstring_view> args) {
    std::wstring out = tr(id);
    std::size_t pos = 0;
    for (const auto a : args) {
        pos = out.find(L"{}", pos);
        if (pos == std::wstring::npos) {
            break;
        }
        out.replace(pos, 2, a);
        pos += a.size();
    }
    return out;
}

std::wstring state_text(ConnectionState s) {
    switch (s) {
    case ConnectionState::Disconnected:
        return tr(S::StateDisconnected);
    case ConnectionState::Starting:
        return tr(S::StateStarting);
    case ConnectionState::Connecting:
        return tr(S::StateConnecting);
    case ConnectionState::Connected:
        return tr(S::StateConnected);
    case ConnectionState::Captcha:
        return tr(S::StateCaptcha);
    case ConnectionState::Reconnecting:
        return tr(S::StateReconnecting);
    case ConnectionState::Stopping:
        return tr(S::StateStopping);
    case ConnectionState::Error:
        return tr(S::StateError);
    }
    return {};
}

std::wstring issue_text(ProfileIssue issue) {
    const bool ru = g_russian;
    switch (issue) {
    case ProfileIssue::MissingName:
        return ru ? L"нет имени" : L"missing name";
    case ProfileIssue::MissingPeer:
        return ru ? L"не указан сервер" : L"missing server";
    case ProfileIssue::BadPeer:
        return ru ? L"сервер должен быть host:port" : L"server must be host:port";
    case ProfileIssue::BadListen:
        return ru ? L"локальный адрес должен быть ip:port" : L"local address must be ip:port";
    case ProfileIssue::BadProvider:
        return ru ? L"неизвестный провайдер" : L"unknown provider";
    case ProfileIssue::MissingLinks:
        return ru ? L"нужна ссылка на звонок" : L"a call link is required";
    case ProfileIssue::BadLink:
        return ru ? L"ссылка на звонок должна начинаться с https://" : L"call links must start with https://";
    case ProfileIssue::BadMode:
        return ru ? L"режим udp или tcp" : L"mode must be udp or tcp";
    case ProfileIssue::BadTransport:
        return ru ? L"транспорт tcp или udp" : L"transport must be tcp or udp";
    case ProfileIssue::BadObfProfile:
        return ru ? L"неизвестный профиль обфускации" : L"unknown obfuscation profile";
    case ProfileIssue::BadObfKey:
        return ru ? L"ключ обфускации - 64 hex-символа" : L"obfuscation key must be 64 hex characters";
    case ProfileIssue::ObfTimingWithoutObf:
        return ru ? L"obf-timing без обфускации" : L"obf-timing without obfuscation";
    case ProfileIssue::BadStreams:
        return ru ? L"потоков от 0 до 256" : L"streams must be 0..256";
    case ProfileIssue::BadStreamsPerCred:
        return ru ? L"streams-per-cred должен быть больше 0" : L"streams-per-cred must be positive";
    case ProfileIssue::BadPlatform:
        return ru ? L"платформа desktop или mobile" : L"platform must be desktop or mobile";
    case ProfileIssue::BadDnsMode:
        return ru ? L"DNS: plain, doh или auto" : L"DNS must be plain, doh or auto";
    case ProfileIssue::BadDnsServer:
        return ru ? L"неверный DNS-сервер" : L"bad DNS server";
    case ProfileIssue::BondNeedsTcp:
        return ru ? L"bond работает только в режиме tcp" : L"bond requires tcp mode";
    case ProfileIssue::KcpNeedsTcp:
        return ru ? L"параметры KCP только для режима tcp" : L"KCP settings require tcp mode";
    case ProfileIssue::BadKcp:
        return ru ? L"неверные параметры KCP" : L"bad KCP settings";
    case ProfileIssue::BadSfu:
        return ru ? L"неверные параметры SFU" : L"bad SFU settings";
    case ProfileIssue::BadClientId:
        return ru ? L"Client ID без пробелов и запятых" : L"Client ID must not contain spaces or commas";
    }
    return {};
}

std::wstring uri_error_text(UriError e) {
    const bool ru = g_russian;
    switch (e) {
    case UriError::BadScheme:
        return ru ? L"ссылка должна начинаться с freeturn://" : L"the link must start with freeturn://";
    case UriError::Empty:
        return ru ? L"пустая ссылка" : L"empty link";
    case UriError::TooLong:
        return ru ? L"слишком длинная ссылка" : L"the link is too long";
    case UriError::BadBase64:
    case UriError::BadJson:
        return ru ? L"ссылка повреждена" : L"the link is damaged";
    case UriError::UnsupportedVersion:
        return ru ? L"ссылка от более новой версии" : L"the link is from a newer version";
    case UriError::MissingProvider:
    case UriError::MissingPeer:
        return ru ? L"в ссылке нет сервера" : L"the link has no server";
    }
    return {};
}

} // namespace ft::ui
