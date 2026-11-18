// Подставной freeturn-core.exe для тестов Engine Adapter: печатает строки как ядро.

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>

namespace {

std::atomic<bool> g_stop{false};
bool g_ignore = false;

BOOL WINAPI on_ctrl(DWORD type) {
    if (type == CTRL_BREAK_EVENT || type == CTRL_C_EVENT) {
        if (!g_ignore) {
            g_stop = true;
        }
        return TRUE;
    }
    return FALSE;
}

void say(const char* text) {
    std::fprintf(stderr, "2026/11/03 09:15:00 %s\n", text);
    std::fflush(stderr);
}

} // namespace

int main(int argc, char** argv) {
    const char* mode = argc > 2 && std::strcmp(argv[1], "-fake") == 0 ? argv[2] : "ok";
    SetConsoleCtrlHandler(on_ctrl, TRUE);

    if (std::strcmp(mode, "echo") == 0) {
        for (int i = 3; i < argc; ++i) {
            std::printf("ARG %s\n", argv[i]);
        }
        return 0;
    }
    if (std::strcmp(mode, "fail") == 0) {
        say("invalid -mode value \"x\": must be udp | tcp");
        return 3;
    }
    g_ignore = std::strcmp(mode, "ignore") == 0;
    say("[INFO] lifecycle start mode=udp streams=2");
    say("[DEBUG] [STREAM 0] Established DTLS connection");
    while (!g_stop) {
        Sleep(20);
    }
    say("[INFO] Terminating...");
    return 0;
}
