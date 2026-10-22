#include "native_http.h"
#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <iostream>
using namespace WereadDownloader;
int main(int argc, char **argv) {
 QCoreApplication app(argc, argv);
 QTcpServer server;
 if (!server.listen(QHostAddress::LocalHost, 0)) return 1;
 bool framed = false;
 bool reject = false;
 QObject::connect(&server, &QTcpServer::newConnection, [&] {
  auto *socket = server.nextPendingConnection();
  auto bytes = std::make_shared<QByteArray>();
  QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket, bytes] {
   *bytes += socket->readAll();
   int at = bytes->indexOf("\r\n\r\n"); if (at < 0) return;
   const auto header = bytes->left(at).toLower();
   const auto marker = QByteArray("content-length: "); int lengthAt = header.indexOf(marker);
   if (lengthAt < 0) { socket->abort(); return; }
   int length = header.mid(lengthAt + marker.size()).split('\r').first().toInt();
   auto body = bytes->mid(at + 4); if (body.size() < length) return;
   int boundaryAt = header.indexOf("boundary=");
   const auto boundary = bytes->left(at).mid(boundaryAt + 9).split('\r').first();
   framed = boundaryAt > 0 && !boundary.startsWith('"') && body.startsWith("--" + boundary + "\r\n")
       && body.contains("Content-Disposition: form-data; name=\"file\"") && body.contains("filename=\"weread-synthetic.epub\"")
       && body.contains("synthetic-file-bytes") && !header.contains("transfer-encoding: chunked");
   socket->write(reject ? "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\nConnection: close\r\n\r\nNo file sent"
                        : "HTTP/1.1 201 Created\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"); socket->disconnectFromHost();
  });
 });
 QNetworkAccessManager manager;
 QTemporaryFile file; if (!file.open()) return 1;
 file.write("synthetic-file-bytes"); file.flush();
 auto *multipart = new NativeUploadBody(file.fileName(), "synthetic");
 const int status = nativeRequest(manager, QNetworkRequest(QUrl("http://127.0.0.1:" + QString::number(server.serverPort()) + "/upload")), multipart);
 if (status != 201 || !framed) return 1;
 reject = true;
 auto *bad = new NativeUploadBody(file.fileName(), "synthetic");
 bool rejected = false;
 const int failure = nativeRequest(manager, QNetworkRequest(QUrl("http://127.0.0.1:" + QString::number(server.serverPort()) + "/upload")), bad, &rejected);
 if (failure != 400 || !rejected) return 1;
 std::cout << "native upload sends unquoted boundary, matching delimiter and explicit content length\n";
}
