#include "cover_store.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
#include <QProcessEnvironment>
#include <sys/stat.h>
#include <memory>
namespace {
const qint64 limit = 4 * 1024 * 1024;
bool validId(const QString &id) { return QRegularExpression("\\A[A-Za-z0-9_-]{1,128}\\z").match(id).hasMatch(); }
bool cached(const QString &path) { const QFileInfo f(path); return f.isFile() && !f.isSymLink() && f.size() > 0 && f.size() <= limit; }
}
CoverStore::CoverStore(QObject *parent, const QString &root, const QString &fetchScript) : QObject(parent), m_root(root), m_fetchScript(fetchScript) {}
bool CoverStore::allowed(const QUrl &url) {
 const auto host = url.host().toLower();
 return url.scheme() == "https" && url.userInfo().isEmpty() && (url.port() == -1 || url.port() == 443)
   && (host == "cdn.weread.qq.com" || host == "res.weread.qq.com" || host.endsWith(".qq.com")
       || host.endsWith(".qpic.cn") || host.endsWith(".file.myqcloud.com") || host.endsWith(".image.myqcloud.com"));
}
void CoverStore::pause() {
 m_paused = true; ++m_generation; m_queue.clear();
 const auto active = m_active; m_active.clear();
 for (auto *process : active) process->kill();
}
void CoverStore::stop() { pause(); m_scope.clear(); m_attempted.clear(); }
void CoverStore::begin(const QString &scope, const QVariantList &books) {
 stop();
 if (!QRegularExpression("\\A[a-f0-9]{64}\\z").match(scope).hasMatch()) return;
 m_scope = scope; m_paused = false;
 for (const auto &row : books) { auto b = row.toMap(); enqueue(b.value("bookId").toString(), b.value("cover").toString(), false); }
 QTimer::singleShot(0, this, [this] { pump(); });
}
QString CoverStore::path(const QString &id, const QString &url) const {
 if (m_scope.isEmpty() || !validId(id) || !allowed(QUrl(url)) || url.size() > 2048) return {};
 return m_root + "/accounts/" + m_scope + "/covers-v2/" + id + '-'
    + QString::fromLatin1(QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha256).toHex()) + ".png";
}
QString CoverStore::source(const QString &id, const QString &url) {
 const auto file = path(id, url); if (file.isEmpty()) return {};
 if (cached(file)) return QUrl::fromLocalFile(file).toString();
 enqueue(id, url, true); pump(); return {};
}
void CoverStore::enqueue(const QString &id, const QString &url, bool priority) {
 if (m_paused) return;
 const auto file = path(id, url); if (file.isEmpty() || cached(file)) return;
 for (int i = 0; i < m_queue.size(); ++i) if (m_queue[i].path == file) {
  if (priority) m_queue.prepend(m_queue.takeAt(i));
  return;
 }
 if (m_attempted.contains(file)) return;
 m_attempted.insert(file); Job job{id, url, file};
 if (priority) m_queue.prepend(job); else m_queue.append(job);
}
bool CoverStore::saveImage(const QByteArray &bytes, const QString &path) {
 if (bytes.isEmpty() || bytes.size() > limit || QFileInfo(path).isSymLink()) return false;
 QBuffer buffer; buffer.setData(bytes); buffer.open(QIODevice::ReadOnly); QImageReader reader(&buffer);
 const auto size = reader.size();
 if (!size.isValid() || size.width() > 4096 || size.height() > 4096) return false;
 reader.setScaledSize(size.scaled(184, 272, Qt::KeepAspectRatio));
 const auto image = reader.read(); if (image.isNull()) return false;
 const auto directory = QFileInfo(path).absolutePath();
 if (QFileInfo(directory).isSymLink() || !QDir().mkpath(directory)
     || ::chmod(QFile::encodeName(directory).constData(), 0700)) return false;
 QSaveFile file(path); file.setDirectWriteFallback(false);
 if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFile::ReadOwner | QFile::WriteOwner)
      || !image.save(&file, "PNG")) return false;
 return file.commit();
}
void CoverStore::pump() { if (m_paused) return; while (m_active.size() < 2 && !m_queue.isEmpty()) fetch(m_queue.takeFirst()); }
void CoverStore::fetch(Job job) {
 if (!allowed(QUrl(job.url))) return;
 const auto generation = m_generation;
 auto *process = new QProcess(this); m_active.insert(process); ++m_requests;
 QProcessEnvironment env;
 env.insert("PATH", "/usr/bin:/bin:/usr/sbin:/sbin"); env.insert("HOME", "/home/root");
 env.insert("LANG", "C.UTF-8"); env.insert("LC_ALL", "C.UTF-8");
 process->setProcessEnvironment(env); process->setStandardErrorFile(QProcess::nullDevice());
 auto bytes = std::make_shared<QByteArray>();
 connect(process, &QProcess::readyReadStandardOutput, process, [process, bytes] {
  const auto chunk = process->readAllStandardOutput();
  if (bytes->size() + chunk.size() > limit + 128) { process->kill(); return; } bytes->append(chunk);
 });
 auto *deadline = new QTimer(process); deadline->setSingleShot(true);
 connect(deadline, &QTimer::timeout, process, &QProcess::kill); deadline->start(65000);
 connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
  [this, process, bytes, job, generation](int code, QProcess::ExitStatus status) {
   process->deleteLater(); if (generation != m_generation) return;
   m_active.remove(process); bytes->append(process->readAllStandardOutput());
   const QByteArray marker("WRDP-COVER\n"); const int end = bytes->indexOf('\n', marker.size());
   bool lengthValid = false; const int length = bytes->mid(marker.size(), end - marker.size()).toInt(&lengthValid);
   if (code == 0 && status == QProcess::NormalExit && bytes->startsWith(marker) && end > 0 && lengthValid
      && length > 0 && length <= limit && bytes->size() == end + 1 + length && saveImage(bytes->mid(end + 1), job.path)) {
    if (ready) ready();
   } else { ++imageFailures; }
   pump();
  });
 connect(process, &QProcess::errorOccurred, this, [this, process, generation](QProcess::ProcessError error) {
   if (error != QProcess::FailedToStart || generation != m_generation) return;
   m_active.remove(process); process->deleteLater(); ++imageFailures; pump();
 });
 process->start("/home/root/xovi/exthome/appload/koreader/luajit", QStringList{m_fetchScript, job.url});
}
