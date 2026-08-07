#include "artlink.h"

#include <QFile>
#include <QObject>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>

namespace ArtLink {

namespace {

// Key names are shared with the Android client (utils/ShortcutHelper.java:34).
const char* k_KeyHostUuid = "host_uuid";
const char* k_KeyHostName = "host_name";
const char* k_KeyAppUuid = "app_uuid";
const char* k_KeyAppName = "app_name";
const char* k_KeyAppId = "app_id";

Target buildLaunchTarget(const QString& hostUuid, const QString& hostName,
                         const QString& appUuid, const QString& appName,
                         const QString& appId)
{
    Target target;

    if (hostUuid.isEmpty() && hostName.isEmpty()) {
        target.error = QObject::tr("No host was specified.");
        return target;
    }

    if (appUuid.isEmpty() && appName.isEmpty() && appId.isEmpty()) {
        target.error = QObject::tr("No app was specified.");
        return target;
    }

    target.kind = Target::Launch;
    target.hostUuid = hostUuid;
    target.hostName = hostName;
    target.appUuid = appUuid;
    target.appName = appName;
    target.appId = appId;
    return target;
}

} // anonymous namespace

bool isArtArgument(const QString& arg)
{
    return arg.startsWith(QLatin1String("art://"), Qt::CaseInsensitive) ||
           arg.endsWith(QLatin1String(".art"), Qt::CaseInsensitive);
}

Target parseUrl(const QString& url)
{
    Target target;

    QUrl parsed(url);
    if (!parsed.isValid() || parsed.scheme().compare(QLatin1String("art"), Qt::CaseInsensitive) != 0) {
        target.error = QObject::tr("%1 is not a valid art:// link.").arg(url);
        return target;
    }

    QUrlQuery query(parsed);

    // Android distinguishes the two forms by the host: the literal word
    // "launch" means a shortcut, anything else is a machine to pair with
    // (AddComputerManually.java:288).
    if (parsed.host().compare(QLatin1String("launch"), Qt::CaseInsensitive) == 0) {
        return buildLaunchTarget(query.queryItemValue(k_KeyHostUuid, QUrl::FullyDecoded),
                                 query.queryItemValue(k_KeyHostName, QUrl::FullyDecoded),
                                 query.queryItemValue(k_KeyAppUuid, QUrl::FullyDecoded),
                                 query.queryItemValue(k_KeyAppName, QUrl::FullyDecoded),
                                 query.queryItemValue(k_KeyAppId, QUrl::FullyDecoded));
    }

    if (parsed.host().isEmpty()) {
        target.error = QObject::tr("%1 does not name a host.").arg(url);
        return target;
    }

    QString pin = query.queryItemValue(QLatin1String("pin"), QUrl::FullyDecoded);
    if (pin.isEmpty()) {
        target.error = QObject::tr("%1 is a pairing link but carries no PIN.").arg(url);
        return target;
    }

    target.kind = Target::Pair;

    // QUrl::host() strips the brackets from an IPv6 literal, so they have to go
    // back on before a port is appended. Without them "::1" plus port 47989
    // becomes "::1:47989", which is not an address anything can parse. This is
    // the same shape NvAddress::toString() produces.
    QString host = parsed.host();
    if (parsed.port() != -1) {
        host = host.contains(QLatin1Char(':'))
                   ? QStringLiteral("[%1]:%2").arg(host).arg(parsed.port())
                   : QStringLiteral("%1:%2").arg(host).arg(parsed.port());
    }
    target.host = host;
    target.pin = pin;
    target.passphrase = query.queryItemValue(QLatin1String("passphrase"), QUrl::FullyDecoded);
    return target;
}

Target parseFile(const QString& path)
{
    Target target;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        target.error = QObject::tr("Unable to read %1.").arg(path);
        return target;
    }

    QString hostUuid, hostName, appUuid, appName, appId;

    // Line format is "[key] value", with # comments, per ShortcutHelper.java:243.
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        int closing = line.indexOf(QLatin1Char(']'));
        if (!line.startsWith(QLatin1Char('[')) || closing < 0) {
            target.error = QObject::tr("%1 is not a valid .art file.").arg(path);
            return target;
        }

        QString key = line.mid(1, closing - 1).trimmed();
        QString value = line.mid(closing + 1).trimmed();
        if (value.isEmpty()) {
            continue;
        }

        if (key == QLatin1String(k_KeyHostUuid)) {
            hostUuid = value;
        } else if (key == QLatin1String(k_KeyHostName)) {
            hostName = value;
        } else if (key == QLatin1String(k_KeyAppUuid)) {
            appUuid = value;
        } else if (key == QLatin1String(k_KeyAppName)) {
            appName = value;
        } else if (key == QLatin1String(k_KeyAppId)) {
            appId = value;
        }
        // Unknown keys are ignored so newer files stay readable here.
    }

    return buildLaunchTarget(hostUuid, hostName, appUuid, appName, appId);
}

QStringList toArguments(const Target& target)
{
    switch (target.kind) {
    case Target::Launch: {
        // UUIDs first: they survive the host or app being renamed, and
        // ComputerSeeker::matchComputer already accepts a UUID in place of a
        // name or address.
        QString host = !target.hostUuid.isEmpty() ? target.hostUuid : target.hostName;

        QString app = target.appUuid;
        if (app.isEmpty()) {
            app = !target.appName.isEmpty() ? target.appName : target.appId;
        }

        return { QStringLiteral("stream"), host, app };
    }

    case Target::Pair: {
        QStringList args = { QStringLiteral("pair"), target.host,
                             QStringLiteral("--pin"), target.pin };
        if (!target.passphrase.isEmpty()) {
            args << QStringLiteral("--passphrase") << target.passphrase;
        }
        return args;
    }

    case Target::Invalid:
    default:
        return {};
    }
}

QStringList rewriteArguments(const QStringList& args, QString* error)
{
    for (int i = 1; i < args.count(); i++) {
        if (!isArtArgument(args.at(i))) {
            continue;
        }

        const QString& arg = args.at(i);
        Target target = arg.startsWith(QLatin1String("art://"), Qt::CaseInsensitive)
                            ? parseUrl(arg)
                            : parseFile(arg);

        if (target.kind == Target::Invalid) {
            if (error != nullptr) {
                *error = target.error;
            }
            return args;
        }

        // Keep argv[0] so anything that reports the program name still works,
        // and drop the rest: mixing a link with unrelated CLI options has no
        // meaning and would only produce confusing parses.
        QStringList rewritten;
        rewritten << args.first();
        rewritten << toArguments(target);
        return rewritten;
    }

    return args;
}

}
