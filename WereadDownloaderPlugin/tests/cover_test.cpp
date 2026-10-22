#include "cover_store.h"
#include <QGuiApplication>
#include <QBuffer>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
#include <stdexcept>
void check(bool value) { if (!value) throw std::runtime_error("cover test failed"); }
int main(int argc, char **argv) {
 QGuiApplication app(argc, argv);
 try {
  QTemporaryDir temporary; check(temporary.isValid());
  const QString scope(64, 'a'), url("https://cdn.weread.qq.com/synthetic.png");
  const auto file = temporary.path() + "/accounts/" + scope + "/covers-v2/synthetic-"
    + QString::fromLatin1(QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha256).toHex()) + ".png";
  QImage image(320, 640, QImage::Format_RGB32); image.fill(Qt::green);
  QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); check(image.save(&buffer,"PNG"));
  check(CoverStore::saveImage(bytes, file));
  check(!CoverStore::saveImage("not an image", temporary.path()+"/bad.png"));
  check(!CoverStore::allowed(QUrl("https://cdn.weread.qq.com.evil.example/x")));
  check(!CoverStore::allowed(QUrl("http://cdn.weread.qq.com/x")));
  check(!CoverStore::allowed(QUrl("https://user@cdn.weread.qq.com/x")));
  check(QImage(file).size().width() <= 184 && QImage(file).size().height() <= 272);
  check(!(QFileInfo(file).permissions() & QFile::ReadOther));
  const QVariantList books{QVariantMap{{"bookId","synthetic"},{"cover",url}}};
  {
   CoverStore covers(nullptr,temporary.path()); covers.begin(scope,books);
   check(covers.source("synthetic",url).startsWith("file:"));
   QCoreApplication::processEvents(); check(covers.requestsStarted()==0);
  }
  {
   CoverStore reopened(nullptr,temporary.path()); reopened.begin(scope,books);
   check(reopened.source("synthetic",url).startsWith("file:"));
   QCoreApplication::processEvents(); check(reopened.requestsStarted()==0);
  }
  if (argc == 2 && QString::fromLocal8Bit(argv[1]) == "--private-shelf") {
   QProcess worker; worker.start("/home/root/weread-downloader/weread-downloader-worker",QStringList{"cached-shelf"});
   check(worker.waitForFinished(15000)); check(worker.exitCode()==0);
   const auto row=QJsonDocument::fromJson(worker.readAllStandardOutput().trimmed()).object();
   const auto account=row.value("accountScope").toString(); auto rows=row.value("books").toArray();
   QVariantList selected; for (int i=0; i<qMin(5,int(rows.size()));++i) selected.append(rows[i].toObject().toVariantMap());
   CoverStore covers(nullptr,"/home/root/.local/share/weread-downloader", "/tmp/wrdp-touch-cover-qa/fetch-cover.lua");
   covers.begin(account,selected); QElapsedTimer deadline; deadline.start(); int saved=0;
   while(deadline.elapsed()<45000) {
    saved=0; for(const auto &value:selected) { auto b=value.toMap(); if(!covers.source(b.value("bookId").toString(),b.value("cover").toString()).isEmpty()) ++saved; }
    if(saved==selected.size()) break;
    QCoreApplication::processEvents(QEventLoop::AllEvents,50); QThread::msleep(10);
   }
   if(saved!=selected.size()) std::cerr << "saved=" << saved << " requests=" << covers.requestsStarted() << " invalid_image=" << covers.imageFailures << "\n";
   covers.stop(); check(saved==selected.size());
   CoverStore reopened(nullptr,"/home/root/.local/share/weread-downloader"); reopened.begin(account,selected);
   for(const auto &value:selected) { auto b=value.toMap(); check(!reopened.source(b.value("bookId").toString(),b.value("cover").toString()).isEmpty()); }
   QCoreApplication::processEvents(); check(reopened.requestsStarted()==0);
   std::cout << "device covers saved and reused offline without requests: " << saved << "\n";
  }
  std::cout << "persistent cover reuse, image validation, private permissions and URL isolation ok\n";
 } catch(const std::exception &error) { std::cerr<<error.what()<<'\n'; return 1; }
}
