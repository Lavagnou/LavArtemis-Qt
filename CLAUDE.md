# CLAUDE.md

> **LavArtemis-Qt** — Client desktop de *game streaming* pour les hôtes **Apollo** / **Sunshine**.
> Moitié desktop de [`Lavagnou/LavArtemis`](https://github.com/Lavagnou/LavArtemis), consommé par
> celui-ci comme sous-module `desktop/`.
> *The desktop half of LavArtemis, consumed by the Android repo as the `desktop/` submodule.*

---

## 📖 Lignage

**Moonlight-Qt** → **[wjbeckett/artemis](https://github.com/wjbeckett/artemis)** (portage desktop des
features Artemis) → **LavArtemis-Qt**.

Remotes :
- `origin` → https://github.com/Lavagnou/LavArtemis-Qt
- `upstream-qt` → https://github.com/moonlight-stream/moonlight-qt — **la vraie source amont**
- `upstream-artemis` → https://github.com/wjbeckett/artemis — dormant depuis 09/2025

> ⚠️ **moonlight-qt n'est pas à l'abandon.** Aucune release taguée depuis v6.1.0 (09/2024), mais le
> `master` reçoit des commits en continu, dont des corrections du pipeline vidéo D3D11VA. C'est
> précisément pour garder `git merge upstream-qt/master` peu coûteux que ce client vit dans son
> propre dépôt plutôt que dans un monorepo.

## 🧱 Stack technique

| Élément | Valeur |
|---|---|
| Langage | **C++ / QML** |
| Qt | **6.11** |
| Build | **qmake** (pas CMake) — `lavartemis.pro` |
| Compilateur Windows | MSVC (VS2026) |
| Plateformes CI | Windows **x64** + **ARM64**, Linux x86_64 |
| Installeur | **WiX 7** |
| Version | lue depuis `app/version.txt`, **écrite depuis le tag au build**, jamais committée |

macOS et Steam Link : le code amont est là mais **hors périmètre** — aucune CI, non testé, non publié.

## 🗂️ Architecture

| Dossier | Rôle |
|---|---|
| `app/backend/` | Découverte d'hôtes, `nvhttp` (pairing, HTTP), `computermanager`, `keymacromanager` |
| `app/streaming/` | Session de stream, entrées (clavier/souris/manette), `panzoom`, `streamutils` |
| `app/streaming/video/` | Décodeurs, `pacingstats`, et `ffmpeg-renderers/` (un backend par API graphique) |
| `app/gui/` | QML — `SettingsView`, `AppView`, `QuickMenu`, `PcView` |
| `app/settings/` | `streamingpreferences`, `artemissettings`, `profilemanager` |
| `app/cli/` | Ligne de commande, `artlink` (liens `art://` et fichiers `.art`) |
| `wix/` | Installeur Windows |
| `moonlight-common-c/` | Sous-module du cœur natif (**partagé avec l'Android**) |

## 🚀 Build

```sh
git submodule update --init --recursive

# Windows
powershell ./setup-deps.ps1            # libs prébuildées
scripts\build-arch.bat Release x64     # puis arm64
scripts\generate-bundle.bat Release    # installeur WiX combiné (a besoin des DEUX archis)

# Linux
qmake6 lavartemis.pro && make -j$(nproc) release
```

## ⚡ Valeur ajoutée LavArtemis (hot path)

Portée depuis le client Android, sauf mention contraire :

| Feature | Où | Note |
|---|---|---|
| **`PacingStats`** | `streaming/video/pacingstats.{h,cpp}` | Fenêtre roulante 512, p50/p95/p99 des intervalles present-to-present. Même filtre d'outliers que l'Android (rejet des deltas ≤0 ou ≥1 s). |
| **Scanout réel** | `d3d11va.cpp::getLastPresentTimeUs()` | **Meilleur que l'Android.** `GetFrameStatistics()`/`SyncQPCTime` = le vblank où la frame est *réellement* sortie. L'Android mesure le hand-off faute de mieux (SurfaceFlinger), ce qui mélange le jitter de soumission du client aux percentiles censés décrire l'écran. Virtuelle sur `IFFmpegRenderer`, défaut 0 → les autres backends gardent le hand-off. |
| **CSV perf** | `FFmpegVideoDecoder::writePerfCsvRow()` | 11 colonnes **identiques** à l'Android → les runs se comparent directement. |
| **Priorité threads** | `Session::drSubmitDecodeUnit` | `SDL_SetThreadPriority(HIGH)` one-shot. L'audio était déjà fait en amont. |
| **LTO** | `moonlight-common-c.pro` | `/GL`+`/LTCG` (MSVC), `-O3 -flto` (GCC/Clang). Gagne sur la récupération FEC et la dépacketisation, qui tournent par paquet. |
| **Cap audio pending** | `ArtemisSettings::maxPendingAudioMs` | 10–100 ms, défaut 30. Lu **à la construction** de `sdlaud.cpp` → s'applique au stream suivant. |

> ⚠️ **Époques d'horloge.** `LiGetMicroseconds()` compte depuis le démarrage du cœur natif,
> `SyncQPCTime` est du QPC brut. Même cadence, **époque différente** : basculer d'une source à
> l'autre produit un intervalle aberrant. `PacingStats` le rejette déjà dans les deux sens (test
> d'ordre pour un saut arrière, plafond 1 s pour un saut avant) — coût : 1 échantillon sur 512.

### 🖥️ Multi-écran émulé

Réglage **« Virtual Display Multi-Screen »** (`StreamingPreferences::useMultiDisplay`, défaut off, grisé si
« Use Virtual Display » est décoché). `DisplayLayout::detect()`
(`streaming/displaylayout.{h,cpp}`) décrit la disposition des moniteurs ; l'hôte en émule un écran
virtuel chacun, et le flux transporte **une seule toile** = la bounding box de la disposition.

Trois choses qui ne se devinent pas :

- **Une seule fenêtre borderless couvre toute la disposition**, dimensionnée à la **toile** et pas à
  la bounding box (elles diffèrent du pixel ajouté par axe pour garder une taille paire ; coller à la
  toile garde le 1:1 exact). Chaque moniteur affiche alors sa propre zone — **aucun code renderer,
  aucun code d'entrée**. Le mapping souris marche parce que `src` et `dst` de
  `scaleSourceToDestinationSurfaceWithPanZoom()` deviennent identiques et que `SdlInputHandler`
  reçoit déjà les dimensions de la toile.
- **Ça marche même si les moniteurs ne pavent pas leur bounding box.** Tout point du rectangle est
  soit sur un moniteur, soit sur aucun, et ce qui tombe sur aucun n'est jamais scanné — la même
  raison qui permet à une fenêtre de déborder d'un écran. `tilesBoundingBox()` n'est resté qu'à
  titre informatif. Un renderer multi-fenêtres n'apporterait rien.
- ⚠️ **Le cas qui dégrade vraiment est le DPI mixte** : le compositeur redimensionne la partie de la
  fenêtre posée sur un moniteur à échelle différente. `hasMixedScaling()` le détecte et un launch
  warning suggère d'uniformiser. Rien ne peut l'empêcher côté client.

> ⚠️ Pas de plein écran pour ces fenêtres : le plein écran exclusif est mono-sortie et le flag
> desktop-fullscreen ramènerait la fenêtre sur un seul écran. Borderless à la taille exacte est
> visuellement identique.
>
> Deux conséquences que ce choix traîne derrière lui, toutes deux corrigées mais faciles à
> réintroduire : `m_IsFullScreen = false` fait hériter `SDL_WINDOW_MAXIMIZED` de la fenêtre Qt si
> elle est maximisée — et une fenêtre maximisée l'est **sur un seul écran** ; et « capturer les
> raccourcis système *en plein écran* » teste `SDL_WINDOW_FULLSCREEN`, donc ne s'activait jamais.
> D'où `SdlInputHandler::setSpansEntireDesktop()`, qui rend la fenêtre couvrante équivalente au
> plein écran pour les deux tests de capture clavier.

> ⚠️ `DisplayLayout::detect()` refuse la disposition si `SDL_GetDisplayBounds()` et le mode natif
> divergent — ce serait mélanger coordonnées DPI et pixels physiques. Mesuré : Qt et SDL passent le
> processus en PerMonitorV2, où les deux concordent, donc c'est un garde-fou et non un chemin normal.

> ⚠️ **Ne jamais écrire une préférence depuis `onCheckedChanged`.** Ce signal se déclenche à chaque
> réévaluation du binding, pas seulement au clic. La case « Virtual Display Multi-Screen » liait
> `checked: enabled && useMultiDisplay` et réécrivait la préférence : décocher « Use Virtual
> Display » un instant suffisait à l'effacer **définitivement**, la valeur source étant détruite.
> `onToggled` ne réagit qu'à une action utilisateur. La case reste cochée en grisé, ce qui est la
> lecture honnête — `Session::initialize()` exige les deux préférences de toute façon.

Un bouton de bascule est aussi posé dans la barre d'outils de l'accueil (`gui/main.qml`), parce que
c'est le réglage qu'on change selon l'endroit où l'on est assis. Il **persiste lui-même**
(`StreamingPreferences.save()`) : contrairement à `SettingsView`, l'accueil n'a pas d'événement de
navigation sur lequel sauvegarder.

> ⚠️ Un renoncement silencieux est pire que le renoncement lui-même : avec l'option cochée et un
> seul écran, `detect()` sortait sans `problem()` et `session.cpp` n'avertissait que si `problem()`
> était non vide — donc **rien**. Impossible de distinguer « option désactivée », « second écran
> replié » et « cassé ». La décision est maintenant loguée dans tous les cas et avertie quand il n'y
> a qu'un écran à refléter.

### Délibérément **non** portés

| Feature Android | Pourquoi |
|---|---|
| `checkbox_paced_ull` | C'est un rattrapage Android : en ULL le renderer présente sans alignement vsync. Le desktop a un vrai `IVsyncSource` derrière l'option **Frame pacing** — l'activer *est* le mode pacé, en mieux (vsync réel, pas extrapolé). |
| `checkbox_predictive_pacing` | L'équivalent supposé, `SetMaximumFrameLatency(1)`, est **délibérément évité** en amont : cf. le commentaire de `d3d11va.cpp` (~l. 551) — avec `SyncInterval 0`, ça fait bloquer `Present()` sur DWM et **augmente** la latence. Le `+2 vsync` Android est un détail SurfaceFlinger sans équivalent. |
| AV1 auto (≤15 Mbps ou 4K) | Le mode Auto amont sonde déjà les décodeurs et *déprioritise* AV1 selon la dispo HEVC HW (`session.cpp` ~l. 909-944). Ajouter le seuil Android *retirerait* AV1 à >15 Mbps en <4K et ferait retomber en H.264 les GPU qui ne décodent qu'AV1 : régression. |
| ADPF | Pas d'équivalent Windows. L'inhibition de veille est déjà couverte par `SDL_DisableScreenSaver()`. |
| SBS 3D MiDaS | Imposerait de réimplémenter la passe DIBR dans **chaque** backend, de remplacer TFLite par ONNX Runtime/DirectML et d'embarquer ~50-100 Mo de deps, pour une valeur desktop étroite. |

## 🧩 Features Artemis portées

| Feature | Où | Note |
|---|---|---|
| **Profils de réglages** | `settings/profilemanager.{h,cpp}` | `profiles.json` dans `AppConfigLocation`. Un profil est un **snapshot complet**, pas un patch épars. `language` et `defaultver` **bypassent** les profils. |
| **Send keys + macros** | `backend/keymacromanager.{h,cpp}` | 16 presets + `keyboard-macros.json`. **Même format et mêmes noms `VK_` que l'Android** → un fichier marche sur les deux. Table des 174 noms extraite de `KeyMapper.java`, pas retapée. |
| **Liens `art://` + `.art`** | `cli/artlink.{h,cpp}` | `rewriteArguments()` **traduit le lien en ligne de commande existante** au lieu d'ajouter un chemin de lancement parallèle → hérite du host seeking, du wake-on-LAN et de l'UI de segue. Export via `AppModel::exportArtFile()`. |
| **Pan & zoom** | `streaming/panzoom.{h,cpp}` | Combos Ctrl+Alt+Shift (`+`/`-`/`0`, flèches). |

> ⚠️ **Le bug Android des profils n'est pas reproduit.** Côté Android,
> `OverlaySharedPreferences.edit()` renvoie l'éditeur de base (`ProfilesManager.java:251`) : les
> écritures s'échappent du profil vers les prefs globales. Ici, `reload()` **et** `save()` passent
> tous deux par `ProfileManager::value()`/`setValue()` — lecture et écriture ne peuvent pas diverger
> par construction.

## ⚠️ Pièges / Gotchas

1. **`ArtemisSettings` ne persiste pas tout seul.** Les setters n'écrivent pas ; `SettingsView.qml`
   appelle `ArtemisSettings.save()` dans `StackView.onDeactivating` et `Component.onDestruction`.
   **Tout nouveau point d'édition doit faire pareil.**

2. **Un seul helper de géométrie, deux usages.** Tous les renderers *et* le mapping des coordonnées
   souris passent par `StreamUtils::scaleSourceToDestinationSurfaceWithPanZoom()` — c'est ce qui fait
   que le pointeur suit le zoom sans code dédié. Mais `Session::getWindowDimensions()`
   (`session.cpp:1538`) réutilise l'ajustement d'aspect pour **dimensionner la fenêtre** et doit
   rester sur `scaleSourceToDestinationSurface()` : y appliquer le zoom redimensionnerait la fenêtre
   au lieu d'agrandir la vidéo.

3. **`UpgradeCode` WiX propre à LavArtemis** — ne jamais reprendre celui de Moonlight ou d'Artemis,
   sous peine d'écraser leur installation.

4. **Le thème du bootstrapper référence ses assets *par nom de fichier*.**
   `wix/LavArtemisSetup/RtfTheme.xml` (`IconFile=`, `ImageFile=`) doit correspondre exactement aux
   `Payload Name=` de `Bundle.wxs`. Burn résout ces noms **à l'exécution**, dans le dossier
   d'extraction : un nom qui ne correspond pas fait échouer le parsing du thème avec `FILE_NOT_FOUND`
   **avant qu'aucune fenêtre n'existe** — l'installeur semble ne rien faire du tout, sans erreur ni
   SmartScreen. WiX embarque sans broncher un thème dont les références ne résolvent pas : build vert,
   zéro warning. C'est ce qui a cassé la v20.3.0 (renommage `artemis.ico` → `lavartemis.ico`
   non répercuté). Le job `build-windows` de LavArtemis ouvre désormais le bundle et vérifie chaque
   asset — mais si tu bouges ces fichiers, vérifie les deux côtés.
   Diagnostic : le log Burn est dans `%TEMP%\LavArtemis_Game_Streaming_Client_*.log`, et
   `wix burn extract <exe> -oba <dir> -acceptEula wix7` montre le contenu réel.

5. **`app/version.txt` est écrit par la CI depuis le tag**, jamais committé à la main.

6. **`ArtemisApplication`, `artemis.png`, `artemissettings`** — le rebrand est partiel, ces noms sont
   hérités. Renommer touche le QML, le `.pro` et le WiX ensemble.

## 🤖 CI

- **`.github/workflows/build.yml`** — compile check à chaque push/PR sur `main` (Windows x64/arm64 +
  Linux). ~7 min. Ne produit **pas** de release.
- Les **artefacts publiés** viennent du workflow de release de
  [`Lavagnou/LavArtemis`](https://github.com/Lavagnou/LavArtemis), qui consomme ce dépôt comme
  sous-module `desktop/` et publie Android + desktop sous un tag unique.
- ⚠️ **`build-appimage.yml` est orphelin** (`workflow_call` que personne n'appelle) : c'est la
  référence dont le job `build-linux` de LavArtemis est dérivé. **Toute correction doit aller dans
  les deux.**

### Faire remonter un changement jusqu'à la release

Commiter ici suffit. Le pointeur `desktop/` côté LavArtemis est avancé par le workflow **Update
desktop pointer** (manuel), qui n'épingle que des commits dont le build est vert. La release signale
d'elle-même si le pointeur est en retard.

## 🔗 Cœur natif partagé

`moonlight-common-c/moonlight-common-c` → [`Lavagnou/moonlight-common-c`](https://github.com/Lavagnou/moonlight-common-c),
branche `lavartemis` = `moonlight-stream/master` + `84af637` (`LiSendExecServerCmd`, extension
Apollo) + `c999436` (`LiSendEmptyPayload`).

**Le client Android pointe le même commit.** `LiSendEmptyPayload` est du code mort ici ; il est sur
la branche pour que l'Android puisse la partager. Voir `CLAUDE.md` côté LavArtemis pour ce qu'un
rebump implique là-bas (FEC nanors, timestamps en µs).

## 📝 Conventions

- C++ / QML, style existant. Pas de reformatage massif : ça rendrait les merges amont douloureux.
- Merger `upstream-qt/master` régulièrement — c'est la principale raison d'être de ce dépôt séparé.
- GPLv3. Conserver les attributions Moonlight / Artemis / wjbeckett.
