#pragma once
#include <QString>
#include <QRegularExpression>
namespace WereadDownloader {
// Qt fixed offsets use ISO signs; libc TZ uses the opposite POSIX signs.
// Keep IANA identifiers intact so their daylight-saving rules still apply.
inline QString helperTimeZone(const QString &id) {
    static const QRegularExpression fixed(QStringLiteral("^UTC([+-])([0-9]{2}:[0-9]{2})$"));
    const auto match=fixed.match(id);
    if (!match.hasMatch()) return id;
    return QStringLiteral("UTC%1%2").arg(match.captured(1)=="+"?"-":"+",match.captured(2));
}
}
