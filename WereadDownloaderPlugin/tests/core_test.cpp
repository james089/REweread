#include "epub.h"
#include "cache_cleanup.h"
#include "reading_progress.h"
#include "import_registry.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QTemporaryDir>
#include <functional>
#include <iostream>
#include <stdexcept>

using namespace WereadDownloader;
static void check(bool value) { if (!value) throw std::runtime_error("test assertion failed"); }
static void rejects(const std::function<void()> &fn) {
    bool rejected = false;
    try { fn(); } catch (const std::exception &) { rejected = true; }
    check(rejected);
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        check(argc == 2);
        QJsonObject progressMetadata{{"lastOpenedPage",1}};
        QJsonObject progressContent{{"pageCount",3},{"pages",QJsonArray{"a","b","c"}}};
        check(nativeReadingPercent(progressMetadata,progressContent)==50);
        progressMetadata["lastOpenedPage"]=2; check(nativeReadingPercent(progressMetadata,progressContent)==100);
        progressMetadata["lastOpenedPage"]=0; check(nativeReadingPercent(progressMetadata,progressContent)==0);
        progressMetadata["lastOpenedPage"]=3; check(nativeReadingPercent(progressMetadata,progressContent)<0);
        progressMetadata["lastOpenedPage"]=0.5; check(nativeReadingPercent(progressMetadata,progressContent)<0);
        progressMetadata["lastOpenedPage"]=0; progressMetadata["parent"]="trash"; check(nativeReadingPercent(progressMetadata,progressContent)<0);
        const QString fixtures = QString::fromLocal8Bit(argv[1]);
        QTemporaryDir temporary; check(temporary.isValid());
        const auto stamped = temporary.path() + "/stamped.epub";
        const auto twice = temporary.path() + "/twice.epub";
        Epub original(fixtures + "/valid.epub");
        check(original.sourceBookId().isEmpty());
        original.stamp("synthetic-book-1", stamped);
        check(Epub(stamped).sourceBookId() == "synthetic-book-1");
        Epub(stamped).stamp("synthetic-book-1", twice);
        check(Epub(twice).sourceBookId() == "synthetic-book-1");
        rejects([&] { Epub(stamped).stamp("different-id", twice); });
        check(!original.hasCover());
        const auto covered = temporary.path() + "/covered.epub";
        const auto png = QByteArray::fromBase64("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+jRZkAAAAASUVORK5CYII=");
        original.stamp("synthetic-book-1", covered, png);
        check(Epub(covered).hasCover() && Epub(covered).sourceBookId() == "synthetic-book-1");
        const auto coveredSize = QFileInfo(covered).size();
        Epub(covered).stamp("synthetic-book-1", covered, png);
        check(Epub(covered).hasCover() && QFileInfo(covered).size() == coveredSize);
        rejects([&] { Epub(fixtures + "/valid.epub").stamp("synthetic", twice, "not an image"); });
        rejects([&] { original.stamp("../escape", twice); });
        for (const auto &name : {"traversal", "duplicate", "missing-resource", "bad-spine", "checksum", "ambiguous", "encrypted"}) {
            rejects([&] { Epub epub(fixtures + "/" + name + ".epub"); epub.sourceBookId(); });
        }
        // Failed tagging must not overwrite an unrelated, existing output.
        QFile before(twice); check(before.open(QIODevice::ReadOnly)); const auto bytes = before.readAll(); before.close();
        rejects([&] { original.stamp("bad/id", twice); });
        check(before.open(QIODevice::ReadOnly)); check(before.readAll() == bytes); before.close();
        {
            ImportRegistry rejected(temporary.path() + "/rejected");
            rejected.reserve("synthetic-book-1", QString(64, 'a'));
            rejects([&] { rejected.rejectSubmission("synthetic-book-1"); });
            rejected.prepareSubmission("synthetic-book-1", stamped);
            rejected.rejectSubmission("synthetic-book-1");
            check(rejected.record("synthetic-book-1").value("state") == "failed");
            rejected.reserve("synthetic-book-1", QString(64, 'a'));
        }
        const auto root = temporary.path() + "/registry";
        const auto scope = QString(64, 'a');
        const QString uuid = "00000000-0000-4000-8000-000000000001";
        const QString uuid2 = "00000000-0000-4000-8000-000000000002";
        {
            ImportRegistry registry(root);
            rejects([&] { ImportRegistry other(root); });
            registry.reserve("synthetic-book-1", scope);
            rejects([&] { registry.reserve("synthetic-book-1", scope); });
            rejects([&] { registry.prepareSubmission("synthetic-book-1", fixtures + "/valid.epub"); });
            registry.prepareSubmission("synthetic-book-1", stamped);
            check(registry.record("synthetic-book-1").value("state") == "pending");
            rejects([&] { registry.failBeforeSubmission("synthetic-book-1"); });
            rejects([&] { registry.reconcile({{}, false}); });
            registry.reconcile({{}, true});
            check(registry.record("synthetic-book-1").value("state") == "pending");
        }
        {
            ImportRegistry registry(root);
            rejects([&] { registry.reserve("synthetic-book-1", QString(64, 'b')); });
            registry.reconcile({{{uuid, "synthetic-book-1", false}}, true});
            check(registry.record("synthetic-book-1").value("uuid") == uuid);
            rejects([&] { registry.reserve("synthetic-book-1", scope); });
            rejects([&] { registry.reconcile({{}, false}); });
            check(registry.record("synthetic-book-1").value("state") == "imported");
            registry.reconcile({{{uuid, "synthetic-book-1", false}, {uuid2, "synthetic-book-1", false}}, true});
            check(registry.record("synthetic-book-1").value("state") == "conflict");
            registry.reconcile({{{uuid, "synthetic-book-1", false}}, true});
            check(registry.record("synthetic-book-1").value("state") == "imported");
            registry.reconcile({{{uuid, "synthetic-book-1", true}}, true});
            check(registry.record("synthetic-book-1").value("state") == "removed");
            registry.reserve("synthetic-book-1", scope);
            registry.failBeforeSubmission("synthetic-book-1");
            registry.reserve("synthetic-book-1", scope); // simulate crash before submission
        }
        {
            ImportRegistry registry(root);
            check(registry.record("synthetic-book-1").value("state") == "failed");
            registry.reserve("synthetic-book-1", scope);
        }
        {
            ImportRegistry rebuilt(temporary.path() + "/rebuilt");
            rebuilt.reconcile({{{uuid, "synthetic-book-1", false}, {uuid2, {}, false}}, true});
            check(rebuilt.record("synthetic-book-1").value("state") == "imported");
            check(rebuilt.record("historical-book").isEmpty());
        }
        const auto account = temporary.path() + "/cache-account";
        check(QDir().mkpath(account + "/books/synthetic-clean"));
        check(QDir().mkpath(account + "/books/synthetic-other"));
        auto write = [&](const QString &name, const QByteArray &bytes) { QFile f(name); check(f.open(QIODevice::WriteOnly)); check(f.write(bytes) == bytes.size()); };
        write(account + "/books/synthetic-clean/full.epub", "synthetic cache");
        write(account + "/books/synthetic-other/full.epub", "preserve sibling");
        write(account + "/book-status.json", R"({"synthetic-clean":{"downloadState":"full","fullFile":"old","title":"synthetic title"},"synthetic-other":{"downloadState":"full"}})");
        clearDownloadCache(account, "synthetic-clean");
        check(!QFileInfo::exists(account + "/books/synthetic-clean"));
        check(QFileInfo::exists(account + "/books/synthetic-other/full.epub"));
        QFile statusFile(account + "/book-status.json"); check(statusFile.open(QIODevice::ReadOnly));
        const auto statusRows = QJsonDocument::fromJson(statusFile.readAll()).object();
        check(statusRows.value("synthetic-clean").toObject().value("downloadState") == "remote");
        check(statusRows.value("synthetic-clean").toObject().value("fullFile").toString().isEmpty());
        check(statusRows.value("synthetic-other").toObject().value("downloadState") == "full");
        rejects([&] { clearDownloadCache(account, "../escape"); });
        check(QFile::link(account + "/books/synthetic-other", account + "/books/synthetic-clean"));
        rejects([&] { clearDownloadCache(account, "synthetic-clean"); });
        check(QFileInfo::exists(account + "/books/synthetic-other/full.epub"));
        QFile corrupt(root + "/imports.json"); check(corrupt.open(QIODevice::WriteOnly));
        corrupt.write("{broken"); corrupt.close();
        rejects([&] { ImportRegistry registry(root); });
        check(!(QFileInfo(root).permissions() & (QFile::ReadOther | QFile::WriteOther)));
        check(!(QFileInfo(stamped).permissions() & (QFile::ReadOther | QFile::WriteOther)));
        std::cout << "EPUB source identity and import transaction behavior ok\n";
    } catch (const std::exception &error) {
        std::cerr << "core test failed: " << error.what() << '\n'; return 1;
    }
}
