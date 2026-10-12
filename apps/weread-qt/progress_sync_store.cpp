#include "progress_sync_store.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcessEnvironment>
#include <QRegularExpression>

namespace {
constexpr int kProgressHelperTimeoutMs = 25000;
}

ProgressSyncStore::ProgressSyncStore(QObject *parent)
    : QObject(parent),
      m_running(false),
      m_statusText(QStringLiteral("进度未同步")) {
    m_processTimeoutTimer.setSingleShot(true);
    m_processTimeoutTimer.setInterval(kProgressHelperTimeoutMs);
    connect(&m_processTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (!m_running) {
            return;
        }
        m_timedOut = true;
        setState(true, failureText(QStringLiteral("超时")));
        if (m_process.state() != QProcess::NotRunning) {
            m_process.kill();
        } else {
            finishProcess(-1, QProcess::CrashExit);
        }
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        appendOutput(m_stdoutBuffer, m_process.readAllStandardOutput(), true);
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this]() {
        appendOutput(m_stderrBuffer, m_process.readAllStandardError(), false);
    });
    connect(&m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &ProgressSyncStore::finishProcess);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (!m_running || error != QProcess::FailedToStart) {
            return;
        }
        m_processStartFailed = true;
        setState(false, failureText(QStringLiteral("组件无法启动")));
        m_processTimeoutTimer.stop();
        finishProcess(-1, QProcess::CrashExit);
    });
}

bool ProgressSyncStore::running() const {
    return m_running;
}

QString ProgressSyncStore::statusText() const {
    return m_statusText;
}

void ProgressSyncStore::syncProgress(const QString &bookId, double progress, const QString &summary, int elapsedSeconds,
                                     const QString &chapterUid, int chapterIdx, int chapterOffset) {
    if (m_running || bookId.trimmed().isEmpty()) {
        return;
    }

    const QString helper = QStringLiteral("/home/root/weread-qt/helper/tools/sync-progress.lua");
    QStringList args;
    args << bookId.trimmed()
         << QString::number(qBound(0.0, progress, 100.0), 'f', 2)
         << summary.left(80)
         << QString::number(qMax(0, elapsedSeconds));
    const bool hasVerifiedPosition = !chapterUid.trimmed().isEmpty()
        && chapterUid.trimmed() != QStringLiteral("0")
        && chapterIdx >= 0
        && chapterOffset >= 0;
    if (hasVerifiedPosition) {
        args << chapterUid.trimmed()
             << QString::number(chapterIdx)
             << QString::number(chapterOffset);
    }
    startHelper(helper, args, QStringLiteral("正在同步进度..."), QStringLiteral("push"));
}

void ProgressSyncStore::pullProgress(const QString &bookId) {
    if (m_running || bookId.trimmed().isEmpty()) {
        return;
    }

    const QString helper = QStringLiteral("/home/root/weread-qt/helper/tools/fetch-progress.lua");
    startHelper(helper, QStringList() << bookId.trimmed(), QStringLiteral("正在拉取进度..."), QStringLiteral("pull"));
}

void ProgressSyncStore::startHelper(const QString &helper, const QStringList &arguments, const QString &statusText, const QString &mode) {
    const QString luajit = QStringLiteral("/home/root/xovi/exthome/appload/koreader/luajit");
    resetRequestState();
    m_mode = mode;
    m_bookId = arguments.isEmpty() ? QString() : arguments.first();
    m_elapsedSeconds = mode == QStringLiteral("push") && arguments.size() > 3
        ? qMax(0, arguments.at(3).toInt())
        : 0;
    if (!QFileInfo::exists(luajit) || !QFileInfo::exists(helper)) {
        setState(false, failureText(QStringLiteral("组件缺失")));
        m_requestFinished = true;
        emit progressFinished(m_bookId, m_elapsedSeconds, false);
        return;
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("RM_WEREAD_APP_DIR"), QStringLiteral("/home/root/weread-qt/helper"));
    env.insert(QStringLiteral("KO_DIR"), QStringLiteral("/home/root/xovi/exthome/appload/koreader"));
    m_process.setProcessEnvironment(env);
    m_process.setProgram(luajit);
    m_process.setArguments(QStringList() << helper << arguments);
    setState(true, statusText);
    m_process.start();
    m_processTimeoutTimer.start();
}

QString ProgressSyncStore::dataDir() const {
    const QByteArray overrideDir = qgetenv("RM_WEREAD_DATA_DIR");
    if (!overrideDir.isEmpty()) {
        return QString::fromUtf8(overrideDir);
    }
    return QStringLiteral("/home/root/.local/share/rm-weread");
}

void ProgressSyncStore::setState(bool running, const QString &statusText) {
    m_running = running;
    m_statusText = statusText;
    emit changed();
}

void ProgressSyncStore::appendOutput(QByteArray &buffer, const QByteArray &bytes, bool parseProtocol) {
    buffer.append(bytes);
    int newline = -1;
    while (true) {
        const int carriageReturn = buffer.indexOf('\r');
        const int lineFeed = buffer.indexOf('\n');
        if (carriageReturn < 0) {
            newline = lineFeed;
        } else if (lineFeed < 0) {
            newline = carriageReturn;
        } else {
            newline = qMin(carriageReturn, lineFeed);
        }
        if (newline < 0) {
            return;
        }

        // Decode only a complete byte line. QProcess may split a UTF-8
        // character between readyRead notifications; decoding each chunk first
        // would turn a valid Chinese summary into replacement characters.
        const QByteArray lineBytes = buffer.left(newline);
        int consumed = newline + 1;
        while (consumed < buffer.size()
               && (buffer.at(consumed) == '\r' || buffer.at(consumed) == '\n')) {
            ++consumed;
        }
        buffer.remove(0, consumed);
        // stderr may contain arbitrary Lua/HTTP diagnostics. It must never
        // turn a request into a protocol `done`, even if a diagnostic happens
        // to contain a status-looking line.
        if (parseProtocol) {
            processOutputLine(QString::fromUtf8(lineBytes).trimmed());
        }
    }
}

void ProgressSyncStore::flushOutput(QByteArray &buffer, bool parseProtocol) {
    if (buffer.isEmpty()) {
        return;
    }
    const QByteArray lineBytes = buffer;
    buffer.clear();
    if (parseProtocol) {
        processOutputLine(QString::fromUtf8(lineBytes).trimmed());
    }
}

void ProgressSyncStore::processOutputLine(const QString &line) {
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(line.toUtf8(), &parseError);
    if (parseError.error == QJsonParseError::NoError && json.isObject()) {
        const QJsonObject row = json.object();
        const QString state = row.value(QStringLiteral("state")).toString();
        if (state == QStringLiteral("done")) {
            m_helperDone = true;
            const QJsonValue progressValue = row.value(QStringLiteral("progress"));
            if (progressValue.isDouble()) {
                const double progress = qBound(0.0, progressValue.toDouble(), 100.0);
                if (m_mode == QStringLiteral("pull")) {
                    m_hasPulledProgress = true;
                    m_pulledProgress = progress;
                    const QString chapterUid = row.value(QStringLiteral("chapterUid")).toString().trimmed();
                    const QString summary = row.value(QStringLiteral("summary")).toString().trimmed();
                    if (!chapterUid.isEmpty() && !summary.isEmpty()) {
                        m_pulledPosition = row.toVariantMap();
                    }
                    setState(m_running, QStringLiteral("已拉取到 %1%").arg(QString::number(progress, 'f', 2)));
                } else {
                    setState(m_running, QStringLiteral("已同步到 %1%").arg(QString::number(progress, 'f', 2)));
                }
            } else {
                setState(m_running, m_mode == QStringLiteral("pull") ? QStringLiteral("进度已拉取") : QStringLiteral("进度已同步"));
            }
            return;
        }
        if (state == QStringLiteral("error")) {
            m_helperFailed = true;
            setState(m_running, failureText(row.value(QStringLiteral("message")).toString()));
            return;
        }
    }

    if (line.startsWith(QStringLiteral("state=done"))) {
        m_helperDone = true;
        const QRegularExpressionMatch progressMatch = QRegularExpression(QStringLiteral("progress=([0-9.]+)")).match(line);
        if (progressMatch.hasMatch()) {
            const double progress = qBound(0.0, progressMatch.captured(1).toDouble(), 100.0);
            if (m_mode == QStringLiteral("pull")) {
                m_hasPulledProgress = true;
                m_pulledProgress = progress;
                setState(m_running, QStringLiteral("已拉取到 %1%").arg(QString::number(progress, 'f', 2)));
            } else {
                setState(m_running, QStringLiteral("已同步到 %1%").arg(progressMatch.captured(1)));
            }
        } else {
            setState(m_running, m_mode == QStringLiteral("pull") ? QStringLiteral("进度已拉取") : QStringLiteral("进度已同步"));
        }
        return;
    }
    if (line.startsWith(QStringLiteral("state=error"))) {
        m_helperFailed = true;
        const QRegularExpressionMatch messageMatch = QRegularExpression(QStringLiteral("message=(.*)")).match(line);
        const QString helperErrorMessage = messageMatch.hasMatch() ? messageMatch.captured(1).trimmed() : QString();
        setState(m_running, failureText(helperErrorMessage));
    }
}

void ProgressSyncStore::finishProcess(int exitCode, QProcess::ExitStatus exitStatus) {
    if (m_requestFinished || (!m_running && !m_processStartFailed)) {
        return;
    }
    m_requestFinished = true;
    m_processTimeoutTimer.stop();
    flushOutput(m_stdoutBuffer, true);
    flushOutput(m_stderrBuffer, false);

    const bool succeeded = !m_processStartFailed
        && !m_timedOut
        && !m_helperFailed
        && m_helperDone
        && exitStatus == QProcess::NormalExit
        && exitCode == 0;
    if (!succeeded) {
        if (!m_processStartFailed && !m_timedOut && !m_helperFailed) {
            setState(false, failureText());
        } else {
            setState(false, m_statusText);
        }
        emit progressFinished(m_bookId, m_elapsedSeconds, false);
        return;
    }

    setState(false, m_statusText);
    if (m_mode == QStringLiteral("pull") && m_hasPulledProgress) {
        emit progressPulled(m_bookId, m_pulledProgress);
        if (!m_pulledPosition.isEmpty()) {
            emit progressPulledDetailed(m_bookId, m_pulledPosition);
        }
    } else if (m_mode == QStringLiteral("push")) {
        emit progressSynced(m_bookId, m_elapsedSeconds);
    }
    emit progressFinished(m_bookId, m_elapsedSeconds, true);
}

QString ProgressSyncStore::failureText(const QString &reason) const {
    const QString trimmedReason = reason.trimmed();
    if (m_mode == QStringLiteral("pull")) {
        return trimmedReason.isEmpty()
            ? QStringLiteral("进度拉取失败")
            : QStringLiteral("进度拉取失败：%1").arg(trimmedReason);
    }
    return trimmedReason.isEmpty()
        ? QStringLiteral("进度同步失败")
        : QStringLiteral("进度同步失败：%1").arg(trimmedReason);
}

void ProgressSyncStore::resetRequestState() {
    m_processTimeoutTimer.stop();
    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    m_helperDone = false;
    m_helperFailed = false;
    m_timedOut = false;
    m_processStartFailed = false;
    m_requestFinished = false;
    m_hasPulledProgress = false;
    m_pulledProgress = 0.0;
    m_pulledPosition.clear();
    m_elapsedSeconds = 0;
}
