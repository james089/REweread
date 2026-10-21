#pragma once

#include <QObject>

class AppControl : public QObject {
    Q_OBJECT

public:
    explicit AppControl(QObject *parent = nullptr);

    Q_INVOKABLE void quitToSystem();
    Q_INVOKABLE void finishQuitToSystem();
    Q_INVOKABLE bool saveSelfTestScreen();
    Q_INVOKABLE bool injectSelfTestSwipe(int x1, int y1, int x2, int y2);
    // Deliberately unavailable outside the isolated reader-ink device test.
    // It returns only a digest, never pixels or user content.
    Q_INVOKABLE QString readerInkSelfTestDigest(int x, int y, int width, int height) const;
signals:
    void quitRequested();

private:
    bool m_quitting = false;
};
