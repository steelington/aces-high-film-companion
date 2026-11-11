#include "dock.h"
#include <QGuiApplication>
#include <QScreen>
#include <algorithm>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#endif

Dock::Dock(QObject *parent) : QObject(parent) {
    m_debounce.setSingleShot(true); m_debounce.setInterval(40);
    connect(&m_debounce, &QTimer::timeout, this, &Dock::update);
    m_find.setInterval(1000);   // the viewer may start (or restart) after the companion
    connect(&m_find, &QTimer::timeout, this, [this] { if (m_enabled && !m_viewer) update(); });
    m_find.start();
}

Dock::~Dock() {
#ifdef Q_OS_WIN
    if (m_viewer && IsWindow(HWND(m_viewer))) SetWindowLongPtrW(HWND(m_viewer), GWLP_HWNDPARENT, 0);
#endif
}

void Dock::setCompanion(QWindow *w) {
    m_win = w;
    if (!w) return;
    for (auto sig : {&QWindow::xChanged, &QWindow::yChanged, &QWindow::widthChanged, &QWindow::heightChanged})
        connect(w, sig, &m_debounce, qOverload<>(&QTimer::start));
    connect(w, &QWindow::visibilityChanged, &m_debounce, qOverload<>(&QTimer::start));
    m_debounce.start();
}

void Dock::setEnabled(bool on) {
    if (on == m_enabled) return;
    m_enabled = on;
#ifdef Q_OS_WIN
    if (!on && m_viewer && IsWindow(HWND(m_viewer))) SetWindowLongPtrW(HWND(m_viewer), GWLP_HWNDPARENT, 0);   // let it go
    m_owned = 0;
#endif
    emit enabledChanged();
    m_debounce.start();
}

quintptr Dock::viewerWindow() {
#ifdef Q_OS_WIN
    if (m_viewer && IsWindow(HWND(m_viewer))) return m_viewer;
    m_viewer = 0; m_owned = 0;
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe{sizeof(pe)};
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
        if (_wcsicmp(pe.szExeFile, L"ahfilmai.exe") == 0 || _wcsicmp(pe.szExeFile, L"ahfilm.exe") == 0) { pid = pe.th32ProcessID; break; }
    CloseHandle(snap);
    if (!pid) return 0;
    // Its main window: the one titled "Film Viewer ..." - whatever owns it now (us, or a companion that was closed
    // without letting it go, which leaves it hidden).
    struct Find { DWORD pid; HWND found; } f{pid, nullptr};
    EnumWindows([](HWND h, LPARAM l) -> BOOL {
        auto *f = reinterpret_cast<Find *>(l); DWORD p = 0; GetWindowThreadProcessId(h, &p);
        if (p != f->pid) return TRUE;
        wchar_t title[64]{}; GetWindowTextW(h, title, 63);
        if (wcsncmp(title, L"Film Viewer", 11) == 0) { f->found = h; return FALSE; }
        return TRUE;
    }, LPARAM(&f));
    m_viewer = quintptr(f.found);
    if (f.found && !IsWindowVisible(f.found)) ShowWindow(f.found, SW_SHOWNOACTIVATE);
#endif
    return m_viewer;
}

void Dock::update() {
#ifdef Q_OS_WIN
    if (!m_enabled || !m_win) return;
    HWND v = HWND(viewerWindow());
    if (!v) return;
    HWND me = HWND(m_win->winId());
    if (m_owned != quintptr(me)) { SetWindowLongPtrW(v, GWLP_HWNDPARENT, LONG_PTR(me)); m_owned = quintptr(me); }
    if (m_win->visibility() == QWindow::Minimized || m_win->visibility() == QWindow::Hidden) return;   // owned: it minimised with us
    // Our frame in screen pixels, and the work area of the screen we're on.
    RECT ours; GetWindowRect(me, &ours);
    MONITORINFO mi{sizeof(mi)}; GetMonitorInfoW(MonitorFromWindow(me, MONITOR_DEFAULTTONEAREST), &mi);
    const RECT work = mi.rcWork;
    int w = ours.left - work.left, h = ours.bottom - ours.top;
    if (w < 480) {   // no room on our left: sit below the top of the screen at a usable size instead, left-aligned
        w = std::max(640, int((work.right - work.left) / 2)); h = std::min(h, int(w * 9 / 16) + 40);
        SetWindowPos(v, nullptr, work.left, work.top, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
        return;
    }
    h = std::min(h, int(w * 9 / 16) + 40);   // a 16:9 picture plus its title bar, not taller than us
    SetWindowPos(v, nullptr, work.left, ours.top, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
#endif
}
