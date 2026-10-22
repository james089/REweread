#include "native_address.h"
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <stdexcept>

namespace {
const QByteArray markerBytes("weread-downloader:usb1:10.11.99.1/27:v1\n");
QByteArray readSmall(const QString &path) {
    QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {};
    return f.read(512).trimmed();
}
bool ip(const NativeAddressEnvironment &environment, const QStringList &args, QByteArray *output = nullptr) {
    QProcess process;
    QProcessEnvironment env; env.insert("PATH", "/usr/sbin:/usr/bin:/sbin:/bin"); env.insert("LC_ALL", "C");
    process.setProcessEnvironment(env); process.setProgram(environment.ipProgram); process.setArguments(args);
    process.setStandardErrorFile(QProcess::nullDevice()); process.start();
    if (!process.waitForFinished(3000)) { process.kill(); process.waitForFinished(500); return false; }
    auto bytes = process.readAllStandardOutput();
    if (bytes.size() > 8192) return false;
    if (output) *output = bytes;
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}
bool hasAddress(const QByteArray &text) {
    return text.contains("inet 10.11.99.1/27 ") || text.contains("inet 10.11.99.1/29 ");
}
bool restoreMarker(const QString &marker, const NativeAddressEnvironment &environment) {
    QFileInfo info(marker);
    if (!info.exists() && !info.isSymLink()) return true;
    if (info.isSymLink() || !info.isFile() || info.size() > 128) return false;
    QFile file(marker);
    if (!file.open(QIODevice::ReadOnly) || file.readAll() != markerBytes) return false;
    file.close();
    QByteArray addresses;
    if (!ip(environment, {"-4", "addr", "show", "dev", "usb1"}, &addresses)) return false;
    if (!hasAddress(addresses)) return QFile::remove(marker);
    // If a cable was attached during an initially cable-free import, preserve
    // USB/SSH. The next operation without a cable recovers this owned address.
    const auto online = readSmall(environment.chargerFile);
    if (online != "0") return true;
    if (!addresses.contains("inet 10.11.99.1/27 ")) return false;
    if (!ip(environment, {"addr", "del", "10.11.99.1/27", "dev", "usb1"})) return false;
    return QFile::remove(marker);
}
}
NativeAddress::NativeAddress(const QString &root, const NativeAddressEnvironment &environment)
    : m_marker(root + "/owned-import-address"), m_environment(environment) {}
NativeAddress::~NativeAddress() { restore(); }
bool NativeAddress::recover(const QString &root, const NativeAddressEnvironment &environment) {
    return restoreMarker(root + "/owned-import-address", environment);
}
void NativeAddress::prepare() {
    if (readSmall(m_environment.versionFile) != "20260827113527") throw std::runtime_error("unsupported_firmware");
    if (!restoreMarker(m_marker, m_environment)) throw std::runtime_error("address_restore_failed");
    QByteArray addresses;
    if (!ip(m_environment, {"-4", "addr", "show", "dev", "usb1"}, &addresses)) throw std::runtime_error("usb_interface_unavailable");
    if (hasAddress(addresses)) return;
    if (addresses.contains("inet ")) throw std::runtime_error("unknown_usb_configuration");
    QSaveFile file(m_marker); file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFile::ReadOwner | QFile::WriteOwner)
        || file.write(markerBytes) != markerBytes.size() || !file.commit()) throw std::runtime_error("address_marker_failed");
    if (!ip(m_environment, {"addr", "add", "10.11.99.1/27", "dev", "usb1"})) {
        // Do not claim an address added by a racing stock network service.
        QFile::remove(m_marker); throw std::runtime_error("address_prepare_failed");
    }
    m_owned = true;
}
bool NativeAddress::restore() {
    if (!m_owned) return true;
    const bool good = restoreMarker(m_marker, m_environment);
    if (good) m_owned = false;
    return good;
}
