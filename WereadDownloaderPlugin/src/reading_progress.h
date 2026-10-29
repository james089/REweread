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
    const auto pages = content.value("formatVersion")==2
        ? content.value("cPages").toObject().value("pages") : content.value("pages");
    int visiblePages=0;
    if (pages.isArray()) for (const auto &entry : pages.toArray()) {
        if (content.value("formatVersion")!=2) { ++visiblePages; continue; }
        if (!entry.isObject()) return -1;
        const auto object=entry.toObject();
        if (!object.contains("deleted")) { ++visiblePages; continue; }
        const auto flag=object.value("deleted").toObject().value("value");
        if (flag==QJsonValue(false) || flag==QJsonValue(0)) ++visiblePages;
        else if (flag!=QJsonValue(true) && flag!=QJsonValue(1)) return -1;
    }
    if (!std::isfinite(page) || !std::isfinite(count) || page!=std::floor(page) || count!=std::floor(count)
        || count<1 || page<0 || page>=count || !pages.isArray() || visiblePages!=count) return -1;
    return count==1 ? 0 : page*100.0/(count-1);
}
}
