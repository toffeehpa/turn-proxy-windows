# Сторонние Компоненты

## FreeTurn.exe

Сторонних библиотек нет: Win32 API и стандартная библиотека C++ (MSVC STL, лицензия Apache-2.0 WITH LLVM-exception), которая линкуется компилятором.

## Установщик

MSI собирается [WiX Toolset](https://github.com/wixtoolset/wix) v5 (MS-RL). В установщик встроены custom actions из `WixToolset.Util.wixext`: `util:CloseApplication` закрывает FreeTurn при обновлении, `WixShellExec` запускает его с последнего окна, `WixQuietExec` при удалении стирает автозапуск из HKCU. Окна установщика свои, по раскладке установщика OpenVPN; его код не заимствован.

## Ядро

`freeturn-core.exe` - переименованный `client-windows-<arch>.exe` из [free-turn-proxy](https://github.com/toffeehpa/free-turn-proxy), лицензия HBL (Happy Bunny License). В репозитории его нет: CI добавляет ядро в установщик вместе с `LICENSE-freeturn-core.txt`. Зависимости ядра перечислены в его репозитории.

## Wintun

`wintun.dll` - драйвер сетевого адаптера [Wintun](https://www.wintun.net) 0.14.1 от WireGuard LLC, через него ядро поднимает туннель из ссылки. Распространяется без изменений по Prebuilt Binaries License, которая разрешает поставлять его вместе с программой, работающей с ним только через API `wintun.h`. CI скачивает официальный архив и сверяет SHA-256, в установщик идёт вместе с `LICENSE-wintun.txt`.

## Ориентиры

- **[OpenVPN GUI](https://github.com/OpenVPN/openvpn-gui)** (GPL-2.0): только архитектурный ориентир. Код, ресурсы и переводы не заимствованы
- **[turn-proxy-android](https://github.com/toffeehpa/turn-proxy-android)** (GPL-3.0): модель профилей и поведение. Код не заимствован

## Иконка

`res/freeturn-logo-512.png` и иконки из неё - логотип FreeTurn.
