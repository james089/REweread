#pragma once
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
namespace WereadDownloader {
// A local change detector, never a cloud position or percentage. Opening and
// closing snapshots share this format. Opening timestamps are ignored.
inline QString exitPositionSignature(const QString &base,int page=-1) {
    const auto read=[](const QString &path){
        QFile f(path);if(QFileInfo(path).isSymLink() || !f.open(QIODevice::ReadOnly) || f.size()>8*1024*1024)return QByteArray();
        return f.readAll();
    };
    const auto metadata=QJsonDocument::fromJson(read(base+".metadata")).object();
    const auto content=read(base+".content");const auto layout=QJsonDocument::fromJson(content).object();
    if(page<0)page=metadata.value("lastOpenedPage").toInt(-1);
    const auto pdf=QFileInfo(base+".pdf");
    if(metadata.isEmpty() || metadata.value("deleted").toBool() || metadata.value("parent")=="trash"
       || content.isEmpty() || page<0 || page>=layout.value("pageCount").toInt() || !pdf.isFile() || pdf.isSymLink())return {};
    const auto material=QByteArray("exit-position-v1:")+QByteArray::number(page)+":"+
        QCryptographicHash::hash(content,QCryptographicHash::Sha256).toHex()+":"+
        QByteArray::number(pdf.size())+":"+QByteArray::number(pdf.lastModified().toMSecsSinceEpoch());
    return QString::fromLatin1(QCryptographicHash::hash(material,QCryptographicHash::Sha256).toHex());
}
}
