#pragma once
// The companion's core: the film, the timeline's marks, the camera rig and its live preview, the shot list, and
// renders - everything both faces (the desktop window and the tablet page) use. Every action is one method, reached
// from QML directly and from the tablet page through call() (http_api.cpp), so the two always offer the same.
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <functional>
#include "rig.h"
#include "viewer_link.h"

struct Shot { double start = 0, duration = 4; QString label; Rig rig; };

class Core : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ stateMap NOTIFY changed)
public:
    using Done = std::function<void(const QJsonObject &result, const QString &error)>;
    explicit Core(QObject *parent = nullptr);

    // The tablet page's way in: one method by name with JSON params; done gets {result} or an error.
    void call(const QString &method, const QJsonObject &params, Done done);
    // The desktop window's way in: the same methods; the answer comes back through `answered` (or is ignored).
    Q_INVOKABLE void invoke(const QString &method, const QVariantMap &params = {}, const QString &tag = {});
    Q_INVOKABLE QStringList methods() const;

    QJsonObject state() const;
    QVariantMap stateMap() const { return state().toVariantMap(); }

signals:
    void changed();
    void answered(const QString &tag, const QVariantMap &result, const QString &error);
    void message(const QString &text);   // for the status line

private:
    void refreshTime();
    void loadMarks();
    void readPlanes(std::function<void()> then = {});
    void applyCameras();                  // the live rig, or the shot list, into the viewer
    QJsonObject shotsJson() const;
    QString videosFolder() const;
    bool planeOf(int id, Pose &p) const;

    ViewerLink m_link;
    QTimer m_poll;
    QString m_film, m_viewerPath, m_waiting;
    double m_length = 0, m_time = 0;
    bool m_playing = false, m_preview = true, m_rendering = false;
    int m_me = -1;
    QJsonArray m_planes, m_marks;
    Rig m_rig;
    QList<Shot> m_shots;
    QString m_lastRender;
};
