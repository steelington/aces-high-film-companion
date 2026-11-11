#include "viewer_link.h"

ViewerLink::ViewerLink(QObject *parent) : QObject(parent) {
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] { emit log("timed out: " + m_job.lines.join(" | ")); finish(true); });
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &ViewerLink::tryPort);
    connect(&m_sock, &QTcpSocket::readyRead, this, &ViewerLink::onReadyRead);
    connect(&m_sock, &QTcpSocket::connected, this, [this] { m_greeting = true; m_buf.clear(); m_lines.clear(); });
    connect(&m_sock, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (m_ready) return;   // the disconnected handler deals with a lost link
        // Not this port: try the next, and round again after a pause (the viewer may still be starting).
        m_port = m_port >= 48229 ? 48221 : m_port + 1;
        m_retry.start(m_port == 48221 ? 2000 : 50);
    });
    connect(&m_sock, &QTcpSocket::disconnected, this, [this] {
        const bool was = m_ready;
        m_ready = false; m_greeting = false;
        if (m_busy) finish(true);
        while (!m_queue.isEmpty()) { Job j = m_queue.dequeue(); if (j.done) j.done({}, true); }
        if (was) { emit connectedChanged(false); emit log("the film viewer closed the link"); }
        m_retry.start(2000);
    });
}

void ViewerLink::connectToViewer() { if (!m_ready) tryPort(); }

void ViewerLink::tryPort() {
    if (m_ready || m_sock.state() != QAbstractSocket::UnconnectedState) return;
    m_sock.connectToHost(QStringLiteral("127.0.0.1"), quint16(m_port));
}

void ViewerLink::send(const QString &line, Reply done, int timeoutMs) {
    m_queue.enqueue(Job{{line}, std::move(done), timeoutMs, {}});
    next();
}

void ViewerLink::sendLatest(const QString &prefix, const QStringList &lines, Reply done) {
    for (int i = m_queue.size() - 1; i >= 0; --i)
        if (m_queue[i].tag == prefix) { if (m_queue[i].done) m_queue[i].done({}, true); m_queue.removeAt(i); }
    m_queue.enqueue(Job{lines, std::move(done), 30000, prefix});
    next();
}

void ViewerLink::next() {
    if (!m_ready || m_busy || m_queue.isEmpty()) return;
    m_job = m_queue.dequeue();
    m_busy = true; m_lines.clear(); m_pending = m_job.lines.size();
    QByteArray out;
    for (const QString &l : m_job.lines) out += l.toUtf8() + '\n';
    m_sock.write(out);
    m_timeout.start(m_job.timeoutMs);
}

void ViewerLink::finish(bool error) {
    m_timeout.stop();
    Job j = std::move(m_job); m_job = {};
    m_busy = false;
    const QStringList lines = m_lines; m_lines.clear();
    if (j.done) j.done(lines, error || (!lines.isEmpty() && lines.first().startsWith("error:")));
    next();
}

void ViewerLink::onReadyRead() {
    m_buf += m_sock.readAll();
    int nl;
    while ((nl = m_buf.indexOf('\n')) >= 0) {
        QString line = QString::fromUtf8(m_buf.left(nl)).trimmed();
        m_buf.remove(0, nl + 1);
        if (m_greeting) {   // the greeting, up to its .end
            if (line.startsWith("ahfilmclaude ")) m_protocol = line.section(' ', 1, 1).toInt();
            if (line == ".end") {
                m_greeting = false;
                if (m_protocol > 0) { m_ready = true; emit connectedChanged(true); emit log(QString("connected to the film viewer on port %1").arg(m_port)); next(); }
                else m_sock.disconnectFromHost();
            }
            continue;
        }
        if (line.startsWith("waiting for the user:")) { emit waitingForUser(line.mid(21).trimmed()); continue; }
        if (!m_busy) continue;
        if (line == ".end") {
            if (--m_pending <= 0) finish(false);
            continue;
        }
        m_lines << line;
    }
}
