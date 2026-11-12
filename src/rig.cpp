#include "rig.h"
#include <QtMath>
#include <cmath>

namespace {
QString n(double v, int dp = 4) { return QString::number(v, 'f', dp); }

// An aircraft's axes from its yaw, pitch and roll (the viewer's convention): forward, right, up.
void axes(const Pose &p, double f[3], double r[3], double u[3]) {
    f[0] = -std::sin(p.yaw) * std::cos(p.pitch); f[1] = std::sin(p.pitch); f[2] = std::cos(p.yaw) * std::cos(p.pitch);
    const double r0[3] = {std::cos(p.yaw), 0, std::sin(p.yaw)};
    const double u0[3] = {f[1] * r0[2] - f[2] * r0[1], f[2] * r0[0] - f[0] * r0[2], f[0] * r0[1] - f[1] * r0[0]};
    const double c = std::cos(p.roll), s = std::sin(p.roll);
    for (int i = 0; i < 3; ++i) { r[i] = r0[i] * c - u0[i] * s; u[i] = u0[i] * c + r0[i] * s; }
}
}

QJsonObject Rig::toJson() const {
    return {{"kind", kind}, {"distance", distance}, {"angle", angle}, {"height", height}, {"zoom", zoom}, {"roll", roll},
            {"target", target}, {"hung", hung}, {"hungX", hungX}, {"hungAlt", hungAlt}, {"hungZ", hungZ}};
}

Rig Rig::fromJson(const QJsonObject &o) {
    Rig r;
    r.kind = o.value("kind").toString(r.kind); r.distance = o.value("distance").toDouble(r.distance);
    r.angle = o.value("angle").toDouble(r.angle); r.height = o.value("height").toDouble(r.height);
    r.zoom = o.value("zoom").toDouble(r.zoom); r.roll = o.value("roll").toDouble(r.roll);
    r.target = o.value("target").toInt(r.target); r.hung = o.value("hung").toBool(r.hung);
    r.hungX = o.value("hungX").toDouble(); r.hungAlt = o.value("hungAlt").toDouble(); r.hungZ = o.value("hungZ").toDouble();
    return r;
}

void Rig::offsets(double &right, double &up, double &ahead, double &yaw, double &pitch) const {
    const double a = qDegreesToRadians(angle), e = qDegreesToRadians(height), d = std::max(1.0, distance);
    right = d * std::cos(e) * std::sin(a);
    up = d * std::sin(e);
    ahead = -d * std::cos(e) * std::cos(a);
    // Looking back at him: the direction from the camera to him is minus the offset.
    const double fx = -right / d, fy = -up / d, fz = -ahead / d;
    yaw = std::atan2(-fx, fz);
    pitch = std::asin(std::clamp(fy, -1.0, 1.0));
}

void Rig::hangHere(const Pose &me) {
    double right, up, ahead, yaw, pitch;
    offsets(right, up, ahead, yaw, pitch);
    double f[3], r[3], u[3];
    axes(me, f, r, u);
    const double p[3] = {me.x, me.alt, me.z};
    double w[3];
    for (int i = 0; i < 3; ++i) w[i] = p[i] + r[i] * right + u[i] * up + f[i] * ahead;
    hungX = w[0]; hungAlt = w[1]; hungZ = w[2]; hung = true;
}

QString Rig::camLine(double start, double duration, int me, const Pose &mePose) const {
    const QString head = QString("cam %1 %2 ").arg(n(start, 3), n(std::max(0.1, duration), 3));
    const double fov = std::clamp(qDegreesToRadians(zoom), 0.09, 2.5);
    const double rollR = qDegreesToRadians(roll);
    double right, up, ahead, yaw, pitch;
    offsets(right, up, ahead, yaw, pitch);
    const QString place = QString("%1 %2 %3 %4 %5 %6 %7").arg(n(right, 2), n(up, 2), n(ahead, 2), n(yaw), n(pitch), n(rollR), n(fov));
    // The filmer is always "me" (the viewer's own word for him), so cameras work even before his id is known (he can
    // sit in the tower at the start of a film, where `planes` doesn't mark him).
    Q_UNUSED(me);
    const QString who = QStringLiteral("me");
    if (kind == "cockpit") return head + "recorded";
    if (kind == "seat") return head + QString("mount %1 0 0 0 0 0 %2 %3").arg(who, n(rollR), n(fov));
    if (kind == "weapon") return head + QString("weapon %1").arg(who);
    if (kind == "chase") return head + QString("chase %1 %2").arg(who, place);
    if (kind == "padlock") {
        if (target < 0) return {};
        return head + QString("mount %1 %2 at %3 outside").arg(who, place).arg(target);
    }
    if (kind == "hung") {
        if (!hung) return {};
        const double dx = mePose.x - hungX, dy = mePose.alt - hungAlt, dz = mePose.z - hungZ;
        const double y = std::atan2(-dx, dz), p = std::atan2(dy, std::hypot(dx, dz));
        return head + QString("fixed %1 %2 %3 %4 %5 %6 %7 at %8").arg(n(hungX, 1), n(hungAlt, 1), n(hungZ, 1), n(y), n(p), n(rollR), n(fov)).arg(who);
    }
    return head + QString("mount %1 %2 outside").arg(who, place);   // orbit
}
