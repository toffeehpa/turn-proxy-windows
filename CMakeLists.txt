cmake_minimum_required(VERSION 3.25)

# Полная версия с beta-суффиксом; release-please правит строку по маркеру.
set(FREETURN_VERSION "0.1.0-beta.1") # x-release-please-version
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" FREETURN_VERSION_NUMERIC "${FREETURN_VERSION}")

project(FreeTurn
    VERSION ${FREETURN_VERSION_NUMERIC}
    DESCRIPTION "FreeTurn tray client for Windows"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FREETURN_BUILD_TESTS "Собирать unit-тесты" ON)
option(FREETURN_WARNINGS_AS_ERRORS "Предупреждения как ошибки" ON)

include(cmake/Warnings.cmake)

# Версия ядра для окна "О программе" - из того же core.version, по которому CI берёт ядро.
file(STRINGS core.version FREETURN_CORE_VERSION LIMIT_COUNT 1)
string(REGEX REPLACE "^v" "" FREETURN_CORE_VERSION "${FREETURN_CORE_VERSION}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS core.version)

configure_file(src/core/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/core/version.h @ONLY)
configure_file(src/core/version_rc.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/core/version_rc.h @ONLY)

# Логика без Win32 API - её гоняют unit-тесты с Mock движка.
add_library(freeturn_core STATIC
    src/core/base64.cpp
    src/core/connection_manager.cpp
    src/core/connection_state.cpp
    src/core/core_args.cpp
    src/core/file_io.cpp
    src/core/freeturn_uri.cpp
    src/core/ipc_protocol.cpp
    src/core/json.cpp
    src/core/log_parser.cpp
    src/core/logger.cpp
    src/core/profile.cpp
    src/core/profile_json.cpp
    src/core/profile_store.cpp
    src/core/semver.cpp
    src/core/service_protocol.cpp
    src/core/settings.cpp
)
if(WIN32)
    # Атомарная запись на Windows - через Win32 со сбросом на диск; вариант на std только для тестов вне Windows.
    target_sources(freeturn_core PRIVATE src/win/file_io_win.cpp src/win/wstr.cpp)
    target_compile_definitions(freeturn_core PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
else()
    target_sources(freeturn_core PRIVATE src/core/file_io_std.cpp)
endif()
target_include_directories(freeturn_core PUBLIC src ${CMAKE_CURRENT_BINARY_DIR}/generated)
freeturn_set_warnings(freeturn_core)

if(WIN32)
    enable_language(RC)

    add_library(freeturn_win STATIC
        src/win/dpapi.cpp
        src/win/paths.cpp
        src/win/pipe_ipc.cpp
        src/win/process_engine.cpp
        src/win/service_engine.cpp
        src/win/service_host.cpp
        src/win/single_instance.cpp
    )
    target_link_libraries(freeturn_win PUBLIC freeturn_core crypt32 shell32 ole32 advapi32 user32)
    freeturn_set_warnings(freeturn_win)

    add_executable(FreeTurn WIN32
        src/app/app.cpp
        src/app/dialogs.cpp
        src/app/main.cpp
        src/app/strings.cpp
        src/app/tray.cpp
        res/freeturn.rc
    )
    target_link_libraries(FreeTurn PRIVATE freeturn_win comctl32 comdlg32 ole32 dwmapi uxtheme)
    target_include_directories(FreeTurn PRIVATE res)
    freeturn_set_warnings(FreeTurn)
    if(MSVC)
        # Манифест свой (asInvoker, DPI, comctl32 v6) - линкерный не нужен.
        target_link_options(FreeTurn PRIVATE /MANIFEST:NO)
    endif()

    add_executable(freeturn-service src/service/main.cpp res/service.rc)
    target_link_libraries(freeturn-service PRIVATE freeturn_win)
    target_include_directories(freeturn-service PRIVATE res)
    freeturn_set_warnings(freeturn-service)
endif()

if(FREETURN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
