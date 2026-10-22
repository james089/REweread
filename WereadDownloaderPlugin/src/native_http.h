#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "native_upload.h"
#include <QEventLoop>
#include <QTimer>
#include <csignal>
namespace WereadDownloader {
inline int nativeRequest(QNetworkAccessManager &manager, QNetworkRequest request, NativeUploadBody *body = nullptr, bool *rejected = nullptr, const volatile std::sig_atomic_t *cancelFlag = nullptr) {
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(120000);
    if (body) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, body->contentType());
        request.setHeader(QNetworkRequest::ContentLengthHeader, body->size());
    }
    auto *reply = body ? manager.post(request, body) : manager.get(request);
    if (body) body->setParent(reply);
    QEventLoop loop;
    QTimer deadline; deadline.setSingleShot(true); deadline.start(body ? 180000 : 5000);
    QTimer cancelPoll; cancelPoll.setInterval(100); cancelPoll.start();
    QObject::connect(&cancelPoll, &QTimer::timeout, reply, [reply, cancelFlag] { if (cancelFlag && *cancelFlag) reply->abort(); });
    QObject::connect(&deadline, &QTimer::timeout, reply, &QNetworkReply::abort);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    // Classify only bounded known pre-import failures. Never emit a response body.
    QByteArray diagnostic;
    QObject::connect(reply, &QIODevice::readyRead, reply, [reply, &diagnostic] {
        const auto bytes = reply->readAll();
        if (diagnostic.size() < 4096) diagnostic += bytes.left(4096 - diagnostic.size());
    });
    loop.exec();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool good = reply->error() == QNetworkReply::NoError;
    if (rejected) *rejected = status == 400 && (diagnostic.contains("No file sent")
        || diagnostic.contains("Importing empty file") || diagnostic.contains("Invalid filename"));
    reply->deleteLater();
    return status > 0 ? status : (good ? 200 : 0);
}
}
