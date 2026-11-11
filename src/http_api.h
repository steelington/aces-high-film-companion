#pragma once
// The tablet page's way in: a small HTTP server on the home network.
//   GET  /         the control page (web/control.html)
//   POST /call     {"method": ..., "params": {...}} -> {"ok": true, "result": ...} or {"ok": false, "error": ...}
// Requests from this PC are trusted; from anywhere else they must carry the pairing code shown in the desktop window
// (header X-Code), so nobody else on the network can drive the viewer.
#include <QObject>
#include <QTcpServer>

class Core;

class HttpApi : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString code READ code CONSTANT)
    Q_PROPERTY(QString address READ address CONSTANT)
    Q_PROPERTY(bool enabled READ enabled CONSTANT)
public:
    HttpApi(Core *core, quint16 port, bool enabled, QObject *parent = nullptr);
    bool enabled() const { return m_enabled; }
    QString code() const { return m_code; }
    QString address() const { return m_address; }

private:
    void serve(QTcpSocket *s);
    Core *m_core;
    QTcpServer m_server;
    QString m_code, m_address;
    bool m_enabled = false;
};
