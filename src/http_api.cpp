#include "http_api.h"
#include "core.h"
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QPointer>
#include <QRandomGenerator>
#include <QTcpSocket>

namespace {
void respond(QTcpSocket *s, int status, const QByteArray &type, const QByteArray &body) {
    const QByteArray reason = status == 200 ? "OK" : status == 403 ? "Forbidden" : status == 404 ? "Not Found" : "Bad Request";
    s->write("HTTP/1.1 " + QByteArray::number(status) + " " + reason + "\r\nContent-Type: " + type +
             "\r\nCache-Control: no-store\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
    s->disconnectFromHost();
}
void json(QTcpSocket *s, const QJsonObject &o, int status = 200) { respond(s, status, "application/json", QJsonDocument(o).toJson(QJsonDocument::Compact)); }
}

HttpApi::HttpApi(Core *core, quint16 port, bool enabled, QObject *parent) : QObject(parent), m_core(core), m_enabled(enabled) {
    // A pairing code without look-alike characters.
    const QString chars = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
    for (int i = 0; i < 6; ++i) m_code += chars[QRandomGenerator::global()->bounded(chars.size())];
    for (const QHostAddress &a : QNetworkInterface::allAddresses())
        if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback() && a.isPrivateUse()) { m_address = QString("http://%1:%2/").arg(a.toString()).arg(port); break; }
    if (m_address.isEmpty()) m_address = QString("http://127.0.0.1:%1/").arg(port);
    connect(&m_server, &QTcpServer::newConnection, this, [this] {
        while (QTcpSocket *s = m_server.nextPendingConnection()) {
            connect(s, &QTcpSocket::readyRead, this, [this, s] { serve(s); });
            connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
        }
    });
    if (m_enabled) m_server.listen(QHostAddress::Any, port);   // off: no server at all
}

void HttpApi::serve(QTcpSocket *s) {
    // Wait for the whole request: the headers, then Content-Length bytes of body.
    QByteArray buf = s->property("buf").toByteArray() + s->readAll();
    const int headEnd = buf.indexOf("\r\n\r\n");
    if (headEnd < 0) { s->setProperty("buf", buf); return; }
    const QList<QByteArray> head = buf.left(headEnd).split('\n');
    qsizetype length = 0; QByteArray code;
    for (const QByteArray &h : head) {
        const QByteArray l = h.trimmed().toLower();
        if (l.startsWith("content-length:")) length = l.mid(15).trimmed().toLongLong();
        if (l.startsWith("x-code:")) code = h.trimmed().mid(7).trimmed().toUpper();
    }
    if (buf.size() < headEnd + 4 + length) { s->setProperty("buf", buf); return; }
    s->setProperty("buf", QByteArray());
    const QList<QByteArray> request = head.value(0).trimmed().split(' ');
    const QByteArray verb = request.value(0), path = request.value(1).split('?').value(0);
    const QByteArray body = buf.mid(headEnd + 4, length);
    const bool local = s->peerAddress().isLoopback();

    if (verb == "GET" && (path == "/" || path == "/control")) {
        QFile f(":/qt/qml/Companion/web/control.html");
        f.open(QIODevice::ReadOnly);
        respond(s, 200, "text/html; charset=utf-8", f.readAll());
        return;
    }
    if (verb == "GET" && (path == "/icon.png" || path == "/favicon.ico")) {
        QFile f(":/qt/qml/Companion/assets/app_icon.png");
        f.open(QIODevice::ReadOnly);
        respond(s, 200, "image/png", f.readAll());
        return;
    }
    if (verb != "POST" || path != "/call") { respond(s, 404, "text/plain", "not here"); return; }
    if (!local && code != m_code.toUtf8()) { json(s, {{"ok", false}, {"error", "pairing code needed"}, {"pair", true}}, 403); return; }
    const QJsonObject req = QJsonDocument::fromJson(body).object();
    QPointer<QTcpSocket> sock(s);
    m_core->call(req["method"].toString(), req["params"].toObject(), [sock](const QJsonObject &r, const QString &e) {
        if (!sock) return;
        if (e.isEmpty()) json(sock, {{"ok", true}, {"result", r}});
        else json(sock, {{"ok", false}, {"error", e}});
    });
}
