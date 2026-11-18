#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ft {

// Значения 1:1 с флагами ядра (internal/config): строки уходят в CLI как есть.
inline constexpr std::string_view kProviderVk = "vk";
inline constexpr std::string_view kProviderDirect = "direct";
inline constexpr std::string_view kProviderSfu = "sfu";

inline constexpr std::string_view kModeUdp = "udp";
inline constexpr std::string_view kModeTcp = "tcp";

inline constexpr std::string_view kTransportTcp = "tcp";
inline constexpr std::string_view kTransportUdp = "udp";

inline constexpr std::string_view kObfNone = "none";

inline constexpr std::string_view kDefaultListen = "127.0.0.1:9000";
inline constexpr int kDefaultStreams = 12;
inline constexpr int kDefaultDirectStreams = 1;
inline constexpr int kDefaultStreamsPerCred = 12;
inline constexpr int kBondMaxLanes = 256;
// Пейсинг с обфускацией: 20 мс - ptime Opus.
inline constexpr int kDefaultObfTimingMs = 20;

struct KcpProfile {
    int no_delay = 1;
    int interval = 20;
    int resend = 2;
    int nc = 1;
    int snd_wnd = 512;
    int rcv_wnd = 512;
    int mtu = 1200;
    bool ack_no_delay = true;

    bool operator==(const KcpProfile&) const = default;
};

// Секреты держим отдельно: на диск они идут только через DPAPI.
struct ProfileSecrets {
    std::string obf_key;
    std::string client_id;
    std::vector<std::string> links;
    // Конфиг WireGuard/AmneziaWG из поля wg ссылки - с ним служба поднимает туннель сама.
    std::string wg_conf;

    bool operator==(const ProfileSecrets&) const = default;
};

struct Profile {
    std::string id;
    std::string name;

    std::string provider = std::string(kProviderVk);
    std::string peer;
    std::string listen = std::string(kDefaultListen);
    std::string mode = std::string(kModeUdp);
    std::string transport = std::string(kTransportTcp);
    // 0 - дефолт ядра по провайдеру.
    int streams = 0;
    int streams_per_cred = kDefaultStreamsPerCred;
    bool bond = false;

    std::string obf_profile = std::string(kObfNone);
    int obf_timing_ms = 0;

    std::string dns_mode = "auto";
    std::vector<std::string> dns_servers;

    bool manual_captcha = false;
    // Звонки по очереди: ядру - один, на ошибке ссылки или без связи - следующий.
    bool rolling_calls = false;
    std::string platform = "desktop";

    KcpProfile kcp;
    int sfu_fps = 0;
    int sfu_batch = 0;

    std::string turn_host;
    std::string turn_port;

    ProfileSecrets secrets;

    bool operator==(const Profile&) const = default;

    int effective_streams() const;
    std::string local_address() const { return listen; }
};

enum class ProfileIssue {
    MissingName,
    MissingPeer,
    BadPeer,
    BadListen,
    BadProvider,
    MissingLinks,
    BadLink,
    BadMode,
    BadTransport,
    BadObfProfile,
    BadObfKey,
    ObfTimingWithoutObf,
    BadStreams,
    BadStreamsPerCred,
    BadPlatform,
    BadDnsMode,
    BadDnsServer,
    BondNeedsTcp,
    KcpNeedsTcp,
    BadKcp,
    BadSfu,
    BadClientId,
};

std::string_view to_string(ProfileIssue issue);

// Пусто - профиль годится для запуска ядра. Правила повторяют internal/config/validate.go.
std::vector<ProfileIssue> validate(const Profile& p);

bool is_host_port(std::string_view s);
bool is_hex_key(std::string_view s);

std::string sanitize_name(std::string_view name);

std::string unique_name(std::string_view base, const std::vector<std::string>& taken);

// 32 hex-символа. Не секрет - просто имя файла профиля.
std::string new_profile_id();
bool is_profile_id(std::string_view id);

} // namespace ft
