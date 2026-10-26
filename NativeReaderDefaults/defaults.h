#pragma once
#include <QObject>
#include <QString>

// Reads metadata only. All changes are made through the native controller.
class NativeReaderDefaults final : public QObject {
    Q_OBJECT
public:
    explicit NativeReaderDefaults(QObject *parent = nullptr);
    NativeReaderDefaults(const QString &libraryPath, QObject *parent);
    Q_INVOKABLE bool needsContrastDefault(const QString &uuid, const QString &currentFilter) const;
private:
    QString m_libraryPath;
};
