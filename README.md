# LavArtemis (desktop)

LavArtemis is a game-streaming client for [LavApollo](https://github.com/Lavagnou/LavApollo),
Apollo and Sunshine hosts (NVIDIA GameStream protocol). This repository is the **desktop client**
(C++ / Qt 6, Windows and Linux); the Android client and the releases live in
[Lavagnou/LavArtemis](https://github.com/Lavagnou/LavArtemis), which includes this repository as its
`desktop/` submodule.

## Two apps: LavArtemis and LavArtemis Dev

| | LavArtemis | LavArtemis Dev |
|---|---|---|
| Branch | `main` | `dev` |
| Version | `X.Y.Z` | `X.Y.Z-dev.N` (GitHub prerelease) |
| Interface | the interface inherited from Moonlight / Artemis | the redesigned interface, in French |

Both install side by side on Windows, with separate settings (LavArtemis Dev copies the stable
app's settings once, on its first start). New work goes to LavArtemis Dev first; the stable app
is updated only once Dev has been tested for a while.

LavArtemis Dev opens on a home page that shows one PC at a time: its **Bureau** (the Windows
desktop), **Playnite Fullscreen**, and the host's Playnite library (most recently played first,
through LavApollo's *LavApollo Link* extension), with the running session staged on top. **Mes PC**
adds, pairs (PIN), renames, wakes, forgets and benchmarks PCs. **Réglages** has a normal and an
expert mode, and the in-game menu, texts and options match the Android client.

## Features

On top of Moonlight's (hardware decoding, H.264 / HEVC / AV1, 4:4:4, surround sound, gamepads,
system shortcut capture):

- **Apollo / LavApollo**: virtual display, PIN and OTP pairing, server commands, clipboard sync,
  `art://` links and `.art` shortcut files (same format as Android).
- **Playnite games** as apps, started inside Playnite, and « Quitter le jeu » back to Playnite.
- **Multi-monitor**: the host emulates one virtual display per local monitor.
- **Stream at the host display's mode** when joining a session or streaming without a virtual display.
- **PyroWave** intra-only GPU codec (experimental; Windows / Linux x64, wired networks) and **VRR pacing**
  (frames presented at their capture time on G-Sync / FreeSync displays, Windows).
- **Microphone passthrough** to a virtual microphone on the host.
- **Settings benchmark**: streams a test scene with several codec / bitrate / pacing combinations
  and scores latency and smoothness.
- Settings profiles, send-keys and keyboard macros (same files as Android), gamepad mapper with a
  clickable controller diagram and a guessed layout for unknown gamepads, client-side zoom,
  touchscreen as Multitouch or Trackpad.
- In-app updater (Windows: downloads and runs the installer).

## Install

Download `LavArtemis-<version>-win-installer.exe` from the
[LavArtemis releases](https://github.com/Lavagnou/LavArtemis/releases): the latest release for
LavArtemis, the newest `-dev.` prerelease for LavArtemis Dev. Each app then updates itself from its
own channel. Releases are built for Windows x64; ARM64 and the Linux AppImage are only in releases
built by the manual release workflow. Otherwise build from source.

Up to 21.x the version number came from Moonlight; LavArtemis now has its own (Dev from
**0.7.0-dev.1**, stable **1.0.0** later). A 21.x app does not offer the new, lower-looking numbers as
updates: run the first new installer by hand, once. It replaces the 21.x version and keeps your
settings and pairings (never uninstall first: that erases them); the old 21.x entry stays once in
*Installed apps* and can be uninstalled safely. From then on an older installer refuses to run.

## Building

See [docs/BUILD_SYSTEM.md](docs/BUILD_SYSTEM.md). In short:

```sh
git submodule update --init --recursive
# Windows (Qt 6.11 MSVC, Visual Studio 2026, WiX 7, 7-Zip), from a Qt command prompt
powershell ./setup-deps.ps1
scripts\build-arch.bat Release x64
# Linux
qmake6 lavartemis.pro && make -j$(nproc) release
```

## Lineage and credits

[Moonlight PC](https://github.com/moonlight-stream/moonlight-qt) (Cameron Gutman and contributors)
→ [wjbeckett/artemis](https://github.com/wjbeckett/artemis) (desktop port of the Artemis features of
[ClassicOldSong's Artemis for Android](https://github.com/ClassicOldSong/moonlight-android))
→ LavArtemis. PyroWave decoding and the VRR pacer are ported from
[Nonary's moonlight-qt fork](https://github.com/Nonary/moonlight-qt); PyroWave itself is by
[Themaister](https://github.com/Themaister/pyrowave).

Licensed under the GPLv3, like Moonlight.
