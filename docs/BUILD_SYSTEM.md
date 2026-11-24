# Building LavArtemis (desktop)

Supported targets: **Windows x64 and ARM64** (MSVC) and **Linux x86_64**. The macOS and Steam Link
code inherited from Moonlight is still in the tree but is not built, tested or published.

## Requirements

- Qt **6.11** (MSVC 2022 kits on Windows: `msvc2022_64`, and `msvc2022_arm64` for ARM64)
- Windows: Visual Studio 2026, [WiX 7](https://wixtoolset.org/), 7-Zip on `%PATH%`
- Linux: GCC or Clang, FFmpeg, SDL, libplacebo (the release AppImage compiles SDL3, sdl2-compat,
  SDL_ttf, libva, libplacebo, dav1d and FFmpeg from source, see below)

```sh
git submodule update --init --recursive   # moonlight-common-c, pyrowave, ...
powershell ./setup-deps.ps1               # Windows: prebuilt libraries
```

## Windows

From a Qt command prompt, at the repository root:

```bat
scripts\build-arch.bat Release            :: architecture = the Qt kit whose qmake is on PATH
scripts\generate-bundle.bat Release       :: combined installer (WiX bundle)
```

`build-arch.bat` reads the architecture from the Qt kit found on `PATH` (`_arm64` in its path means
ARM64), not from an argument. Output: `build\deploy-<arch>-release\` (portable files) and
`build\build-<arch>-release\LavArtemis.msi`.

`generate-bundle.bat` wraps the MSIs in `LavArtemisSetup-<version>.exe`, which installs the x64 or
ARM64 MSI according to the machine. It expects both MSIs, unless **`LAV_X64_ONLY=1`** (x64-only
bundle, for a PC without the Qt ARM64 kit).

- **Version**: `app/version.txt`, overridden by the `CI_VERSION` environment variable. A version
  containing `-dev.` builds **LavArtemis Dev** (`LAV_DEV_CHANNEL` in `app.pro`, `LAV_CHANNEL=dev` for
  WiX: its own name, folder, registry key and UpgradeCodes, no `art://` / `.art` registration).
  qmake and MSI need numbers only: `21.8.0-dev.3` becomes `21.8.0.3`.
- PyroWave is built for x64 only (`HAVE_PYROWAVE`).

## Linux

```sh
qmake6 lavartemis.pro && make -j$(nproc) release
```

The AppImage is made by `scripts/build-appimage.sh` (linuxdeploy) in the `build-linux` job of the
LavArtemis release workflow, which first builds the dependencies listed above under `deps/`.

## Releases

Releases are published from the LavArtemis repository, which pins this repository as `desktop/`.
They are built **locally**: `scripts/local-release.ps1` in LavArtemis builds the signed Android
APKs and this client (Windows x64 only, `LAV_X64_ONLY=1`, from a LavArtemis-Qt clone checked out
on the pinned commit), then tags and runs `gh release create`. The GitHub workflows
(`.github/workflows/release.yml` in LavArtemis, `build.yml` here) run only when started by hand;
the release workflow is the way to get ARM64 or the Linux AppImage.

Published desktop files: `LavArtemis-<tag>-win-installer.exe`, `LavArtemis-<tag>-win-x64.zip`
(portable), and, from the workflow only, `-win-arm64.zip` and `-linux-x86_64.AppImage`.
