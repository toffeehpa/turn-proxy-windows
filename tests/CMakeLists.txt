set(FREETURN_TEST_SOURCES
    test_main.cpp
    test_base64.cpp
    test_connection_manager.cpp
    test_connection_state.cpp
    test_core_args.cpp
    test_ipc_protocol.cpp
    test_json.cpp
    test_log_parser.cpp
    test_profile.cpp
    test_profile_store.cpp
    test_semver.cpp
    test_service_protocol.cpp
    test_settings.cpp
    test_uri.cpp
)

add_executable(freeturn_tests ${FREETURN_TEST_SOURCES})
target_link_libraries(freeturn_tests PRIVATE freeturn_core)
freeturn_set_warnings(freeturn_tests)
add_test(NAME unit COMMAND freeturn_tests)

if(WIN32)
    # Win32-часть: DPAPI, Named Pipe, процессы и Job Object - только на Windows.
    add_executable(freeturn_win_tests
        test_main.cpp
        win/test_dpapi.cpp
        win/test_pipe_ipc.cpp
        win/test_process_engine.cpp
        win/test_service.cpp
    )
    target_link_libraries(freeturn_win_tests PRIVATE freeturn_win)
    freeturn_set_warnings(freeturn_win_tests)
    target_include_directories(freeturn_win_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    # Подставной freeturn-core.exe: печатает строки как ядро и ждёт Ctrl+Break.
    add_executable(fake_core win/fake_core_main.cpp)
    freeturn_set_warnings(fake_core)
    target_compile_definitions(freeturn_win_tests PRIVATE FAKE_CORE_PATH="$<TARGET_FILE:fake_core>")
    add_dependencies(freeturn_win_tests fake_core)
    add_test(NAME win COMMAND freeturn_win_tests)
endif()
