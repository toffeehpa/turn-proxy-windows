<div align="center">

[![Core](https://img.shields.io/badge/Core-free--turn--proxy-blue?logo=github&logoColor=white)](https://github.com/toffeehpa/free-turn-proxy)
![Windows](https://img.shields.io/badge/Windows-10%2B-0078D4?logo=windows&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus&logoColor=white)
![Win32](https://img.shields.io/badge/UI-Win32-757575)
![License](https://img.shields.io/badge/license-MIT-blue)
</div>

# FreeTurn для Windows

> **Disclaimer.** Проект предназначен **исключительно для образовательных и исследовательских целей.**

> [!WARNING]
> **Beta.** Релизов пока нет. Установщик собирает GitHub Actions: артефакт `freeturn-windows-amd64` с `freeturn-<версия>-windows-amd64.msi`.

Приложение живёт в области уведомлений и управляет ядром [free-turn-proxy](https://github.com/toffeehpa/free-turn-proxy). Устроено как OpenVPN GUI: трей работает без прав, а ядро запускает служба `FreeTurnService` - ей нужны права на маршруты. Сейчас FreeTurn поднимает локальный адрес (по умолчанию `127.0.0.1:9000`), а WireGuard, AmneziaWG или Xray вы направляете на него сами.

> [!NOTE]
> Встроенный VPN "одной кнопкой", как в Android, ждёт поддержки туннеля на Windows в ядре.

## Возможности

- **Только трей**: меню профилей, подключение, состояние, без главного окна
- **Несколько профилей** и их клонирование
- **Импорт** по ссылке `freeturn://` или из JSON-файла (свой формат или JSON ядра)
- **Relay / Direct** - через TURN-реле звонка или напрямую на сервер
- **SFU** - экспериментально, включается в настройках
- **Переподключение** при обрыве, с паузой между попытками
- **Секреты под DPAPI**: ключ обфускации, Client ID и ссылки на звонки не лежат на диске открытым текстом
- **Маршруты без петли**: служба запускает ядро с `-routes`, трафик к TURN-серверам не уходит в ваш же туннель
- **Трей без прав**: права администратора нужны только установщику, всё привилегированное делает служба

## Как Это Работает

- **Иконка**: серая - отключено или ошибка, жёлтая - подключение, цветная - подключено
- **Служба**: `FreeTurnService` запускает `freeturn-core.exe` из папки установки; закрылся трей - ядро остановлено
- **Режим udp**: направьте WireGuard / AmneziaWG на локальный адрес профиля
- **Режим tcp**: направьте Xray / VLESS на локальный адрес профиля
- **Второй запуск**: `FreeTurn.exe freeturn://...` открывает импорт в уже запущенном приложении; `--connect <профиль>`, `--disconnect`, `--reconnect`, `--quit`

Флаги и режимы ядра - [docs ядра](https://github.com/toffeehpa/free-turn-proxy/tree/master/docs).

## Требования

- **Windows 10** или новее, x64
- **Права администратора** на установку: MSI ставит службу
- **freeturn-core.exe** - уже в установщике: CI берёт `client-windows-amd64.exe` из [релиза ядра](https://github.com/toffeehpa/free-turn-proxy/releases), сверяет с `checksums.txt` и переименовывает
- **VPS** с сервером free-turn-proxy
- **Ссылка на звонок** для Relay; для Direct не требуется
- **VPN-клиент** - WireGuard, AmneziaWG или Xray

## Документы

- [docs/architecture.md](docs/architecture.md) - устройство приложения
- [docs/building.md](docs/building.md) - сборка и тесты
- [docs/security.md](docs/security.md) - модель безопасности и сообщения об уязвимостях
- [docs/privacy.md](docs/privacy.md) - какие данные и где хранятся
- [docs/code-signing.md](docs/code-signing.md) - подпись сборок
- [docs/third-party-notices.md](docs/third-party-notices.md) - сторонние компоненты

## Благодарности

- **[@samosvalishe](https://github.com/samosvalishe)** (Антон) - прежний владелец и автор ядра и Android-приложения, отказался от проекта
- **[@Moroka8](https://github.com/Moroka8)** - форк ядра [vk-turn-proxy](https://github.com/Moroka8/vk-turn-proxy)
- **[@alexmac6574](https://github.com/alexmac6574)** - форк ядра [vk-turn-proxy](https://github.com/alexmac6574/vk-turn-proxy)
- **[@cacggghp](https://github.com/cacggghp)** - оригинальное [vk-turn-proxy](https://github.com/cacggghp/vk-turn-proxy)
- **[@OpenVPN](https://github.com/OpenVPN)** - [OpenVPN GUI](https://github.com/OpenVPN/openvpn-gui) как архитектурный ориентир для трей-клиента
