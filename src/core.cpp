#include "core.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

namespace {
QString n(double v, int dp = 3) { return QString::number(v, 'f', dp); }
}

Core::Core(QObject *parent) : QObject(parent) {
    QSettings s;
    m_viewerPath = s.value("viewerPath").toString();
    connect(&m_link, &ViewerLink::connectedChanged, this, [this](bool on) {
        emit message(on ? "Connected to the film viewer" : "The film viewer is not connected");
        if (on && !m_film.isEmpty()) { m_film.clear(); m_marks = {}; m_planes = {}; }   // a new viewer has no film open
        emit changed();
    });
    connect(&m_link, &ViewerLink::waitingForUser, this, [this](const QString &q) { m_waiting = q; emit message("The film viewer is asking: " + q); emit changed(); });
    connect(&m_link, &ViewerLink::log, this, &Core::message);
    m_poll.setInterval(250);
    connect(&m_poll, &QTimer::timeout, this, &Core::refreshTime);
    m_link.connectToViewer();
}

QStringList Core::methods() const {
    return {"status", "startViewer", "films", "open", "play", "pause", "seek", "planes", "setRig", "hangHere", "preview",
            "addShot", "updateShot", "deleteShot", "clearShots", "savePlan", "loadPlan", "saveCams", "render", "still"};
}

QJsonObject Core::state() const {
    return {{"connected", m_link.connected()}, {"protocol", m_link.protocol()}, {"film", m_film}, {"length", m_length},
            {"time", m_time}, {"playing", m_playing}, {"me", m_me}, {"preview", m_preview}, {"rendering", m_rendering},
            {"waiting", m_waiting}, {"viewerPath", m_viewerPath}, {"rig", m_rig.toJson()}, {"shots", shotsJson()["shots"]},
            {"marks", m_marks}, {"planes", m_planes}, {"lastRender", m_lastRender}, {"videosFolder", videosFolder()}};
}

// Where renders go by default: AcesHighFilms in the user's Videos folder, or C:\AcesHighFilms if that path has a
// space (the viewer's video command can't take one).
QString Core::videosFolder() const {
    const QString v = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    const QString dir = QDir::toNativeSeparators(QDir(v).filePath("AcesHighFilms"));
    return dir.contains(' ') ? QStringLiteral("C:\\AcesHighFilms") : dir;
}

QJsonObject Core::shotsJson() const {
    QJsonArray a;
    for (const Shot &s : m_shots) a.append(QJsonObject{{"start", s.start}, {"duration", s.duration}, {"label", s.label}, {"rig", s.rig.toJson()}});
    return {{"shots", a}};
}

bool Core::planeOf(int id, Pose &p) const {
    for (const QJsonValue &v : m_planes) {
        const QJsonObject o = v.toObject();
        if (o["id"].toInt() != id) continue;
        p = Pose{o["x"].toDouble(), o["alt"].toDouble(), o["z"].toDouble(), o["yaw"].toDouble(), o["pitch"].toDouble(), o["roll"].toDouble()};
        return true;
    }
    return false;
}

void Core::invoke(const QString &method, const QVariantMap &params, const QString &tag) {
    call(method, QJsonObject::fromVariantMap(params), [this, tag](const QJsonObject &r, const QString &e) {
        if (!e.isEmpty()) emit message(e);
        emit answered(tag, r.toVariantMap(), e);
    });
}

void Core::refreshTime() {
    if (!m_link.connected() || m_film.isEmpty()) return;
    m_link.sendLatest("time", {"time"}, [this](const QStringList &l, bool err) {
        if (err || l.isEmpty()) return;
        static const QRegularExpression re(R"(^time ([\d.]+) length ([\d.]+) paused ([01]))");
        const auto m = re.match(l.first());
        if (!m.hasMatch()) return;
        m_time = m.captured(1).toDouble(); m_length = m.captured(2).toDouble();
        const bool playing = m.captured(3) == "0";
        if (playing != m_playing) { m_playing = playing; if (!playing) m_poll.stop(); }
        emit changed();
    });
}

void Core::readPlanes(std::function<void()> then) {
    m_link.send("planes", [this, then](const QStringList &l, bool err) {
        if (!err) {
            static const QRegularExpression re(R"re(^plane (\d+) "([^"]*)" (\S+) (\S+) (\S+) (\S+) (\S+) (\S+)( recorder)?$)re");
            QJsonArray a;
            for (const QString &line : l) {
                const auto m = re.match(line);
                if (!m.hasMatch()) continue;
                const int id = m.captured(1).toInt();
                if (!m.captured(9).isEmpty()) m_me = id;
                a.append(QJsonObject{{"id", id}, {"name", m.captured(2)}, {"x", m.captured(3).toDouble()}, {"alt", m.captured(4).toDouble()},
                                     {"z", m.captured(5).toDouble()}, {"yaw", m.captured(6).toDouble()}, {"pitch", m.captured(7).toDouble()},
                                     {"roll", m.captured(8).toDouble()}, {"me", !m.captured(9).isEmpty()}});
            }
            m_planes = a;
            emit changed();
        }
        if (then) then();
    });
}

void Core::loadMarks() {
    // The timeline's marks: kills (the arena's kill messages the recorder got), landings, his drops, his hits on
    // others, and hits he took - read from the film without playing it.
    m_link.send("events kill,land,drop,impact,hit", [this](const QStringList &l, bool err) {
        if (err) return;
        static const QRegularExpression kill(R"re(^event ([\d.]+) kill "([^"]*)" "([^"]*)" (\S+) (\S+) (\S+))re");
        static const QRegularExpression land(R"re(^event ([\d.]+) land "([^"]*)" "([^"]*)")re");
        static const QRegularExpression drop(R"re(^event ([\d.]+) drop (\S+) (\S+) "([^"]*)")re");
        static const QRegularExpression impact(R"re(^event ([\d.]+) impact (\S+) (\S+) "([^"]*)" (\d+))re");
        static const QRegularExpression hit(R"re(^event ([\d.]+) hit (\S+) (\d+))re");
        QJsonArray a;
        for (const QString &line : l) {
            QRegularExpressionMatch m;
            if ((m = kill.match(line)).hasMatch())
                a.append(QJsonObject{{"t", m.captured(1).toDouble()}, {"kind", "kill"}, {"text", m.captured(2) + " shot down " + m.captured(3)},
                                     {"killer", m.captured(5).toInt()}, {"victim", m.captured(6).toInt()}, {"killerName", m.captured(2)}, {"victimName", m.captured(3)}});
            else if ((m = land.match(line)).hasMatch())
                a.append(QJsonObject{{"t", m.captured(1).toDouble()}, {"kind", "land"}, {"text", m.captured(2) + ": " + m.captured(3)}});
            else if ((m = drop.match(line)).hasMatch())
                a.append(QJsonObject{{"t", m.captured(1).toDouble()}, {"kind", "drop"}, {"text", m.captured(3) + " " + m.captured(4)}, {"who", m.captured(2).toInt()}});
            else if ((m = impact.match(line)).hasMatch())
                a.append(QJsonObject{{"t", m.captured(1).toDouble()}, {"kind", "impact"}, {"text", m.captured(5) + " hits with " + m.captured(4)},
                                     {"shooter", m.captured(2).toInt()}, {"target", m.captured(3).toInt()}});
            else if ((m = hit.match(line)).hasMatch())
                a.append(QJsonObject{{"t", m.captured(1).toDouble()}, {"kind", "hit"}, {"text", m.captured(3) + " hits taken"}});
        }
        m_marks = a;
        emit changed();
    }, 300000);
}

void Core::applyCameras() {
    if (!m_link.connected() || m_film.isEmpty()) return;
    Pose me;
    planeOf(m_me, me);
    QStringList lines{"cams clear"};
    if (m_preview) {
        const double start = std::max(0.0, m_time - 0.05);
        const QString l = m_rig.camLine(start, std::max(1.0, m_length - start + 1), m_me, me);
        if (!l.isEmpty()) lines << l;
    } else {
        for (const Shot &s : m_shots) {
            const QString l = s.rig.camLine(s.start, s.duration, m_me, me);
            if (!l.isEmpty()) lines << l;
        }
    }
    lines << "cams on";
    m_link.sendLatest("cams", lines);
}

void Core::call(const QString &method, const QJsonObject &p, Done done) {
    auto ok = [done](const QJsonObject &r = {}) { if (done) done(r, {}); };
    auto fail = [done](const QString &e) { if (done) done({}, e); };
    const bool needFilm = !QStringList{"status", "startViewer", "films", "open", "setRig", "addShot", "updateShot", "deleteShot",
                                       "clearShots", "loadPlan", "savePlan", "preview"}.contains(method);
    if (needFilm && m_film.isEmpty()) { fail("open a film first"); return; }
    if (method != "status" && method != "startViewer" && !m_link.connected()) { fail("the film viewer isn't connected: start it first"); return; }

    if (method == "status") { ok(state()); return; }
    if (method == "startViewer") {
        const QString path = p["path"].toString(m_viewerPath);
        if (!QFileInfo::exists(path)) { fail("no film viewer at " + path); return; }
        m_viewerPath = path; QSettings().setValue("viewerPath", path);
        if (!QProcess::startDetached(path, {}, QFileInfo(path).absolutePath())) { fail("couldn't start " + path); return; }
        m_link.connectToViewer();
        ok({{"started", path}});
        return;
    }
    if (method == "films") {
        m_link.send("films", [ok, fail](const QStringList &l, bool err) {
            if (err) { fail(l.value(0, "films failed")); return; }
            QJsonArray a;
            for (const QString &line : l) if (line.startsWith("film ")) a.append(line.section(' ', 1, -3));
            ok({{"films", a}, {"folder", l.value(0).section(' ', 1)}});
        });
        return;
    }
    if (method == "open") {
        const QString path = p["path"].toString();
        m_link.send("open " + path, [this, ok, fail, path](const QStringList &l, bool err) {
            if (err) { fail(l.value(0, "couldn't open it")); return; }
            static const QRegularExpression re(R"(^opened (.+) length ([\d.]+) s)");
            const auto m = re.match(l.value(0));
            m_film = path; m_length = m.hasMatch() ? m.captured(2).toDouble() : 0; m_time = 0; m_playing = false;
            m_me = -1; m_marks = {}; m_planes = {}; m_waiting.clear();
            emit changed();
            readPlanes(); loadMarks();
            ok({{"film", path}, {"length", m_length}});
        }, 180000);
        return;
    }
    if (method == "play" || method == "pause") {
        m_link.send(method, [this, method, ok, fail](const QStringList &l, bool err) {
            if (err) { fail(l.value(0)); return; }
            m_playing = method == "play";
            if (m_playing) m_poll.start(); else { m_poll.stop(); refreshTime(); readPlanes(); }
            emit changed(); ok();
        });
        return;
    }
    if (method == "seek") {
        const double t = std::clamp(p["time"].toDouble(), 0.0, m_length);
        const bool rough = p["rough"].toBool();
        // While scrubbing only the last position matters: a newer seek replaces one still waiting.
        m_link.sendLatest("seek", {QString("seek %1%2").arg(n(t)).arg(rough ? " rough" : "")}, [this, ok, fail, t](const QStringList &l, bool err) {
            if (err) { fail(l.value(0, "seek replaced")); return; }
            m_time = t; m_playing = false; m_poll.stop();
            readPlanes([this] { if (m_preview) applyCameras(); });
            emit changed(); ok({{"time", t}});
        });
        return;
    }
    if (method == "planes") { readPlanes([this, ok] { ok({{"planes", m_planes}, {"me", m_me}}); }); return; }
    if (method == "setRig") {
        m_rig = Rig::fromJson(p["rig"].toObject().isEmpty() ? p : p["rig"].toObject());
        m_preview = true;
        emit changed();
        applyCameras();
        ok({{"rig", m_rig.toJson()}});
        return;
    }
    if (method == "hangHere") {
        readPlanes([this, ok, fail] {
            Pose me;
            if (!planeOf(m_me, me)) { fail("he isn't in an aircraft at this moment"); return; }
            m_rig.hangHere(me); m_rig.kind = "hung"; m_preview = true;
            emit changed(); applyCameras(); ok({{"rig", m_rig.toJson()}});
        });
        return;
    }
    if (method == "preview") { m_preview = p["on"].toBool(true); emit changed(); applyCameras(); ok(); return; }
    if (method == "addShot") {
        Shot s; s.start = p.contains("start") ? p["start"].toDouble() : m_time; s.duration = p["duration"].toDouble(4);
        s.label = p["label"].toString(m_rig.kind); s.rig = m_rig;
        int at = 0; while (at < m_shots.size() && m_shots[at].start <= s.start) ++at;
        m_shots.insert(at, s);
        emit changed(); ok(shotsJson());
        return;
    }
    if (method == "updateShot" || method == "deleteShot") {
        const int i = p["index"].toInt(-1);
        if (i < 0 || i >= m_shots.size()) { fail("no such shot"); return; }
        if (method == "deleteShot") m_shots.removeAt(i);
        else {
            Shot &s = m_shots[i];
            if (p.contains("start")) s.start = p["start"].toDouble();
            if (p.contains("duration")) s.duration = std::max(0.1, p["duration"].toDouble());
            if (p.contains("label")) s.label = p["label"].toString();
            if (p.contains("rig")) s.rig = Rig::fromJson(p["rig"].toObject());
            if (p["useCurrentRig"].toBool()) s.rig = m_rig;
            std::sort(m_shots.begin(), m_shots.end(), [](const Shot &a, const Shot &b) { return a.start < b.start; });
        }
        if (!m_preview) applyCameras();
        emit changed(); ok(shotsJson());
        return;
    }
    if (method == "clearShots") { m_shots.clear(); if (!m_preview) applyCameras(); emit changed(); ok(); return; }
    if (method == "savePlan" || method == "loadPlan") {
        const QString path = p["path"].toString();
        if (path.isEmpty()) { fail("a file name is needed"); return; }
        QFile f(path);
        if (method == "savePlan") {
            if (!f.open(QIODevice::WriteOnly)) { fail("can't write " + path); return; }
            QJsonObject o = shotsJson(); o["film"] = m_film; f.write(QJsonDocument(o).toJson());
            ok({{"saved", path}});
        } else {
            if (!f.open(QIODevice::ReadOnly)) { fail("can't read " + path); return; }
            m_shots.clear();
            for (const QJsonValue &v : QJsonDocument::fromJson(f.readAll()).object()["shots"].toArray()) {
                const QJsonObject o = v.toObject();
                m_shots << Shot{o["start"].toDouble(), o["duration"].toDouble(4), o["label"].toString(), Rig::fromJson(o["rig"].toObject())};
            }
            if (!m_preview) applyCameras();
            emit changed(); ok(shotsJson());
        }
        return;
    }
    if (method == "saveCams") {   // the viewer's own Shots file of the plan
        const QString path = p["path"].toString();
        m_preview = false; applyCameras();
        m_link.send("cams save " + path, [ok, fail](const QStringList &l, bool err) { if (err) fail(l.value(0)); else ok({{"reply", l.value(0)}}); });
        emit changed();
        return;
    }
    if (method == "render") {
        const double a = p["start"].toDouble(m_shots.isEmpty() ? m_time : m_shots.first().start);
        const double b = p["end"].toDouble(m_shots.isEmpty() ? a + 10 : m_shots.last().start + m_shots.last().duration);
        // No path, or just a folder: name the file from the film and the time, in the videos folder.
        QString path = QDir::toNativeSeparators(p["path"].toString().trimmed());
        if (path.isEmpty() || QFileInfo(path).isDir()) {
            const QString dir = path.isEmpty() ? videosFolder() : path;
            QString name = QFileInfo(m_film).completeBaseName() + QString("_%1").arg(int(a));
            name.replace(QRegularExpression("[^A-Za-z0-9_.-]+"), "_");
            path = QDir::toNativeSeparators(QDir(dir).filePath(name + ".mp4"));
        }
        if (!path.endsWith(".mp4", Qt::CaseInsensitive)) path += ".mp4";
        if (path.contains(' ')) { fail("the video's path can't contain spaces (the film viewer's rule): " + path); return; }
        QDir().mkpath(QFileInfo(path).absolutePath());
        const int w = p["width"].toInt(1920), h = p["height"].toInt(1080), fps = p["fps"].toInt(60);
        m_preview = false; applyCameras();
        m_rendering = true; emit changed();
        m_link.send(QString("seek %1").arg(n(std::max(0.0, a - 0.5))));
        m_link.send(QString("video %1 %2 %3 %4 %5 %6").arg(n(a), n(b), path).arg(w).arg(h).arg(fps),
                    [this, ok, fail, path](const QStringList &l, bool err) {
                        m_rendering = false;
                        if (err) { emit changed(); fail(l.value(0, "the render failed")); return; }
                        m_lastRender = path; emit changed(); emit message("Rendered " + path);
                        ok({{"file", path}, {"reply", l.value(0)}});
                    }, 6 * 3600 * 1000);
        return;
    }
    if (method == "still") {
        const QString path = QDir::toNativeSeparators(p["path"].toString());
        m_link.send("shot " + path, [ok, fail](const QStringList &l, bool err) { if (err) fail(l.value(0)); else ok({{"file", l.value(0).mid(6)}}); });
        return;
    }
    fail("unknown method " + method);
}
