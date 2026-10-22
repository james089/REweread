#pragma once
#include "epub.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <stdexcept>

namespace WereadDownloader {
// Only the selected account's download directory/status is touched; shelf,
// credentials, cover thumbnails and other books are retained.
inline void clearDownloadCache(const QString &account, const QString &id) {
    auto require = [](bool ok) { if (!ok) throw std::runtime_error("cache_cleanup_failed"); };
    require(Epub::validBookId(id));
    const QFileInfo accountInfo(account), books(account + "/books"), target(account + "/books/" + id), status(account + "/book-status.json");
    require(accountInfo.isDir() && !accountInfo.isSymLink() && !books.isSymLink() && !target.isSymLink() && !status.isSymLink());
    QJsonObject rows;
    if (status.exists()) {
        require(status.isFile() && status.size() <= 8 * 1024 * 1024);
        QFile file(status.filePath()); require(file.open(QIODevice::ReadOnly));
        QJsonParseError error; const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
        require(error.error == QJsonParseError::NoError && doc.isObject()); rows = doc.object();
    }
    if (target.exists()) {
        require(target.isDir() && target.canonicalFilePath().startsWith(books.canonicalFilePath() + '/'));
        require(QDir(target.filePath()).removeRecursively());
    }
    auto row = rows.value(id).toObject();
    row["bookId"] = id; row["downloadState"] = "remote"; row["fullFile"] = ""; row["cachedFile"] = "";
    row["downloadProgress"] = 0; row["failedChapterCount"] = 0; row["downloadStage"] = ""; row["lastError"] = "";
    rows[id] = row;
    QSaveFile output(status.filePath()); output.setDirectWriteFallback(false);
    const auto bytes = QJsonDocument(rows).toJson(QJsonDocument::Compact);
    require(output.open(QIODevice::WriteOnly) && output.setPermissions(QFile::ReadOwner | QFile::WriteOwner)
        && output.write(bytes) == bytes.size() && output.commit());
}
}
