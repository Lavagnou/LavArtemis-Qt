// The in-app update's rules (app/backend/updaterules), the same as Android's UpdateChecker:
// LavArtemis' own numbering stays below 20.0.0 and beats the old 21.x one, a stable release beats
// a prerelease of the same base, the Dev app only takes "-dev." releases, the Windows asset is the
// installer, and the cleanup of downloaded installers stays in its own channel.

#include "../../app/backend/updaterules.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>

#include <cstdio>

using namespace UpdateRules;

namespace {
int failures = 0;

void check(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

int sign(int v)
{
    return v < 0 ? -1 : (v > 0 ? 1 : 0);
}

void testCompare()
{
    check(sign(compareVersionStrings("0.7.0", "0.7.0")) == 0, "equal versions");
    check(sign(compareVersionStrings("0.7.0", "0.7.1")) < 0, "patch");
    check(sign(compareVersionStrings("0.10.0", "0.9.9")) > 0, "numbers, not text");
    check(sign(compareVersionStrings("1.0", "1.0.0")) == 0, "a missing place counts as 0");
    check(sign(compareVersionStrings("0.7.0-dev.3", "0.7.0-dev.10")) < 0, "dev builds by number");
    check(sign(compareVersionStrings("0.7.0-dev.9", "0.7.0")) < 0, "a stable release beats its prereleases");
    check(sign(compareVersionStrings("0.7.0", "0.7.0-dev.9")) > 0, "and the other way round");
    check(sign(compareVersionStrings("0.7.1-dev.1", "0.7.0")) > 0, "a newer base beats an older stable");
    check(sign(compareVersionStrings("0.7.0-dev.3-ci.42", "0.7.0-dev.3")) == 0, "a CI build of the same Dev release");
    // The new numbering (below 20.0.0) is newer than the old one (21.x), whatever the numbers
    check(sign(compareVersionStrings("21.8.0-dev.9", "0.7.0-dev.1")) < 0, "21.x Dev is older than 0.7 Dev");
    check(sign(compareVersionStrings("21.7.1", "0.7.0-dev.1")) < 0, "21.x stable is older than 0.7 Dev");
    check(sign(compareVersionStrings("0.7.0", "21.7.1")) > 0, "0.7 stable is newer than 21.x");
    check(sign(compareVersionStrings("19.9.9", "20.0.0")) > 0, "19.9.9 is the newest of the new numbering");
    check(sign(compareVersionStrings("1.0.0", "20.0.0")) > 0, "20.0.0 is old numbering");
    check(sign(compareVersionStrings("20.0.0", "21.7.1")) < 0, "the old numbering still compares within itself");
    check(sign(compareVersionStrings("21.8.0", "21.7.1")) > 0, "21.8 after 21.7");
}

QJsonObject release(const char* tag, bool draft = false)
{
    return QJsonObject { { "tag_name", tag }, { "draft", draft } };
}

void testPickDevRelease()
{
    QJsonArray releases {
        release("v0.7.0-dev.5", true),        // draft
        release("v0.7.0-dev.4-ci.12"),         // a CI build from the dev branch
        release("v1.0.0"),                     // stable
        release("v0.7.0-dev.4"),
        release("v0.7.0-dev.3"),
    };
    check(pickDevRelease(releases) == 3, "the newest Dev release, not a draft, a CI build or a stable one");
    check(pickDevRelease(QJsonArray { release("v1.0.0"), release("v0.6.0") }) == -1, "none: -1");
    check(pickDevRelease(QJsonArray()) == -1, "empty list: -1");
    check(isDevVersion("0.7.0-dev.3") && !isDevVersion("0.7.0") && !isDevVersion("0.7.0-ci.4"), "isDevVersion");
}

void testAsset()
{
    QJsonObject rel {
        { "assets", QJsonArray {
            QJsonObject { { "name", "LavArtemis-v0.7.0-dev.4-android-arm64-v8a.apk" }, { "browser_download_url", "https://x/apk" } },
            QJsonObject { { "name", "LavArtemis-v0.7.0-dev.4-win-x64.zip" }, { "browser_download_url", "https://x/zip" } },
            QJsonObject { { "name", "LavArtemis-v0.7.0-dev.4-win-installer.exe" }, { "browser_download_url", "https://x/exe" } },
        } },
    };
    check(findAssetDownloadUrl(rel, "-win-installer.exe") == "https://x/exe", "the installer, never the portable zip");
    check(findAssetDownloadUrl(rel, ".dmg").isEmpty(), "no asset for this platform: empty");
    check(findAssetDownloadUrl(rel, QString()).isEmpty(), "no suffix: empty");
    check(findAssetDownloadUrl(QJsonObject(), "-win-installer.exe").isEmpty(), "no assets: empty");
#ifdef Q_OS_WIN32
    check(platformAssetSuffix() == "-win-installer.exe", "Windows takes the installer whatever the architecture");
#endif
}

void testStaleInstaller()
{
    // Dev app 0.7.0-dev.9
    check(isStaleInstaller("LavArtemis-v0.7.0-dev.8-win-installer.exe", "0.7.0-dev.9", true), "an older Dev installer goes");
    check(isStaleInstaller("LavArtemis-v0.7.0-dev.9-win-installer.exe", "0.7.0-dev.9", true), "the one just installed goes");
    check(!isStaleInstaller("LavArtemis-v0.7.0-dev.10-win-installer.exe", "0.7.0-dev.9", true), "a newer one may be downloading: kept");
    check(!isStaleInstaller("LavArtemis-v0.6.0-win-installer.exe", "0.7.0-dev.9", true), "a stable installer is never the Dev app's");
    check(!isStaleInstaller("LavArtemis-v21.7.1-win-installer.exe", "0.7.0-dev.9", true), "nor an old stable one");
    check(isStaleInstaller("LavArtemis-v21.8.0-dev.9-win-installer.exe", "0.7.0-dev.9", true), "an old-numbering Dev installer is older");
    // Stable app 1.0.0
    check(isStaleInstaller("LavArtemis-v0.9.0-win-installer.exe", "1.0.0", false), "an older stable installer goes");
    check(isStaleInstaller("LavArtemis-v21.7.1-win-installer.exe", "1.0.0", false), "21.x is older than 1.0.0");
    check(!isStaleInstaller("LavArtemis-v0.7.0-dev.12-win-installer.exe", "1.0.0", false), "a Dev installer is never the stable app's");
    check(!isStaleInstaller("LavArtemis-v1.0.1-win-installer.exe", "1.0.0", false), "a newer stable one is kept");
    // Not ours at all
    check(!isStaleInstaller("Moonlight-v6.0.0-win-installer.exe", "1.0.0", false), "another app's file");
    check(!isStaleInstaller("LavArtemis-v0.9.0-win-x64.zip", "1.0.0", false), "not an installer");
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    testCompare();
    testPickDevRelease();
    testAsset();
    testStaleInstaller();
    if (failures == 0) {
        std::printf("tst_updater: all checks passed\n");
    }
    return failures == 0 ? 0 : 1;
}
