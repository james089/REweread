#pragma once
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimeZone>
#include <utility>

// One writer (the UI) owns local monotonic counters. Upload acknowledgements
// live in a different file owned by the serialized worker, avoiding lost writes.
namespace WereadDownloader {
class ReadingTimeStore {
public:
    explicit ReadingTimeStore(QString root):m_root(std::move(root)){}
    ~ReadingTimeStore(){save();}
    bool open(const QString &scope,const QString &id,const QString &uuid,const QString &title) {
        if(!close())return false;
        const auto dir=m_root+"/accounts/"+scope;
        if(!QFileInfo(dir).isDir() || QFileInfo(dir).isSymLink() || QFileInfo(m_root+"/accounts").isSymLink())return false;
        m_path=dir+"/local-reading-time-v1.json";
        m_books={};QFile f(m_path);
        if(f.exists()) {
            if(QFileInfo(f).isSymLink() || !f.open(QIODevice::ReadOnly) || f.size()>8*1024*1024)return false;
            QJsonParseError error;auto v=QJsonDocument::fromJson(f.readAll(),&error).object();
            if(error.error!=QJsonParseError::NoError || v.value("version")!=1 || !v.value("books").isObject())return false;
            m_books=v.value("books").toObject();
        }
        if(m_books.contains(id)) {
            const auto row=m_books.value(id).toObject();const auto n=row.value("seconds").toDouble(-1);
            if(!row.value("seconds").isDouble() || n<0 || n>315360000 || n!=qint64(n))return false;
            if(row.contains("days") && !row.value("days").isObject())return false;
            const auto days=row.value("days").toObject();double sum=0;
            for(auto d=days.begin();d!=days.end();++d) {
                const auto value=d.value().toDouble(-1);const auto date=QDate::fromString(d.key(),Qt::ISODate);
                if(!date.isValid() || date.toString(Qt::ISODate)!=d.key() || !d.value().isDouble() || value<0 || value!=qint64(value) || value>n)return false;
                sum+=value;
            }
            if(sum>n)return false;
        }
        m_id=id;m_uuid=uuid;m_title=title;m_idleMs=0;m_remainder=0;m_paused=false;
        return true;
    }
    bool close(){m_paused=true;if(!save())return false;m_id.clear();m_path.clear();m_books={};return true;}
    void paused(bool value){m_paused=value;}
    void activity(){m_idleMs=0;}
    bool active()const{return !m_id.isEmpty() && !m_paused && m_idleMs<300000;}
    void advance(qint64 ms){advanceAt(ms,QDateTime::currentMSecsSinceEpoch());}
    void advance(qint64 ms,qint64 now){advanceAt(ms,now*1000);}
    void advanceAt(qint64 ms,qint64 endMs) {
        const auto now=endMs/1000;
        if(!active() || ms<=0)return;
        // A missed suspend callback must never turn hours of sleep into reading.
        if(ms>15000){m_idleMs=300000;return;}
        const auto accepted=qMin(ms,qMax<qint64>(0,300000-m_idleMs));m_idleMs+=ms;
        auto row=m_books.value(m_id).toObject();const auto old=row.value("seconds").toDouble();
        if(old<0 || old>315360000 || old!=qint64(old))return;
        auto days=row.value("days").toObject();
        qint64 cursor=endMs-ms,remaining=accepted,added=0;
        while(remaining>0) {
            const auto start=QDateTime::fromMSecsSinceEpoch(cursor);
            const auto next=QDateTime(start.date().addDays(1),QTime(0,0),start.timeZone());
            const auto part=qMin(remaining,next.toMSecsSinceEpoch()-cursor);
            if(part<=0)return;
            m_remainder+=part;const auto seconds=m_remainder/1000;m_remainder%=1000;
            if(seconds) {
                const auto key=start.date().toString(Qt::ISODate);
                days[key]=days.value(key).toDouble()+seconds;added+=seconds;
            }
            cursor+=part;remaining-=part;
        }
        if(!added)return;
        row["seconds"]=old+added;row["days"]=days;row["uuid"]=m_uuid;row["title"]=m_title;row["lastRead"]=double(now);
        m_books[m_id]=row;m_dirty=true;
    }
    bool save() {
        if(!m_dirty || m_path.isEmpty())return true;
        if(QFileInfo(m_path).isSymLink())return false;
        QSaveFile file(m_path);file.setDirectWriteFallback(false);
        if(!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFile::ReadOwner|QFile::WriteOwner))return false;
        const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"books",m_books}}).toJson(QJsonDocument::Compact);
        if(file.write(bytes)!=bytes.size() || !file.commit())return false;
        m_dirty=false;return true;
    }
private:
    QString m_root,m_path,m_id,m_uuid,m_title;QJsonObject m_books;
    qint64 m_idleMs=0,m_remainder=0;bool m_paused=false,m_dirty=false;
};
}
