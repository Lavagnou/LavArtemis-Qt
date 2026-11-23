#pragma once

#include <QDir>
#include <QSettings>

// LavArtemis Dev is installed next to the stable app with its own Qt
// organisation ("LavArtemis Dev"), hence its own registry key and AppData
// folders. On its very first start it copies the stable app's settings once,
// so paired hosts, profiles and key macros carry over without re-pairing.
//
// The stable side is only ever read. See
// docs/superpowers/specs/2026-11-23-stable-dev-split-design.md in LavArtemis.
namespace ChannelMigration
{
    // Copies every key of `from` into `to` and syncs `to`. Returns the number of keys copied.
    int copySettings(QSettings& from, QSettings& to);

    // Copies the regular files directly inside `from` (not sub-folders: the
    // cache lives there) into `to`, never overwriting. Returns the number copied.
    int copyTopLevelFiles(const QDir& from, const QDir& to);

    // Dev builds only: imports the stable app's settings if this app has none yet.
    void importStableSettingsOnce();
}
