r"""Take the manual's window pictures (docs/img/*.png): the companion runs offscreen (nothing on screen but the film
viewer), started with --tablet only so this script can drive it through the local API, and saves a picture of each
scene. Retake them whenever the window changes (the manual stays current).

    python -I tools/manual_shots.py --viewer <ahfilmai.exe> --film <film.ahf> [--time 61] [--qt <Qt>/msvc2022_64]

The Release build needs Qt's DLLs: --qt (default: the Qt this PC builds with) puts its bin on the PATH.
"""
import argparse, json, os, subprocess, sys, time, urllib.request

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
EXE = os.path.join(ROOT, "build", "Release", "film_companion.exe")
IMG = os.path.join(ROOT, "docs", "img")
API = "http://127.0.0.1:8780/call"


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
    return False


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
    ap.add_argument("--qt", default=r"D:\DEV\aces_investigation\ah_film_studio\codex\.tooling\Qt\6.8.3\msvc2022_64")
    a = ap.parse_args()
    os.makedirs(IMG, exist_ok=True)
    scene("window", lambda: call("setRig", rig=dict(kind="orbit", angle=0, height=12, distance=90, zoom=60)), a)
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


if __name__ == "__main__":
    main()
