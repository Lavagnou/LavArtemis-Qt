#include "updaterules.h"

#include <QRegularExpression>
#include <QStringList>
#include <QVector>

namespace UpdateRules {

namespace {

QVector<int> versionQuad(const QString& base)
{
    QVector<int> quad;
    for (const QString& component : base.split('.')) {
        quad.append(component.toInt());
    }
    return quad;
}

int compareQuads(const QVector<int>& a, const QVector<int>& b)
{
    // Missing decimal places count as 0
    for (int i = 0; i < qMax(a.size(), b.size()); i++) {
        const int va = a.value(i);
        const int vb = b.value(i);
        if (va != vb) {
            return va < vb ? -1 : 1;
        }
    }
    return 0;
}

} // namespace

int compareVersionStrings(const QString& a, const QString& b)
{
    // Versions look like "0.7.0", "0.7.0-dev.3" or "0.7.0-dev.3-ci.42"
    const QStringList partsA = a.split('-');
    const QStringList partsB = b.split('-');
    const QVector<int> quadA = versionQuad(partsA.value(0));
    const QVector<int> quadB = versionQuad(partsB.value(0));

    // Up to 21.x the version came from Moonlight; LavArtemis then started its own numbering
    // (0.7.0-dev.1, 1.0.0), which stays below 20.0.0 forever. A 20+ version is therefore the old
    // numbering, older than any new one.
    const bool legacyA = quadA.value(0) >= kLegacyVersionMajor;
    const bool legacyB = quadB.value(0) >= kLegacyVersionMajor;
    if (legacyA != legacyB) {
        return legacyA ? -1 : 1;
    }

    const int baseCmp = compareQuads(quadA, quadB);
    if (baseCmp != 0) {
        return baseCmp;
    }

    // Same base: a stable release (no suffix) is newer than any prerelease
    const bool preA = partsA.size() > 1;
    const bool preB = partsB.size() > 1;
    if (preA != preB) {
        return preA ? -1 : 1;
    }
    if (preA) {
        // Both prereleases, e.g. "dev.3": compare the trailing build number
        const int buildA = partsA.value(1).split('.').last().toInt();
        const int buildB = partsB.value(1).split('.').last().toInt();
        return buildA < buildB ? -1 : (buildA > buildB ? 1 : 0);
    }
    return 0;
}

bool isDevVersion(const QString& version)
{
    return version.contains(QStringLiteral("-dev."));
}

int pickDevRelease(const QJsonArray& releases)
{
    // Newest first. Stable and "-ci." builds are never offered to the Dev app.
    for (int i = 0; i < releases.size(); i++) {
        const QJsonObject release = releases[i].toObject();
        if (release["draft"].toBool()) {
            continue;
        }
        // A manual run of release.yml from the dev branch publishes "v…-dev.N-ci.M": a build, not a
        // Dev release, though its tag carries "-dev." too
        const QString tag = release["tag_name"].toString();
        if (isDevVersion(tag) && !tag.contains(QStringLiteral("-ci."))) {
            return i;
        }
    }
    return -1;
}

QString findAssetDownloadUrl(const QJsonObject& release, const QString& suffix)
{
    if (suffix.isEmpty()) {
        return QString();
    }
    const QJsonArray assets = release["assets"].toArray();
    for (const QJsonValue& assetVal : assets) {
        const QJsonObject asset = assetVal.toObject();
        if (asset["name"].toString().endsWith(suffix)) {
            return asset["browser_download_url"].toString();
        }
    }
    return QString();
}

QString platformAssetSuffix()
{
#if defined(Q_OS_WIN32)
    // One installer covers both architectures: the WiX bundle carries the x64 and the arm64 MSI
    // and picks by NativeMachine. The per-arch zips published beside it are portable builds with
    // no installer inside, so pointing arm64 at one of those left installAndRestart() with a .zip
    // to execute.
    return QStringLiteral("-win-installer.exe");
#elif defined(Q_OS_DARWIN)
    return QStringLiteral(".dmg");
#elif defined(APP_IMAGE)
    return QStringLiteral("-linux-x86_64.AppImage");
#else
    // A distro or source build updates through whatever installed it
    return QString();
#endif
}

bool isStaleInstaller(const QString& fileName, const QString& runningVersion, bool devChannel)
{
    static const QRegularExpression versionRe(QStringLiteral("^LavArtemis-v(.+)-win-installer\\.exe$"),
                                              QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = versionRe.match(fileName);
    if (!match.hasMatch()) {
        return false;
    }
    const QString version = match.captured(1);
    // The other app's installer: never ours to delete, whatever its version
    if (isDevVersion(version) != devChannel) {
        return false;
    }
    // A newer one may be a download in flight from another instance -- there is no single-instance
    // lock -- so it stays. Everything else has either been installed already or been abandoned, and
    // comparing versions rather than timestamps is what makes that distinction safe without locking.
    return compareVersionStrings(version, runningVersion) <= 0;
}

}
