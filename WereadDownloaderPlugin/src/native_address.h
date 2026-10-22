#pragma once
#include <QString>
struct NativeAddressEnvironment {
    QString ipProgram = QStringLiteral("/usr/sbin/ip");
    QString versionFile = QStringLiteral("/etc/version");
    QString chargerFile = QStringLiteral("/sys/class/power_supply/max77818-charger/online");
};

// Exact .172 USB-local address lease; never changes Wi-Fi, routes or addresses
// that were present before this operation. The marker survives worker crashes.
class NativeAddress {
public:
    explicit NativeAddress(const QString &privateRoot, const NativeAddressEnvironment &environment = {});
    ~NativeAddress();
    void prepare();
    bool restore();
    static bool recover(const QString &privateRoot, const NativeAddressEnvironment &environment = {});
private:
    QString m_marker;
    bool m_owned = false;
    NativeAddressEnvironment m_environment;
};
