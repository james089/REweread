#pragma once

#include <QObject>

class AppControl : public QObject {
    Q_OBJECT

public:
    explicit AppControl(QObject *parent = nullptr);

    Q_INVOKABLE void quitToSystem();
    Q_INVOKABLE bool saveSelfTestScreen();
    // Deliberately unavailable outside the isolated reader-ink device test.
    // It returns only a digest, never pixels or user content.
    Q_INVOKABLE QString readerInkSelfTestDigest(int x, int y, int width, int height) const;
};
