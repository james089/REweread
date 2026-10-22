#include "native_address.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <functional>
#include <iostream>
#include <stdexcept>

void check(bool good) { if (!good) throw std::runtime_error("address lease test failed"); }
void write(const QString &path, const QByteArray &bytes) {
    QFile f(path); check(f.open(QIODevice::WriteOnly)); check(f.write(bytes) == bytes.size());
}
QByteArray read(const QString &path) { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
void rejects(const std::function<void()> &fn) { bool failed = false; try { fn(); } catch (...) { failed = true; } check(failed); }
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir temporary; check(temporary.isValid());
        const auto root = temporary.path();
        NativeAddressEnvironment env{root + "/ip", root + "/version", root + "/online"};
        write(env.versionFile, "20260827113527\n"); write(env.chargerFile, "0\n");
        write(env.ipProgram, R"SH(#!/bin/sh
task_root=${0%/*}
case "$*" in
'-4 addr show dev usb1') /bin/cat "$task_root/addresses" ;;
'addr add 10.11.99.1/27 dev usb1') printf '    inet 10.11.99.1/27 scope global usb1\n' > "$task_root/addresses"; printf 'add\n' >> "$task_root/events" ;;
'addr del 10.11.99.1/27 dev usb1') : > "$task_root/addresses"; printf 'del\n' >> "$task_root/events" ;;
*) exit 1 ;;
esac
)SH");
        check(QFile::setPermissions(env.ipProgram, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(root + "/addresses", "    inet 10.11.99.1/27 scope global usb1\n");
        { NativeAddress lease(root, env); lease.prepare(); check(lease.restore()); }
        check(read(root + "/events").isEmpty()); // Never remove a preexisting stock address.
        write(root + "/addresses", "");
        { NativeAddress lease(root, env); lease.prepare(); check(lease.restore()); }
        check(read(root + "/events") == "add\ndel\n");
        check(!QFileInfo::exists(root + "/owned-import-address"));
        write(env.chargerFile, "1\n");
        { NativeAddress lease(root, env); lease.prepare(); check(lease.restore()); }
        check(QFileInfo::exists(root + "/owned-import-address")); // Preserve cable/SSH attached mid-session.
        write(env.chargerFile, "0\n");
        check(NativeAddress::recover(root, env));
        check(!QFileInfo::exists(root + "/owned-import-address"));
        write(root + "/addresses", "    inet 192.0.2.1/24 scope global usb1\n");
        { NativeAddress lease(root, env); rejects([&] { lease.prepare(); }); }
        check(read(root + "/addresses").contains("192.0.2.1"));
        write(env.versionFile, "unknown\n");
        { NativeAddress lease(root, env); rejects([&] { lease.prepare(); }); }
        write(root + "/owned-import-address", "invalid-marker");
        check(!NativeAddress::recover(root, env));
        std::cout << "owned USB address lease, preservation, crash recovery and firmware refusal ok\n";
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
