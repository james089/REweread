#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
namespace WereadDownloader {
// Page-position estimate in the CURRENT native layout: first=0%, last=100%.
// Not a WeRead text offset; never use this to construct a sync payload.
inline double nativeReadingPercent(const QJsonObject &metadata, const QJsonObject &content) {
    if (metadata.value("deleted").toBool() || metadata.value("parent")=="trash") return -1;
    const auto p=metadata.value("lastOpenedPage"), n=content.value("pageCount");
    if (!p.isDouble() || !n.isDouble()) return -1;
    const double page=p.toDouble(), count=n.toDouble();
    if (!std::isfinite(page) || !std::isfinite(count) || page!=std::floor(page) || count!=std::floor(count)
        || count<1 || page<0 || page>=count || content.value("pages").toArray().size()!=count) return -1;
    return count==1 ? 0 : page*100.0/(count-1);
}
}
