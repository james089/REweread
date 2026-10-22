#include "epub.h"
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
