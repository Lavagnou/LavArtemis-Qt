#pragma once

#include <QString>
#include <QtGlobal>

// Test build only (CONFIG+=lav_test): the allowlist of addresses the client may talk to.
// Test hosts run on this PC, on loopback, with a base port in 40000-44999. The real
// LavApollo of this PC listens on 45989 and must never be reached, nor any LAN host.
namespace TestGuard {

constexpr quint16 kFirstTestPort = 40000;
constexpr quint16 kLastTestPort = 44999;

bool isAllowedAddress(const QString& host, quint16 port);

// French, informal: shown when the user types an address that is refused.
QString refusalMessage();

}
