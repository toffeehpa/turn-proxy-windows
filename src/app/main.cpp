#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>

#include <string>
#include <vector>

#include "app/app.h"
#include "app/dialogs.h"
#include "app/strings.h"
#include "core/ipc_protocol.h"
#include "win/pipe_ipc.h"
#include "win/handle.h"
#include "win/single_instance.h"
#include "win/wstr.h"

namespace {

std::vector<std::string> command_args() {
    std::vector<std::string> args;
    int argc = 0;
    ft::win::LocalPtr<LPWSTR> argv(CommandLineToArgvW(GetCommandLineW(), &argc));
    for (int i = 1; argv && i < argc; ++i) {
        args.push_back(ft::win::narrow(argv.get()[i]));
    }
    return args;
}

} // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    // DLL только из System32: рядом с exe в Загрузках может лежать что угодно.
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    ft::ui::init_language();
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    const auto args = command_args();
    ft::win::SingleInstance instance;
    if (!instance.is_first()) {
        const auto msg = ft::encode_ipc(ft::command_from_args(args));
        auto result = ft::win::PipeSendResult::NotRunning;
        // Первый экземпляр мог ещё не поднять пайп - даём ему пару секунд.
        for (int i = 0; i < 10 && result == ft::win::PipeSendResult::NotRunning; ++i) {
            result = ft::win::send_to_pipe(ft::win::default_pipe_name(), msg);
            if (result == ft::win::PipeSendResult::NotRunning) {
                Sleep(300);
            }
        }
        if (result == ft::win::PipeSendResult::Untrusted) {
            ft::ui::message_box(nullptr, ft::ui::tr(ft::ui::S::UntrustedPipe), MB_ICONWARNING);
        }
        return 0;
    }

    ft::ui::App app;
    const int code = app.run(inst, args);
    CoUninitialize();
    return code;
}
