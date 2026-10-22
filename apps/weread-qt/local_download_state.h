#pragma once

#include <QDir>
#include <QFileInfo>
#include <QVariantMap>

// fullFile is the last successfully packaged full EPUB. A new/failed repair
// changes downloadState and failedChapterCount but must not revoke that file.
inline QString completedLocalEpub(const QString &bookDirectory, const QVariantMap &status) {
    const QString directory = QFileInfo(bookDirectory).canonicalFilePath();
    if (directory.isEmpty()) return {};
    auto valid = [&directory](const QString &path) {
        const QFileInfo file(path);
        return file.isFile() && file.size() > 0 && file.fileName().endsWith(QStringLiteral("full.epub"))
            && file.canonicalFilePath().startsWith(directory + QLatin1Char('/'));
    };
    const QString completed = status.value(QStringLiteral("fullFile")).toString();
    if (!completed.isEmpty()) return valid(completed) ? completed : QString();
    // Older full records omitted the path. An arbitrary filename by itself
    // is insufficient; only an acknowledged full record may use this fallback.
    if (status.value(QStringLiteral("downloadState")).toString() != QStringLiteral("full")
        || status.value(QStringLiteral("failedChapterCount")).toInt() != 0) return {};
    const QDir dir(bookDirectory);
    const auto files = dir.entryInfoList({QStringLiteral("*full.epub")}, QDir::Files, QDir::Name);
    for (const auto &file : files) if (valid(file.absoluteFilePath())) return file.absoluteFilePath();
    return {};
}
