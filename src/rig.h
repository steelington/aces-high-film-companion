#pragma once
// The camera rig: what the sliders mean, turned into the film viewer's `cam` lines.
//
// Kinds: cockpit (the recorded view), seat (his cockpit from the eye point), orbit (a camera riding his airframe,
// placed round him by distance / angle / height and looking at him), chase (the viewer's chase camera at that place),
// padlock (the orbit camera turned to keep another aircraft in frame), hung (a camera standing in the world where the
// orbit camera was, watching him), weapon (riding his next bomb, rocket or torpedo).
//
// Conventions are the viewer's (see its help): feet, radians; yaw 0 looks along +z and + turns left; pitch + looks up;
// a mount's offsets are from the pilot's eye point in the aircraft's axes; fov is the horizontal field of view.
#include <QJsonObject>
#include <QString>

struct Pose { double x = 0, alt = 0, z = 0, yaw = 0, pitch = 0, roll = 0; };   // an aircraft now (from `planes`)

struct Rig {
    QString kind = "orbit";
    double distance = 90;    // ft from him
    double angle = 0;        // degrees round him: 0 behind, 90 his right, 180 ahead, -90 his left
    double height = 15;      // degrees above (negative: below)
    double zoom = 60;        // the lens, degrees across
    double roll = 0;         // degrees
    int target = -1;         // padlock: the aircraft to keep in frame
    double hungX = 0, hungAlt = 0, hungZ = 0;   // hung: where the camera stands (set by hangHere, or dragged on the map)
    bool hung = false;
    QString hungAim = "him";                     // hung: "him" (turns to keep him in frame) or "free" (looks where it's set)
    double hungYaw = 0, hungPitch = 0;           // hung, free aim: degrees (yaw 0 looks along +z, + turns left; pitch + up)

    QJsonObject toJson() const;
    static Rig fromJson(const QJsonObject &o);
    // The camera's place round him, in his axes (right, up, ahead from the eye point), and the angles that look at him.
    void offsets(double &right, double &up, double &ahead, double &yaw, double &pitch) const;
    // Stand a camera in the world where the orbit camera is now (him at `me`).
    void hangHere(const Pose &me);
    // The cam line for this rig over [start, start + duration]; me = the filmer's id. Empty if it can't be made.
    QString camLine(double start, double duration, int me, const Pose &mePose) const;
};
