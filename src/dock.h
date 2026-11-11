#pragma once
// Keeps the film viewer's window beside the companion: snug against its left edge on the same screen, the same top,
// following it when it moves or resizes, and owned by it so the two stay together in front of other windows and
// minimise together. Done from outside with Windows' own window calls; nothing touches the viewer itself.
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QWindow>

class Dock : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
public:
    explicit Dock(QObject *parent = nullptr);
    ~Dock() override;   // lets the viewer go, so closing the companion never hides it
    void setCompanion(QWindow *w);
    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);

signals:
    void enabledChanged();

private:
    void update();
    quintptr viewerWindow();   // the viewer's main window (found by its exe's process), 0 if it isn't running
    QPointer<QWindow> m_win;
    QTimer m_debounce, m_find;
    bool m_enabled = true;
    quintptr m_viewer = 0, m_owned = 0;
};
