import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The camera map: the moment seen from above. His path (cream; the part still to come fainter), the other aircraft's
// (grey), each aircraft pointing the way it flies, buildings near him, and the hung camera with the cone it sees.
// Drag the camera to move it, drag the dot at the tip of its cone to turn it, double-click anywhere to hang a camera
// there. Drag empty ground to look about; − + and Fit for the scale (a trackball has no wheel).
// North (+z) is up and +x is to the right, as in the viewer.
Item {
    id: root
    required property var st          // core.state
    required property var theme
    signal changeRig(var changes)      // the window passes these to setRig

    readonly property var map: st.map || ({})
    readonly property var tracks: map.tracks || ({})
    readonly property var rig: st.rig || ({})
    readonly property bool hasCamera: rig.kind === "hung" && rig.hung === true
    property real scale: 0.05          // pixels per foot
    property real cx: 0                // the point at the centre (feet)
    property real cz: 0
    property bool follow: true         // keep him in the middle as the film moves

    // Where an aircraft is at film time t, from its path (straight between the half-second steps); null if not there.
    function at(path, t) {
        if (!path || path.length === 0 || t < path[0][0] - 0.6 || t > path[path.length - 1][0] + 0.6) return null
        for (let i = 1; i < path.length; ++i) if (path[i][0] >= t) {
            const a = path[i - 1], b = path[i], f = b[0] > a[0] ? Math.max(0, Math.min(1, (t - a[0]) / (b[0] - a[0]))) : 0
            return {x: a[1] + (b[1] - a[1]) * f, alt: a[2] + (b[2] - a[2]) * f, z: a[3] + (b[3] - a[3]) * f, yaw: f < 0.5 ? a[4] : b[4]}
        }
        const l = path[path.length - 1]; return {x: l[1], alt: l[2], z: l[3], yaw: l[4]}
    }
    function name(id) { const p = (st.planes || []).find(q => q.id === id); return p ? p.name : "" }
    function me() { return at(tracks[String(st.me)], st.time) }
    function toScreen(x, z) { return {x: canvas.width / 2 + (x - cx) * scale, y: canvas.height / 2 - (z - cz) * scale} }
    function toWorld(px, py) { return {x: cx + (px - canvas.width / 2) / scale, z: cz - (py - canvas.height / 2) / scale} }
    // Which way the camera looks on the map (radians, the viewer's yaw): set, or towards him.
    function cameraYaw() {
        if (rig.hungAim === "free") return rig.hungYaw * Math.PI / 180
        const m = me(); if (!m) return 0
        return Math.atan2(-(m.x - rig.hungX), m.z - rig.hungZ)
    }
    function coneLength() { return Math.min(canvas.width, canvas.height) * 0.28 }
    function handle() {   // the dot at the tip of the cone (screen)
        const c = toScreen(rig.hungX, rig.hungZ), y = cameraYaw(), l = coneLength()
        return {x: c.x - Math.sin(y) * l, y: c.y - Math.cos(y) * l}
    }
    // Scale (and centre) to take in his path and the camera: centred on him while following him, so he stays in the
    // middle as the film moves; otherwise on the middle of it all.
    function fit() {
        const p = tracks[String(st.me)] || []
        if (p.length === 0 || canvas.width < 50 || canvas.height < 50) return
        const pts = p.map(q => [q[1], q[3]]); if (hasCamera) pts.push([rig.hungX, rig.hungZ])
        const m = me()
        if (follow && m) { cx = m.x; cz = m.z }
        else { const xs = pts.map(q => q[0]), zs = pts.map(q => q[1]); cx = (Math.min(...xs) + Math.max(...xs)) / 2; cz = (Math.min(...zs) + Math.max(...zs)) / 2 }
        let dx = 100, dz = 100
        for (const q of pts) { dx = Math.max(dx, Math.abs(q[0] - cx)); dz = Math.max(dz, Math.abs(q[1] - cz)) }
        scale = Math.max(0.002, Math.min(1.5, 0.9 * Math.min(canvas.width / (2 * dx), canvas.height / (2 * dz))))
        canvas.requestPaint()
    }
    onMapChanged: { if (follow || cx === 0) fit(); canvas.requestPaint() }
    onRigChanged: canvas.requestPaint()
    onVisibleChanged: if (visible) Qt.callLater(() => { if (follow) fit(); canvas.requestPaint() })   // a tab opened: it has a size now
    Connections { target: root; function onStChanged() { if (root.follow && !drag.active) { const m = root.me(); if (m) { root.cx = m.x; root.cz = m.z } } canvas.requestPaint() } }

    ColumnLayout {
        anchors.fill: parent
        spacing: 4
        Canvas {
            id: canvas
            Layout.fillWidth: true; Layout.fillHeight: true
            // A map on a hidden tab has no size: fit again once it has one.
            onWidthChanged: { if (root.follow && width > 50) root.fit(); requestPaint() }
            onHeightChanged: { if (root.follow && height > 50) root.fit(); requestPaint() }
            onPaint: {
                const g = getContext("2d"), T = root.theme
                g.reset(); g.fillStyle = T.alternate; g.fillRect(0, 0, width, height)
                // Grid: a line every 1,000 ft, or 5,000 / 10,000 ft when zoomed out.
                const step = root.scale > 0.05 ? 1000 : root.scale > 0.01 ? 5000 : 20000
                g.strokeStyle = T.border; g.lineWidth = 1; g.beginPath()
                const w0 = root.toWorld(0, height), w1 = root.toWorld(width, 0)
                for (let x = Math.ceil(w0.x / step) * step; x < w1.x; x += step) { const s = root.toScreen(x, 0); g.moveTo(s.x, 0); g.lineTo(s.x, height) }
                for (let z = Math.ceil(w0.z / step) * step; z < w1.z; z += step) { const s = root.toScreen(0, z); g.moveTo(0, s.y); g.lineTo(width, s.y) }
                g.stroke()
                // Buildings (their size where the viewer gives it; turned by their yaw).
                g.fillStyle = "#3b4350"
                for (const o of (root.map.objects || [])) {
                    const s = root.toScreen(o[0], o[1]); const w = o[2] * root.scale, l = o[3] * root.scale
                    if (w < 1.5 && l < 1.5) { g.fillRect(s.x - 1, s.y - 1, 2, 2); continue }
                    g.save(); g.translate(s.x, s.y); g.rotate(-o[4]); g.fillRect(-w / 2, -l / 2, w, l); g.restore()
                }
                // Paths: the others grey, his cream; what's still to come at half strength.
                const drawPath = (path, colour, width_) => {
                    g.lineWidth = width_
                    for (const future of [false, true]) {
                        g.strokeStyle = colour; g.globalAlpha = future ? 0.35 : 1; g.beginPath(); let started = false
                        for (const q of path) {
                            if ((q[0] > st.time) !== future && !(future && Math.abs(q[0] - st.time) < 0.5)) continue
                            const s = root.toScreen(q[1], q[3]); if (!started) { g.moveTo(s.x, s.y); started = true } else g.lineTo(s.x, s.y)
                        }
                        g.stroke()
                    }
                    g.globalAlpha = 1
                }
                const plane = (p, colour, size, label) => {   // a triangle pointing the way it flies
                    const s = root.toScreen(p.x, p.z), y = p.yaw
                    g.save(); g.translate(s.x, s.y); g.rotate(-y)
                    g.fillStyle = colour; g.beginPath(); g.moveTo(0, -size); g.lineTo(size * 0.6, size * 0.7); g.lineTo(-size * 0.6, size * 0.7); g.closePath(); g.fill()
                    g.restore()
                    if (label) { g.fillStyle = colour; g.font = "11px 'Segoe UI'"; g.fillText(label, s.x + size + 3, s.y + 4) }
                }
                for (const id in root.tracks) if (+id !== st.me) {
                    drawPath(root.tracks[id], T.muted, 1)
                    const p = root.at(root.tracks[id], st.time); if (p) plane(p, T.muted, 6, root.name(+id))
                }
                const mine = root.tracks[String(st.me)]
                if (mine) drawPath(mine, T.cream, 2)
                // The hung camera: its cone (the lens's width), the dot to turn it by, and the camera itself.
                if (root.hasCamera) {
                    const c = root.toScreen(root.rig.hungX, root.rig.hungZ), y = root.cameraYaw(), half = (root.rig.zoom || 60) * Math.PI / 360, l = root.coneLength()
                    g.fillStyle = Qt.rgba(0.36, 0.55, 0.94, 0.18); g.strokeStyle = T.blue; g.lineWidth = 1.5
                    g.beginPath(); g.moveTo(c.x, c.y)
                    g.lineTo(c.x - Math.sin(y - half) * l, c.y - Math.cos(y - half) * l)
                    g.lineTo(c.x - Math.sin(y + half) * l, c.y - Math.cos(y + half) * l)
                    g.closePath(); g.fill(); g.stroke()
                    const h = root.handle()
                    g.fillStyle = T.blue; g.beginPath(); g.arc(h.x, h.y, 6, 0, 2 * Math.PI); g.fill()
                    g.fillStyle = T.cream; g.beginPath(); g.arc(c.x, c.y, 8, 0, 2 * Math.PI); g.fill()
                    g.fillStyle = T.blue; g.beginPath(); g.arc(c.x, c.y, 5, 0, 2 * Math.PI); g.fill()
                }
                // Him last, on top of everything, with his name.
                if (mine) { const p = root.me(); if (p) plane(p, T.red, 10, root.name(st.me) || "him") }
                // Scale bar.
                g.fillStyle = T.muted; g.font = "11px 'Segoe UI'"
                const bar = step * root.scale; g.fillRect(10, height - 14, bar, 2)
                g.fillText((step >= 1000 ? step / 1000 + ",000" : step) + " ft", 14 + bar, height - 9)
                if (!mine) { g.fillStyle = T.muted; g.font = "13px 'Segoe UI'"; g.fillText(st.film ? "Pause or pick a moment to see the map" : "Open a film to see the map", 14, 22) }
            }

            MouseArea {
                id: drag
                anchors.fill: parent
                property string what: ""      // camera, turn, pan
                property real lx: 0
                property real ly: 0
                readonly property bool active: pressed
                onPressed: (e) => {
                    lx = e.x; ly = e.y
                    if (root.hasCamera) {
                        const h = root.handle(), c = root.toScreen(root.rig.hungX, root.rig.hungZ)
                        if (Math.hypot(e.x - h.x, e.y - h.y) < 12) { what = "turn"; return }
                        if (Math.hypot(e.x - c.x, e.y - c.y) < 14) { what = "camera"; return }
                    }
                    what = "pan"; root.follow = false
                }
                onPositionChanged: (e) => {
                    if (what === "pan") { root.cx -= (e.x - lx) / root.scale; root.cz += (e.y - ly) / root.scale; lx = e.x; ly = e.y; canvas.requestPaint() }
                    else if (what === "camera") { const w = root.toWorld(e.x, e.y); root.changeRig({hungX: w.x, hungZ: w.z}) }
                    else if (what === "turn") {
                        const w = root.toWorld(e.x, e.y)
                        root.changeRig({hungAim: "free", hungYaw: Math.atan2(-(w.x - root.rig.hungX), w.z - root.rig.hungZ) * 180 / Math.PI})
                    }
                }
                onReleased: what = ""
                onDoubleClicked: (e) => {   // hang a camera here, at his height, watching him
                    const w = root.toWorld(e.x, e.y), m = root.me()
                    root.changeRig({kind: "hung", hung: true, hungX: w.x, hungZ: w.z, hungAlt: m ? m.alt : (root.rig.hungAlt || 1000), hungAim: "him"})
                }
                cursorShape: what === "pan" ? Qt.ClosedHandCursor : Qt.ArrowCursor
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Button { text: "−"; implicitWidth: 34; onClicked: { root.scale /= 1.6; canvas.requestPaint() } }
            Button { text: "+"; implicitWidth: 34; onClicked: { root.scale *= 1.6; canvas.requestPaint() } }
            Button { text: "Fit"; onClicked: { root.follow = true; root.fit() } }
            CheckBox { text: "Follow him"; checked: root.follow; onToggled: root.follow = checked }
            Item { Layout.fillWidth: true }
            Label { visible: !root.hasCamera; color: root.theme.muted; text: "Double-click the map to hang a camera there" }
        }
        // The hung camera's own settings: what it looks at, its height, and (free aim) its tilt.
        RowLayout {
            Layout.fillWidth: true; visible: root.hasCamera
            Button { text: "Watch him"; checkable: true; checked: root.rig.hungAim !== "free"; onClicked: root.changeRig({hungAim: "him"}) }
            Button { text: "Look where it points"; checkable: true; checked: root.rig.hungAim === "free"
                     onClicked: root.changeRig({hungAim: "free", hungYaw: root.cameraYaw() * 180 / Math.PI}) }
            Label { text: "Height" }
            SpinBox {
                from: 0; to: 45000; stepSize: 50; editable: true; value: Math.round(root.rig.hungAlt || 0)
                onValueModified: root.changeRig({hungAlt: value})
            }
            Label { text: "ft" }
        }
        RowLayout {
            Layout.fillWidth: true; visible: root.hasCamera && root.rig.hungAim === "free"
            Label { text: "Tilt"; Layout.preferredWidth: 40 }
            Slider { Layout.fillWidth: true; from: -89; to: 89; value: root.rig.hungPitch || 0; onMoved: root.changeRig({hungPitch: value}) }
            Label { text: Math.round(root.rig.hungPitch || 0) + "°"; Layout.preferredWidth: 40 }
        }
    }
}
