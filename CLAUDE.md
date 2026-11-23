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
| `app/backend/` | Découverte d'hôtes, `nvhttp` (pairing, HTTP), `computermanager`, `keymacromanager`, `autoupdatechecker` |
| `app/streaming/` | Session de stream, entrées (clavier/souris/manette), `panzoom`, `streamutils` |
| `app/streaming/video/` | Décodeurs, `pacingstats`, et `ffmpeg-renderers/` (un backend par API graphique) |
| `app/gui/` | QML — `main.qml`, `v2/` (toute l'interface), `QuickMenu` (menu en jeu), segues de stream, mapper de manettes, benchmark |
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
> Trois conséquences que ce choix traîne derrière lui, toutes corrigées mais faciles à
> réintroduire : `m_IsFullScreen = false` fait hériter `SDL_WINDOW_MAXIMIZED` de la fenêtre Qt si
> elle est maximisée — et une fenêtre maximisée l'est **sur un seul écran** ; « capturer les
> raccourcis système *en plein écran* » teste `SDL_WINDOW_FULLSCREEN`, donc ne s'activait jamais.
> D'où `SdlInputHandler::setSpansEntireDesktop()`, qui rend la fenêtre couvrante équivalente au
> plein écran pour les deux tests de capture clavier.
>
> Et la troisième : **la barre des tâches reste au-dessus**. Le shell ne s'efface que pour une
> fenêtre qui coïncide avec *un* moniteur, ce qu'une fenêtre couvrante ne fait jamais. D'où
> `SDL_WINDOW_ALWAYS_ON_TOP` à la création, **retiré au `FOCUS_LOST` et remis au `FOCUS_GAINED`**
> (`Session::setSpanningWindowOnTop()`) : sans ce va-et-vient, alt-tab enverrait l'application
> choisie *derrière* un stream qui couvre tout le bureau.

> ⚠️ `DisplayLayout::detect()` refuse la disposition si `SDL_GetDisplayBounds()` et le mode natif
> divergent — ce serait mélanger coordonnées DPI et pixels physiques. Mesuré : Qt et SDL passent le
> processus en PerMonitorV2, où les deux concordent, donc c'est un garde-fou et non un chemin normal.

> ⚠️ **Ne jamais écrire une préférence depuis `onCheckedChanged`.** Ce signal se déclenche à chaque
> réévaluation du binding, pas seulement au clic. La case « Virtual Display Multi-Screen » liait
> `checked: enabled && useMultiDisplay` et réécrivait la préférence : décocher « Use Virtual
> Display » un instant suffisait à l'effacer **définitivement**, la valeur source étant détruite.
> `onToggled` ne réagit qu'à une action utilisateur. La case reste cochée en grisé, ce qui est la
> lecture honnête — `Session::initialize()` exige les deux préférences de toute façon.

Une pastille de bascule est aussi posée dans l'en-tête de l'accueil (`gui/v2/HomeView.qml`), parce que c'est le
réglage qu'on change selon l'endroit où l'on est assis. Il **persiste lui-même**
(`StreamingPreferences.save()`) : contrairement aux Réglages, l'accueil n'a pas d'événement de
navigation sur lequel sauvegarder.

Il est dans l'en-tête de l'accueil (pastille Multi-écran), là où l'on lance les jeux : le moment où
l'on décide de streamer sur un ou plusieurs écrans est celui juste avant de lancer un jeu.

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
| **Profils de réglages** | `settings/profilemanager.{h,cpp}` | `profiles.json` dans `AppConfigLocation`. Un profil est un **snapshot complet**, pas un patch épars. `language`, `defaultver` et les clés propres à la machine (`mdns`, `detectnetblocking`, `uidisplaymode`, `tracevrrframes`) **bypassent** les profils (`isGlobalKey()`). |
| **Send keys + macros** | `backend/keymacromanager.{h,cpp}` | 16 presets + `keyboard-macros.json`. **Même format et mêmes noms `VK_` que l'Android** → un fichier marche sur les deux. Table des 174 noms extraite de `KeyMapper.java`, pas retapée. |
| **Liens `art://` + `.art`** | `cli/artlink.{h,cpp}` | `rewriteArguments()` **traduit le lien en ligne de commande existante** au lieu d'ajouter un chemin de lancement parallèle → hérite du host seeking, du wake-on-LAN et de l'UI de segue. Export via `AppModel::exportArtFile()`. |
| **Pan & zoom** | `streaming/panzoom.{h,cpp}` | Combos Ctrl+Alt+Shift (`+`/`-`/`0`, flèches). |

> ⚠️ **Le bug Android des profils n'est pas reproduit.** Côté Android,
> `OverlaySharedPreferences.edit()` renvoie l'éditeur de base (`ProfilesManager.java:251`) : les
> écritures s'échappent du profil vers les prefs globales. Ici, `reload()` **et** `save()` passent
> tous deux par `ProfileManager::value()`/`setValue()` — lecture et écriture ne peuvent pas diverger
> par construction.

> ⚠️ **Sortir une clé des profils = la migrer.** Ajoutée à `k_AppWideKeys` (`profilemanager.cpp`),
> une clé n'est plus lue dans le profil actif : sans `migrateAppWideKeys()` (appelé par `load()`), la
> valeur en vigueur (celle du profil actif) serait remplacée en silence par la globale. Il copie celle
> du profil actif dans les réglages globaux et retire la clé de tous les profils. Un changement de
> profil ne peut plus couper la découverte des PC.

## 🎮 Mapper de manettes

SDL ne présente un périphérique comme *game controller* que s'il a une entrée pour son GUID.
`MappingManager::applyFallbackMappings()` en devine une à partir de la forme de l'appareil (4-8 axes,
≥8 boutons, ≤1 hat — le **même** test que `SdlInputHandler::getUnmappedGamepads()`), mais une
supposition peut placer les contrôles au mauvais endroit. Le mapper est là pour corriger, et pour
faire marcher les appareils trop atypiques pour être devinés.

**On clique le contrôle sur un schéma de manette, puis on l'actionne.** Pas de séquence imposée :
`SdlGamepadMapper` n'a qu'une cible à la fois (`m_TargetElement`) et une file
(`selectElements(QStringList)`) qui sert aux **trois** usages — un clic sur un bouton (file de 1), un
clic sur un stick (file de 2 : X puis Y), et « Tout mapper à la suite » (file de 21). L'ancien
assistant linéaire est donc devenu un cas particulier gratuit du même moteur.

| Fichier | Rôle |
|---|---|
| `gui/sdlgamepadmapper.{h,cpp}` | Le moteur : capture, évaluation live, préremplissage, sauvegarde |
| `gui/ControllerDiagram.qml` | Le schéma : repère logique 1000×620, `res/gamepad_body.svg` en fond |
| `gui/GamepadControl.qml` | Un point cliquable ; dérive son état du `ControllerDiagram` qu'il reçoit |
| `gui/GamepadMapper.qml` | La page : sélecteur de périphérique + schéma |
| `settings/mappingmanager.{h,cpp}` | Persistance (`QSettings`, tableau `gcmapping`), `gamecontrollerdb.txt`, repli deviné |

> ⚠️ **L'invariant qui a coûté le plus cher : tout entrée doit revenir au repos avant de compter à
> nouveau.** Les boutons avaient ce verrou, les axes et les hats **non**. Comme les quatre étapes
> d'axe se suivaient et que le timer tourne à 20 ms, une seule poussée du stick liait `leftx`,
> `lefty`, `rightx` **et** `righty` au même `a0` — et la croix recevait `h0.1` sur ses quatre
> directions. `beginListening()` désarme désormais tout ce qui est actif au moment où l'écoute
> commence, sur les trois familles.

Trois autres règles dont dépend la justesse des captures :

- **Écart au repos, pas valeur absolue.** Un interrupteur ou une gâchette peut reposer en butée. Le
  repos est échantillonné à l'ouverture, puis **ré-échantillonné à chaque sélection pour les axes
  calmes seulement** — un axe qu'on tient ne peut ni devenir le nouveau repos, ni déclencher.
- **Le plus grand écart gagne**, pas le premier index : sinon un seul axe qui dérive rafle tous les
  éléments.
- **Le repos décide de la syntaxe d'un axe**, pas la nature de l'élément — `MappingManager::axisSourceToken()`,
  partagé avec le repli deviné pour que les deux ne puissent pas diverger. Repos en butée ⇒ axe plein
  `a2` ; repos au centre ⇒ demi-axe `+a2`. À l'envers, une gâchette perd la moitié de sa course, ou
  lit **50 % en permanence au repos**.
- **Hat : cardinales uniquement.** SDL teste `(valeur & masque) == masque`, donc un `h0.3` capturé en
  diagonale ne se déclencherait qu'avec deux directions tenues.

> 📋 **Le retour visuel est un outil de diagnostic, pas une décoration.** Chaque tick réévalue les
> liaisons posées contre l'état brut et publie `elementValues` ; le schéma anime les sticks, remplit
> les gâchettes et allume les boutons. C'est ce qui rend un axe inversé ou une gâchette à moitié
> enfoncée visible **avant** la sauvegarde. `rawActivity` nomme en plus l'entrée brute active, ce qui
> distingue « mapping faux » de « appareil muet ».

> ⚠️ **La sauvegarde n'est pas destructive.** Le schéma se préremplit depuis
> `SDL_GameControllerMappingForGUID()`, et les champs qu'on ne modélise pas (paddles, touchpad,
> `type:`, sorties demi-axe `+lefty:b11`) sont conservés **verbatim** dans `m_PassthroughFields` et
> réécrits — sauf si leur élément a depuis été lié ici, auquel cas SDL aurait deux sources pour un
> contrôle. Sans ça, corriger un stick effacerait le reste d'une entrée de la base.
>
> Deux exceptions, jetées volontairement : `platform:` (redérivé pour la plateforme courante) et
> **`crc:`**. Un `crc:` départage des appareils qui partagent un GUID, et celui d'une entrée de base
> appartient au nom d'appareil de *cette* entrée. Notre GUID est lu directement sur le joystick
> ouvert, donc SDL sait apparier sans aide — alors qu'un `crc:` faux ferait échouer l'application du
> mapping **en silence**.

> ⚠️ **La navigation UI à la manette lit les mêmes boutons.** `GamepadMapper.qml` appelle
> `SdlGamepadKeyNavigation.disable()` sur `StackView.onActivated` et `.enable()` sur
> `onDeactivating`. Et `Escape` doit être **consommé** (`event.accepted = true`) tant qu'une capture
> attend, sinon le `Keys.onEscapePressed` du `StackView` dépile la page sous la capture.

> 📋 **Choix assumé : pas de notification au démarrage pour un appareil deviné.** Il *fonctionne* ;
> un modal à chaque lancement serait du harcèlement. L'information est donnée là où elle est
> actionnable — au lancement d'un stream, et dans la liste du mapper (« Layout guessed — may be
> wrong »). Ce badge se lit par **GUID** (`getGuessedDeviceGuids()`), pas par nom : deux manettes du
> même modèle portent le même nom.

> ⚠️ **Reste à vérifier avec un appareil en main** : `synthesizeMapping()` suppose l'ordre d'axes
> XInput dès 6 axes (`rightx:a3, righty:a4`), alors que les périphériques que ce repli vise sont
> typiquement DirectInput, où le stick droit est souvent sur Z/Rz (`a2`/`a5`). Le raisonnement
> actuel reprend l'heuristique Android et n'a pas été changé à l'aveugle ; le mapper est le chemin de
> correction en attendant.

## 🔄 Mise à jour in-app

`backend/autoupdatechecker.{h,cpp}` interroge les releases GitHub de **LavArtemis** (pas Moonlight) et,
sous Windows, télécharge et lance l'installeur lui-même. Réglages › Avancé : « Chercher les mises à
jour au démarrage » et le bouton Vérifier / Installer. `main.qml` démarre le check et garde la version
trouvée (`window.updateVersion`, `updateDownloadUrl`, `updateBrowserUrl`) pour ses deux dialogues.

| | |
|---|---|
| Canal stable | `/releases/latest` — les builds CI sont publiés en *prerelease* et n'y apparaissent pas |
| Canal prerelease | `/releases` (liste complète), premier élément |
| Comparaison | `compareVersionStrings()` : `20.8.2` > `20.8.2-ci.41` — une stable bat toujours la prerelease de même base |
| Asset Windows | **`-win-installer.exe`, pour les deux architectures.** Le bundle WiX embarque les MSI x64 *et* arm64 et choisit par `NativeMachine` ; les zips `-win-x64`/`-win-arm64` sont des builds **portables**, sans installeur à exécuter dedans |
| Auto-install | Windows uniquement (`canSelfInstall()`). macOS et AppImage ouvrent la page de release |

> ⚠️ **Un `Dialog` Qt Quick se ferme dès qu'on presse un bouton standard.** `onAccepted` s'exécute
> *après* la fermeture : y afficher une barre de progression, c'est dessiner sur une boîte qui n'existe
> déjà plus. C'est le bug de la 20.8.1 — le téléchargement partait bien et arrivait **complet** dans
> `%TEMP%`, sans que rien ne s'affiche jamais, et rouvrir la boîte remettait `downloadDone` à faux,
> jetant le fichier déjà téléchargé. D'où **deux** dialogues dans `main.qml` : `updateDialog` demande,
> `updateDownloadDialog` (`closePolicy: Popup.NoAutoClose`) survit au clic qui l'a ouvert.

> ⚠️ **Tout check se termine par exactement un signal** — `onUpdateAvailable`, `noUpdateAvailable` ou
> `updateCheckFailed`. N'émettre que le premier laissait « Checking… » tourner indéfiniment dans les
> réglages : « déjà à jour » et « échec réseau » sont *les cas les plus fréquents*, pas des cas limites.

> ⚠️ **Le `QNetworkAccessManager` est reconstruit à chaque check.**
> `handleUpdateCheckRequestFinished()` le détruit pour que le plugin bearer ne poll pas entre deux
> checks ; tant que `performCheck()` ne le recréait pas, le check du démarrage consommait l'unique
> instance et **tous les suivants sortaient sans rien envoyer**.

> ⚠️ **Le bouton des réglages appelle `checkNow()`, pas `start()`.** `start()` respecte la case
> « vérifier au démarrage » — décochée, elle lui faisait refuser le check qu'on venait de demander.

> 📋 Une correction de l'updater **ne peut pas se livrer par l'updater** : la version cassée est celle
> qui télécharge. Prévoir une installation manuelle et le dire dans les notes de release.

> ⚠️ **L'installeur ne peut pas se supprimer lui-même.** `installAndRestart()` lui passe le fichier
> puis quitte : le seul processus qui pourrait nettoyer est celui qu'on remplace. C'est donc le
> **démarrage suivant** qui le fait (`cleanupDownloadedInstallers()`, appelé depuis le constructeur),
> et à ce moment-là l'installeur a forcément fini — l'app qu'il a installée est celle qui tourne.

> ⚠️ **Le nettoyage compare des versions, pas des dates.** Un installeur **plus récent** que la version
> qui tourne peut être un téléchargement en cours dans une **autre instance** — il n'y a pas de verrou
> d'instance unique côté desktop — donc il est conservé. Tout le reste (plus ancien ou égal) a déjà été
> installé ou abandonné. Un critère par ancienneté de fichier n'aurait pas cette garantie.

## 🧪 Canal Dev (LavArtemis Dev)

Une version qui contient `-dev.` (`app/version.txt`, écrit par la CI depuis le tag) construit
**LavArtemis Dev**, installée à côté de la stable. Branche `dev` ici, branche `dev` dans LavArtemis.
Vue d'ensemble et tableau stable/dev : section « Stable / Dev » du `CLAUDE.md` de LavArtemis.

- `app/app.pro` : `LAV_DEV_CHANNEL` défini, `VERSION` numérique (`21.8.0-dev.3` → `21.8.0.3`),
  `VERSION_STR` lisible.
- `main.cpp` : organisation Qt `LavArtemis Dev` (registre, AppData séparés), nom affiché
  « LavArtemis Dev », puis `ChannelMigration::importStableSettingsOnce()` (`settings/channelmigration.*`) :
  copie unique des réglages de la stable au premier lancement, la stable n'étant que lue.
- `autoupdatechecker.cpp` : stable → `/releases/latest` ; Dev → `/releases` + `pickDevRelease()`.
- WiX : `scripts/build-arch.bat` et `generate-bundle.bat` posent `LAV_CHANNEL=dev`, que les deux
  `.wixproj` passent en `LavChannel` ; `Product.wxs`/`Bundle.wxs` choisissent noms, dossiers, clé de
  registre et UpgradeCodes. La Dev n'enregistre ni `art://` ni `.art`.

> ⚠️ **Conditions WiX entre guillemets** : `<?if "$(var.LavChannel)" = "dev" ?>`. Sans guillemets, une
> valeur vide (build stable) donne `<?if  = "dev" ?>`, une erreur de préprocesseur.

> ⚠️ **Ne jamais modifier les valeurs stables** (organisation `LavArtemis`, UpgradeCodes, clés
> `HKCU\Software\LavArtemis`) : les installations existantes ne se mettraient plus à jour.

## 🖼️ Icônes

Générées par `docs/branding/` du dépôt **LavArtemis** (README : formes, couleurs, destination de chaque
fichier) ; `scripts/generate-ico.sh` n'est plus qu'un renvoi vers lui. Ne jamais retoucher un rendu ici.

| Fichier | Rôle |
|---|---|
| `app/res/lavartemis.svg` / `lavartemis-dev.svg` | icône des fenêtres Qt (`main.cpp`, la Dev selon `LAV_DEV_CHANNEL`) |
| `app/res/lavartemis-session.svg` / `lavartemis-dev-session.svg` | icône de la **fenêtre du stream** (SDL, `Session` : l'écran allumé = session en cours, comme l'icône de LavApollo) |
| `app/lavartemis.ico` / `lavartemis-dev.ico` | icône de l'exe (`RC_ICONS`, la Dev le remplace dans le bloc `-dev.` d'`app.pro`) et de l'installeur (`Bundle.wxs`, `BundleIcon`) |
| `app/artemis_wix.png` / `artemis_wix-dev.png` | logo 64 px de l'installeur (`BundleLogo`) |

> 📋 Le `.ico` a des entrées DIB jusqu'à 128 px et PNG à 256 px : la forme que lisent `rc.exe`, WiX et NSIS.

## 🎨 Interface v2 (LavArtemis Dev)

La refonte (spec : `docs/superpowers/specs/2026-11-23-ui-redesign-design.md` dans LavArtemis,
prototype de référence dans `docs/ui-redesign/prototype/`) vit dans **`app/gui/v2/`**, à côté de
l'interface d'origine. La Dev l'ouvre toujours au démarrage (`main.cpp`, `initialView`) : le retour
à l'ancienne interface (`uiLegacy`) a été retiré, puis **l'ancienne interface elle-même** (`PcView`,
`AppView`, `SettingsView`, la barre d'outils et leurs briques). Restent hors de `v2/` : `main.qml`
(fenêtre, dialogues de démarrage et de mise à jour), `QuickMenu`, les segues de stream et de CLI, le
mapper de manettes et le benchmark (contrôles Material aux couleurs du thème, en-tête `PageHeader`).

- `Theme.qml` (singleton, `qmldir`) : jetons des thèmes Braise / Affiche / Ludique en clair et sombre,
  `ArtemisSettings.uiTheme` / `uiThemeMode` (0 Système qui suit `Qt.styleHints.colorScheme` en
  direct, 1 Clair, 2 Sombre). `Theme.input` = dernière entrée (`pad`/`keys`/`mouse`/`touch`) :
  indications et anneau de focus seulement pour la manette et le clavier.
  `SdlGamepadKeyNavigation::gamepadInput()` est émis **après** la touche synthétique : le gestionnaire
  `Keys` (qui pose `keys`) passe d'abord, le signal (qui pose `pad`) gagne.
- `HomeView.qml` : un hôte à la fois, sections par `AppKindModel` (proxy filtrant
  `AppModel::KindRole` : `desktop`, `playnite`, `game`), Scène de la session en cours,
  Projecteur (`dimmed`) et halo (`LiveHalo`). Les autres applis de l'hôte (`app`, ex. « Virtual
  Desktop » d'Apollo) ne sont **pas** affichées, à la demande de l'utilisateur. La bibliothèque
  Playnite garde **l'ordre de l'hôte**, du plus récemment joué au plus ancien
  (`NvComputer::sortApps()` : nom pour le reste). Une jaquette absente (`no_app_image.png`, ou 404
  de LavApollo pour un jeu Playnite sans jaquette, mémorisé par `BoxArtManager::m_Missing`) affiche
  le nom du jeu.
- **Projecteur = `Theme.dim()`**, les valeurs `dimOff` / `dimFocus` du prototype par thème et mode :
  gris, luminosité puis opacité, sur **toute** la face (jaquette et nom). ⚠️ La luminosité
  **multiplie** (calque noir) : `MultiEffect.brightness` ajoute, et noircissait les jaquettes
  sombres. `AppMenu.qml` : popover au pointeur à la souris,
  centré sinon (`customEntries` pour d'autres menus du même style).
- **`HostsView.qml` (Mes PC, prototype « hosts »)** : une carte par PC appairé (état, adresse, version
  LavApollo, ce qui tourne), puis « Ajouter un PC » (PC trouvés sur le réseau ou par adresse, puis
  appairage par code PIN). Menu : Ouvrir, Benchmark, Réveiller, Renommer, Oublier. ⚠️ Les rôles de
  `ComputerModel` sont lus par numéro (`roleId()`, ordre de l'enum depuis `Qt::UserRole`) : un rôle
  ajouté ou retiré dans `computermodel.h` doit l'être dans cette liste. Et **jamais de propriété
  `action`** sur un `AbstractButton` (elle existe déjà : le composant refuse de se charger).
- Les jeux **masqués** (« Masquer ») se réaffichent depuis l'accueil (bouton sous la bibliothèque) et se
  démasquent depuis leur menu.
- **Quitter et Relancer la session demandent confirmation** (Scène et menu d'une appli), avec les textes
  d'Android (« Quitter %1 ? Ce qui n'est pas sauvegardé sera perdu. »). Le Bureau et Playnite Fullscreen
  sont nommés par leur titre de carte (`runningTitle`, « Bureau »), jamais par le nom brut de l'hôte
  (« Desktop »), dans le menu, les dialogues et les segues.
- **`SettingsView2.qml`** (Réglages) : pages à gauche, lignes à droite, d'après le tri Normal / Expert
  / Supprimer des réglages. Les lignes *expert* n'apparaissent qu'avec `ArtemisSettings.uiExpertMode`
  (bascule en tête de la liste des pages, propre à la machine) ; une page sans ligne visible disparaît
  (Manettes hors mode expert). Briques : `SettingRow` (+ `SettingToggle`, `SettingChoice` segmenté ou
  `LavDropdown`, `SettingSlider`, `SettingAction`), popups `LavPopup` / `LavDialog`,
  `PyroWaveCalibration` (SDR seulement). Les réglages retirés par le tri sont **forcés à leur valeur
  par défaut** dans `StreamingPreferences::reload()` (HDR, mise à l'échelle, boutons de souris inversés,
  défilement inversé, choix de rendu ; optimisations de jeu toujours actives) : plus rien ne pourrait
  les remettre à zéro. La taille de paquet (`--packet-size`) n'est plus enregistrée non plus : elle
  ne vaut que pour le stream lancé en ligne de commande. Les propriétés `ArtemisSettings` que rien ne
  lisait (commandes serveur, OTP, doublons d'affichage, tri des applis, permissions, input-only,
  prereleases CI) ont été retirées ; leurs groupes sont effacés d'`artemis-settings.ini` au
  `save()` suivant.
- **Avertissements au lancement : deux fonctions.** `Session::emitLaunchWarning()` obéit à
  « Avertissements de configuration » (`configurationWarnings`) et sert aux messages hérités de
  Moonlight ; `emitLavArtemisWarning()` passe **toujours** : micro refusé, multi-écran ou VRR
  ignorés, repli PyroWave, disposition de manette devinée. Ces fonctions ne s'expliquent que par là :
  un nouvel avertissement LavArtemis prend la seconde.
- Une ligne qui dépend du matériel ou de la plateforme se masque quand elle ne ferait rien (VRR hors
  Windows, « Écran tactile » sans écran tactile — `SystemProperties.hasTouchScreen` —, limite de débit avec
  PyroWave, où la préférence est gardée pour les autres codecs). Un réglage qui agit encore **reste
  visible tant qu'il est actif** (le mode Trackpad) : on ne retire pas un réglage allumé de sous les
  yeux de l'utilisateur.
- **« Écran tactile » : Multitouch / Trackpad**, en mode normal, en tête de la page Entrées, **mêmes libellés et
  même description que l'Android** (qui ne garde plus que ces deux modes en Dev). Multitouch =
  `absoluteTouchMode` vrai (`abstouch.cpp` : toucher natif si l'hôte le prend en charge, sinon souris
  émulée) ; Trackpad = faux (`reltouch.cpp` : le doigt déplace le curseur, tap = clic gauche, appui à deux
  doigts = clic droit, appui long 650 ms = clic gauche maintenu — droit avec deux doigts — et **deux doigts
  qui bougent = défilement**, porté de `RelativeTouchContext` d'Android : le doigt principal, une fois sorti
  de la zone de tap, fait défiler de 5 unités par pixel d'un écran de référence de 720 lignes, et ne bouge
  plus le curseur tant que deux doigts sont posés). Le menu en jeu a « Passer en trackpad / multitouch »
  (`SdlInputHandler::setAbsoluteTouchMode()`, pour ce stream seulement : la préférence ne change pas, comme
  Android sans « Retenir le mode tactile choisi en jeu »). Écarts restants avec Android : pas de zoom au
  pincement, et l'appui long à deux doigts (clic droit maintenu ici, rien sur Android).
- **Parité avec Android** (règle de l'utilisateur : tout ce que les deux clients ont en commun a le même
  libellé, la même description, les mêmes options, le même défaut, la même place et le même ordre ; seules
  les features propres à une plateforme diffèrent). État et écarts restants :
  `docs/superpowers/specs/2026-11-24-pc-android-parity.md` du dépôt LavArtemis. Un changement d'un texte ou
  d'un menu commun se fait des deux côtés.
- **Menu en jeu (`QuickMenu.qml`)** : construit à chaque ouverture (`rebuildMainMenu()`), entrées communes
  **dans l'ordre et avec les libellés d'Android** (Quitter <jeu Playnite>, Déconnecter, Quitter la session
  — confirmée par une sous-page « Voulez-vous vraiment quitter ? », textes d'Android —, Commandes du PC hôte,
  Envoyer des touches, presse-papiers, Statistiques, micro seulement s'il est envoyé et libellé selon son
  état, Passer en trackpad / multitouch sur un PC tactile), puis les entrées du PC : Libérer la souris et le
  clavier, Capturer le clavier, Plein écran / fenêtre. ⚠️ Ces trois-là **ne faisaient rien** avant (un
  `sendKeyCombo()` vide) : elles appellent maintenant `SdlInputHandler::toggleInputCapture()` /
  `toggleSystemKeyCapture()` / `toggleFullScreenMode()`, le code des raccourcis Z, K et X. Le menu tourne sur
  le thread principal, comme la boucle SDL (`Session::getInputHandler()` y est sûr). Commandes du PC hôte :
  plus de liste anglaise inventée quand l'hôte n'en donne pas (`ServerCommandManager`), une ligne « Aucune
  commande » explique pourquoi, comme Android.
- **Aucun bouton Aide** (demande de l'utilisateur, comme l'Android) : `NavigableMessageDialog` n'a plus
  `helpText` / `helpUrl` / `onHelpRequested`, `ErrorMessageDialog` n'a que OK ; les phrases « Click the Help
  button… » de `main.qml` sont parties avec. Tous ouvraient un wiki Moonlight en anglais. Ne pas remettre de
  `Dialog.Help`. La liste des micros se relit à l'ouverture de la page Audio et de la
  liste (`LavDropdown.opening`).
- **`Stage.qml`** : composée sur 1600 px de maquette, agrandie au-delà ; en dessous, la page rétrécit
  avec la fenêtre **mais jamais sous `minScale` (0,8)** et se compose alors sur une scène plus large
  que la fenêtre réduite. Tout réduire rendait les boutons trop petits, la taille réelle trop gros :
  0,8 est le réglage validé à l'écran. Une page qui a besoin de plus de place peut poser
  `minLayoutWidth` (plus réduite plutôt que cassée). Une page ne doit jamais supposer 1600 de
  large : largeurs en `parent.width`, pas en constantes. La fenêtre a un minimum de 1000 × 600
  (`main.qml`).
- **En-tête de l'accueil** (prototype V5, piste A validée) : une ligne si tout tient (logo + « Mise à
  jour » | sélecteur de PC centré | boutons) ; sinon une grille de deux lignes : logo | sélecteur de PC,
  puis « Mise à jour » | boutons, chaque côté aligné sur son bord (`headerBar.twoRows`, calculé sur des
  largeurs qui n'en dépendent pas : pas de boucle). Le logo suit le thème (Braise, Affiche recolorés,
  Ludique d'origine), la **Dev garde toujours son logo**.
- **Réglages en fenêtre étroite** (prototype V6, piste A) : dans `SettingRow`, quand le réglage
  laisserait moins de `minTextWidth` (320) au texte, il passe sous le libellé (`stacked`) ; une liste
  ou un curseur prend alors la largeur de la ligne. Un réglage qui s'étire déclare sa largeur côte à
  côte dans `naturalControlWidth`, sinon `stacked` dépendrait de la largeur étirée (boucle).
- **L'en-tête de l'accueil reste fixe** : il est hors du `Flickable`, la page défile dessous.
- **Barre de titre aux couleurs du thème** : la vraie barre de Windows (déplacement, ancrage, boutons
  natifs), recolorée par DWM (`SystemProperties::setWindowFrameColors()`, fond et texte du thème,
  Windows 11 ; sous Windows 10 elle reste celle du système). `main.qml` la réapplique à chaque
  changement de thème (`frameKey`) ; la fenêtre SDL du
  stream reprend les mêmes couleurs (`applyWindowFrameColors()` dans `Session`).
- **Mise à jour** : la v2 masque la barre d'outils et son bouton de mise à jour. `main.qml` expose
  `window.updateVersion` et `openUpdateDialog()` ; l'accueil (bouton « Mise à jour ») et Réglages ›
  Avancé (« Installer ») les utilisent. Sans eux, une mise à jour s'annonçait sans pouvoir s'installer.
- **Tout changement de design passe d'abord par le prototype** (canvas « LavArtemis — 4 directions »)
  et n'est codé qu'une fois validé.
- Polices OFL embarquées (`res/fonts/`, enregistrées dans `main.cpp`).

> 📋 Le fond de la fenêtre est `Theme.bg` dès la première image, et les contrôles Material restants
> prennent les couleurs et la police du thème depuis `main.qml` (`Material.*`, `font`).

## 🇫🇷 Langue

L'interface v2 est **écrite en français** (sources `qsTr("…")` en français). Le reste (mapper,
benchmark, menu en jeu, messages de stream et d'erreur, C++) a des sources anglaises traduites par
**`languages/qml_fr.ts`** (compilé en `qml_fr.qm`, embarqué par `resources.qrc`). L'utilisateur ne veut
**aucune phrase anglaise** en français : tout texte affiché passe par `tr()`/`qsTr()` (jamais une
chaîne nue dans un `emit …Failed("…")` ou un toast), et après un ajout :

```
lupdate -recursive app -extensions cpp,h,qml -no-obsolete -locations none -ts app/languages/qml_fr.ts
# traduire les entrées « unfinished » dont la source est en anglais
lrelease app/languages/qml_fr.ts -qm app/languages/qml_fr.qm
```
Les anglicismes courants restent tels quels (stream, benchmark, frame pacing, mapping, FPS, codec).
⚠️ Hors classe `QObject`, `tr()` n'existe pas : `QCoreApplication::translate("Contexte", "…")`.

> ⚠️ **Réglages v2 : une ligne lit la préférence, elle ne la garde jamais.** `checked`, `value` sont
> des liaisons sur `StreamingPreferences` / `ArtemisSettings` ; un clic émet `toggled` / `chosen` /
> `moved` et la page écrit, puis `persist()` (sauvegarde différée de 400 ms, refaite en quittant la
> page) : les setters ne persistent pas seuls. C'est ce qui fait suivre un changement de profil
> (`notifyAllPropertiesChanged()`) ou un calibrage. D'où aussi un curseur dessiné à la main : le
> `Slider` de Qt Quick Controls borne sa valeur à l'ancienne plage quand débit et plage changent
> ensemble (bascule PyroWave) et l'aurait affichée fausse.

> ⚠️ **Popups hors de la scène mise à l'échelle.** Un `Popup` vit dans l'overlay de la fenêtre : son
> contenu est composé en pixels de maquette et mis à l'échelle par `Theme.stageScale` (posé par
> `Stage.qml`). La liste de `LavDropdown` a `parent: Overlay.overlay` et se place par `mapToItem(null, …)`.

> 📋 **Clavier / manette dans Réglages** : haut/bas entre les lignes de la page (sans en sortir),
> gauche/droite règlent un choix ou un curseur, sinon gauche revient aux pages ; Entrée/Espace
> basculent ; Page préc./suiv. changent de page ; Échap quitte. Une ligne à plusieurs boutons met
> `focusable: false` et son `Row` porte `hgroup: true` (gauche/droite passent d'un bouton à l'autre).

> 📋 **Session verrouillée** : sans rendu, les positionneurs (`Column`) ne sont plus recalculés ; leurs
> hauteurs lues dans un test automatisé sont fausses. Ne vérifier la mise en page que session ouverte.

> ⚠️ **Délégués : `model.xxx`, pas `required property`** quand le rôle porte le nom d'une
> propriété du composant (`running` existe déjà sur `AppCard`/`CoverTile`).

> 📋 **Captures** : sur une session Windows verrouillée, Qt ne rend plus la fenêtre (PrintWindow
> rend une image figée, `grabToImage` n'aboutit jamais). Vérifier l'interface session ouverte.

## 🌊 PyroWave

[PyroWave](https://github.com/Themaister/pyrowave) est un codec intra-only en ondelettes, décodé sur
GPU en Vulkan : moins d'une milliseconde, mais des centaines de Mbit/s, donc réseau filaire. Porté
depuis le fork moonlight-qt de Nonary (`release/6.1.0-vrr18`), sans sa partie VRR ; le protocole est
celui de Vibepollo, inchangé (`docs/pyrowave-protocol.md`, **identique** à celui de LavApollo —
changer les deux ensemble). Jamais choisi en Auto : uniquement via le codec « PyroWave ».

| Pièce | Où |
|---|---|
| Lib (codec + Granite élagué) | sous-module `pyrowave/pyrowave` → `Lavagnou/pyrowave` (branche `lavartemis`), sources dans `pyrowave/lavartemis/pyrowave.pri`, projet `pyrowave/pyrowave.pro` |
| Parsing du record framing, frames partielles | `streaming/video/pyrowave/pyrowaveframing.{h,cpp}` |
| Décodeur (Vulkan) | `streaming/video/pyrowave/pyrowavedecoder.{h,cpp}` |
| Rendu Windows | `ffmpeg-renderers/d3d11pyrowave.{h,cpp}` + `d3d11va.cpp` + shader `d3d11_yuv_planar_pixel` |
| Rendu Linux | `pyrowave/pyrowaveplacebo.{h,cpp}` + `plvk.cpp` |
| Intégration décodeur | `FFmpegVideoDecoder::initializePyroWave()` / `sendPyroWaveFrame()` / `receiveFrame()` (`ffmpeg.cpp`) |
| Calibrage | `pyrowave/pyrowavecalibrator.{h,cpp}`, `backend/networkbuffers.{h,cpp}`, `NvHTTP::probePyroWaveDownloadMbps()` |

> ⚠️ **Un seul PyroWave.** Le bitstream n'a pas de champ de version : LavApollo, ce client et
> l'Android doivent épingler **le même commit** de `Lavagnou/pyrowave`. `PYROWAVE_BITSTREAM_ID`
> (`Limelight.h`) vaut `502a3b52` (v1) ; `186f0393` (Vibepollo/Nonary) est accepté, le format réseau
> étant identique.

> ⚠️ **Pas de patchs Nonary dans la lib.** Le décodeur de Nonary appelait trois fonctions ajoutées
> par ses patchs locaux de PyroWave (cache de vues, 4 frame contexts, timing de contexte). Le fork
> reste au plus près d'upstream (MIT) : ces appels sont retirés, on garde les 2 frame contexts
> d'upstream. Si des frames « stale » sautent sous charge GPU, c'est la première piste.

> ⚠️ **FFmpeg n'est pas impliqué.** `completeInitialization()` a été scindé : la partie codec FFmpeg
> est dans `initializeAVCodecContext()`, sautée quand `decoder == nullptr` (PyroWave). Une frame
> PyroWave est un `AVFrame` dont `buf[0]` porte un `PyroWaveFrameRef` (surface + valeurs de fence) ;
> `d3d11va.cpp` la reconnaît avec `PyroWaveFrameRef::fromFrame()`.

> 📋 **x86_64 uniquement** (`lavartemis.pro` / `app.pro`, condition identique aux deux endroits) :
> les maths de Granite n'ont pas été vérifiées sous MSVC ARM64. Le build arm64 compile sans
> `HAVE_PYROWAVE` et n'affiche pas l'option.

> 📋 Le calibrateur classe les formats par marge de décodage ; les paliers « vrr » / « vrrLarge » de
> Nonary restent présentés comme « Tight » / « Very tight ». La VRR (ci-dessous) est arrivée après le
> calibrateur et ne l'a pas changé.

## 🖥️ VRR (présentation adaptative, Windows)

Port **fidèle** du pacer VRR de [Nonary/moonlight-qt](https://github.com/Nonary/moonlight-qt)
(`release/6.1.0-vrr18`, 18e itération réglée sur matériel). Sur un écran G-Sync/FreeSync, chaque image
est présentée **à l'instant que vise sa timestamp RTP** (slot source + délai de lecture appris), avec
`Present(0, ALLOW_TEARING)` en plein écran sans bordure, au lieu d'attendre le vsync suivant.

| Pièce | Où |
|---|---|
| Contrôleur de timing (horloge hôte→locale, budgets appris, 3 profils) | `pacer/vrr/vrrtimingcontroller.{h,cpp}` + en-têtes de politique `pacer/vrr/*` |
| Worker de pacing (admission, attente de cible, Present) | `pacer/vrrpacingworker.{h,cpp}`, créé par `Pacer::initialize()` si le renderer expose un `IVrrFramePresenter` |
| Présentateur D3D11 | `d3d11va.{h,cpp}` (`prepareFrame()` / `presentAdaptive()`, frontières de décodage par fence, échantillonnage du balayage), `d3d11composition.*`, `dxgipresent.h` |
| Activation | `Session::snapshotPresentationSettings()` : V-Sync effective, fréquence d'écran connue, marge (`VrrRatePolicy`), puis plein écran **sans bordure** forcé pour la session (la préférence enregistrée n'est pas touchée) |
| Demande à l'hôte | `&clientVrrRequested=1` sur `/launch` (`NvHTTP::startApp`) → LavApollo crée l'écran virtuel à 1000 Hz |
| PyroWave | `holdPyroWaveDecodeForPresent()` retient un décodage qui chevaucherait le Present, et l'échéance de réassemblage passe par `VrrReceiveDeadline` (`LiSetVideoReassemblyDeadlineCallback`) |
| Diagnostics | `diagnostics/` (traces par stream dans `vrr-diagnostics` du dossier de données de l'appli, `AppDataLocation`, à côté des CSV de perf ; plus sur le Bureau), export ZIP et ouverture du dossier par le singleton QML `VrrDiagnostics` (Réglages › Avancé, mode expert) |
| Tests | `tests/` (opt-in : `qmake tests/tests.pro CONFIG+=tests`), 14 suites portées dans `tests/vrr` (+ `compositionprobe` sous Windows), plus `tests/benchmark` ; `tst_overlay` se lance avec `app/` en argument (police ModeSeven) |

> ⚠️ **Le pacer VRR alimente aussi `PacingStats`** (`VrrPacingWorker`, paramètre `pacingStats`) : sans
> ça, l'overlay et le CSV de perf ne décriraient que le chemin classique et une session VRR paraîtrait
> vide. La mesure est l'instant de soumission, comme le hand-off du chemin classique.

> ⚠️ **Jamais avec le multi-écran émulé** : la toile couvre plusieurs moniteurs dans une seule fenêtre,
> la présentation adaptative suit un seul écran. `snapshotPresentationSettings()` refuse la VRR et le
> dit par un launch warning.

> ⚠️ **Fusion sélective.** Les fichiers partagés ont été fusionnés en 3-way contre moonlight-qt
> `17d5b1a8` (base de Nonary) en ne gardant que la VRR : **ni** l'haptique DualSense (et son mapping
> `psmap`), **ni** Gamescope, **ni** leur versionnage n'ont été repris. Les stats suivent leur
> refonte (`totalPacerTimeUs`/`totalRenderTimeUs` → files/pacing/rendu séparés, télémétrie du Pacer
> fusionnée par le décodeur via `syncPacerTelemetry()`).

> 📋 **Linux : pas encore.** Le présentateur Vulkan (`plvk`) de Nonary n'est pas porté. Le code compile
> sur Linux et une demande VRR y retombe proprement sur le pacing fixe (« unsupported renderer »).
> D'où : hors Windows, `StreamingPreferences::reload()` force `enableVrr` à faux (une demande forçait
> quand même le plein écran sans bordure) et Réglages n'offre pas l'option.

## 🎤 Passthrough du micro

Le micro de ce PC est joué par LavApollo dans un micro virtuel de l'hôte (Steam Streaming Microphone).
Réglage **LavArtemis Features → Microphone**, désactivé par défaut, avec le choix du micro. Protocole :
`LavApollo/docs/mic-passthrough.md`, vue d'ensemble dans le `CLAUDE.md` racine de LavArtemis.

- `streaming/audio/micstreamer.{h,cpp}` : capture SDL 48 kHz mono S16 par blocs de 960, découpe en
  trames exactes de 20 ms, Opus VOIP 32 kbit/s (FEC + DTX), `LiSendMicrophoneData()`. **Tout dans le
  callback SDL** : c'est le seul thread qui appelle l'envoi, ce qu'exige moonlight-common-c.
- **Session** : `validateLaunch()` lit `NvComputer::micPassthroughState` (serverinfo) et décide
  `m_MicRequested` (sinon avertissement de lancement avec la raison) ; `startApp()` ajoute
  `clientMicRequested=1` ; le micro démarre après `LiStartConnection()` et s'arrête dans
  `DeferredSessionCleanupTask` **avant** `LiStopConnection()`.
- `NvComputer::update()` recopie maintenant `micPassthroughState` **et** `isMultiDisplayCapable` : sans ça
  un droit coché sur l'hôte (ou un hôte mis à jour) n'était vu qu'au redémarrage du client.
- **Couper** : `KeyComboToggleMicrophone` (Ctrl+Alt+Shift+**U**, M étant le mode souris) et l'entrée
  `toggle_microphone` du menu rapide, tous deux vers `Session::toggleMicrophoneMute()`. Le message
  « Microphone muted » passe par `Overlay::StatusSource::Microphone`, affiché **en plus** des autres
  messages d'état, jamais masqué par eux.
- ⚠️ **Sous-système audio SDL partagé** avec `SdlAudioRenderer` : ses deux `SDL_assert(!SDL_WasInit(...))`
  ont été retirés, le comptage de références de SDL suffit.
- ⚠️ **Pas d'annulation d'écho** côté SDL : avec des haut-parleurs, le son du jeu repart vers l'hôte.
  Le texte du réglage recommande un casque.

## 📊 Benchmark des réglages

Clic droit sur un hôte → **Benchmark Testing** (`gui/BenchmarkView.qml`) : le client streame plusieurs fois
la scène animée de l'app cachée « Benchmark » de LavApollo, une combinaison codec / débit / pacing par
run, et note chaque run (latence, fluidité) face aux réglages de l'utilisateur. Spécification commune aux
deux clients (planification, mesures, formules, JSON) : `LavApollo/docs/benchmark.md`.

| Pièce | Où |
|---|---|
| Logique pure (planificateur Rapide / Complet / Adaptatif, scores, rapport JSON + historique) | `backend/benchmark/benchmark{types,planner,scoring,report}.*`, `presenthistogram.h`, testés par `tests/benchmark` |
| Chef d'orchestre (sessions, mesure, rapport, appliquer) | `backend/benchmark/benchmarkrunner.{h,cpp}`, singleton QML `BenchmarkRunner` |
| Stats d'une session | `IVideoDecoder::getSessionStats()` (publié par `FFmpegVideoDecoder::publishBenchmarkStats()` à chaque fenêtre d'1 s), `Session::benchmarkSnapshot()` |
| Calendrier d'un run | `Session::setBenchmarkMode()` / `benchmarkTick()` |
| Histogramme des présentations | `PresentHistogram` dans `PacingStats` (`histogram()`), alimenté par les deux chemins (classique et VRR) |
| Réglages d'un run | `StreamingPreferences::createDetached()` |

> ⚠️ **Le calendrier d'un run vit dans la boucle SDL de `Session::exec()`, pas dans un `QTimer`.** Pendant un
> stream, la boucle d'événements Qt ne tourne pas : un timer du runner ne se déclencherait qu'à la fin.
> `benchmarkTick()` est appelé à chaque tour (au moins une fois par seconde, le `SDL_WaitEventTimeout` expire),
> prend les instantanés et pousse `SDL_QUIT` à la fin prévue.

> ⚠️ **Un instantané porte sa propre horloge** (`RawStats::elapsedUs` = l'instant où le décodeur a publié sa
> fenêtre) : deux instantanés se soustraient exactement, même pris en retard. Ne jamais dater un instantané
> avec l'heure de la prise.

> ⚠️ **Un décodeur recréé en cours de stream repart de zéro** (changement de fenêtre, reset du device) :
> `Session::carryBenchmarkStats()` garde les compteurs de l'ancien avant le `delete`. Sans ça, une recréation
> pendant un run donnerait des différences négatives, ramenées à 0.

> ⚠️ **Les réglages d'un run sont détachés et jamais sauvegardés.** `createDetached()` copie les réglages
> courants (profil actif compris), ne suit pas les changements de profil, et son `save()` ne fait rien. Le run
> de référence (« Your settings ») et sa répétition streament les réglages de l'utilisateur sans y toucher.

> ⚠️ **On note la config *effective*, pas la demandée** (`Session::effectiveBenchmarkConfig()`) : AV1 sans
> décodeur retombe en HEVC, la VRR refusée par l'écran retombe sur le pacing fixe. Un run replié est marqué
> `fallback` et le planificateur saute les suivants du même genre. Et le plan part de ce que la référence a
> réellement streamé : en Auto, le codec n'est connu qu'après.

- Le micro n'est jamais envoyé pendant un run (`setBenchmarkMode` → `m_MicRequested` reste faux), et
  `StreamSegue` en `benchmarkMode` saute les toasts d'avertissement (3,5 s chacun) et la boîte d'erreur : les
  erreurs vont dans le rapport.
- Quitter un run (Ctrl+Alt+Shift+Q) avant sa fin arrête tout le benchmark ; une erreur de connexion ne
  fait qu'échouer ce run.
- L'app de l'hôte est quittée avant le premier run, entre deux runs dont la demande VRR diffère (l'écran à
  1000 Hz n'existe qu'au `/launch`) et après le dernier.
- Rapports dans `AppDataLocation/benchmarks/`, 50 au plus ; « Open a report file… » ouvre aussi ceux de
  l'Android (même JSON). Les appliquer n'est possible que pour un rapport desktop (noms de pacing différents).

## 📐 Résolution calée sur l'écran du PC

Rejoindre une session en cours, ou lancer **sans** écran virtuel, streame l'écran que l'hôte affiche :
au **format de cet écran**, à la plus grande taille qui tient dans nos réglages **et** dans cet écran
(jamais agrandi), à `min(fréquence réglée, fréquence de l'écran)`. LavApollo annonce le mode dans
`serverinfo` (`CurrentDisplayWidth/Height/RefreshRate`, lus par `NvComputer`) ; sans eux, rien ne
change. Règle et valeurs de référence identiques à l'Android : `backend/hostdisplayfit.{h,cpp}`, testés
par `tests/hostdisplay`.

- Appliquée dans `Session::initialize()` après le calcul de la toile multi-écran, **jamais** pour le
  multi-écran émulé (il a sa propre toile). En mémoire seulement.
- **Suivi en cours de session** : `Session::hostDisplayWatchTick()` (appelé dans la boucle SDL comme
  `benchmarkTick()` : les timers Qt ne tournent pas pendant un stream) relit `serverinfo` toutes les
  2,5 s sur un worker. Sur un changement confirmé, il recopie le nouveau mode dans `NvComputer`, pousse
  `SDL_QUIT` et `StreamSegue.qml` démarre `createRestartSession()` à la place du retour à la liste.
  Le protocole fixe la taille à la connexion : pas de changement à chaud.

> ⚠️ **`HostDisplayWatch` compare la cible au stream en cours**, jamais l'écran au stream : un écran
> 2400×1080 streamé en 1920×864 est l'état ajusté. Comparer à l'écran reconnecterait en boucle. Deux
> lectures identiques avant d'agir : Windows passe par des modes intermédiaires.

> ⚠️ **La session relancée lit le mode dans `NvComputer`**, que le `ComputerManager` ne rafraîchit pas
> pendant un stream : `hostDisplayWatchTick()` y écrit le nouveau mode **avant** de demander la
> relance, sinon elle repartirait à l'ancien.

## ⚠️ Pièges / Gotchas

1. **`ArtemisSettings` ne persiste pas tout seul.** Les setters n'écrivent pas ; `SettingsView2.qml`
   appelle `persist()` à chaque changement et `ArtemisSettings.save()` dans `StackView.onDeactivating`
   et `Component.onDestruction`.
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
- ⚠️ **Le compile-check Linux construit *sans* `APP_IMAGE`**, alors que la release construit l'AppImage
  *avec*. Les branches `#ifdef` compilées ne sont donc pas les mêmes : une erreur qui ne touche qu'un
  chemin laisse la release **verte** pendant que `build.yml` est rouge. C'est ce qui a caché 17 jours
  de compile-check cassé (10 → 26 août 2026) : `findAssetDownloadUrl()` sortait par
  `return QString();` dans son `#else`, mais **`return` n'empêche pas le compilateur d'analyser la
  suite** — la boucle en dessous référençait un `wantedSuffix` jamais déclaré. Un `#else` qui ne fait
  que sortir doit quand même **déclarer** ce dont le code suivant se sert. Et un compile-check rouge
  n'est jamais cosmétique, même quand la release passe.
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
branche `lavartemis` @ `fc72797` = `moonlight-stream/master` + `ebb15ef` (`LiSendExecServerCmd`,
extension Apollo) + `0da9626` (`LiSendEmptyPayload`) + les 7 commits PyroWave de Nonary (négociation,
frames partielles, délai de réassemblage) + l'ID de bitstream v1 (`63c75f1`) + `LiSendMicrophoneData`
(`fc72797`, passthrough du micro). Les hashes `84af637` / `c999436` sont ceux de la branche `master`
du fork, pas des ancêtres de `lavartemis`.

**Le client Android pointe le même commit.** `LiSendEmptyPayload` est du code mort ici ; il est sur
la branche pour que l'Android puisse la partager. Voir `CLAUDE.md` côté LavArtemis pour ce qu'un
rebump implique là-bas (FEC nanors, timestamps en µs).

## 📝 Conventions

- C++ / QML, style existant. Pas de reformatage massif : ça rendrait les merges amont douloureux.
- Merger `upstream-qt/master` régulièrement — c'est la principale raison d'être de ce dépôt séparé.
- GPLv3. Conserver les attributions Moonlight / Artemis / wjbeckett.
