#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QString>

// Session files are produced by a Lua helper which can retain opaque cookie
// bytes. Qt correctly rejects invalid UTF-8 JSON, but the account identity we
// need is an ASCII decimal wr_vid. This helper only retries that one parse
// error in memory; callers must never write the re-encoded bytes back.
namespace SessionJsonReader {
struct Result {
    QJsonDocument document;
    bool usedLatin1Fallback = false;
};

inline Result parse(const QByteArray &bytes) {
    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (document.isObject() || error.error != QJsonParseError::IllegalUTF8String) {
        return {document, false};
    }
    // Latin-1 maps every original byte one-to-one before UTF-8 encoding. This
    // makes an unrelated opaque cookie value parseable without changing the
    // source file or relaxing syntax errors.
    return {QJsonDocument::fromJson(QString::fromLatin1(bytes).toUtf8()), true};
}

inline bool isAsciiDecimalIdentifier(const QString &value) {
    if (value.isEmpty()) {
        return false;
    }
    for (const QChar character : value) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9')) {
            return false;
        }
    }
    return true;
}
}
