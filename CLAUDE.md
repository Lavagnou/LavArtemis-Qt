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
| Plateformes | Windows **x64** (releases locales) ; ARM64 et Linux x86_64 seulement par le workflow manuel |
| Installeur | **WiX 7** |
| Version | `app/version.txt`, remplacée au build par `CI_VERSION` (posée par `local-release.ps1` de LavArtemis, ou la CI) ; la valeur committée ne sert pas aux releases |

macOS et Steam Link : le code amont est là mais **hors périmètre** — non buildé, non testé, non publié.

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
scripts\build-arch.bat Release         # archi = celle du kit Qt dont le qmake est dans le PATH
scripts\generate-bundle.bat Release    # installeur WiX combiné (x64 + arm64, ou x64 seul si LAV_X64_ONLY=1)
scripts\build-test.bat                 # client du banc de test (CONFIG+=lav_test), voir « Build de test »

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

Réglage **« Un écran virtuel par moniteur »** (Réglages › Affichage, `StreamingPreferences::useMultiDisplay`,
défaut off, grisé sans l'écran virtuel). `DisplayLayout::detect()`
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
> réévaluation du binding, pas seulement au clic. L'ancienne case liait
> `checked: enabled && useMultiDisplay` et réécrivait la préférence : décocher l'écran virtuel un
> instant suffisait à l'effacer **définitivement**, la valeur source étant détruite. `onToggled` ne
> réagit qu'à une action utilisateur. La ligne reste cochée en grisé, ce qui est la lecture
> honnête — `Session::initialize()` exige les deux préférences de toute façon.

Une pastille « Multi-écran » est aussi posée dans l'en-tête de l'accueil (`gui/v2/HomeView.qml`), là où
l'on lance les jeux : c'est le réglage qu'on change selon l'endroit où l'on est assis, juste avant de
lancer. Elle **persiste elle-même** (`StreamingPreferences.save()`) : contrairement aux Réglages,
l'accueil n'a pas d'événement de navigation sur lequel sauvegarder.

> ⚠️ Un renoncement silencieux est pire que le renoncement lui-même : avec l'option cochée et un
> seul écran, `detect()` sortait sans `problem()` et `session.cpp` n'avertissait que si `problem()`
> était non vide — donc **rien**. Impossible de distinguer « option désactivée », « second écran
> replié » et « cassé ». La décision est maintenant loguée dans tous les cas et avertie quand il n'y
> a qu'un écran à refléter.

### Délibérément **non** portés

| Feature Android | Pourquoi |
|---|---|
| `checkbox_paced_ull` | C'est un rattrapage Android : en ULL le renderer présente sans alignement vsync. Le desktop a un vrai `IVsyncSource` derrière l'option **« Régularité des images (frame pacing) »** (Réglages › Affichage) — l'activer *est* le mode pacé, en mieux (vsync réel, pas extrapolé). |
| `checkbox_predictive_pacing` | L'équivalent supposé, `SetMaximumFrameLatency(1)`, est **délibérément évité** en amont : cf. le commentaire de `d3d11va.cpp` (~l. 551) — avec `SyncInterval 0`, ça fait bloquer `Present()` sur DWM et **augmente** la latence. Le `+2 vsync` Android est un détail SurfaceFlinger sans équivalent. |
| AV1 auto (≤15 Mbps ou 4K) | Le mode Auto amont sonde déjà les décodeurs et *déprioritise* AV1 selon la dispo HEVC HW (`session.cpp` ~l. 909-944). Ajouter le seuil Android *retirerait* AV1 à >15 Mbps en <4K et ferait retomber en H.264 les GPU qui ne décodent qu'AV1 : régression. |
| ADPF | Pas d'équivalent Windows. L'inhibition de veille est déjà couverte par `SDL_DisableScreenSaver()`. |
| SBS 3D MiDaS | Imposerait de réimplémenter la passe DIBR dans **chaque** backend, de remplacer TFLite par ONNX Runtime/DirectML et d'embarquer ~50-100 Mo de deps, pour une valeur desktop étroite. |

## 🧩 Features Artemis portées

| Feature | Où | Note |
|---|---|---|
| **Profils de réglages** | `settings/profilemanager.{h,cpp}` | `profiles.json` dans `AppConfigLocation`. Un profil est un **snapshot complet**, pas un patch épars. `language`, `defaultver` et les clés propres à la machine (`mdns`, `detectnetblocking`, `uidisplaymode`, `tracevrrframes`) **bypassent** les profils (`isGlobalKey()`). |
| **Send keys + macros** | `backend/keymacromanager.{h,cpp}` | 15 presets (mêmes que l'Android Dev ; le raccourci HDR Win+Alt+B est retiré des deux) + `keyboard-macros.json`. **Même format et mêmes noms `VK_` que l'Android** → un fichier marche sur les deux. Table des 174 noms extraite de `KeyMapper.java`, pas retapée. |
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
clic sur un stick (file de 2 : X puis Y), et « Tout mapper dans l'ordre » (file de 21). L'ancien
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
> actionnable — au lancement d'un stream, et dans la liste du mapper (« Disposition devinée — peut-être
> fausse »). Ce badge se lit par **GUID** (`getGuessedDeviceGuids()`), pas par nom : deux manettes du
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
| Canal stable | `/releases/latest` (GitHub n'y met jamais une prerelease) |
| Canal Dev | `/releases` (liste complète), `pickDevRelease()` garde la plus récente `-dev.` ; ni stable ni `-ci.` |
| Comparaison | `compareVersionStrings()` : à base égale, une stable bat toujours un suffixe (`-dev.N`, `-ci.N`) |
| Asset Windows | **`-win-installer.exe`, quelle que soit l'architecture.** Le bundle WiX embarque les MSI présents (x64, et arm64 quand la release vient du workflow) et choisit par `NativeMachine` (⚠️ l'installeur x64 seul d'une release locale n'installe donc rien sur un PC ARM64) ; les zips `-win-x64`/`-win-arm64` sont des builds **portables**, sans installeur à exécuter dedans |
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

> 📋 **« Ignorer cette version »** (comme Android, `UpdateChecker.skipVersion`) : troisième bouton du
> dialogue de mise à jour (`Dialog.Discard` renommé). `AutoUpdateChecker::skipVersion()` garde la version
> (`QSettings`, `updateSkippedVersion`) ; le check **du démarrage** (`start()`, `m_AutomaticCheck`) finit
> alors par `noUpdateAvailable` pour cette version, `checkNow()` la propose toujours. La pastille
> « Mise à jour » disparaît aussitôt.

> ⚠️ **Le bouton des réglages appelle `checkNow()`, pas `start()`.** `start()` respecte la case
> « vérifier au démarrage » — décochée, elle lui faisait refuser le check qu'on venait de demander.

> 📋 Une correction de l'updater **ne peut pas se livrer par l'updater** : la version cassée est celle
> qui télécharge. Prévoir une installation manuelle et le dire dans les notes de release.

> 📋 **Mise à jour sans clic** (depuis 0.7.0-dev.5) : `installAndRestart()` lance le bundle en
> **`/passive /norestart`** (barre de progression seule : ni licence, ni « Suivant », ni « Launch » ; Windows
> demande quand même l'élévation) par un `cmd /c start "" /wait "<installeur>" … & start "" "<app>"`
> invisible (`CREATE_NO_WINDOW`, `setNativeArguments` pour garder les guillemets des chemins avec espaces),
> qui **relance l'appli** une fois le bundle fini — aussi si l'élévation est refusée, pour ne jamais laisser
> l'utilisateur sans appli. Vérifié avec des chemins à espaces : l'appli part après la fin de l'installeur.
> La fenêtre affichée pendant ce passage est la page `ProgressPassive` du thème (« Mise à jour… », « se
> relance à la fin ») : voir « Fenêtre de l'installeur ».

> ⚠️ **L'installeur ne peut pas se supprimer lui-même.** `installAndRestart()` lui passe le fichier
> puis quitte : le seul processus qui pourrait nettoyer est celui qu'on remplace. C'est donc le
> **démarrage suivant** qui le fait (`cleanupDownloadedInstallers()`, appelé depuis le constructeur),
> et à ce moment-là l'installeur a forcément fini — l'app qu'il a installée est celle qui tourne.

> ⚠️ **Le nettoyage compare des versions, pas des dates.** Un installeur **plus récent** que la version
> qui tourne peut être un téléchargement en cours dans une **autre instance** — il n'y a pas de verrou
> d'instance unique côté desktop — donc il est conservé. Tout le reste (plus ancien ou égal) a déjà été
> installé ou abandonné. Un critère par ancienneté de fichier n'aurait pas cette garantie.

## 🧪 Canal Dev (LavArtemis Dev)

Une version qui contient `-dev.` (`app/version.txt` ou `CI_VERSION`, voir « Stack technique ») construit
**LavArtemis Dev**, installée à côté de la stable. Branche `dev` ici, branche `dev` dans LavArtemis.
Vue d'ensemble et tableau stable/dev : section « Stable / Dev » du `CLAUDE.md` de LavArtemis.

- `app/app.pro` : `LAV_DEV_CHANNEL` défini, `VERSION` numérique (`0.7.0-dev.3` → `0.7.0.3`),
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

> ⚠️ **Ne jamais modifier les valeurs stables** (organisation `LavArtemis`, UpgradeCode **MSI**, clés
> `HKCU\Software\LavArtemis`) : les installations existantes ne se mettraient plus à jour. Les
> UpgradeCodes du **bundle** ont changé une fois, avec la renumérotation (ci-dessous) ; les nouveaux
> sont à leur tour intouchables.

## 🔢 Numérotation propre et installeur (depuis 0.7.0-dev.1)

Jusqu'à `21.8.0-dev.9` / `21.7.1`, le numéro venait de Moonlight. LavArtemis compte désormais seul : la
Dev repart à **`0.7.0-dev.N`**, la stable passera à **`1.0.0`** à la toute fin. Saut unique, à la main :
l'updater d'une 21.x voit 0.7 comme plus ancien et ne le propose pas (accepté). Vue d'ensemble : section
« Numérotation propre » du `CLAUDE.md` de LavArtemis.

> ⚠️ **INVARIANT : la nouvelle numérotation reste sous `20.0.0`, pour toujours.** Un majeur ≥ 20 est
> l'ancienne numérotation, plus vieille que toute nouvelle : `AutoUpdateChecker::compareVersionStrings()`
> (`LEGACY_VERSION_MAJOR`, updater **et** nettoyage des installeurs téléchargés — sans la règle, un
> `LavArtemis-v21.8.0-dev.9-win-installer.exe` resté dans `%TEMP%` passerait pour un téléchargement en
> cours et ne serait jamais supprimé) et la table Upgrade du MSI. `scripts/build-arch.bat` refuse de
> construire une version ≥ 20.

**MSI** (`wix/LavArtemis/Product.wxs`, les deux canaux, UpgradeCode MSI inchangé) : `MajorUpgrade`
remplacé par une table Upgrade explicite. La version de comparaison est celle de l'exe
(`!(bind.fileVersion.LavArtemisExe)`), et le MSI n'en compare que trois nombres.

| Ligne | Versions installées | Attributs | Effet |
|---|---|---|---|
| (a) `LAV_UPGRADE_DETECTED` | `0.0.0` → cette version, incluse pour la Dev (769), exclue pour la stable (257), comme l'ancien `AllowSameVersionUpgrades` | MigrateFeatures | mise à niveau |
| (b) `LAV_NEWER_DETECTED` | au-dessus de cette version, sous `20.0.0` (2) | OnlyDetect | `Launch Condition="NOT LAV_NEWER_DETECTED"` refuse : « Une version plus récente de [ProductName] est déjà installée. » — **protection permanente contre le retour en arrière** |
| (c) `LAV_LEGACY_DETECTED` | `20.0.0` et plus (257) | MigrateFeatures | mise à niveau, quel que soit le numéro : le saut 21.x → 0.7 / 1.0 |

- `RemoveExistingProducts After="InstallInitialize"` (1501), comme le `Schedule="afterInstallInitialize"`
  d'avant ; `FindRelatedProducts` (25) passe avant `LaunchConditions` (100).
- ⚠️ **`REINSTALLMODE=dmus`** (Property du MSI) : un fichier est copié dès que la version installée
  **diffère**, pas seulement quand elle est plus ancienne. Le calcul des fichiers à copier se fait **avant**
  que `RemoveExistingProducts` retire l'ancien produit : au saut 21.x → 0.7, `LavArtemis.exe` 21.8.0.x
  paraissait « plus récent » que 0.7.0.x, n'était pas copié, puis la désinstallation de l'ancien produit
  le supprimait — **0.7.0-dev.1 s'est installée sans son exe** (journal MSI : `Note: 1: 2753 2:
  LavArtemisExe`). La protection contre le retour en arrière, c'est la table Upgrade ci-dessus, pas les
  règles de version des fichiers.
- **Réglages gardés** : l'ancien produit est désinstallé par **ses propres tables**, avec
  `UPGRADINGPRODUCTCODE` posé par `RemoveExistingProducts` quelle que soit la ligne qui l'a trouvé. Son
  `DeleteRegistryKey` est conditionné `NOT UPGRADINGPRODUCTCODE` (vérifié sur le MSI 21.8 en cache) ; le
  `RemoveFolderEx` de l'AppData n'a pas bougé et n'a jamais vidé l'AppData sur les mises à niveau de la Dev
  21.8 (dossier du 23/11 06:26 toujours là après six mises à niveau). **Ne jamais exiger une
  désinstallation** : elle effacerait `HKCU\Software\LavArtemis[ Dev]` et l'AppData.

**Bundle** (`wix/LavArtemisSetup/Bundle.wxs`) : **nouveaux UpgradeCodes** (Dev
`72171b88-37c9-4543-9707-1cd668b848a4`, stable `51150c91-b139-4a69-aa95-e8e6df6efaec` ; anciens, à ne
jamais reprendre : Dev `0f199e84-…`, stable `0dc77dbb-…`). Burn refuse un bundle plus ancien qu'un bundle
apparenté installé, et 0.7 < 21.8 : sous l'ancien code, le saut échouait. Le bundle 21.x n'étant plus
apparenté, c'est la ligne (c) du MSI qui remplace son MSI ; ensuite le contrôle de Burn (qui compare les
quatre nombres) protège la nouvelle numérotation, y compris d'un build Dev plus ancien que le MSI laisserait
passer.

> 📋 **Reste une fois** : l'entrée du bundle 21.x dans « Applications installées ». La désinstaller est sans
> risque : son MSI (retrouvé par son propre ProductCode, celui du nouveau MSI est différent) est déjà
> parti, ses deux `ExePackage` VC++ sont `Permanent="yes"`. Ne **pas** la réparer : elle réinstallerait
> son MSI 21.x, dont le `MajorUpgrade` voit 0.7 comme plus ancien. Même limite pour un ancien installeur
> 21.x lancé à la main : ses règles sont figées. Écartés : `RelatedBundle` sur l'ancien code (Burn y voit
> un retour en arrière et échoue, sauf `SuppressDowngradeFailure` qui couperait aussi la protection) et
> une version de bundle décalée (100.7.0.1) qui garderait l'ancien code mais s'afficherait partout.

## 🖼️ Icônes

Générées par `docs/branding/` du dépôt **LavArtemis** (README : formes, couleurs, destination de chaque
fichier) ; `scripts/generate-ico.sh` n'est plus qu'un renvoi vers lui. Ne jamais retoucher un rendu ici.

| Fichier | Rôle |
|---|---|
| `app/res/lavartemis.svg` / `lavartemis-dev.svg` | icône des fenêtres Qt (`main.cpp`, la Dev selon `LAV_DEV_CHANNEL`) |
| `app/res/lavartemis-session.svg` / `lavartemis-dev-session.svg` | icône de la **fenêtre du stream** (SDL, `Session` : l'écran allumé = session en cours, comme l'icône de LavApollo) |
| `app/lavartemis.ico` / `lavartemis-dev.ico` | icône de l'exe (`RC_ICONS`, la Dev le remplace dans le bloc `-dev.` d'`app.pro`) et de l'installeur (`Bundle.wxs`, `BundleIcon`) |
| `wix/LavArtemisSetup/theme/<dev\|stable>/` | **fenêtre de l'installeur** : `thm.xml`, images, `payloads.wxi`, générés par `generate-installer.mjs` (voir « Fenêtre de l'installeur ») |

> 📋 `app/artemis_wix*.png` (ancien logo 64 px de l'installeur) ne sert plus qu'au `wix/MoonlightSetup` hérité, que rien ne construit.

> 📋 Le `.ico` a des entrées DIB jusqu'à 128 px et PNG à 256 px : la forme que lisent `rc.exe`, WiX et NSIS.

## 🪟 Fenêtre de l'installeur (depuis 0.7.0-dev.7)

Direction « A » du prototype (page V7, validée le 2026-11-24), **au plus près de la maquette** : toute la
fenêtre, barre de titre comprise, prend le fond du logo — **plan d'atelier** quadrillé `#1D4E89` pour la Dev,
**dégradé nuit** pour la stable —, logo à gauche, titres en Space Grotesk, textes en Inter, boutons pilule de
48 px, case à cocher et champ du dossier de la maquette, en français au tutoiement, **sans page de licence**.
Pages : accueil (raccourci sur le Bureau, dossier + « Changer… »), dossier d'installation, progression,
**mise à jour passive** (celle de l'updater), déjà installée (Désinstaller / Réparer / Fermer), terminé
(« Lancer LavArtemis »), échec (lien vers le journal), aide. (La 0.7.0-dev.6 avait une première version, avec
la barre de titre Windows et les polices système : refusée.)

Trois morceaux :
- **`theme/<dev|stable>/`** (thème, images, `payloads.wxi`) et **`theme/fonts/`** (Inter 400/500/700 et Space
  Grotesk 700, sous-ensembles latins renommés `LavInstall …`) : générés par `docs/branding/` du dépôt LavArtemis
  (`generate-installer-fonts.py`, puis `generate-installer.mjs`), jamais retouchés à la main. Les textes des
  boutons, de la case, du lien et de la barre de titre sont **dessinés dans les images** par le générateur.
- **`theme/thm.wxl`** : les textes des libellés (titres, sous-titres, dossier…), écrits à la main, communs aux
  deux canaux (`[LavDisplayName]` et `[LavVersion]` viennent de `Bundle.wxs`, `LAV_VERSION` posé par
  `generate-bundle.bat`).
- **`bafunctions/lavbafunctions.cpp`** : une DLL BAFunctions de WixStdBA, compilée par la cible
  `LavBuildBAFunctions` de `LavArtemisSetup.wixproj` (cl/link x86, CRT statique ; en-têtes et bibliothèques des
  paquets NuGet `WixToolset.WixStandardBootstrapperApplicationFunctionApi`, `…BootstrapperApplicationApi`,
  `…DUtil`, seulement téléchargés). Elle charge les polices en privé (`AddFontResourceEx(FR_PRIVATE)`, avant que
  le thème crée les siennes), arrondit les coins (DWM, Windows 11), fait marcher « réduire » (`LavMinimize`), la
  case à cocher (`LavShortcutOn` / `LavShortcutOff`, deux boutons image échangés, variable
  `AddDesktopShortcutCheckbox`), retire le cadre 3D du champ du dossier, et remplace le ClearType des libellés
  par un lissage gris (franges colorées sur le texte clair, absentes de la maquette).

`Theme="hyperlinkLicense"` ne sert que de plomberie (`ThemeFile` / `LocalizationFile` remplacent fenêtre et
textes) ; le thème n'a pas de case « J'accepte », donc pas de licence à accepter.

> ⚠️ **`Theme="none"` ne démarre pas** : WiX n'écrit alors pas `WixStdbaInformation`, que WixStdBA exige,
> et le bundle quitte sans fenêtre (`0x80070490` dans le journal Burn).

> ⚠️ **Le moteur de thème (thmutil) a ses règles**, que le générateur respecte — les connaître avant de
> retoucher la mise en page :
> - Fenêtre `WS_POPUP` (pas de barre Windows, déplacée en attrapant le fond) **sans `WS_SYSMENU`** : avec lui,
>   `DpiuAdjustWindowRect` compte une barre de titre et la zone client dépasse le fond (bandes noires).
> - Le fond est `Theme/@ImageFile`, sans autre résolution : il est agrandi au DPI de l'écran. Il doit faire
>   **un pixel de plus** que la fenêtre en largeur et en hauteur (thmutil refuse `SourceX + Width` égal à la
>   largeur de l'image : thème rejeté, aucune fenêtre).
> - Les libellés n'ont pas de couleur de fond : leur texte est dessiné sur le fond, et **ne doit pas changer
>   pendant qu'une page est affichée** (rien ne repeint le fond derrière : texte fantôme). D'où l'absence du
>   texte « paquet en cours » de la progression.
> - Boutons, logo et images sont des fenêtres enfants : chaque image **contient le morceau de fond** sur
>   lequel elle est posée (rendue à 200 %), sinon ses coins transparents montreraient n'importe quoi.
> - **Un contrôle nommé sans texte reçoit `#(loc.<Nom>)`** s'il existe, et thmutil le dessine par-dessus son
>   image : aucune chaîne de `thm.wxl` ne doit porter le nom d'un bouton (un « Valider » blanc s'affichait sur
>   le bouton `OptionsOkButton`).
> - Le premier contrôle d'une page qui prend le focus le reçoit à l'ouverture : le bouton principal vient en
>   premier sur chaque page.
> - Une barre de progression est liée **par son nom**, un seul contrôle par nom : la page interactive a
>   `OverallCalculatedProgressbar`, la page passive `OverallProgressbar`. Ses bouts arrondis sont deux petites
>   images de part et d'autre.
> - Couleurs des polices en **BGR** (`F5E0CF` = `#CFE0F5`) ; chaque police du thème est sa propre famille, à
>   la graisse 400 (GDI choisit par famille et graisse).

> 📋 Fermer avant la fin demande « Fermer l'installeur ? » (comportement de WixStdBA sur toutes les pages
> avant l'installation ; le texte est neutre pour la page « déjà installée »).

## 🚫 Pas de stream vers son propre PC

`NvComputer::isThisPc` (calculé à chaque `serverinfo` par `isThisMachine()`) : l'adresse du PC est une
adresse de bouclage ou une adresse de cette machine (`QNetworkInterface::allAddresses()`), ou la MAC qu'il
annonce est celle d'une de ses cartes réseau. Lancer ou reprendre une session vers ce PC est refusé avec
« Impossible de streamer %1 depuis lui-même : LavApollo tourne sur ce PC. Lance le stream depuis un autre
appareil. » : à l'accueil (`HomeView.launch`), au benchmark de Mes PC (`HostsView`), et dans
`Session::validateLaunch`, par où passent aussi les liens `art://` et la ligne de commande. L'appairage reste
possible. Rôle `thisPc` de `ComputerModel` (en dernier : `HostsView` lit les rôles par numéro). Propre au
desktop : un appareil Android n'est jamais son propre hôte.

> ⚠️ **Build de test (`CONFIG+=lav_test`, voir « Build de test »)** : tout hôte de test tourne sur 127.0.0.1, donc tous
> seraient « ce PC ». `isThisPc` y vaut `testHostThisPc` (balise `LavTestHostThisPc` du `serverinfo`), jamais
> `isThisMachine()`.

## 🧪 Build de test (`CONFIG+=lav_test`)

Le client que pilote le banc de test (`tools/testbench/` de LavArtemis) : mêmes sources, compilées avec
`LAV_TEST_BUILD` (+ `LAV_DEV_CHANNEL` : interface v2). **Rien de ce qui suit n'existe dans un autre build** : tout est
sous `#ifdef LAV_TEST_BUILD` ou dans le bloc `lav_test` d'`app/app.pro` ; une release ne change pas (vérifié à chaque
étape en compilant les `.obj` touchés dans une config normale). Jamais construit par `local-release.ps1` ni la CI.

### Construire et lancer

```bat
set LAV_TEST_JOBS=2             :: jobs de jom (défaut 2 : le PC sert pendant ce temps)
scripts\build-test.bat          :: vcvars64 + Qt 6.11.1 msvc2022_64, qmake CONFIG+=lav_test, jom, windeployqt
```
Dossier à part, `build\test-x64` (jamais `build-x64-release` / `deploy-x64-release`) ; exe
`build\test-x64\deploy\LavArtemisTest.exe` (icône Dev, titre « LavArtemis Test », sous-système GUI). Tests de la
logique : `tests/testguard` (suite opt-in, `qmake tests/tests.pro CONFIG+=tests`, ou son `.pro` seul) : liste
blanche, protocole du canal, et `uiprobe` sur des scènes QML offscreen (rendu logiciel, aucune fenêtre à l'écran).

> ⚠️ **Pas de `QT += testlib` dans `lav_test`** : il ajoute `CONFIG += console`, et un exe en sous-système console
> ouvre une console (un onglet Windows Terminal) qui **prend le premier plan** au lancement depuis un programme
> graphique. Le canal n'utilise pas QTest (voir plus bas) ; seule la suite `tests/testguard` le lie.

### Isolation

- Organisation Qt **« LavArtemis Test »** (`main.cpp`) : registre `HKCU\Software\LavArtemis Test`, dossiers
  `%LOCALAPPDATA%` / `%APPDATA%\LavArtemis Test`. `ChannelMigration` ne tourne pas (rien n'est importé d'une autre appli).
- Pas de découverte mDNS (`ComputerManager::startPolling`) : les PC s'ajoutent par adresse.
- Pas de mise à jour in-app (`AutoUpdateChecker` émet `noUpdateAvailable` sans requête), ni `CompatFetcher` /
  `MappingFetcher` (données embarquées ou en cache, comme quand le téléchargement échoue).
- Tout hôte de test est sur 127.0.0.1 : `isThisPc` vaut `testHostThisPc` (balise `LavTestHostThisPc`), voir « Pas de
  stream vers son propre PC ».

### Garde-fous réseau

`app/backend/testguard.{h,cpp}` : liste blanche = **bouclage (`127.0.0.1`, `::1`, `localhost`) et port 40000-44999**.
Le vrai LavApollo de ce PC écoute sur 45989, les vrais hôtes sont sur le LAN : le client de test ne doit jamais leur
écrire. Contrôle **avant toute requête** dans `NvHTTP::openConnection()` (le seul départ de tout GET : serverinfo,
applist, appairage, launch/resume/cancel, jaquettes, sonde PyroWave, presse-papiers lu) et dans
`sendClipboardContent()` (le seul POST) ; un refus lève une `QtNetworkReplyException` (hôte injoignable, aucune E/S) et
écrit une ligne dans le journal (une fois par hôte:port). Redirections désactivées. Deuxième couche : `getServerInfo()`
traite un serveur sans `<LavTestHost>1</LavTestHost>` comme hors ligne (« Pas un hôte de test »). L'ajout manuel d'une
adresse refusée affiche un message (`ComputerManager::testGuardRefusal`, appelé par `HostsView.addByAddress`). **Tout
nouveau chemin réseau doit passer par `NvHTTP`**, sinon il contourne la liste blanche.

Chemins réseau **hors NvHTTP** et pourquoi ils sont sûrs :
- Wake-on-LAN (`NvComputer::wake()`, paquets vers LAN / WAN / diffusion) : **désactivé**, une ligne de journal.
- Sonde `LiTestClientConnectivity` (`PendingAddTask`, `Session::clStageFailed` / `clConnectionTerminated`) et STUN
  `LiFindExternalAddressIP4` : **désactivés** (résultat « non concluant »).
- Sonde de joignabilité (`NvComputer::getActiveAddressReachability`, `QTcpSocket`) : vers `activeAddress`, posée
  seulement après un `serverinfo` qui a passé la liste blanche.
- Le stream (moonlight-common-c) : `RemoteAddr` = `activeAddress`, il ne démarre qu'après un launch / resume gardé.
- `NetworkBuffers::routedWiredLinkMbps` (`QUdpSocket`) : simple recherche de route, aucun paquet.
- Mise à jour, `CompatFetcher`, `MappingFetcher` : désactivés (ci-dessus) ; c'était le dernier trafic non bouclé
  (fichiers de moonlight-stream.org au démarrage, qui rendaient les runs non déterministes).

### Mode discret

Le client tourne sur le PC de l'utilisateur pendant qu'il travaille : fenêtres visibles mais **jamais le focus,
jamais son pointeur, jamais de son, jamais son vrai micro**.
- **Fenêtre du stream** (`session.cpp`) : toujours fenêtrée (`m_IsFullScreen = false`, `m_FullScreenFlag = 0`,
  `toggleFullscreen()` sans effet), ni multi-écran (fenêtre bordless always-on-top), ni état maximisé / minimisé
  hérité du GUI ; titre « … - Test ». SDL2 ici = **sdl2-compat 2.32.70 sur SDL3**, qui ignore
  `SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN` : on pose les hints SDL3 par leur nom, `SDL_WINDOW_ACTIVATE_WHEN_SHOWN=0` et
  `SDL_WINDOW_ACTIVATE_WHEN_RAISED=0`, avant `SDL_CreateWindow`. La fenêtre est créée `SDL_WINDOW_HIDDEN`, reçoit
  `WS_EX_NOACTIVATE`, puis `ShowWindow(SW_SHOWNOACTIVATE)` (`lavTestShowNoActivate()`) ; `WS_EX_NOACTIVATE` est reposé
  (`lavTestMakeNonActivating()`) sur `SDL_WINDOWEVENT_SHOWN` / `RESTORED` et après chaque (re)création du décodeur (le
  renderer peut recréer le HWND, `SDL_RecreateWindow`). Aucun `SDL_RaiseWindow` dans le code.
- **GUI** : `main.qml` reçoit le contexte `lavTestBuild` : `flags` = `Qt.Window | Qt.WindowDoesNotAcceptFocus` (Qt :
  `SW_SHOWNOACTIVATE` + `WS_EX_NOACTIVATE`) et un simple `show()` (jamais maximisé / plein écran) ; titre « LavArtemis
  Test ». Le menu rapide perd `raise()` / `requestActivate()` et prend `WindowDoesNotAcceptFocus`. Aucun autre
  `requestActivate`, `raise`, `alert`, `SetForegroundWindow` dans le code.
  - ⚠️ Sans focus OS, **Qt Quick ne donne jamais de focus actif** (`forceActiveFocus()` n'y fait rien) : le canal livre
    lui-même les touches (voir `key`). Le menu rapide se ferme au clic.
  - ⚠️ Les `FileDialog` natifs (export `.art` de `HomeView`, `BenchmarkView`) **s'activent** : aucun scénario ne doit les
    ouvrir (le canal n'en ouvre aucun de lui-même ; un `click` qui en ouvrirait un, si).
- **Pointeur / clavier** : capture **logique seulement** : `isCaptureActive()` vaut toujours vrai (les événements
  injectés passent), `setCaptureActive()`, `updateKeyboardGrabState()`, `updatePointerRegionLock()` et
  `SDL_CaptureMouse` ne font rien côté OS (pas de mode relatif, de verrou, de curseur masqué, de warp).
- **Seule l'entrée injectée atteint l'hôte** (contrat dans `streaming/input/testinput.h`) :
  - souris (mouvement, boutons, molette) seulement si `which == kTestInjectedMouseId` (`0x4C415654`, « LAVT ») ; le
    mouvement lit les boutons tenus dans l'événement (`state`), la molette sa position (`mouseX/Y`), jamais la vraie
    souris ;
  - **tactile non injectable**, donc tout toucher est ignoré (`SDL_GetTouchDeviceType` est INVALID pour un id jamais
    enregistré) ; pas de commande tactile ;
  - manettes : seulement un appareil nommé `kTestGamepadName` (« LavArtemis Test Pad », la manette virtuelle du canal,
    reconnue par son nom, sans VID/PID connu) ; les manettes physiques ne sont ni ouvertes ni comptées
    (`handleControllerDeviceEvent`, `handleJoystickArrivalEvent`, `getAttachedGamepadMask`) ;
    `ALLOW_BACKGROUND_EVENTS=1` ;
  - clavier sans marqueur : aucune fenêtre n'a le focus, donc aucune vraie touche n'arrive ; `handleKeyEvent` ne
    dépend pas du focus.
- **Son** : `SDL_AUDIODRIVER=dummy` (`qputenv` **et** hint, en tête de `main()`, avant tout `SDL_Init*`).
- **Micro** (`micstreamer.cpp`) : aucun périphérique ouvert ni énuméré ; un thread produit un sinus 440 Hz (48 kHz mono,
  trames de 20 ms) qui passe par le même encodeur Opus et `LiSendMicrophoneData` à 50 trames/s (le compteur de trames
  de l'hôte voit du trafic) ; joint dans `stop()`, avant `LiStopConnection()`.

### Canal de commande

`app/testctl/`, démarré par `main.cpp` après `main.qml` : `QLocalServer` **`lavartemis-test`**
(`\\.\pipe\lavartemis-test`, `UserAccessOption` : cet utilisateur seulement). **Un client à la fois** : un second reçoit
`{"id":null,"ok":false,"error":"another client is already connected"}` et est fermé ; se reconnecter après une
déconnexion marche. Une requête JSON par ligne `{"id":n,"cmd":"…",…}` → une ligne `{"id":n,"ok":true,…}` ou
`{"id":n,"ok":false,"error":"…"}` (le résultat est fusionné dans la réponse ; `id` est renvoyé tel quel). Ligne
illisible ou sans `cmd` texte → erreur (`id` null si ce n'est pas un objet JSON), connexion gardée. Lignes découpées par
`LineBuffer` : coupées après espaces (`\r\n` accepté), vides ignorées ; une ligne de **plus de 1 Mio** (fin de ligne
non comprise), qu'elle arrive d'un bloc ou en morceaux, donne **une** erreur `"line too long"` (`id` null) et le reste de
la ligne est jeté jusqu'à son `\n`. Fichiers : `testprotocol` (Qt Core : requêtes, `Dispatcher`, `LineBuffer`, noms de
touches, garde de `reset`), `uiprobe` (Qt Quick : arbre, clic, touches, recouvrement), `testcontrol` (serveur, SDL,
session, réglages).

- **Tout sur le thread GUI**, qui fait aussi tourner la boucle SDL du stream : Qt y livre toujours timers, événements
  postés et données du pipe, car SDL pompe les messages Windows (mesuré avec sdl2-compat : QTimer 50 ms → 46 ticks,
  20/20 lignes reçues pendant 3 s de `SDL_WaitEventTimeout`). Le « Qt timers do not fire while streaming » de
  `session.h` (benchmark) ne vaut donc pas ici. Pendant un stream, les commandes s'exécutent dans la pompe de SDL.
- ⚠️ **Aucune boucle d'événements imbriquée, aucune commande dans une autre.** `readyRead` ne fait que découper les
  lignes dans une file ; chaque commande part d'un appel en file (`runNext`), une par tour de boucle, et
  `Dispatcher::isRunning()` refuse d'en démarrer une pendant qu'une autre tourne (un `runNext` livré par une boucle
  imbriquée attend). Le canal n'appelle jamais `processEvents()` : événements Qt par `QCoreApplication::sendEvent`,
  **pas QTest** (ses fonctions finissent par `processEvents()` ; `Session::exec()`, la boucle SDL bloquante, démarre sur
  un signal en file : un clic pendant la connexion aurait fait tourner tout le stream dans la commande, canal figé). La
  réponse va au client qui a posé la question s'il est encore connecté ; les requêtes d'un client parti sont jetées.
- **Horodatage** des événements synthétiques : `UiProbe::inputTimestamp()` = `GetTickCount()`, l'horloge de
  `GetMessageTime()` que le plugin Windows de Qt met sur les vraies entrées : jamais en arrière par rapport au survol
  réel de l'utilisateur. Appui et relâché d'un clic portent le même horodatage (un tap, pas un appui long).
- **Coordonnées** Qt = pixels **logiques** de la fenêtre (`tree`, `click {x,y}`, `wheel`) ; une capture est en pixels
  physiques (× `dpr`). Stream = coordonnées SDL (`w`/`h` du stream dans `windows`). Rien de réel : le vrai curseur ne
  bouge pas, aucune fenêtre ne s'active (vérifié : premier plan inchangé, curseur jamais amené au point cliqué).
- `window` = indice d'une fenêtre `kind:"qt"` de `windows` (0 = principale). Cible de `key`/`text`/`wheel` (`target`) :
  `"auto"` = le stream si sa boucle SDL tourne et que le menu rapide est fermé, sinon Qt ; `"qt"` ; `"stream"`. Fenêtre
  Qt par défaut : le menu rapide visible (`Qt::Tool`), sinon la principale.

| Commande | Arguments | Réponse |
|---|---|---|
| `windows` | — | `windows:[{index, kind:"qt", role:"main"/"quickmenu"/"toast"/"other", title, hwnd, x, y, w, h, dpr}…, {kind:"stream", title, hwnd, x, y, w, h}]` (stream : pendant sa boucle SDL) |
| `tree` | `textOnly?`, `window?` | `items:[{window, type, objectName, text, x, y, w, h, focus, enabled}]` |
| `click` | `text` / `objectName` (+ `contains?`, `index?`) ou `x`,`y` (+ `window?`) ; `button?` (left, right, middle, x1, x2) | `{window, x, y, item?}` |
| `key` | `key`, `modifiers?`, `state?` ("click", "down", "up"), `target?`, `window?` | `{target, focus?}` |
| `text` | `s`, `target?`, `window?` | `{target}` |
| `wheel` | `dx?`, `dy?` (crans, `dy > 0` = vers le haut) ; Qt : `text`/`objectName` ou `x`,`y` (défaut : centre) ; stream : `x`,`y` | `{target}` |
| `mouse` (stream) | `{action:"move", x, y}` ; `{action:"button", button?, down}` ; `{action:"click", button?, x?, y?}` | `{x, y, buttons, windowW, windowH}` |
| `gamepad` (stream) | `{attach:true}` d'abord ; puis `{button, value}` (bool ou 0/1), `{axis, value}` ; `{detach:true}` | `{pad, instance, attached?, …}` |
| `screenshot` | `path` (absolu, disque local, PNG), `window?` (0) | `{path, width, height, dpr}` |
| `state` | — | `{page, pageTitle, pageBusy, quickMenu, pad, computers:[{name, uuid, address, online, state, paired, testHost, thisPc, currentGameId, apps:[noms]}], session:{active, streaming, frames, received, decoded, rendered, dropped}}` |
| `setting` | `action:"get"/"set"/"reset"/"list"`, `key`, `value` | `{key, source:"streaming"/"artemis", value}` ; `list` → `{streaming:{…}, artemis:{…}}` |
| `reset` | — | `{folders, registry, notYetRemoved, quitting:true}`, puis le client quitte |
| `quit` | — | `{quitting:true, session}`, puis le client quitte |
| `commands` | — | `commands:[…]` |

- **`tree`** : visibles seulement (`isVisible`, opacité effective > 0, partie visible non vide, coupée par la fenêtre et
  les ancêtres `clip`) ; `x,y,w,h` = cette partie visible ; `text` = la propriété `text` ; `type` = type QML (`Text`,
  `MouseArea`, `LavPill`, `SettingChoice`…) ; `focus` = sur la chaîne de focus (l'élément qui prend les touches,
  `keyTarget()`, et ses ancêtres qui ont le focus), pas le focus actif, que personne n'a dans ces fenêtres.
- **`click`** : `text` exact (casse comprise), `contains:true` = sous-chaîne, `objectName` exact ; `index` (défaut 0,
  négatif = depuis la fin) parmi les correspondances dans l'ordre de l'arbre (profondeur d'abord, fenêtres dans l'ordre
  de `windows`) ; clic au centre de la partie visible. **Refusé** (`"covered: a … takes the click at its centre"`) si
  l'élément le plus haut qui prend ce bouton en ce point (ordre de peinture : `z` puis ordre des enfants ; `clip`
  respecté) n'est ni l'élément, ni un de ses ancêtres ou descendants (`UiProbe::coveringItem`). Prend un clic : un
  élément avec un *pointer handler* autre que Hover / Wheel / Pinch, actif et acceptant le bouton (les cartes à
  `TapHandler` : `AppCard`, `CoverTile`, Mes PC, `SettingRow`), une `MouseArea`, un bouton, un champ de texte, un
  `Flickable`, une popup ; sans handler, tout élément actif qui accepte le bouton.
  - ⚠️ Ajouter un handler, **même un `HoverHandler`**, fait accepter tous les boutons à l'élément
    (`acceptedMouseButtons`) : c'est le type de ses handlers (`children()` qui héritent de `QQuickPointerHandler`) qui
    décide, sinon un survol décoratif passerait pour un obstacle.
  - L'overlay de Qt Quick Controls (popups) ne recouvre la page que **tant qu'une popup modale est ouverte** (erreur
    `"covered: a modal popup is open (the overlay takes the click)"`) ; le contenu de la popup reste cliquable. Sans popup
    modale, seules les popups elles-mêmes (menu, infobulle) peuvent être en travers.
  - `click {x,y}` et `wheel {x,y}` : refusés hors de la fenêtre.
- ⚠️ **Transitions de `StackView`** : la page qui arrive a une opacité nulle et l'ancienne est encore à l'écran ; attendre
  `state.pageBusy == false` avant `tree`, `click` ou `screenshot` (`page` change tout de suite). `page` = type de la page
  du haut (`HomeView`, `SettingsView2`…), `pageTitle` = son `objectName`.
- **Touches** (noms insensibles à la casse) : `A`…`Z`, `0`…`9`, `F1`…`F12`, `Enter` (`Return`), `Escape` (`Esc`),
  `Tab`, `Backspace`, `Space`, `Delete`, `Insert`, `Home`, `End`, `PageUp`, `PageDown`, `Left`, `Right`, `Up`, `Down`,
  `Minus`, `Equals`, `LeftBracket`, `RightBracket`, `Backslash`, `Semicolon`, `Apostrophe`, `Grave` (à gauche de 1 : ²
  en AZERTY), `Comma`, `Period`, `Slash`, `CapsLock`, `PrintScreen`, `ScrollLock`, `Pause`, `Menu`, `Ctrl` (`LCtrl`),
  `RCtrl`, `Shift` (`LShift`), `RShift`, `Alt` (`LAlt`), `RAlt`, `Meta` (`LMeta`, `Win`), `RMeta`. `modifiers` :
  `"Ctrl+Shift"` ou `["Ctrl","Shift"]` (Ctrl, Shift, Alt, Meta/Win).
  - Qt : l'événement va à `keyTarget()` = élément actif, sinon bout de la chaîne de focus depuis la racine, puis à ses
    parents jusqu'à acceptation (ce que fait Qt Quick) ; `focus` de la réponse le nomme. `text` Qt accepte tout Unicode.
  - Stream : comme un vrai clavier (modificateurs enfoncés, touche, relâchés), scancode et keycode **US fixes**, jamais la
    disposition de ce PC. `text` = une touche par caractère sur clavier US (Maj pour majuscules et symboles) ; un
    caractère sans touche (accent) est refusé et rien n'est envoyé. ⚠️ C'est **la touche US** qui part et l'hôte la lit
    avec **sa** disposition : sur un hôte en AZERTY, `text {"s":"a"}` tape « q » et `"1"` tape « & », comme un clavier
    QWERTY branché sur ce PC. Pour un texte exact, l'hôte de test doit être en disposition US.
  - ⚠️ **Jamais de `SDL_TEXTINPUT` poussé** : sdl2-compat plante dessus (vérifié), et le client n'envoie que des touches.
- **Souris du stream** : `which = kTestInjectedMouseId`, `state` = boutons tenus, position suivie par le canal (au centre
  de chaque nouvelle fenêtre de stream). Molette : `preciseX/Y` (sdl2-compat rend un `y` entier faux pour une molette
  poussée, `y = mouseY` observé ; le client lit `preciseY`), `mouseX/Y` = dernière position.
- **Manette** : joystick virtuel SDL « LavArtemis Test Pad » (`SDL_JoystickAttachVirtualEx`, type GAMECONTROLLER, 6 axes,
  15 boutons, aucun VID/PID) : SDL en fait une manette avec un mapping automatique (`a:b0` … `dpright:b14`, `leftx:a0` …
  `righttrigger:a5`), nom intact (vérifié). Pendant un stream seulement ; **`{attach:true}` d'abord** (un `button`/`axis`
  sans manette est refusé, il ne la crée pas), puis laisser le client l'ouvrir (~0,5 s) : un bouton pressé avant serait
  perdu. Boutons `a b x y back guide start leftstick rightstick leftshoulder rightshoulder dpup dpdown dpleft dpright` ;
  axes `leftx lefty rightx righty` (-32768…32767), `lefttrigger righttrigger` (0…32767). Le canal tient sa propre
  référence au sous-système joystick (la session le quitte avant lui, ce qui détacherait la manette sous nos pieds) ;
  manette retirée à la fin du stream (contrôle toutes les 500 ms), par `detach` et à `quit`. Les deux
  `SDL_assert(!SDL_WasInit(SDL_INIT_JOYSTICK))` d'`input.cpp` ne sont donc compilés que hors du build de test.
- **`state.session`** : `Session::benchmarkSnapshot()` (cumul du stream, publié à chaque fenêtre de stats ≈ 1 s, 0
  avant) : `frames` envoyées par l'hôte, `decoded`, `rendered`, `dropped` = perdues réseau + jetées par le pacer ;
  `active` = une session existe (connexion comprise), `streaming` = sa boucle SDL tourne
  (`Session::testStreamWindow()`).
- **`setting`** : clés = noms des `Q_PROPERTY` de `StreamingPreferences` puis d'`ArtemisSettings` (`fps`,
  `bitrateKbps`, `videoCodecConfig`, `uiExpertMode`…) ; énumérations en nombre (nom accepté en `set`). `set`/`reset`
  enregistrent comme Réglages : `StreamingPreferences::save()` **et** `ArtemisSettings::save()`. `reset` = valeur d'une
  installation neuve, lue sur un objet qui ne lit rien (`ProfileManager::setReadDefaultsForTest`,
  `ArtemisSettings::createDefaultsForTest`) ; le débit par défaut est celui de la résolution par défaut. La langue ne
  change qu'au prochain démarrage.
- **`reset`** : refusé si l'organisation n'est pas **exactement** « LavArtemis Test » ; supprime
  `HKCU\Software\LavArtemis Test` et les dossiers d'organisation de ce nom au-dessus de `AppConfigLocation`,
  `AppDataLocation`, `AppLocalDataLocation`, `CacheLocation` (nom revérifié avant chaque suppression), tout de suite
  **et** à la sortie (`atexit` : quitter réenregistre des réglages), puis quitte : relancer le client pour repartir de
  zéro. Vérifié : `LavArtemis` et `LavArtemis Dev` intacts. ⚠️ Suppression par `removeTreeNoFollow` : un dossier racine
  qui est un lien (jonction, lien symbolique) est refusé, et un lien à l'intérieur est supprimé comme lien, sans
  descendre dans sa cible. Jamais `QDir::removeRecursively()`, qui suit les jonctions.
- **`quit`** : répond, interrompt la session s'il y en a une, attend sa fin, retire la manette, quitte. ⚠️ Au bout de
  30 s, il **cesse d'attendre** et quitte quand même, session encore en cours de nettoyage.
- **`screenshot`** : fenêtres Qt seulement (`grabWindow`), chemin absolu sur un disque local : `\\serveur`, `//serveur`,
  `\\?\` et les lecteurs réseau mappés (`GetDriveTypeW == DRIVE_REMOTE`) refusés. La fenêtre du stream se capture par
  `PrintWindow` sur son `hwnd`.

**Limites connues** :
- `NvHTTP` attend ses réponses dans un `QEventLoop(ExcludeUserInputEvents)` sur le thread GUI (appairage OTP, commandes du
  serveur, presse-papiers…). Lancé par l'appli hors de toute commande, cette boucle peut exécuter des commandes du canal en
  son sein, et un stream pourrait même démarrer dans une telle commande. Peu probable, et c'est une propriété de l'appli,
  pas du canal (dans une commande, la file attend : voir plus haut).
- `text` vers le stream = touches US lues avec la disposition de l'hôte (ci-dessus) ; `quit` n'attend la session que
  30 s ; pas de toucher ; un `click` peut ouvrir un `FileDialog` qui, lui, s'active.

## 🎨 Interface v2 (LavArtemis Dev)

La refonte (spec : `docs/superpowers/specs/2026-11-23-ui-redesign-design.md` dans LavArtemis,
prototype de référence dans `docs/ui-redesign/prototype/`) vit dans **`app/gui/v2/`**. L'appli
s'ouvre toujours sur l'accueil v2 (`main.cpp`, `initialView`) : le retour à l'ancienne interface
(`uiLegacy`) a été retiré, puis **l'ancienne interface elle-même** (`PcView`, `AppView`,
`SettingsView`, la barre d'outils et leurs briques) — elle n'existe plus que sur `main` (stable). Restent hors de `v2/` : `main.qml`
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
- **Survol et focus des cartes et jaquettes** (`CoverTile`, `AppCard`, cartes de Mes PC), comme le prototype :
  à la souris (`mouseHover`), la carte monte de 5 px avec une ombre douce (`HoverDecor.qml`, sous le fond
  comme `LiveHalo`) et un fin liseré (`Theme.hoverRing` / `hoverRingWidth`, prototype `ringMouse`) ; à la
  manette ou au clavier (`padFocus`), elle grossit à `Theme.focusScale` (1,04) avec l'anneau de focus. Les
  deux comptent comme le focus de l'assombrissement : `Theme.dim(mouseHover || padFocus)`, la couleur
  revient. ⚠️ Avant le 2026-11-24, une carte assombrie ne réagissait pas du tout à la souris.
- **`HostsView.qml` (Mes PC, prototype « hosts »)** : une carte par PC appairé (état, adresse, version
  LavApollo, ce qui tourne), puis « Ajouter un PC » (PC trouvés sur le réseau ou par adresse, puis
  appairage par code PIN). Menu : Ouvrir, Benchmark, Réveiller, Renommer, Oublier. ⚠️ Les rôles de
  `ComputerModel` sont lus par numéro (`roleId()`, ordre de l'enum depuis `Qt::UserRole`) : un rôle
  ajouté ou retiré dans `computermodel.h` doit l'être dans cette liste. Et **jamais de propriété
  `action`** sur un `AbstractButton` (elle existe déjà : le composant refuse de se charger).
- Les jeux **masqués** (« Masquer ») se réaffichent depuis l'accueil (bouton sous la bibliothèque) et se
  démasquent depuis leur menu. **« Masquer » ne s'offre que pour les jeux Playnite**, comme sur Android
  (`openMenu(…, isGame)` → `AppMenu.canHide`) ; une carte Bureau / Playnite masquée par une version
  antérieure garde « Ne plus masquer ».
- **États de l'accueil, ceux d'Android** (`HomeScreen.bind`, mêmes textes) : « Aucun PC appairé », « Connexion
  à %1… » (hôte pas encore joint, `statusUnknown`), « Chargement des applis… » (en ligne, liste d'applis pas
  encore arrivée, `appsLoading`), « %1 est hors ligne » avec **« Réveiller »** quand le PC a donné son adresse
  MAC (`wakeable`, mêmes messages que Mes PC). En-tête : « Recherche… » tant que l'hôte n'a pas répondu.
  Bibliothèque vide : « Aucun jeu Playnite sur ce PC. Installe l'extension LavApollo Link… ».
- **Ligne d'état de la Scène, celle d'Android** : « EN COURS · DANS PLAYNITE FULLSCREEN · DEPUIS 12 MIN »
  (`home.liveLine`). L'hôte ne dit pas quand la session a commencé : la durée part de la première fois que ce
  PC a vu l'appli tourner sur cet hôte, gardée dans `QSettings` (`uiV2SessionClock/<uuid>` =
  `<runningId>:<ms>`, comme `SessionClock` d'Android), oubliée quand rien ne tourne ; rien sous une minute.
- **Logiciel hôte non pris en charge** : la règle d'Android (`NvComputer::isSupportedAppVersion()` : appversion
  en quatre nombres, génération ≥ 3 ; vide = inconnu = accepté) s'ajoute à la liste GeForce Experience de
  moonlight-qt. Vérifié avant d'ouvrir (Mes PC), de lancer (accueil) et de lancer un benchmark, avec le
  message d'Android et la version (`ComputerModel::ServerVersionRole`, rôle `serverVersion`, ajouté **à la fin**
  de l'enum et de la liste `roleId()` de `HostsView`) ; `Session::validateLaunch()` garde le même texte.
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
  Windows, « Écran tactile » sans écran tactile — `SystemProperties.hasTouchScreen`). Un réglage qui agit encore **reste
  visible tant qu'il est actif** (le mode Trackpad) : on ne retire pas un réglage allumé de sous les
  yeux de l'utilisateur.
- **« Écran tactile » : Multitouch / Trackpad**, en mode normal, en tête de la page Entrées, **mêmes libellés et
  même description que l'Android** (qui ne garde plus que ces deux modes en Dev). Multitouch =
  `absoluteTouchMode` vrai (`abstouch.cpp` : toucher natif si l'hôte le prend en charge, sinon souris
  émulée) ; Trackpad = faux (`reltouch.cpp` : le doigt déplace le curseur, tap = clic gauche, appui à deux
  doigts = clic droit, appui long 650 ms = clic gauche maintenu — **un doigt seulement** : l'appui long à
  deux doigts ne fait rien, comme sur Android (décision de l'utilisateur, 2026-11-24 ; il tenait le clic
  droit) — et **deux doigts
  qui bougent = défilement**, porté de `RelativeTouchContext` d'Android : le doigt principal, une fois sorti
  de la zone de tap, fait défiler de 5 unités par pixel d'un écran de référence de 720 lignes, et ne bouge
  plus le curseur tant que deux doigts sont posés). Le menu en jeu a « Passer en trackpad / multitouch »
  (`SdlInputHandler::setAbsoluteTouchMode()`, pour ce stream ; avec **« Retenir le mode tactile choisi en
  jeu »** — `StreamingPreferences::rememberTouchMode`, ligne expert d'Entrées, mêmes textes qu'Android — le
  menu écrit aussi `absoluteTouchMode` et sauvegarde, comme `Game.switchTouchMode()`). Seul écart restant
  avec Android, **voulu** : pas de zoom au pincement sur PC (décision de l'utilisateur).
- **Parité avec Android** (règle de l'utilisateur : tout ce que les deux clients ont en commun a le même
  libellé, la même description, les mêmes options, le même défaut, la même place et le même ordre ; seules
  les features propres à une plateforme diffèrent). État et écarts restants :
  `docs/superpowers/specs/2026-11-24-pc-android-parity.md` du dépôt LavArtemis. Un changement d'un texte ou
  d'un menu commun se fait des deux côtés.
- **Menu en jeu (`QuickMenu.qml`)** : construit à chaque ouverture (`rebuildMainMenu()`), entrées communes
  **dans l'ordre et avec les libellés d'Android** (Quitter <jeu Playnite>, Déconnecter, Quitter la session
  — confirmée par une sous-page « Tu veux vraiment quitter ? », textes d'Android —, Commandes du PC hôte,
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
- **Débit vidéo, celui d'Android** (`SettingsCatalog` / `seekbar_bitrate_kbps` côté Android) : curseur de
  0,5 à 800 Mbit/s, **3 Gbit/s quand le codec est PyroWave** (`bitrateMax` ; Android :
  `SettingsCatalog.BITRATE_MAX_KBPS` / `BITRATE_MAX_PYROWAVE_KBPS`, `SettingDef.maxFrom`), piste cubique (`SettingSlider.curve: 3` : position =
  fraction^(1/3), pas de 500 kbit/s, flèches à 1 Mbit/s ×5 puis ×20 comme Android), et « Saisir un débit » (expert)
  pour toute valeur au-delà. « Débloquer la limite de débit » (`unlockBitrate`, 500 Mbit/s) est **retiré** :
  rien ne borne plus la valeur enregistrée, une valeur saisie, calibrée (PyroWave) ou débloquée auparavant
  reste telle quelle, le curseur se pose alors en bout de piste. PyroWave garde son débit par défaut et son
  calibrage (propres au PC). Le benchmark plafonne à 800 Mbit/s comme Android.
- **Mise à jour** : `main.qml` lance le check et expose `window.updateVersion` et `openUpdateDialog()` ;
  l'accueil (pastille « Mise à jour » de l'en-tête) et Réglages › Avancé (« Installer ») les utilisent.
  Pas de dialogue au démarrage (Android en a un : écart voulu).
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
**Tutoiement partout**, traductions comprises (messages de stream, avertissements de lancement, menu en
jeu) : les dernières phrases au vouvoiement héritées de moonlight-qt ont été passées au « tu » le
2026-11-24, comme l'Android Dev (`app/src/dev/res/values-fr/strings_tu.xml`).
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

> 📋 **Expérimental** : l'utilisateur ne s'en sert pas pour l'instant ; il est gardé parce qu'il est
> déjà construit. Ne pas en faire un défaut ni le mettre en avant.

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
> Nonary sont présentés comme « Juste » / « Très juste ». La VRR (ci-dessous) est arrivée après le
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
Réglages › Audio : **« Envoyer mon micro au PC distant »** (désactivé par défaut) et « Micro utilisé », hors profils (`ArtemisSettings`, propres à la machine). Protocole :
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
  « Micro coupé » passe par `Overlay::StatusSource::Microphone`, affiché **en plus** des autres
  messages d'état, jamais masqué par eux.
- ⚠️ **Sous-système audio SDL partagé** avec `SdlAudioRenderer` : ses deux `SDL_assert(!SDL_WasInit(...))`
  ont été retirés, le comptage de références de SDL suffit.
- ⚠️ **Pas d'annulation d'écho** côté SDL : avec des haut-parleurs, le son du jeu repart vers l'hôte.
  Le texte du réglage recommande un casque.

## 📊 Benchmark des réglages

Mes PC, menu d'un PC → **« Benchmark des réglages »** (`gui/BenchmarkView.qml`) : le client streame plusieurs fois
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
> de référence (« Tes réglages ») et sa répétition streament les réglages de l'utilisateur sans y toucher.

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
- Rapports dans `AppDataLocation/benchmarks/`, 50 au plus ; « Ouvrir un fichier de rapport… » ouvre aussi ceux de
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
   `wix/LavArtemisSetup/theme/<canal>/thm.xml` (`IconFile=`, `ImageFile=`) doit correspondre exactement aux
   `Payload Name=` de `Bundle.wxs` et de `theme/<canal>/payloads.wxi` (les deux sont générés ensemble :
   ne jamais en retoucher un seul). Burn résout ces noms **à l'exécution**, dans le dossier
   d'extraction : un nom qui ne correspond pas fait échouer le parsing du thème avec `FILE_NOT_FOUND`
   **avant qu'aucune fenêtre n'existe** — l'installeur semble ne rien faire du tout, sans erreur ni
   SmartScreen. WiX embarque sans broncher un thème dont les références ne résolvent pas : build vert,
   zéro warning. C'est ce qui a cassé la v20.3.0 (renommage `artemis.ico` → `lavartemis.ico`
   non répercuté). Seul le job `build-windows` du workflow (manuel) de LavArtemis ouvre le bundle et
   vérifie chaque asset ; `local-release.ps1` ne le fait pas : si tu bouges ces fichiers, vérifie les
   deux côtés à la main et lance l'installeur avant de publier.
   Diagnostic : le log Burn est dans `%TEMP%\LavArtemis_Game_Streaming_Client_*.log`, et
   `wix burn extract <exe> -oba <dir> -acceptEula wix7` montre le contenu réel.

5. **La version publiée vient de LavArtemis** (`lavVersionName` d'`app/build.gradle`), passée au build par
   `CI_VERSION` : `local-release.ps1` réécrit `app/version.txt` le temps du build puis le remet. La valeur
   committée ici n'est pas celle des releases.

6. **`ArtemisApplication`, `artemis.png`, `artemissettings`** — le rebrand est partiel, ces noms sont
   hérités. Renommer touche le QML, le `.pro` et le WiX ensemble.

## 🤖 CI

Les minutes Actions sont comptées : **tout est manuel**, et le client est buildé en local avant chaque push.

- **`.github/workflows/build.yml`** — compile check (Windows x64/arm64 + Linux), **lancé à la main
  seulement** (`workflow_dispatch`). ~7 min. Ne produit **pas** de release.
- ⚠️ **Le compile-check Linux construit *sans* `APP_IMAGE`**, alors que la release construit l'AppImage
  *avec*. Les branches `#ifdef` compilées ne sont donc pas les mêmes : une erreur qui ne touche qu'un
  chemin laisse la release **verte** pendant que `build.yml` est rouge. C'est ce qui a caché 17 jours
  de compile-check cassé (10 → 26 août 2026) : `findAssetDownloadUrl()` sortait par
  `return QString();` dans son `#else`, mais **`return` n'empêche pas le compilateur d'analyser la
  suite** — la boucle en dessous référençait un `wantedSuffix` jamais déclaré. Un `#else` qui ne fait
  que sortir doit quand même **déclarer** ce dont le code suivant se sert. Et un compile-check rouge
  n'est jamais cosmétique, même quand la release passe.
- Les **releases** sont publiées par [`Lavagnou/LavArtemis`](https://github.com/Lavagnou/LavArtemis), qui
  consomme ce dépôt comme sous-module `desktop/` et publie Android + desktop sous un tag unique. Elles
  sont **construites en local** (`scripts/local-release.ps1` : client Windows **x64 seul**,
  `LAV_X64_ONLY=1`, depuis ce clone, qui doit être sur le commit épinglé). ARM64 et l'AppImage Linux
  n'existent que par le workflow `release.yml` de LavArtemis, manuel lui aussi.
- ⚠️ **`build-appimage.yml` est orphelin** (`workflow_call` que personne n'appelle) : c'est la
  référence dont le job `build-linux` de LavArtemis est dérivé. **Toute correction doit aller dans
  les deux.**

### Faire remonter un changement jusqu'à la release

Commiter ici, puis avancer le pointeur `desktop/` dans LavArtemis (sa branche `dev` → ce `dev`) par un
commit là-bas, ce que fait chaque release Dev. Le workflow **Update desktop pointer** de LavArtemis ne
sert plus en l'état : il cherche le dernier run **vert déclenché par un push** de `build.yml` sur `main`,
et `build.yml` ne tourne plus sur push.

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
