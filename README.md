# Aces High film companion

Camera controls for the **Aces High film viewer**, for pilots who make films by hand. It drives the viewer through its
socket only - the same published commands an AI uses - and adds nothing to the game or the viewer.

- **Desktop window:** open a film, scrub the timeline (your kills, hits and landings marked), pick a camera (cockpit,
  outside, chase, padlock on a bandit, a camera hung in the sky, riding your bomb), move it with sliders that update
  the picture live, add shots, render the video with the film's sound.

Status: first version, in testing. **[The manual](docs/MANUAL.md)** covers every control.

**Keeping the manual current:** every change to a control, kind, slider or behaviour updates docs/MANUAL.md (and its
picture, made with `--grab`) in the same commit.

## Build

Qt 6.5+ (Core, Gui, Network, Qml, Quick) and CMake:

    cmake -S . -B build -DCMAKE_PREFIX_PATH=<Qt>/msvc2022_64
    cmake --build build --config Release
