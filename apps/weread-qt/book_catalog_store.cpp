#include "book_catalog_store.h"

#include <QFileInfo>
#include <QDir>
#include <QHash>
#include <QXmlStreamReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QVariantMap>

BookCatalogStore::BookCatalogStore(QObject *parent)
    : QObject(parent),
      m_statusText(QStringLiteral("目录未加载")) {
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        appendOutput(QString::fromUtf8(m_process.readAllStandardOutput()));
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this]() {
        appendOutput(QString::fromUtf8(m_process.readAllStandardError()));
    });
    connect(&m_process, &QProcess::finished, this, &BookCatalogStore::finishProcess);
}

bool BookCatalogStore::running() const {
    return m_running;
}

QString BookCatalogStore::statusText() const {
    return m_statusText;
}

QVariantList BookCatalogStore::chapters() const {
    return m_chapters;
}

void BookCatalogStore::loadCatalog(const QString &bookId, const QString &title) {
    const QString trimmed = bookId.trimmed();
    if (trimmed.isEmpty()) {
        setState(false, QStringLiteral("缺少书籍 ID"));
        return;
    }
    if (loadLocalCatalog(trimmed)) return;
    if (m_running) {
        return;
    }

    const QString luajit = QStringLiteral("/home/root/xovi/exthome/appload/koreader/luajit");
    const QString helper = QStringLiteral("/home/root/weread-qt/helper/tools/fetch-catalog.lua");
    if (!QFileInfo::exists(luajit) || !QFileInfo::exists(helper)) {
        setState(false, QStringLiteral("目录组件缺失"));
        return;
    }

    m_bookId = trimmed;
    m_chapters.clear();
    emit changed();

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("RM_WEREAD_APP_DIR"), QStringLiteral("/home/root/weread-qt/helper"));
    env.insert(QStringLiteral("KO_DIR"), QStringLiteral("/home/root/xovi/exthome/appload/koreader"));
    m_process.setProcessEnvironment(env);
    m_process.setProgram(luajit);
    m_process.setArguments(QStringList() << helper << trimmed << (title.trimmed().isEmpty() ? trimmed : title.trimmed()));
    setState(true, QStringLiteral("正在加载目录..."));
    m_process.start();
    if (!m_process.waitForStarted(3000)) {
        setState(false, QStringLiteral("目录组件无法启动"));
    }
}

bool BookCatalogStore::loadLocalCatalog(const QString &bookId) {
    QString safe;
    for (const QChar ch : bookId) safe += (ch.isLetterOrNumber() || ch == '_' || ch == '-' || ch == '.') ? ch : QChar('_');
    if (safe.isEmpty() || safe == "." || safe == "..") return false;
    const QString data = qEnvironmentVariable("RM_WEREAD_DATA_DIR", "/home/root/.local/share/rm-weread");
    const QDir dir(QDir(data).filePath(QStringLiteral("books/%1").arg(safe)));
    const auto files = dir.entryInfoList({QStringLiteral("*full.epub")}, QDir::Files, QDir::Name);
    if (files.isEmpty()) return false;
    if (m_running) { m_process.kill(); m_process.waitForFinished(1000); }
    m_bookId = bookId;
    m_chapters.clear();
    const QString epub = files.first().absoluteFilePath();
    auto member = [&epub](const QString &name) {
        QProcess unzip;
        unzip.start(QStringLiteral("/usr/bin/unzip"), {QStringLiteral("-p"), epub, name});
        if (!unzip.waitForFinished(3000) || unzip.exitCode() != 0) return QByteArray();
        return unzip.readAllStandardOutput();
    };
    QHash<QString, QString> uids;
    QXmlStreamReader opf(member(QStringLiteral("OEBPS/content.opf")));
    while (!opf.atEnd()) {
        opf.readNext();
        if (opf.isStartElement() && opf.name() == QLatin1String("item")) {
            const auto attrs = opf.attributes();
            const QString id = attrs.value(QLatin1String("id")).toString();
            if (QRegularExpression(QStringLiteral("^chapter_[0-9]+$")).match(id).hasMatch())
                uids.insert(attrs.value(QLatin1String("href")).toString(), id.mid(8));
        }
    }
    QXmlStreamReader toc(member(QStringLiteral("OEBPS/toc.ncx")));
    QList<int> stack;
    while (!toc.atEnd()) {
        toc.readNext();
        if (toc.isStartElement()) {
            if (toc.name() == QLatin1String("navPoint")) {
                QVariantMap row;
                row.insert(QStringLiteral("index"), m_chapters.size() + 1);
                row.insert(QStringLiteral("chapterIdx"), m_chapters.size() + 1);
                stack.append(m_chapters.size());
                m_chapters.append(row);
            } else if (!stack.isEmpty() && toc.name() == QLatin1String("text")) {
                auto row = m_chapters.at(stack.last()).toMap();
                row.insert(QStringLiteral("title"), toc.readElementText());
                m_chapters[stack.last()] = row;
            } else if (!stack.isEmpty() && toc.name() == QLatin1String("content")) {
                auto row = m_chapters.at(stack.last()).toMap();
                row.insert(QStringLiteral("chapterUid"), uids.value(toc.attributes().value(QLatin1String("src")).toString().section('#', 0, 0)));
                m_chapters[stack.last()] = row;
            }
        } else if (toc.isEndElement() && toc.name() == QLatin1String("navPoint") && !stack.isEmpty()) stack.removeLast();
    }
    if (toc.hasError()) m_chapters.clear();
    for (int i = 0; i < m_chapters.size(); ++i) {
        auto row = m_chapters.at(i).toMap();
        row.insert(QStringLiteral("label"), QStringLiteral("%1. %2").arg(i + 1).arg(row.value(QStringLiteral("title")).toString()));
        m_chapters[i] = row;
    }
    setState(false, m_chapters.isEmpty() ? QStringLiteral("本地目录不可用，请进入阅读页查看目录")
                                      : QStringLiteral("本地目录：%1 章").arg(m_chapters.size()));
    return true; // A full local book must never silently fall back to the network.
}

void BookCatalogStore::appendOutput(const QString &text) {
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QJsonDocument document = QJsonDocument::fromJson(line.toUtf8());
        if (!document.isObject()) {
            continue;
        }

        const QJsonObject object = document.object();
        const QString state = object.value(QStringLiteral("state")).toString();
        if (state == QStringLiteral("chapter")) {
            QVariantMap row = object.toVariantMap();
            row.insert(QStringLiteral("label"), QStringLiteral("%1. %2")
                .arg(row.value(QStringLiteral("index")).toInt())
                .arg(row.value(QStringLiteral("title")).toString()));
            m_chapters.append(row);
            emit changed();
        } else if (state == QStringLiteral("done")) {
            const int count = object.value(QStringLiteral("count")).toInt();
            setState(m_running, count > 0 ? QStringLiteral("目录已加载：%1 章").arg(count) : QStringLiteral("没有可显示章节"));
        } else if (state == QStringLiteral("error")) {
            const QString message = object.value(QStringLiteral("message")).toString();
            setState(m_running, message.isEmpty() ? QStringLiteral("目录加载失败") : message);
        }
    }
}

void BookCatalogStore::finishProcess(int exitCode, QProcess::ExitStatus exitStatus) {
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        if (m_statusText.contains(QStringLiteral("正在"))) {
            setState(false, m_chapters.isEmpty() ? QStringLiteral("没有可显示章节") : QStringLiteral("目录已加载：%1 章").arg(m_chapters.size()));
        } else {
            setState(false, m_statusText);
        }
    } else if (!m_statusText.contains(QStringLiteral("失败")) && !m_statusText.contains(QStringLiteral("缺失"))) {
        setState(false, m_statusText.isEmpty() || m_statusText.contains(QStringLiteral("正在")) ? QStringLiteral("目录加载失败") : m_statusText);
    } else {
        setState(false, m_statusText);
    }
}

void BookCatalogStore::setState(bool running, const QString &statusText) {
    m_running = running;
    m_statusText = statusText;
    emit changed();
}
