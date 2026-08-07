#pragma once

#include <QString>
#include <QStringList>

/**
 * @brief art:// links and .art launcher files.
 *
 * Ports the Artemis deep links (AddComputerManually.java:283) and shortcut
 * files (utils/ShortcutHelper.java:225) to desktop, where they are arguably
 * more useful than on Android: desktop shortcuts, browser links, and non-Steam
 * game entries all end up invoking us with one of these.
 *
 * Two forms are understood, matching Android exactly so a link or file works on
 * either client:
 *
 *   art://launch?host_uuid=..&host_name=..&app_uuid=..&app_name=..&app_id=..
 *   art://<host>[:<port>]?pin=<pin>&passphrase=<passphrase>
 *
 * A .art file is the same launch payload in Android's line format:
 *
 *   # Artemis app entry
 *   [host_uuid] 01234567-89ab-cdef-0123-456789abcdef
 *   [host_name] GAMING-PC
 *   [app_uuid]  fedcba98-7654-3210-fedc-ba9876543210
 *   [app_name]  Cyberpunk 2077
 *
 * Rather than adding a parallel launch path, these are translated into the
 * command line the app already knows how to run, so they inherit host seeking,
 * wake-on-LAN and the existing segue UI for free.
 */
namespace ArtLink {

struct Target {
    enum Kind {
        Invalid,
        Launch,
        Pair,
    };

    Kind kind = Invalid;

    // Launch. Any of these may be empty; at least one host field and one app
    // field has to be present for the target to be usable.
    QString hostUuid;
    QString hostName;
    QString appUuid;
    QString appName;
    QString appId;

    // Pair
    QString host;
    QString pin;
    QString passphrase;

    // Set when kind is Invalid, to say what was wrong with the input.
    QString error;
};

// True if this argument is an art:// URL or a path ending in .art.
bool isArtArgument(const QString& arg);

Target parseUrl(const QString& url);
Target parseFile(const QString& path);

// Turns a Target into the equivalent argument list, e.g. {"stream", host, app}.
// Empty if the target is not usable.
QStringList toArguments(const Target& target);

// Replaces an art:// URL or .art path in the argument list with the equivalent
// command, leaving the list alone if it contains neither. On a malformed input
// the list is returned unchanged and error is set.
QStringList rewriteArguments(const QStringList& args, QString* error);

}
