#!/usr/bin/env bash
# Package the companion into build/dist: the exe, the Qt parts it uses, and the Microsoft C++ runtime DLLs beside it,
# so it runs on a player's PC with nothing installed. Run after a Release build:
#     QT=<Qt>/msvc2022_64 bash tools/package.sh
set -euo pipefail
cd "$(dirname "$0")/.."
QT=${QT:-/d/DEV/aces_investigation/ah_film_studio/codex/.tooling/Qt/6.8.3/msvc2022_64}
VSWHERE="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
VS=$("$VSWHERE" -latest -products '*' -property installationPath | tr -d '\r\n')

rm -rf build/dist && mkdir -p build/dist
cp build/Release/film_companion.exe build/dist/
"$QT/bin/windeployqt.exe" --release --qmldir qml --no-translations --no-opengl-sw --no-system-d3d-compiler \
    build/dist/film_companion.exe > build/windeployqt.log 2>&1 || { cat build/windeployqt.log; exit 1; }

# The runtime DLLs app-local (not the redistributable installer), from the newest MSVC redist folder.
CRT=$(ls -d "$(cygpath -u "$VS")"/VC/Redist/MSVC/*/x64/Microsoft.VC*.CRT | sort | tail -1)
cp "$CRT"/*.dll build/dist/
cp README.md build/dist/
# The manual as one HTML page with its pictures inside, built from docs/MANUAL.md.
python -I tools/build_manual.py
cp docs/MANUAL.html build/dist/

echo "build/dist: $(du -sh build/dist | cut -f1), runtime from $(basename "$CRT"):"
ls build/dist | grep -iE "vcruntime|msvcp"
