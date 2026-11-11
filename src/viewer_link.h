#pragma once
// The link to the Aces High film viewer's socket - the only part of the companion that talks to it.
// Text commands, one at a time: each reply is read up to its ".end" line, then the next command goes. The viewer
// listens on 127.0.0.1, port 48221 (or the next free one up to 48229) and greets a new client with
// "ahfilmclaude <protocol> port <port>".
#include <QObject>
#include <QQueue>
#include <QStringList>
#include <QTcpSocket>
#include <QTimer>
#include <functional>

class ViewerLink : public QObject {
    Q_OBJECT
public:
    using Reply = std::function<void(const QStringList &lines, bool error)>;
    explicit ViewerLink(QObject *parent = nullptr);

    bool connected() const { return m_ready; }
    int protocol() const { return m_protocol; }
    // Queue a command; done gets its reply lines (without ".end"); error is true for an "error:" reply or a lost link.
    void send(const QString &line, Reply done = {}, int timeoutMs = 60000);
    // Replace any queued commands that start with this prefix (a slider dragging: only the latest matters).
    void sendLatest(const QString &prefix, const QStringList &lines, Reply done = {});

public slots:
    void connectToViewer();

signals:
    void connectedChanged(bool connected);
    void waitingForUser(const QString &question);   // the viewer asked its user something (e.g. a terrain download)
    void log(const QString &line);

private:
    struct Job { QStringList lines; Reply done; int timeoutMs = 60000; QString tag; };
    void tryPort();
    void onReadyRead();
    void next();
    void finish(bool error);

    QTcpSocket m_sock;
    QTimer m_timeout, m_retry;
    int m_port = 48221, m_protocol = 0;
    bool m_ready = false, m_greeting = false, m_busy = false;
    QByteArray m_buf;
    QStringList m_lines;
    QQueue<Job> m_queue;
    Job m_job;
    int m_pending = 0;   // lines of the current job still to answer (a job may send several lines)
};
