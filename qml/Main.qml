import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The desktop face. Every button is one core method (core.invoke), the same ones the tablet page calls.
ApplicationWindow {
    id: win
    width: 560; height: 980
    visible: true
    title: "Aces High film companion"

    readonly property var st: core.state
    property string status: ""
    property bool dragging: false

    function clock(t) { const m = Math.floor(t / 60), h = Math.floor(m / 60); const s = (t - m * 60).toFixed(1);
        return (h > 0 ? h + ":" + String(m % 60).padStart(2, "0") : m) + ":" + (s < 10 ? "0" : "") + s }
    function rig(changes) { const r = Object.assign({}, st.rig, changes); core.invoke("setRig", {rig: r}) }

    Connections { target: core; function onMessage(text) { win.status = text } }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width
            spacing: 10
            anchors.margins: 12

            // ---- connection
            Label { text: st.connected ? "Connected to the film viewer" : "The film viewer isn't running"; font.bold: true; Layout.leftMargin: 12; Layout.topMargin: 10 }
            RowLayout {
                Layout.leftMargin: 12; Layout.rightMargin: 12; visible: !st.connected
                TextField { id: viewerPath; Layout.fillWidth: true; text: st.viewerPath; placeholderText: "…\\Aces High III\\ah32\\ahfilmai.exe" }
                Button { text: "Start it"; onClicked: core.invoke("startViewer", {path: viewerPath.text}) }
            }
            Label { visible: st.waiting !== ""; text: "The film viewer is asking you something - look at its window: " + st.waiting; wrapMode: Text.Wrap; color: "#c33"; Layout.fillWidth: true; Layout.leftMargin: 12 }

            // ---- film and timeline
            GroupBox {
                title: "Film"; Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                ColumnLayout {
                    anchors.fill: parent
                    RowLayout {
                        TextField { id: filmPath; Layout.fillWidth: true; placeholderText: "the film (.ahf) - a full path, or one from the list"; text: st.film }
                        Button { text: "Open"; onClicked: core.invoke("open", {path: filmPath.text}) }
                    }
                    RowLayout {
                        Button { text: st.playing ? "Pause" : "Play"; enabled: st.film !== ""; onClicked: core.invoke(st.playing ? "pause" : "play") }
                        Label { text: clock(st.time) + " / " + clock(st.length) }
                        Item { Layout.fillWidth: true }
                        Label { text: (st.marks || []).filter(m => m.kind === "kill" && m.killer === st.me).length + " kills" }
                    }
                    // The timeline: scrub with quick rough seeks, an exact seek where it's let go. Marks above it.
                    Item {
                        Layout.fillWidth: true; height: 44
                        Repeater {
                            model: st.marks || []
                            Rectangle {
                                readonly property bool mine: modelData.killer === st.me || modelData.shooter === st.me
                                visible: modelData.kind === "kill" || mine || modelData.kind === "land"
                                x: st.length > 0 ? (modelData.t / st.length) * (parent.width - 2) : 0; y: 0
                                width: modelData.kind === "kill" ? 3 : 2; height: modelData.kind === "kill" ? 14 : 8
                                color: modelData.kind === "kill" ? (mine ? "#d22" : "#999") : modelData.kind === "impact" ? "#e90" : "#39c"
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
                title: "Camera"; Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                ColumnLayout {
                    anchors.fill: parent
                    Flow {
                        Layout.fillWidth: true; spacing: 6
                        Repeater {
                            model: [["cockpit", "Cockpit"], ["seat", "Cockpit, looking ahead"], ["orbit", "Outside"], ["chase", "Chase"],
                                    ["padlock", "Padlock"], ["hung", "Hung here"], ["weapon", "Ride his bomb"]]
                            Button {
                                text: modelData[1]; checkable: true; checked: st.rig.kind === modelData[0]
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
                    GridLayout {
                        columns: 3; Layout.fillWidth: true
                        Repeater {
                            model: [["distance", "Distance", 20, 1500, "ft"], ["angle", "Angle round him", -180, 180, "°"], ["height", "Height", -80, 80, "°"],
                                    ["zoom", "Lens (wide ↔ long)", 5, 140, "°"], ["roll", "Roll", -45, 45, "°"]]
                            delegate: RowLayout {
                                Layout.columnSpan: 3; Layout.fillWidth: true
                                Label { text: modelData[1]; Layout.preferredWidth: 140 }
                                Slider {
                                    Layout.fillWidth: true; from: modelData[2]; to: modelData[3]
                                    value: st.rig[modelData[0]]
                                    onMoved: { const c = {}; c[modelData[0]] = value; rig(c) }
                                }
                                Label { text: Math.round(st.rig[modelData[0]]) + modelData[4]; Layout.preferredWidth: 56 }
                            }
                        }
                    }
                }
            }

            // ---- shots
            GroupBox {
                title: "Shots (" + (st.shots || []).length + ")"; Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                ColumnLayout {
                    anchors.fill: parent
                    RowLayout {
                        Button { text: "Add this camera from " + clock(st.time); enabled: st.film !== ""; onClicked: core.invoke("addShot", {duration: shotLen.value}) }
                        SpinBox { id: shotLen; from: 1; to: 60; value: 4 }
                        Label { text: "s" }
                    }
                    Repeater {
                        model: st.shots || []
                        RowLayout {
                            Label { text: clock(modelData.start) + " +" + modelData.duration.toFixed(1) + " s"; Layout.preferredWidth: 120 }
                            Label { text: modelData.label; Layout.fillWidth: true; elide: Text.ElideRight }
                            Button { text: "Go"; onClicked: core.invoke("seek", {time: modelData.start}) }
                            Button { text: "Use this camera"; onClicked: core.invoke("updateShot", {index: index, useCurrentRig: true}) }
                            Button { text: "✕"; onClicked: core.invoke("deleteShot", {index: index}) }
                        }
                    }
                    RowLayout {
                        Switch { text: st.preview ? "Showing the live camera" : "Playing the shots"; checked: !st.preview; onToggled: core.invoke("preview", {on: !checked}) }
                    }
                }
            }

            // ---- render
            GroupBox {
                title: "Render"; Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12
                ColumnLayout {
                    anchors.fill: parent
                    RowLayout {
                        TextField { id: outPath; Layout.fillWidth: true; placeholderText: "C:\\Videos\\mykill.mp4 (no spaces)" }
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

            // ---- tablet
            Label { text: "On a tablet: open " + api.address + " and enter the code " + api.code; Layout.leftMargin: 12; color: "#666" }
            Label { text: win.status; wrapMode: Text.Wrap; Layout.fillWidth: true; Layout.leftMargin: 12; Layout.bottomMargin: 12; color: "#666" }
        }
    }
}
