#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// The rules of the in-app update (AutoUpdateChecker), without network or settings: tested by
// tests/updater. The same rules as Android's UpdateChecker (compareVersions, pickAsset, the
// cleanup of downloaded APKs).
namespace UpdateRules {

// Versions from this major up are the old Moonlight-derived numbering (21.x), older than every
// version of LavArtemis' own (0.7.0-dev.1, 1.0.0), which stays below 20.0.0 forever.
constexpr int kLegacyVersionMajor = 20;

// Compares two version strings "x.y.z" with an optional "-dev.N" / "-ci.N" suffix: < 0, 0, > 0.
// A stable release sorts after a prerelease of the same base; an old 20+ version sorts before
// any version of the new numbering (the MSI's Upgrade table applies the same rule).
int compareVersionStrings(const QString& a, const QString& b);

// A version of the Dev channel: "0.7.0-dev.3"
bool isDevVersion(const QString& version);

// The newest Dev release of GitHub's /releases list (newest first): not a draft, a "-dev." tag,
// never a "-ci." build. -1 when there is none.
int pickDevRelease(const QJsonArray& releases);

// The browser_download_url of the release asset whose name ends with `suffix`, or "" (no suffix,
// or no such asset).
QString findAssetDownloadUrl(const QJsonObject& release, const QString& suffix);

// The installer asset of this platform: "-win-installer.exe" on Windows (one bundle for x64 and
// arm64), ".dmg", the AppImage, "" where the app updates through whatever installed it.
QString platformAssetSuffix();

// Whether a file left in the temp directory is an installer this app downloaded and no longer
// needs: "LavArtemis-v<version>-win-installer.exe" of this app's own channel (the stable app never
// touches a Dev installer, nor the Dev app a stable one: each may be downloading its own right now,
// there is no lock between the two apps), at most the running version (a newer one may be a
// download in flight from another instance of the same app).
bool isStaleInstaller(const QString& fileName, const QString& runningVersion, bool devChannel);

}
