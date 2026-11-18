#pragma once

#include <windows.h>
#include <objbase.h>

#include <memory>
#include <type_traits>
#include <vector>

namespace ft::win {

struct HandleCloser {
    void operator()(HANDLE h) const noexcept {
        if (h != nullptr && h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
        }
    }
};
using UniqueHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, HandleCloser>;

// INVALID_HANDLE_VALUE и nullptr - одно и то же "нет хэндла": API Win32 возвращают то одно, то другое.
inline UniqueHandle make_handle(HANDLE h) {
    return UniqueHandle(h == INVALID_HANDLE_VALUE ? nullptr : h);
}

struct LocalFreer {
    void operator()(void* p) const noexcept { LocalFree(p); }
};
template <class T>
using LocalPtr = std::unique_ptr<T, LocalFreer>;

struct CoTaskFreer {
    void operator()(void* p) const noexcept { CoTaskMemFree(p); }
};
template <class T>
using CoTaskPtr = std::unique_ptr<T, CoTaskFreer>;

struct GlobalFreer {
    void operator()(HGLOBAL h) const noexcept { GlobalFree(h); }
};
using UniqueGlobal = std::unique_ptr<std::remove_pointer_t<HGLOBAL>, GlobalFreer>;

struct GdiDeleter {
    void operator()(HGDIOBJ o) const noexcept { DeleteObject(o); }
};
using UniqueBrush = std::unique_ptr<std::remove_pointer_t<HBRUSH>, GdiDeleter>;

struct MenuDestroyer {
    void operator()(HMENU m) const noexcept { DestroyMenu(m); }
};
using UniqueMenu = std::unique_ptr<std::remove_pointer_t<HMENU>, MenuDestroyer>;

class ProcThreadAttributes {
public:
    explicit ProcThreadAttributes(DWORD count) {
        SIZE_T size = 0;
        InitializeProcThreadAttributeList(nullptr, count, 0, &size);
        buf_.resize(size);
        ok_ = InitializeProcThreadAttributeList(get(), count, 0, &size) != FALSE;
    }
    ~ProcThreadAttributes() {
        if (ok_) {
            DeleteProcThreadAttributeList(get());
        }
    }
    ProcThreadAttributes(const ProcThreadAttributes&) = delete;
    ProcThreadAttributes& operator=(const ProcThreadAttributes&) = delete;

    bool ok() const { return ok_; }
    LPPROC_THREAD_ATTRIBUTE_LIST get() { return reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buf_.data()); }

private:
    std::vector<char> buf_;
    bool ok_ = false;
};

class ClipboardLock {
public:
    explicit ClipboardLock(HWND owner) : open_(OpenClipboard(owner) != FALSE) {}
    ~ClipboardLock() {
        if (open_) {
            CloseClipboard();
        }
    }
    ClipboardLock(const ClipboardLock&) = delete;
    ClipboardLock& operator=(const ClipboardLock&) = delete;

    bool ok() const { return open_; }

private:
    bool open_;
};

} // namespace ft::win
