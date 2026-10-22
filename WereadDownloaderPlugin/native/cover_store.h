#pragma once
#include <QObject>
#include <QProcess>
#include <QUrl>
#include <QSet>
#include <QVariantList>
#include <functional>
class CoverStore : public QObject {
public:
 explicit CoverStore(QObject *parent, const QString &root, const QString &fetchScript = "/home/root/weread-downloader/lua/fetch-cover.lua");
 ~CoverStore() override { ready = {}; stop(); }
 void begin(const QString &scope, const QVariantList &books);
 void stop();
 void pause();
 int requestsStarted() const { return m_requests; }
 int imageFailures = 0;
 QString source(const QString &id, const QString &url);
 std::function<void()> ready;
 static bool allowed(const QUrl &url);
 static bool saveImage(const QByteArray &bytes, const QString &path);
private:
 struct Job { QString id, url, path; };
 QString path(const QString &id, const QString &url) const;
 void enqueue(const QString &id, const QString &url, bool priority);
 void pump();
 void fetch(Job job);
 QString m_root, m_scope, m_fetchScript;
 QList<Job> m_queue;
 QSet<QString> m_attempted;
 QSet<QProcess *> m_active;
 quint64 m_generation = 0;
 int m_requests = 0;
 bool m_paused = true;
};
