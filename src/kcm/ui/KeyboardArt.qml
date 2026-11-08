// SPDX-License-Identifier: GPL-3.0-or-later
// Line-art iLovbee B75, ported from the web studio (case, bezel, key well,
// badge and keycap grid in an 879 x 399 drawing), in Kirigami theme colours.
// Keys are clickable; remapped keys are marked.
//
// The caps light up with a live preview of the selected effect, ported from
// the web studio's previewFrame(): speed, brightness, palette slot and
// colourful mode all apply. Reactive effects (Flash Away, Ripples Shining,
// Reaction) answer real typing while the pointer is over the drawing, and
// play simulated presses otherwise.
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

Item {
    id: art

    required property var device
    signal keyClicked(var key)

    readonly property real k: width / 879
    readonly property color line: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.62)
    readonly property color soft: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.26)

    implicitWidth: Kirigami.Units.gridUnit * 44
    implicitHeight: width * 399 / 879

    // ===================================================== lighting preview

    readonly property var keys: device.keys
    readonly property var effect: device.effects.find(e => e.hw === device.effect) || ({ hw: 0 })
    readonly property bool reactive: [4, 7, 11].indexOf(device.effect) >= 0
    // Per-key glow, indexed like `keys`; written every frame.
    property var colors: []

    // Knob press / turn have no LED under them.
    function lightable(id) {
        return id < 82 || id > 84;
    }

    // Key centres in 0..1 drawing units, and the serpentine path Retro Snake
    // follows (rows left to right, then back).
    readonly property var nodes: keys.map(key => ({
        id: key.id,
        x: (key.x + key.w / 2) / 879,
        y: (key.y + key.h / 2) / 399,
        lit: lightable(key.id)
    }))
    readonly property var snakePath: {
        const rows = {};
        keys.forEach(key => {
            if (key.id >= 81 || key.knob)
                return;
            const r = Math.round(key.y);
            (rows[r] = rows[r] || []).push(key);
        });
        const path = [];
        Object.keys(rows).map(Number).sort((a, b) => a - b).forEach((r, i) => {
            const row = rows[r].sort((a, b) => a.x - b.x);
            (i % 2 ? row.reverse() : row).forEach(key => path.push(key.id));
        });
        return path;
    }

    // Animation state; plain JS so a frame does not trigger bindings.
    property var fx: ({ hw: -1 })

    function hsl(h, a) {
        return Qt.hsla((((h % 360) + 360) % 360) / 360, 1, 0.5, a);
    }

    // Stable pseudo-random hue per key (re-rolled per breathing cycle).
    function keyHue(id, salt) {
        let h = (id * 2654435761 + salt * 40503) >>> 0;
        h ^= h >>> 13;
        h = Math.imul(h, 0x5bd1e995) >>> 0;
        return (h % 3600) / 10;
    }

    function pressKey(i, now) {
        if (i < 0 || i >= nodes.length)
            return;
        const n = nodes[i];
        const hue = Math.random() * 360;
        fx.heat[i] = 1;
        fx.heatHue[i] = hue;
        fx.drops.push({ x: n.x, y: n.y, t0: now, hue: hue });
        if (fx.drops.length > 24)
            fx.drops.shift();
    }

    function pressId(id) {
        if (!reactive && device.effect !== 8)
            return;
        fx.lastReal = Date.now();
        pressKey(nodes.findIndex(n => n.id === id), Date.now());
    }

    function frame() {
        const now = Date.now();
        const hw = device.effect;
        if (fx.hw !== hw || fx.count !== nodes.length) {
            fx = { hw: hw, count: nodes.length, hold: ({}), drops: [], rain: [], heat: nodes.map(() => 0),
                   heatHue: nodes.map(() => 0), snake: 0, nextDrop: 0, nextDemo: 0, lastReal: 0, last: now };
        }
        const dt = Math.min(0.1, (now - fx.last) / 1000);
        fx.last = now;

        const st = { bright: device.brightness, speed: device.speed, slot: device.colorSlot, colorful: device.colorful };
        if (hw === 0 || hw === 19 || (effect.brightness && st.bright === 0)) {
            colors = nodes.map(() => "transparent");
            return;
        }
        const level = effect.brightness ? st.bright / 4 : 1;
        const rate = effect.speed ? 0.35 + st.speed * 0.4 : 0.8;
        const t = now / 1000 * rate;
        const slot = device.palette[st.slot];
        const fixed = effect.color && !st.colorful ? slot : null;
        const pick = (hue, a) => fixed ? Qt.rgba(fixed.r, fixed.g, fixed.b, a) : hsl(hue, a);

        // Reactive effects: simulated presses while nobody is typing.
        if (reactive && now - fx.lastReal > 2500 && now > fx.nextDemo) {
            const lit = nodes.map((n, i) => n.lit ? i : -1).filter(i => i >= 0);
            pressKey(lit[Math.floor(Math.random() * lit.length)], now);
            fx.nextDemo = now + 700 / rate;
        }
        // Stars Twinkle runs on its own.
        if (hw === 8 && now > fx.nextDrop) {
            pressKey(Math.floor(Math.random() * nodes.length), now);
            fx.nextDrop = now + 90 / rate;
        }
        // Raindrops: drops fall down a column with a short trail.
        if (hw === 5) {
            if (now > fx.nextDrop) {
                const src = nodes[Math.floor(Math.random() * nodes.length)];
                fx.rain.push({ x: src.x, y: 0.08, hue: Math.random() * 360 });
                fx.nextDrop = now + 260 / rate;
            }
            fx.rain.forEach(d => { d.y += dt * 0.55 * rate; });
            fx.rain = fx.rain.filter(d => d.y < 1.35);
        }
        fx.drops = fx.drops.filter(d => now - d.t0 < 3500 / rate);
        if (hw === 9)
            fx.snake = (fx.snake + dt * 14 * rate) % Math.max(1, snakePath.length);

        const breathCycle = Math.floor((t * 2.4) / (2 * Math.PI));
        const wiper = (t * 1.3) % (2 * Math.PI);
        const wiperHue = t * 45;

        colors = nodes.map((n, i) => {
            if (!n.lit)
                return "transparent";
            let a = 1;
            let c;
            const id = n.id;
            const dx = n.x - 0.5;
            const dy = (n.y - 0.55) * 0.46;
            const dist = Math.hypot(dx, dy);
            const ang = Math.atan2(dy, dx);
            switch (hw) {
            case 1: // Steady: one colour, or a random colour per key
                return fixed ? pick(0, level) : hsl(keyHue(id, 0), level);
            case 2: // Breathing: fresh random colours each breath
                a = 0.04 + 0.96 * (0.5 - 0.5 * Math.cos(t * 2.4));
                return fixed ? pick(0, a * level) : hsl(keyHue(id, breathCycle), a * level);
            case 3: // Rainbow
                return hsl(t * 70, level);
            case 4: { // Flash Away: the pressed key's row fills outward, then fades
                let best = 0;
                fx.drops.forEach(d => {
                    if (Math.abs(n.y - d.y) > 0.05)
                        return;
                    const age = (now - d.t0) / 1000 * rate;
                    const off = Math.abs(n.x - d.x);
                    if (off > age * 1.6)
                        return;
                    const filled = Math.max(d.x, 1 - d.x) / 1.6;
                    const v = age <= filled ? 1 : Math.max(0, 1 - (age - filled) / 0.7);
                    best = Math.max(best, v);
                });
                return fixed ? pick(0, best * level) : hsl(keyHue(id, 0), best * level);
            }
            case 5: { // Raindrops
                let best = 0;
                let hue = 0;
                fx.rain.forEach(d => {
                    if (Math.abs(n.x - d.x) > 0.03)
                        return;
                    const behind = d.y - n.y;
                    if (behind < -0.04 || behind > 0.3)
                        return;
                    const v = behind < 0 ? 1 + behind / 0.04 : 1 - behind / 0.3;
                    if (v > best) {
                        best = v;
                        hue = d.hue;
                    }
                });
                return pick(hue, best * level);
            }
            case 6: // Rainbow Wheel
                return hsl(ang * 180 / Math.PI + t * 120, level);
            case 7: { // Ripples Shining: a ring from each pressed key
                let best = 0;
                let hue = 0;
                fx.drops.forEach(d => {
                    const r = (now - d.t0) / 1000 * rate * 0.45;
                    const dd = Math.hypot(n.x - d.x, (n.y - d.y) * 0.46);
                    const v = Math.exp(-Math.pow((dd - r) / 0.035, 2)) * Math.max(0, 1 - r / 0.9);
                    if (v > best) {
                        best = v;
                        hue = d.hue;
                    }
                });
                return pick(hue, best * level);
            }
            case 8: // Stars Twinkle
                fx.heat[i] = Math.max(0, fx.heat[i] - dt * 1.4 * rate);
                return pick(fx.heatHue[i] || keyHue(id, 7), fx.heat[i] * level);
            case 9: { // Retro Snake
                const pos = snakePath.indexOf(id);
                const lag = (fx.snake - pos + snakePath.length) % snakePath.length;
                a = pos >= 0 && lag < 9 ? 1 - lag / 9 : 0.03;
                return pick(pos * 9, a * level);
            }
            case 10: // Neon Stream
                return hsl(n.x * 360 - t * 140, level);
            case 11: // Reaction: the pressed key lights and fades
                fx.heat[i] = Math.max(0, fx.heat[i] - dt * 0.9 * rate);
                return pick(fx.heatHue[i] || 0, fx.heat[i] * level);
            case 12: { // Sine Wave: the wave repaints keys it passes
                const yWave = 0.55 + 0.32 * Math.sin(n.x * Math.PI * 3 - t * 3);
                const onWave = Math.abs(n.y - yWave) < 0.09;
                if (fixed)
                    return pick(0, (onWave ? 1 : 0.3) * level);
                if (onWave)
                    fx.hold[id] = t * 55;
                return hsl(fx.hold[id] !== undefined ? fx.hold[id] : n.x * 300, level);
            }
            case 13: { // Rotating Windmill: one wiper repaints keys as it sweeps
                let diff = Math.abs(((ang + Math.PI * 2) % (Math.PI * 2)) - wiper);
                diff = Math.min(diff, Math.PI * 2 - diff);
                if (diff < 0.16)
                    fx.hold[id] = wiperHue;
                return hsl(fx.hold[id] !== undefined ? fx.hold[id] : ang * 180 / Math.PI, level);
            }
            case 14: // Colorful Waterfall
                return hsl(n.y * 260 - t * 120 + n.x * 30, level);
            case 15: // Blossoming
                a = 0.25 + 0.75 * (0.5 + 0.5 * Math.cos(dist * 18 - t * 3));
                return hsl(dist * 700 - t * 120, a * level);
            default:
                return pick(n.x * 300, level);
            }
        });
    }

    Timer {
        interval: 33
        repeat: true
        triggeredOnStart: true
        running: art.visible && art.nodes.length > 0
        onTriggered: art.frame()
    }

    // Typing while pointing at the drawing drives the reactive effects.
    HoverHandler {
        onHoveredChanged: if (hovered && (art.reactive || art.device.effect === 8)) art.forceActiveFocus()
    }
    Keys.onPressed: event => {
        const ids = {};
        ids[Qt.Key_Escape] = 1; ids[Qt.Key_Delete] = 14; ids[Qt.Key_Backspace] = 28; ids[Qt.Key_Home] = 29;
        ids[Qt.Key_Tab] = 30; ids[Qt.Key_Backtab] = 30; ids[Qt.Key_PageUp] = 44; ids[Qt.Key_CapsLock] = 45;
        ids[Qt.Key_Return] = 57; ids[Qt.Key_Enter] = 57; ids[Qt.Key_Shift] = 58; ids[Qt.Key_Up] = 70;
        ids[Qt.Key_Control] = 71; ids[Qt.Key_Meta] = 72; ids[Qt.Key_Super_L] = 72; ids[Qt.Key_Alt] = 73;
        ids[Qt.Key_Space] = 74; ids[Qt.Key_AltGr] = 75; ids[Qt.Key_Left] = 77; ids[Qt.Key_Down] = 78;
        ids[Qt.Key_Right] = 79; ids[Qt.Key_PageDown] = 80; ids[Qt.Key_End] = 81;
        let id = ids[event.key];
        if (id === undefined) {
            let label = "";
            if (event.key >= Qt.Key_F1 && event.key <= Qt.Key_F12)
                label = "F" + (event.key - Qt.Key_F1 + 1);
            else if (event.key >= Qt.Key_A && event.key <= Qt.Key_Z)
                label = String.fromCharCode(event.key);
            else if (event.text.length === 1)
                label = event.text.toUpperCase();
            const key = art.keys.find(k => k.label === label);
            id = key ? key.id : undefined;
        }
        if (id !== undefined) {
            art.pressId(id);
            event.accepted = true;
        }
    }

    // ================================================================= case

    Rectangle { // shell
        x: 2 * art.k
        y: 2 * art.k
        width: 875 * art.k
        height: 395 * art.k
        radius: 30 * art.k
        color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.03)
        border.color: art.line
        border.width: 1.6
    }
    Rectangle { // bezel
        x: 10 * art.k
        y: 9 * art.k
        width: 859 * art.k
        height: 381 * art.k
        radius: 24 * art.k
        color: "transparent"
        border.color: art.soft
    }
    Rectangle { // key well
        x: 27 * art.k
        y: 45 * art.k
        width: 826 * art.k
        height: 333 * art.k
        radius: 12 * art.k
        color: Qt.rgba(0, 0, 0, Kirigami.Theme.textColor.hslLightness > 0.5 ? 0.18 : 0.04)
        border.color: art.soft
    }
    Rectangle { // badge
        x: 34 * art.k
        y: 17 * art.k
        width: 66 * art.k
        height: 21 * art.k
        radius: 5 * art.k
        color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)
        border.color: art.line
        border.width: 1.2

        QQC2.Label {
            anchors.centerIn: parent
            text: "iLovbee"
            color: art.line
            font.bold: true
            font.pixelSize: Math.max(6, 10.5 * art.k)
        }
    }

    // ================================================================= keys

    Repeater {
        model: art.keys
        delegate: QQC2.AbstractButton {
            id: cap
            required property int index
            required property var modelData
            readonly property bool remapped: modelData.type !== "factory"
            readonly property bool knob: modelData.knob === true
            readonly property color glow: art.colors[index] || "transparent"
            readonly property bool glowing: glow.a > 0.02
            // Keycap face: a fixed skirt on the sides like a real cap, so long
            // keys (space, shift, enter) keep the same profile as a 1u key.
            readonly property real faceSide: knob ? width * 0.12 : 4.5 * art.k
            readonly property real faceTop: knob ? height * 0.12 : height * 0.07
            readonly property real faceBottom: knob ? height * 0.12 : height * 0.17

            x: modelData.x * art.k
            y: modelData.y * art.k
            width: modelData.w * art.k
            height: modelData.h * art.k
            hoverEnabled: true
            focusPolicy: Qt.NoFocus
            enabled: art.device.ready && !art.device.busy

            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            QQC2.ToolTip.text: remapped ? i18nc("@info:tooltip key → assignment", "%1 → %2", modelData.label, modelData.text)
                                        : i18nc("@info:tooltip", "%1 (click to remap)", modelData.label)

            onClicked: art.keyClicked(modelData)

            background: Item {
                // light bleeding out between the caps
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -3 * art.k - 1
                    radius: cap.knob ? width / 2 : 11 * art.k
                    visible: cap.glowing
                    color: cap.glow
                    opacity: 0.45
                }
                // cap skirt
                Rectangle {
                    anchors.fill: parent
                    radius: cap.knob ? width / 2 : 8 * art.k
                    color: cap.pressed ? Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.25)
                         : Kirigami.Theme.backgroundColor
                    border.width: cap.hovered ? 2 : 1.4
                    border.color: cap.hovered ? Kirigami.Theme.highlightColor
                                : cap.glowing ? Qt.tint(art.line, Qt.rgba(cap.glow.r, cap.glow.g, cap.glow.b, 0.75 * cap.glow.a))
                                : art.line
                    // inner glow
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: parent.border.width
                        radius: parent.radius
                        visible: cap.glowing
                        color: Qt.rgba(cap.glow.r, cap.glow.g, cap.glow.b, cap.glow.a * 0.22)
                    }
                }
                // keycap face
                Rectangle {
                    x: cap.faceSide
                    y: cap.faceTop
                    width: parent.width - cap.faceSide * 2
                    height: parent.height - cap.faceTop - cap.faceBottom
                    radius: cap.knob ? width / 2 : 6 * art.k
                    color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.04)
                    border.color: art.soft
                    border.width: 1
                }
                // remapped marker
                Rectangle {
                    visible: cap.remapped
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: Math.max(2, 4 * art.k)
                    width: Math.max(4, 7 * art.k)
                    height: width
                    radius: width / 2
                    color: Kirigami.Theme.highlightColor
                }
            }

            contentItem: Item {
                QQC2.Label {
                    x: cap.faceSide
                    y: cap.faceTop
                    width: cap.width - cap.faceSide * 2
                    height: cap.height - cap.faceTop - cap.faceBottom
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    font.pixelSize: Math.max(6, Math.min(12, 12.6 * art.k))
                    font.bold: cap.remapped
                    color: cap.remapped ? Kirigami.Theme.highlightColor
                         : cap.glowing ? Kirigami.Theme.textColor
                         : art.line
                    text: cap.remapped && cap.modelData.text.length <= 6 ? cap.modelData.text : cap.modelData.caption
                }
            }
        }
    }
}
