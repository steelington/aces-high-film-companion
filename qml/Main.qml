import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// The desktop face. Every button is one core method (core.invoke), the same ones the tablet page calls.
ApplicationWindow {
    id: win
    width: 620; height: 1180
    minimumWidth: 560; minimumHeight: 980
    visible: true
    title: st.film ? "Aces High film companion — " + st.film.split(/[\\\/]/).pop() : "Aces High film companion"
    // The studio's look: dark panels, olive highlight, blue accents, red for kills.
    readonly property QtObject theme: QtObject {
        readonly property color background: "#0f1419"
        readonly property color panel: "#1a1f26"
        readonly property color alternate: "#242b33"
        readonly property color border: "#2e3440"
        readonly property color olive: "#4a5a42"
        readonly property color cream: "#e5e9f0"
        readonly property color text: "#d8dee9"
        readonly property color muted: "#8b93a1"
        readonly property color green: "#10b981"
        readonly property color blue: "#5b8def"
        readonly property color red: "#e5484d"
        readonly property color amber: "#e0a030"
    }
    color: theme.background
    font.family: "Segoe UI"
    font.pixelSize: 14
    palette.window: theme.panel
    palette.base: theme.alternate
    palette.button: theme.alternate
    palette.text: theme.text
    palette.buttonText: theme.text
    palette.windowText: theme.text
    palette.placeholderText: theme.muted
    palette.highlight: theme.olive
    palette.highlightedText: theme.cream
    palette.mid: theme.border
    palette.light: theme.alternate
    palette.dark: theme.border

    readonly property var st: core.state
    readonly property var marks: st.marks || []
    readonly property int myKills: marks.filter(m => m.kind === "kill" && m.killer === st.me).length
    readonly property int myHits: marks.filter(m => m.kind === "impact" && m.shooter === st.me).length
    property string status: ""
    property bool dragging: false
    property var films: []

    function clock(t) { const m = Math.floor(t / 60), h = Math.floor(m / 60); const s = (t - m * 60).toFixed(1);
        return (h > 0 ? h + ":" + String(m % 60).padStart(2, "0") : m) + ":" + (s < 10 ? "0" : "") + s }
    function rig(changes) { const r = Object.assign({}, st.rig, changes); core.invoke("setRig", {rig: r}) }
    function openFilm(path) { if (path) core.invoke("open", {path: path}) }

    Connections {
        target: core
        function onMessage(text) { win.status = text }
        function onAnswered(tag, result, error) { if (tag === "films" && !error) win.films = result.films }
        function onChanged() { if (st.connected && win.films.length === 0 && !filmsAsked.running) { filmsAsked.start(); core.invoke("films", {}, "films") } }
    }
    Timer { id: filmsAsked; interval: 5000 }

    FileDialog {
        id: filmDialog
        title: "Choose a film"
        nameFilters: ["Aces High films (*.ahf)"]
        onAccepted: openFilm(decodeURIComponent(selectedFile.toString().replace(/^file:\/\/\//, "")).replace(/\//g, "\\"))
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        // ---- connection
        RowLayout {
            Layout.fillWidth: true
            Label { text: st.connected ? "● Connected to the film viewer" : "○ The film viewer isn't running"; color: st.connected ? theme.green : theme.amber; font.bold: true; Layout.fillWidth: true }
            Switch { text: "Keep the viewer beside me"; checked: dock.enabled; onToggled: dock.enabled = checked }
        }
        RowLayout {
            Layout.fillWidth: true; visible: !st.connected
            TextField { id: viewerPath; Layout.fillWidth: true; text: st.viewerPath; placeholderText: "…\\Aces High III\\ah32\\ahfilmai.exe" }
            Button { text: "Start it"; onClicked: core.invoke("startViewer", {path: viewerPath.text}) }
        }
        Label { visible: st.waiting !== ""; text: "The film viewer is asking you something - look at its window: " + st.waiting; wrapMode: Text.Wrap; color: theme.red; Layout.fillWidth: true }

        // ---- film and timeline
        GroupBox {
            title: st.film ? "Film: " + st.film.split(/[\\\/]/).pop() : "Film"; Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                RowLayout {
                    ComboBox {
                        id: filmList; Layout.fillWidth: true
                        model: win.films; displayText: currentIndex < 0 ? (win.films.length ? "Films in the viewer's folder…" : "No films in the viewer's films folder - use Choose…") : currentText
                        currentIndex: -1
                        onActivated: openFilm(win.films[currentIndex])
                    }
                    Button { text: "Choose…"; enabled: st.connected; onClicked: filmDialog.open() }
                }
                RowLayout {
                    Button { text: st.playing ? "Pause" : "Play"; enabled: st.film !== ""; onClicked: core.invoke(st.playing ? "pause" : "play") }
                    Label { text: clock(st.time) + " / " + clock(st.length) }
                    Item { Layout.fillWidth: true }
                    Label { text: myKills > 0 ? myKills + " kills" : myHits > 0 ? myHits + " runs of hits (no kill messages in this film)" : "" }
                }
                // The timeline: scrub with quick rough seeks, an exact seek where it's let go. Marks above it:
                // red your kills (grey others'), orange your hits, blue landings. Click one to go a few seconds before.
                Item {
                    Layout.fillWidth: true; height: 44
                    Repeater {
                        model: win.marks
                        Rectangle {
                            readonly property bool mine: modelData.killer === st.me || modelData.shooter === st.me
                            visible: modelData.kind === "kill" || mine || modelData.kind === "land"
                            x: st.length > 0 ? (modelData.t / st.length) * (parent.width - 2) : 0; y: 0
                            width: modelData.kind === "kill" ? 3 : 2; height: modelData.kind === "kill" ? 14 : 8
                            color: modelData.kind === "kill" ? (mine ? theme.red : theme.muted) : modelData.kind === "impact" ? theme.amber : theme.blue
                            ToolTip.visible: ma.containsMouse; ToolTip.text: clock(modelData.t) + "  " + modelData.text
                            MouseArea { id: ma; anchors.fill: parent; anchors.margins: -3; hoverEnabled: true; onClicked: core.invoke("seek", {time: Math.max(0, modelData.t - 6)}) }
                        }
                    }
                    Slider {
                        anchors.bottom: parent.bottom; width: parent.width
                        from: 0; to: Math.max(1, st.length); value: win.dragging ? value : st.time
                        onMoved: { win.dragging = true; core.invoke("seek", {time: value, rough: true}) }
                        onPressedChanged: if (!pressed) { win.dragging = false; core.invoke("seek", {time: value}) }
                    }
                }
            }
        }

        // ---- camera
        GroupBox {
            title: "Camera"; Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                Flow {
                    Layout.fillWidth: true; spacing: 6
                    Repeater {
                        model: [["cockpit", "Cockpit"], ["seat", "Cockpit, looking ahead"], ["orbit", "Outside"], ["chase", "Chase"],
                                ["padlock", "Padlock"], ["hung", "Hung here"], ["weapon", "Ride his bomb"]]
                        Button {
                            text: modelData[1]; checkable: true; checked: st.rig.kind === modelData[0]
                            palette.dark: theme.olive; palette.brightText: theme.cream   // Basic draws a checked button in "dark": the studio's olive
                            onClicked: modelData[0] === "hung" ? core.invoke("hangHere") : rig({kind: modelData[0]})
                        }
                    }
                }
                Flow {
                    Layout.fillWidth: true; spacing: 6
                    Repeater {
                        // Places round him: angle (0 behind, 90 his right), height, distance.
                        model: [["Behind", 0, 12, 90], ["Behind, high", 0, 35, 140], ["Left side", -90, 5, 70], ["Right side", 90, 5, 70],
                                ["Ahead, looking back", 180, 8, 90], ["Below", 0, -40, 80], ["Far, long lens", 20, 8, 600]]
                        Button { text: modelData[0]; onClicked: rig({angle: modelData[1], height: modelData[2], distance: modelData[3], zoom: modelData[3] > 300 ? 20 : st.rig.zoom}) }
                    }
                }
                RowLayout {
                    visible: st.rig.kind === "padlock"
                    Label { text: "Keep in frame:" }
                    ComboBox {
                        Layout.fillWidth: true
                        model: (st.planes || []).filter(p => !p.me)
                        textRole: "name"
                        onActivated: rig({target: model[currentIndex].id})
                    }
                }
                Repeater {
                    model: [["distance", "Distance", 20, 1500, " ft"], ["angle", "Angle round him", -180, 180, "°"], ["height", "Height", -80, 80, "°"],
                            ["zoom", "Lens (wide ↔ long)", 5, 140, "°"], ["roll", "Roll", -45, 45, "°"]]
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        Label { text: modelData[1]; Layout.preferredWidth: 150 }
                        Slider {
                            Layout.fillWidth: true; from: modelData[2]; to: modelData[3]
                            value: st.rig[modelData[0]]
                            onMoved: { const c = {}; c[modelData[0]] = value; rig(c) }
                        }
                        Label { text: Math.round(st.rig[modelData[0]]) + modelData[4]; Layout.preferredWidth: 60 }
                    }
                }
            }
        }

        // ---- shots (fills the rest of the window)
        GroupBox {
            title: "Shots (" + (st.shots || []).length + ")"; Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 190
            ColumnLayout {
                anchors.fill: parent
                RowLayout {
                    Button { text: "Add this camera from " + clock(st.time); enabled: st.film !== ""; onClicked: core.invoke("addShot", {duration: shotLen.value}) }
                    SpinBox { id: shotLen; from: 1; to: 60; value: 4 }
                    Label { text: "s" }
                    Item { Layout.fillWidth: true }
                    Switch { text: st.preview ? "Showing the live camera" : "Playing the shots"; checked: !st.preview; onToggled: core.invoke("preview", {on: !checked}) }
                }
                ListView {
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 4
                    model: st.shots || []
                    delegate: RowLayout {
                        width: ListView.view.width
                        Label { text: clock(modelData.start) + "  +" + modelData.duration.toFixed(1) + " s"; Layout.preferredWidth: 130 }
                        Label { text: modelData.label; Layout.fillWidth: true; elide: Text.ElideRight }
                        Button { text: "Go"; onClicked: core.invoke("seek", {time: modelData.start}) }
                        Button { text: "Use this camera"; onClicked: core.invoke("updateShot", {index: index, useCurrentRig: true}) }
                        Button { text: "✕"; onClicked: core.invoke("deleteShot", {index: index}) }
                    }
                    Label { anchors.centerIn: parent; visible: parent.count === 0; color: theme.muted; text: "Set up a camera, then \"Add this camera\" - one shot at a time, in film order." }
                }
            }
        }

        // ---- render
        GroupBox {
            title: "Render"; Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                RowLayout {
                    TextField { id: outPath; Layout.fillWidth: true; placeholderText: "Leave empty for " + (st.videosFolder || "your Videos folder") + "  (no spaces)" }
                    ComboBox { id: size; model: ["1280x720", "1920x1080", "3840x2160"]; currentIndex: 1 }
                    ComboBox { id: fps; model: ["30", "60"]; currentIndex: 1 }
                }
                RowLayout {
                    Button {
                        text: st.rendering ? "Rendering…" : "Render the shots"; enabled: !st.rendering && (st.shots || []).length > 0
                        onClicked: { const wh = size.currentText.split("x"); core.invoke("render", {path: outPath.text, width: +wh[0], height: +wh[1], fps: +fps.currentText}) }
                    }
                    Label { text: st.lastRender ? "Last: " + st.lastRender : ""; elide: Text.ElideLeft; Layout.fillWidth: true }
                }
            }
        }

        // ---- footer
        Label { text: "On a tablet: open " + api.address + " and enter the code " + api.code; color: theme.muted }
        Label { text: win.status; wrapMode: Text.Wrap; Layout.fillWidth: true; color: theme.muted }
    }
}
