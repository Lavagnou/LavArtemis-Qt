#include "testguard.h"

bool TestGuard::isAllowedAddress(const QString& host, quint16 port)
{
    if (port < kFirstTestPort || port > kLastTestPort) {
        return false;
    }

    QString h = host.trimmed();
    if (h.startsWith('[') && h.endsWith(']')) {
        h = h.mid(1, h.size() - 2);
    }

    return h == QLatin1String("127.0.0.1") ||
           h == QLatin1String("::1") ||
           h.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0;
}

QString TestGuard::refusalMessage()
{
    return QString::fromUtf8("Le client de test ne parle qu'aux hôtes de test (127.0.0.1, ports 40000-44999).");
}
