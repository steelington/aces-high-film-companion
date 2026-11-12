r"""Take the README's how-to pictures (docs/img/howto_*.png): the companion ON SCREEN with the film viewer docked
beside it, as a player sees them. The companion is started with --tablet only so this script can drive it through
the local API; each scene is set up, then the two windows are captured together from the screen.

    python -I tools/readme_shots.py --viewer <ahfilm.exe> --film <film.ahf> [--time 61] [--exe <film_companion.exe>]

The film's path shows in the viewer's title bar: open it from a neutral place (e.g. a `subst` drive letter).
Windows only; both windows come up on the left-most screen and close again at the end. Retake when the window changes.
"""
import argparse, ctypes, json, os, subprocess, time, urllib.request
import ctypes.wintypes as wt
from PIL import Image, ImageGrab

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
IMG = os.path.join(ROOT, "docs", "img")
API = "http://127.0.0.1:8780/call"
u = ctypes.windll.user32
u.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))   # per-monitor v2: window and screen coordinates in real pixels


def call(method, **params):
    r = json.loads(urllib.request.urlopen(urllib.request.Request(API, json.dumps({"method": method, "params": params}).encode(),
                                                                 {"Content-Type": "application/json"}), timeout=300).read())
    if not r["ok"]: raise RuntimeError(f"{method}: {r['error']}")
    return r["result"]


def wait(cond, secs=60):
    t = time.time()
    while time.time() - t < secs:
        try:
            if cond(): return True
        except OSError:
            pass
        time.sleep(0.4)
    raise RuntimeError("timed out")


def windows_of(pid):
    hits = []
    def cb(h, _):
        p = wt.DWORD(); u.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value == pid and u.IsWindowVisible(h):
            r = wt.RECT(); u.GetWindowRect(h, ctypes.byref(r)); hits.append(((r.right - r.left) * (r.bottom - r.top), h))
        return 1
    u.EnumWindows(ctypes.WINFUNCTYPE(ctypes.c_int, wt.HWND, wt.LPARAM)(cb), 0)
    return [h for _, h in sorted(hits, reverse=True)]


def pid_of(exe):
    out = subprocess.run(["tasklist", "/FI", f"IMAGENAME eq {exe}", "/FO", "CSV", "/NH"], capture_output=True, text=True).stdout
    return int(out.split('","')[1]) if exe.lower() in out.lower() else None


def frame(h):
    """The window's visible frame (without the invisible resize border)."""
    r = wt.RECT(); ctypes.windll.dwmapi.DwmGetWindowAttribute(h, 9, ctypes.byref(r), ctypes.sizeof(r))
    return r.left, r.top, r.right, r.bottom


def left_work_area():
    found = []
    class MI(ctypes.Structure):
        _fields_ = [("cbSize", wt.DWORD), ("rcMonitor", wt.RECT), ("rcWork", wt.RECT), ("dwFlags", wt.DWORD)]
    def cb(h, dc, r, l):
        mi = MI(); mi.cbSize = ctypes.sizeof(MI); u.GetMonitorInfoW(h, ctypes.byref(mi))
        found.append((mi.rcWork.left, mi.rcWork.top, mi.rcWork.right, mi.rcWork.bottom)); return 1
    u.EnumDisplayMonitors(None, None, ctypes.WINFUNCTYPE(ctypes.c_int, wt.HMONITOR, wt.HDC, ctypes.POINTER(wt.RECT), wt.LPARAM)(cb), 0)
    return min(found)


def controls_bottom(im):
    """The row just below the last group box's bottom border (the Render box) in a picture of the companion. Below
    it is the --tablet pairing line (address and code), which must never be in a picture. A border is a row whose
    middle is one unbroken line a little lighter than the background."""
    g = im.convert("L"); w, h = g.size; px = g.load()
    for y in range(h - 1, h // 2, -1):
        row = [px[x, y] for x in range(w // 8, w * 7 // 8)]
        if min(row) > px[2, y] + 8 and max(row) - min(row) < 12: return y + 2
    raise RuntimeError("couldn't find the bottom of the companion's controls - not taking a picture with the pairing line")


def scale(h):
    return u.GetDpiForWindow(h) / 96


def grab(name, comp, viewer, width=1600):
    """Each window grabbed on its own and placed on a plain background, so nothing behind them gets in; the
    companion's foot (the pairing line) is cut off."""
    for h in (viewer, comp): u.SetForegroundWindow(h)
    time.sleep(1.5)
    c, v = frame(comp), frame(viewer)
    cim = ImageGrab.grab(bbox=c, all_screens=True)
    cim = cim.crop((0, 0, cim.width, controls_bottom(cim)))
    c = (c[0], c[1], c[2], c[1] + cim.height)
    x0, y0 = min(c[0], v[0]), min(c[1], v[1])
    im = Image.new("RGB", (max(c[2], v[2]) - x0, max(c[3], v[3]) - y0), (13, 17, 23))
    im.paste(ImageGrab.grab(bbox=v, all_screens=True), (v[0] - x0, v[1] - y0)); im.paste(cim, (c[0] - x0, c[1] - y0))
    if im.width > width: im = im.resize((width, round(im.height * width / im.width)))
    out = os.path.join(IMG, f"howto_{name}.png"); im.save(out, optimize=True)
    print(name, im.size, "->", out)


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--viewer", required=True); ap.add_argument("--film", required=True)
    ap.add_argument("--time", type=float, default=61.0)
    ap.add_argument("--exe", default=os.path.join(ROOT, "build", "dist", "film_companion.exe"))
    a = ap.parse_args()
    os.makedirs(IMG, exist_ok=True)
    p = subprocess.Popen([a.exe, "--tablet"], cwd=os.path.dirname(a.exe))
    vpid = None
    try:
        wait(lambda: call("status") is not None)
        wait(lambda: windows_of(p.pid))
        comp = windows_of(p.pid)[0]
        l, t, r, b = left_work_area(); cw = round(660 * scale(comp))   # the window's layout needs ~640 logical px
        u.ShowWindow(comp, 9); u.SetWindowPos(comp, 0, r - cw - 20, t, cw, b - t, 0x0004 | 0x0040)
        if not call("status")["connected"]:
            call("startViewer", path=a.viewer)
            wait(lambda: call("status")["connected"], 120)
        vpid = pid_of(os.path.basename(a.viewer))
        call("open", path=a.film)
        wait(lambda: call("status")["marks"], 60)
        time.sleep(2)
        wait(lambda: windows_of(vpid), 30)
        time.sleep(2)   # the dock parks the viewer beside the companion
        viewer = windows_of(vpid)[0]

        call("seek", time=a.time, rough=True); call("seek", time=a.time)
        call("setRig", rig=dict(kind="orbit", angle=0, height=12, distance=90, zoom=60))
        grab("1_camera", comp, viewer)

        call("setRig", rig=dict(kind="orbit", angle=150, height=6, distance=60, zoom=55))
        grab("2_angle", comp, viewer)

        t0 = a.time
        for dt, rig, n in ((-3, dict(kind="orbit", angle=0, height=12, distance=90, zoom=60), 4), (1, dict(kind="seat", zoom=70), 3),
                           (4, dict(kind="orbit", angle=90, height=5, distance=70, zoom=60), 4)):
            call("seek", time=t0 + dt); call("setRig", rig=rig); call("addShot", duration=n)
        grab("3_shots", comp, viewer)
        call("clearShots")
    finally:
        for h in windows_of(p.pid): u.PostMessageW(h, 0x0010, 0, 0)   # WM_CLOSE: the companion lets the viewer go
        try: p.wait(15)
        except subprocess.TimeoutExpired: p.kill()
        vpid = pid_of(os.path.basename(a.viewer)) or vpid
        if vpid: print("closing the viewer:", subprocess.run(["taskkill", "/PID", str(vpid), "/F"], capture_output=True, text=True).stdout.strip())


if __name__ == "__main__":
    main()
