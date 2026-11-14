# Aces High film companion

Camera controls for the **Aces High film viewer**, for pilots who make films by hand - no AI needed. Open one of
your films, put cameras anywhere round your aircraft with sliders that move the picture live, line up your shots and
render the video with the film's sound.

It drives the viewer through its socket only - the same published commands an AI assistant uses - and adds nothing
to the game or the viewer.

## Only in the companion

- **A camera map.** See the moment from above: your path, everyone else's, the buildings around you, and the camera
  with the cone of what it sees. **Drag the camera** to move it, **drag the dot on its cone** to turn it, or
  double-click anywhere to hang a new camera there. You always know where your camera is and where it's pointing.
- **The live picture.** The film viewer itself shows the camera's picture as you drag, a frame later. No preview
  clips to wait for.
- **The viewer beside you.** The viewer parks itself against the companion and stays there, so the controls and the
  picture are side by side.

![The camera map: a camera hung beside his path, watching him fly towards it, and its picture in the viewer](docs/img/howto_0_map.png)

## Download

1. Download the zip from **[Releases](../../releases/latest)** (`aces-high-film-companion-<version>-win64.zip`).
2. Unzip it anywhere (e.g. your Desktop). There's nothing to install.
3. Run `film_companion.exe`.

You need Windows and Aces High with the new film viewer, the one with **Help > Socket Instructions for AI** in its
menu.

## How to use it

**1. Connect and open a film.** The companion finds the film viewer in the usual install folders (or press
**Browse…** and pick `ahfilm.exe` in your `Aces High III\ah32` folder), then press **Start it**. When the top line
turns green, press **Choose…** and open one of your films. The viewer parks itself beside the companion. Your kills,
hits and landings are marked along the timeline: drag the slider to the moment you want.

![The companion beside the film viewer](docs/img/howto_1_camera.png)

**2. Pick a camera and move it.** Press a camera - **Cockpit**, **Outside**, **Chase**, **Padlock** (keeps a bandit
in frame), **Hung here** (a camera left in the sky), **Ride his bomb** - or one of the ready angles (**Behind**,
**Left side**, **Ahead, looking back**...). Then fine-tune it with the sliders: distance, angle round him, height,
lens and roll. The viewer's picture follows as you drag. Or hang a camera on the **Map** tab: double-click where you
want it, drag it about, turn it by the dot on its cone, and choose **Watch him** or **Look where it points**.

![Moving the camera: ahead and to the side, looking back at him](docs/img/howto_2_angle.png)

**3. Add your shots.** When the picture looks right, press **Add this camera** - it adds a shot from that moment for
the number of seconds beside it. Move along the film and add the next. Shots show as bars on the timeline; nudge
their start and length, **Go** to look at one again, **Retake** it with the current camera, or delete it. **Save
the shots…** keeps the list to carry on later. Click one of your kills (red) on the timeline and choose **Add a kill
sequence** for three shots of it in one go.

![Three shots lined up: outside behind, the cockpit, then outside from the right](docs/img/howto_3_shots.png)

**4. Render.** Pick the size and frame rate and press **Render the shots**. The viewer records each shot with the
film's sound into one MP4 (in your Videos folder unless you choose another).

**[The manual](docs/MANUAL.md)** covers every control.

## Build

Qt 6.5+ (Core, Gui, Network, Qml, Quick) and CMake:

    cmake -S . -B build -DCMAKE_PREFIX_PATH=<Qt>/msvc2022_64
    cmake --build build --config Release

`QT=<Qt>/msvc2022_64 bash tools/package.sh` makes the release zip (needs Python's `markdown` package and 7-Zip).
GitHub Actions does the same on every push ([build.yml](.github/workflows/build.yml)); release exes come from there.
The version is set once, in CMakeLists.txt's `project()` line. **Keeping the docs current:** every change to a control, kind, slider or
behaviour updates docs/MANUAL.md (and its pictures, made with `--grab` / `tools/manual_shots.py`) in the same
commit; the README's pictures come from `tools/readme_shots.py`.

## Code signing policy

**Releases are not signed yet**, so Windows may warn when you first run `film_companion.exe`. Each release lists the
zip's SHA-256 so you can check your download is the published one. Code signing will be added when the project
qualifies for an open-source certificate.

- Every release exe is built from this repository by its GitHub Actions workflow
  ([.github/workflows/build.yml](.github/workflows/build.yml)); only those builds will be signed.
- Committers and reviewers: [steelington](https://github.com/steelington)
- Approvers (each signing approved by hand): [steelington](https://github.com/steelington)

## Privacy

The companion sends nothing anywhere. It talks only to the Aces High film viewer on your own PC (the viewer's local
socket). The tablet page is off unless the companion is started with `--tablet`; then it is served for a tablet on
your home network, and any request that doesn't come from the PC itself is refused without the pairing code shown
on the PC. It has no updates check, no analytics and no other network use.

## Licence

MIT - free to use, share and change; see [LICENSE](LICENSE).
