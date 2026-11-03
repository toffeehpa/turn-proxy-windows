# Сборка

Сборки делает GitHub Actions на `windows-latest`, вручную ничего не собирается. Локальная сборка нужна только для отладки.

## CI

- **Формат**: `clang-format --dry-run --Werror` по всем `*.cpp` и `*.h`
- **MSVC Debug и Release**: `/W4 /WX`, затем все тесты через `ctest`
- **Установщик** (Release): `freeturn-<версия>-windows-amd64.msi` в артефакте `freeturn-windows-amd64`. WiX v5 ставится в CI как `dotnet tool`

Тесты не ходят в сеть и не запускают настоящее ядро:

- **`freeturn_tests`**: логика - автомат, профили, `freeturn://`, аргументы ядра, разбор логов, IPC-протокол, версии
- **`freeturn_win_tests`**: Win32 - DPAPI, Named Pipe, процесс ядра и служба целиком (`ServiceHost` + `ServiceEngine`) с подставным ядром `fake_core.exe`, без SCM и SYSTEM

## Ядро

Версия ядра закреплена в `core.version`. Скрипт `tools/fetch-core.ps1` скачивает `client-windows-<arch>.exe` и `checksums.txt` только из релиза `toffeehpa/free-turn-proxy` на github.com, сверяет SHA-256 и кладёт файл как `freeturn-core.exe`. Не совпал хеш - сборка падает.

Новое ядро - правка одной строки в `core.version`.

## Локальная Сборка

- **Visual Studio 2022** (MSVC v143, 17.10 или новее) с компонентом "Разработка классических приложений на C++"
- **CMake** 3.25+

Сторонних библиотек нет: только Win32 API и стандартная библиотека C++23.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
./tools/fetch-core.ps1 -OutDir build/Release
```

Трей без службы не подключится. Для отладки служба запускается в консоли администратора: `build\Release\freeturn-service.exe --console`.

## Установщик

`installer/FreeTurn.wxs` - MSI на всю машину: `Program Files\FreeTurn`, служба `FreeTurnService` (LocalSystem, автозапуск), ярлык в меню Пуск, закрытие запущенного FreeTurn при обновлении. Как у WireGuard для Windows и OpenVPN 2.5+.

```powershell
dotnet tool install --global wix --version 5.0.2
wix extension add -g WixToolset.Util.wixext/5.0.2
wix build installer/FreeTurn.wxs -arch x64 -ext WixToolset.Util.wixext -d Version=0.1.0 -d BinDir=dist/bin -d ResDir=res -o freeturn.msi
```

В `BinDir` должны лежать `FreeTurn.exe`, `freeturn-service.exe`, `freeturn-core.exe`, `LICENSE`, `LICENSE-freeturn-core.txt`, `THIRD_PARTY_NOTICES.md`. MSI понимает только `x.y.z`, поэтому beta одной версии ставятся поверх друг друга (`AllowSameVersionUpgrades`).

## Формат

```powershell
clang-format -i (git ls-files '*.cpp' '*.h')
```

CI проверяет формат тем clang-format, что стоит на `windows-latest`.

## Иконки

`res/icons/*.ico` собираются из `res/freeturn-logo-512.png` скриптом `tools/make_icons.sh` (нужен ImageMagick 7). В трее - только знак F: серый, жёлтый и цветной.

## Выпуск

Версии ведёт release-please по Conventional Commits, теги `vX.Y.Z`, ветка `main`. Пока идёт beta, релизы помечаются Pre-release (`0.1.0-beta.1`, `0.1.0-beta.2`, ...).

> [!NOTE]
> В `release-please-config.json` стоит `"release-as": "0.1.0-beta.1"` - только для первого релиза. После него строку нужно убрать, иначе release-please будет предлагать ту же версию.
