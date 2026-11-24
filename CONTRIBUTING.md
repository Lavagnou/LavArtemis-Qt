# Contributing to LavArtemis (desktop)

## Branches

- **`dev`**: LavArtemis Dev. All new work lands here first.
- **`main`**: the stable LavArtemis. It is updated from `dev` only after Dev has been tested for a
  while; nothing is backported to it in between.
- The channel comes from the version, not the branch: a version containing `-dev.` builds
  LavArtemis Dev (see `CLAUDE.md`, « Canal Dev »).

The LavArtemis repository pins this one as its `desktop/` submodule (its `dev` branch pins this
`dev` branch). A change reaches a release once LavArtemis points `desktop/` at it.

## Before pushing

- Build locally (`docs/BUILD_SYSTEM.md`): the compile check (`.github/workflows/build.yml`) only runs
  when started by hand, to spare the Actions quota.
- Run the tests that cover what you touched (`qmake tests/tests.pro CONFIG+=tests`).
- Any text shown to the user goes through `tr()` / `qsTr()` and gets its French translation
  (`app/languages/qml_fr.ts`, see `CLAUDE.md`, « Langue »).
- A text, menu or setting the Android client also has changes on both sides (parity rule,
  `docs/superpowers/specs/2026-11-24-pc-android-parity.md` in LavArtemis).

## Code

- C++ / QML, following the existing style. No mass reformatting: it makes merging
  `upstream-qt/master` (moonlight-qt) painful.
- Commits are atomic, with a message that says what changes and why.
- AI-written code is allowed but reviewed line by line, and tested by hand.
- GPLv3. Keep the Moonlight / Artemis / wjbeckett attributions.
