r"""Take the manual's window pictures (docs/img/*.png): the companion runs offscreen (nothing on screen but the film
viewer), started with --tablet only so this script can drive it through the local API, and saves a picture of each
scene. Retake them whenever the window changes (the manual stays current).

    python -I tools/manual_shots.py --viewer <ahfilmai.exe> --film <film.ahf> [--time 61] [--qt <Qt>/msvc2022_64]

The Release build needs Qt's DLLs: --qt (or the QT environment variable) puts its bin on the PATH.
"""
import argparse, ctypes, ctypes.wintypes as wt, math, json, os, subprocess, sys, time, urllib.request

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
EXE = os.path.join(ROOT, "build", "Release", "film_companion.exe")
IMG = os.path.join(ROOT, "docs", "img")
API = "http://127.0.0.1:8780/call"


def call(method, **params):
    r = json.loads(urllib.request.urlopen(urllib.request.Request(API, json.dumps({"method": method, "params": params}).encode(),
                                                                 {"Content-Type": "application/json"}), timeout=300).read())
    if not r["ok"]: raise RuntimeError(f"{method}: {r['error']}")
    return r["result"]



def hang_ahead(ahead=800, right=300, below=60, zoom=20):
    """The map's scene: a camera hung beside his path a little ahead of him, watching him fly towards it."""
    st = call("status"); me = next(q for q in st["planes"] if q["id"] == st["me"])
    y = me["yaw"]; fx, fz = -math.sin(y), math.cos(y); rx, rz = math.cos(y), math.sin(y)
    call("setRig", rig=dict(st["rig"], kind="hung", hung=True, hungX=me["x"] + fx * ahead + rx * right, hungZ=me["z"] + fz * ahead + rz * right,
                            hungAlt=me["alt"] - below, hungAim="him", zoom=zoom))

def wait(cond, secs=60):
    t = time.time()
    while time.time() - t < secs:
        try:
            if cond(): return True
        except OSError:
            pass
        time.sleep(0.4)
    return False




def viewer_pids(exe):
    out = subprocess.run(["tasklist", "/fo", "csv", "/nh", "/fi", f"imagename eq {os.path.basename(exe)}"], capture_output=True, text=True).stdout
    return {int(x.split('","')[1]) for x in out.splitlines() if x.startswith('"')}

def viewer_to_left_screen(exe, timeout=30):
    """The viewer is a real window even when the companion is off-screen: put it on the left-most screen's work area
    (Steely's spare screen), never across the main ones."""
    u = ctypes.windll.user32; u.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
    areas = []
    class MI(ctypes.Structure):
        _fields_ = [("cbSize", wt.DWORD), ("rcMonitor", wt.RECT), ("rcWork", wt.RECT), ("dwFlags", wt.DWORD)]
    def mon(h, dc, r, l):
        mi = MI(); mi.cbSize = ctypes.sizeof(MI); u.GetMonitorInfoW(h, ctypes.byref(mi))
        areas.append((mi.rcWork.left, mi.rcWork.top, mi.rcWork.right, mi.rcWork.bottom)); return 1
    u.EnumDisplayMonitors(None, None, ctypes.WINFUNCTYPE(ctypes.c_int, wt.HMONITOR, wt.HDC, ctypes.POINTER(wt.RECT), wt.LPARAM)(mon), 0)
    l, t, r, b = min(areas)
    out = subprocess.run(["tasklist", "/fo", "csv", "/nh", "/fi", f"imagename eq {os.path.basename(exe)}"], capture_output=True, text=True).stdout
    pids = [int(x.split('","')[1]) for x in out.splitlines() if x.startswith('"')]
    end = time.time() + timeout
    while time.time() < end:
        hits = []
        def cb(h, _):
            p = wt.DWORD(); u.GetWindowThreadProcessId(h, ctypes.byref(p))
            if p.value in pids and u.IsWindowVisible(h) and u.GetWindowTextLengthW(h) > 0: hits.append(h)
            return True
        u.EnumWindows(ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)(cb), 0)
        if hits:
            w = min(r - l, 1280); h = min(b - t, 800)
            for hw in hits: u.ShowWindow(hw, 9); u.SetWindowPos(hw, 0, l, t, w, h, 0x0004 | 0x0040)
            return
        time.sleep(0.5)

def scene(name, setup, a):
    """One picture: a fresh companion offscreen, the scene set up through the API, then its grab."""
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_QPA_FONTDIR=r"C:\Windows\Fonts")
    env["PATH"] = os.path.join(a.qt, "bin") + os.pathsep + env.get("PATH", "")
    out = os.path.join(IMG, name + ".png")
    p = subprocess.Popen([EXE, "--tablet", "--grab", out, "--grab-after", "40"], cwd=os.path.dirname(EXE), env=env)
    try:
        wait(lambda: call("status") is not None)
        if not call("status")["connected"]:
            call("startViewer", path=a.viewer)
            wait(lambda: call("status")["connected"], 120)
        viewer_to_left_screen(a.viewer)
        call("open", path=a.film)
        wait(lambda: call("status")["marks"] or True, 20)
        time.sleep(2)
        call("seek", time=a.time)
        setup()
        p.wait(120)
    finally:
        if p.poll() is None: p.kill()
    print(name, "->", out)


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--viewer", required=True); ap.add_argument("--film", required=True)
    ap.add_argument("--time", type=float, default=61.0)
    ap.add_argument("--qt", default=os.environ.get("QT"), required=not os.environ.get("QT"), help="Qt msvc2022_64 folder (or set QT)")
    a = ap.parse_args()
    os.makedirs(IMG, exist_ok=True)
    before = viewer_pids(a.viewer)   # viewers already running (the user's): never closed
    scene("window", lambda: call("setRig", rig=dict(kind="orbit", angle=0, height=12, distance=90, zoom=60)), a)
    def cmap():
        wait(lambda: (call("status").get("map") or {}).get("tracks"), 60); time.sleep(1); hang_ahead()
    scene("map", cmap, a)
    def padlock():
        others = [x for x in call("status")["planes"] if not x["me"]]
        call("setRig", rig=dict(kind="padlock", angle=70, height=5, distance=24, zoom=50, target=others[0]["id"] if others else -1))
    scene("padlock", padlock, a)
    def shots():
        t = a.time
        for dt, rig, n in ((-3, dict(kind="orbit", angle=0, height=12, distance=90, zoom=60), 4), (1, dict(kind="seat", zoom=70), 3),
                           (4, dict(kind="orbit", angle=90, height=5, distance=70, zoom=60), 4)):
            call("seek", time=t + dt); call("setRig", rig=rig); call("addShot", duration=n)
        call("seek", time=t)
    scene("shots", shots, a)
    # Close the viewer the scenes started (it would otherwise stay on screen after the pictures) - only that one.
    for pid in viewer_pids(a.viewer) - before:
        print(subprocess.run(["taskkill", "/PID", str(pid), "/F"], capture_output=True, text=True).stdout.strip())


if __name__ == "__main__":
    main()
