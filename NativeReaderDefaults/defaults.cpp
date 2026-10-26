#include "defaults.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

NativeReaderDefaults::NativeReaderDefaults(QObject *parent)
    : NativeReaderDefaults(QStringLiteral("/home/root/.local/share/remarkable/xochitl"), parent) {}

NativeReaderDefaults::NativeReaderDefaults(const QString &libraryPath, QObject *parent)
    : QObject(parent), m_libraryPath(libraryPath) {}

bool NativeReaderDefaults::needsContrastDefault(const QString &uuid, const QString &currentFilter) const {
    static const QRegularExpression validId(QStringLiteral(
        "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"));
    if (!currentFilter.isEmpty() || !validId.match(uuid).hasMatch()) return false;
    const QString path = QDir(m_libraryPath).filePath(uuid + QStringLiteral(".content"));
    const QFileInfo info(path);
    if (!info.isFile() || info.isSymLink() || info.size() > 16 * 1024 * 1024) return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return false;
    const auto content = document.object();
    const auto type = content.value(QStringLiteral("fileType")).toString();
    if (type != QStringLiteral("pdf") && type != QStringLiteral("epub")) return false;
    // Explicit empty string means Adaptive. Do not turn it into Off.
    return !content.contains(QStringLiteral("viewBackgroundFilter"));
}
