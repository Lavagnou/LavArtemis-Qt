// art:// links and .art files (app/cli/artlink): the formats of the Android client
// (AddComputerManually, ShortcutHelper), and the command line each becomes.

#include "../../app/cli/artlink.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <cstdio>

using namespace ArtLink;

namespace {
int failures = 0;

void check(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

QString writeFile(const QTemporaryDir& dir, const QString& name, const QByteArray& content)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    file.open(QIODevice::WriteOnly);
    file.write(content);
    return path;
}

void testLaunchLinks()
{
    Target t = parseUrl("art://launch?host_uuid=AB-CD&host_name=Atlas&app_uuid=12-34&app_name=Cyber%20Game&app_id=7");
    check(t.kind == Target::Launch, "a launch link");
    check(t.hostUuid == "AB-CD" && t.hostName == "Atlas" && t.appUuid == "12-34" && t.appName == "Cyber Game" && t.appId == "7",
          "every field, decoded");
    check(toArguments(t) == QStringList({ "stream", "AB-CD", "12-34" }), "UUIDs first: they survive a rename");

    t = parseUrl("art://launch?host_name=Atlas&app_name=Bureau");
    check(toArguments(t) == QStringList({ "stream", "Atlas", "Bureau" }), "names when there is no UUID");
    t = parseUrl("art://launch?host_name=Atlas&app_id=12");
    check(toArguments(t) == QStringList({ "stream", "Atlas", "12" }), "an app id last");
    t = parseUrl("ART://LAUNCH?host_name=Atlas&app_name=Bureau");
    check(t.kind == Target::Launch, "scheme and host in any case");

    t = parseUrl("art://launch?host_uuid=AB");
    check(t.kind == Target::Invalid && !t.error.isEmpty(), "no app: invalid, with a reason");
    t = parseUrl("art://launch?app_name=Bureau");
    check(t.kind == Target::Invalid && !t.error.isEmpty(), "no host: invalid, with a reason");
    t = parseUrl("http://launch?host_name=Atlas&app_name=Bureau");
    check(t.kind == Target::Invalid, "another scheme");
}

void testPairLinks()
{
    Target t = parseUrl("art://192.168.1.24:47989?pin=1234&passphrase=abc%20def");
    check(t.kind == Target::Pair && t.host == "192.168.1.24:47989" && t.pin == "1234" && t.passphrase == "abc def", "a pairing link");
    check(toArguments(t) == QStringList({ "pair", "192.168.1.24:47989", "--pin", "1234", "--passphrase", "abc def" }),
          "pair with the OTP passphrase");
    t = parseUrl("art://atlas?pin=5678");
    check(t.kind == Target::Pair && toArguments(t) == QStringList({ "pair", "atlas", "--pin", "5678" }), "no passphrase, no port");
    t = parseUrl("art://[::1]:47989?pin=1234");
    check(t.kind == Target::Pair && t.host == "[::1]:47989", "an IPv6 address keeps its brackets before the port");
    t = parseUrl("art://atlas");
    check(t.kind == Target::Invalid && !t.error.isEmpty(), "a pairing link without a PIN");
}

void testFiles(const QTemporaryDir& dir)
{
    Target t = parseFile(writeFile(dir, "ok.art",
        "# Artemis app entry\n[host_uuid] AB-CD\n[host_name] Atlas\n\n[app_name]  Cyber Game  \n[future_key] x\n"));
    check(t.kind == Target::Launch && t.hostUuid == "AB-CD" && t.appName == "Cyber Game", "a .art file, comments and unknown keys ignored");
    check(toArguments(t) == QStringList({ "stream", "AB-CD", "Cyber Game" }), "becomes a stream command");

    t = parseFile(writeFile(dir, "bad.art", "not a shortcut\n"));
    check(t.kind == Target::Invalid && t.error.contains("bad.art"), "a malformed line: invalid, naming the file");
    t = parseFile(writeFile(dir, "noapp.art", "[host_name] Atlas\n"));
    check(t.kind == Target::Invalid, "a file without an app");
    t = parseFile(dir.filePath("missing.art"));
    check(t.kind == Target::Invalid && !t.error.isEmpty(), "a file that cannot be read");
}

void testRewrite(const QTemporaryDir& dir)
{
    QString error;
    QStringList out = rewriteArguments({ "app.exe", "art://launch?host_name=Atlas&app_name=Bureau" }, &error);
    check(out == QStringList({ "app.exe", "stream", "Atlas", "Bureau" }) && error.isEmpty(), "a link in first place is rewritten");

    const QString file = writeFile(dir, "Bureau.art", "[host_name] Atlas\n[app_name] Bureau\n");
    out = rewriteArguments({ "app.exe", file }, &error);
    check(out == QStringList({ "app.exe", "stream", "Atlas", "Bureau" }), "a .art file in first place is rewritten");

    // Only argv[1]: further along it belongs to an ordinary command line
    const QStringList cli { "app.exe", "stream", "Atlas", "Mon jeu.art" };
    error.clear();
    out = rewriteArguments(cli, &error);
    check(out == cli && error.isEmpty(), "an app named *.art is not a .art file");
    const QStringList cliLink { "app.exe", "stream", "Atlas", "art://launch?host_name=X&app_name=Y" };
    check(rewriteArguments(cliLink, &error) == cliLink, "nor a link in third place");

    out = rewriteArguments({ "app.exe", "art://launch?host_name=Atlas" }, &error);
    check(!error.isEmpty() && out == QStringList({ "app.exe", "art://launch?host_name=Atlas" }),
          "a malformed link: the error, and the arguments as they were (main.cpp starts normally then)");
    check(rewriteArguments({ "app.exe" }, &error) == QStringList({ "app.exe" }), "no argument");
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    testLaunchLinks();
    testPairLinks();
    testFiles(dir);
    testRewrite(dir);
    if (failures == 0) {
        std::printf("tst_artlink: all checks passed\n");
    }
    return failures == 0 ? 0 : 1;
}
