# Strong Pickle (Studio Wiga) — VST3

Fuzz au format pédale simple (référence de format : Echoflex / Doorn), reproduction de la
**Way Huge Swollen Pickle Jumbo Fuzz MkIII** (WM41, série « Smalls »), fuzz de la famille Big Muff
connue pour son grave énorme, sa réserve de volume et ses crachotements (« sputter ») à mi-course.
Usage personnel. Windows 11 et macOS (binaire universel). Visuel fourni par Studio Wiga (`design/source/strong_pickle_visuel.png`), intégré tel quel.

## Installation via GitHub

1. Le push sur `main` lance la compilation (onglet **Actions**) : Windows (`windows-2022`) et macOS.
2. Télécharger l'artefact :
   - **Windows** : copier `Strong Pickle.vst3` dans `C:\Program Files\Common Files\VST3\`
   - **macOS** : dézipper puis copier `Strong Pickle.vst3` dans `~/Library/Audio/Plug-Ins/VST3/`
     (premier lancement : `xattr -cr ~/Library/Audio/Plug-Ins/VST3/Strong\ Pickle.vst3` si macOS le bloque)
3. Relancer l'analyse des plugins. Le plugin apparaît comme **Strong Pickle (Studio Wiga)**.

## La pédale d'origine et le plugin

| Élément | Swollen Pickle MkIII | Dans Strong Pickle |
|---|---|---|
| Circuit | Big Muff modifié : étage d'entrée, deux étages d'écrêtage à diodes, tone stack passif, étage de sortie | **v1.1 : modèle composant par composant** (`PickleCircuit.h`), résolu à chaque échantillon (Newton, sur-échantillonnage x4) : transistors 2N3904 (Ebers-Moll), diodes 1N914 en série avec 1 µF, Miller 330 pF, contre-réaction 470 k, liaisons 150 / 100 nF sur 10 k (grave réduit avant l'écrêtage : fuzz définie, pas de bave) |
| SUSTAIN | Quantité de fuzz ; gain total 18 à 62 dB (fiche Way Huge) | Diviseur entre l'étage d'entrée et le 1er étage d'écrêtage : 18 → 62 dB mesurés en petits signaux (`tests/test_dsp.cpp`) ; le niveau suit le jeu, pas de pompage |
| LOUDNESS | Volume de sortie, réserve énorme | Courbe audio, 0 = coupé, midi ≈ +4,5 dB au-dessus du bypass, +12 dB en haut de course (limiteur de sécurité à 0 dBFS) |
| FILTER | Du sombre / gras au brillant / fin | Tone stack passif aux valeurs Big Muff : 39 k / 10 nF (passe-bas), 4 nF / 22 k (passe-haut), pot 100 k |
| SCOOP (inverseur) | Creuse les médiums | Bas = voix classique (creux ≈ −5 dB vers 800 Hz) ; haut = valeurs écartées 22 nF / 2,2 nF, creux ≈ −12 dB |
| CRUNCH (inverseur) | Change l'intensité de la compression | Bas = diodes directes (fuzz dense, compressée) ; haut = 8,2 k en série avec les diodes : fuzz plus ouverte, granuleuse et dynamique, +2 à 3 dB |
| Les deux inverseurs en bas | La Swollen Pickle « trois potards » classique | Preset 1 *Classic Pickle* |
| Grave « jumbo » | Condensateurs de liaison plus gros qu'une Big Muff | Liaisons 150 / 100 nF : grave plein mais serré |
| Footswitch / LED | True bypass, LED bleue | Footswitch du visuel (bascule à l'appui, bouton assombri tant qu'on appuie), **LED jaune** en fondu, la façade s'éclaire autour ; le mot **PICKLE** s'allume en vert quand l'effet est actif (même fondu) |
| Noise gate (ajout) | — | Clic droit > *Noise gate* : Off (défaut, comme la pédale) / Light / Medium / Strong |

Sources : manuel Way Huge WM41 (Dunlop), fiches revendeurs (Reverb, Guitar Center, Musik Store), test Bonedo
(Swollen Pickle MkIII), test du MkII sur basse (forum Seymour Duncan : rôle de Sustain, Filter, Scoop, Crunch),
analyse du Big Muff (Electrosmash), Effects Layouts (Swollen Pickle Mk2).
Note : les textes officiels se contredisent sur le sens du SCOOP (« creuse » ou « garde » les médiums) ; le manuel
et le test Bonedo disent qu'il creuse : c'est ce qui est reproduit.

## Interface

- Fenêtre ouverte à **493 px de haut** (391 × 493), impossible de la réduire en dessous, proportions bloquées, jusqu'à 150 %. Clic droit > *Plugin size* : 60 % / 75 % / 100 %, « (default) » sur la taille par défaut choisie ; *Default size (new windows)* choisit la taille à laquelle s'ouvrent les nouvelles fenêtres du plugin (réglage commun à toutes les instances) et redimensionne aussitôt la fenêtre courante.
- **Barre de presets** sous la ligne d'état : flèches = preset précédent / suivant, clic sur le nom = menu (presets d'usine, presets utilisateur 1 à 9).
- **Ligne d'état** : réglage de la commande que l'on bouge (potards en %, inverseurs), police Poppins ; apparaît quand un réglage bouge, puis s'efface.
- Potards : seule la barrette claire tourne, les reflets du chapeau restent fixes (lumière en haut à gauche). Double-clic = midi.
- Inverseurs : bagues du visuel, levier nickel rendu en 3D avec la lumière du visuel (bas / haut, image intermédiaire ~55 ms pendant la bascule) ; clic = bascule, molette possible.
- Footswitch d'Echoflex (référence Studio Wiga), bouton assombri tant qu'on appuie.
- Jacks sur les tranches, au milieu de la hauteur (OUT à gauche, IN à droite) : embases vues de dessus (rondelle, écrou hexagonal, douille filetée), sans ombre portée puisqu'elles sont sur le côté du boîtier. Pas d'inscription 9 V.
- **Anti-gel v2** : moteur de rendu logiciel forcé sous Windows (réappliqué à chaque nouvelle fenêtre hôte), fenêtre opaque, timer 30 Hz qui ne redessine que ce qui a changé (aucun redessin au repos), caches d'images reconstruits par le timer seulement, chien de garde (1,5 s), jamais de redimensionnement forcé ; côté audio, aucun appel à l'hôte depuis le thread audio (file vidée sur le thread de messages), paramètres lus par pointeurs mis en cache. JUCE 8.0.15.

## MIDI

MIDI Learn sur toutes les commandes (clic droit sur une commande ou menu MIDI), assignations sauvegardées avec le projet.

| Message | Fonction |
|---|---|
| CC 20 / 21 / 22 | Loudness / Sustain / Filter |
| CC 52 / 53 / 54 | Poids faible 14 bits des CC 20 à 22 (si la pédale l'envoie) |
| CC 23 | Crunch (≥ 64 = haut) |
| CC 24 | Scoop (≥ 64 = haut) |
| CC 25 | Footswitch (on / bypass) |
| Program Change 1 à 12 | Presets d'usine |

Footswitch MIDI à accrochage (127 = on / 0 = off, par défaut) ou momentané (bascule à chaque appui) : clic droit > Settings.
Pédale d'expression sans saccades : rampe vers chaque nouvelle valeur sur l'intervalle mesuré entre deux CC, puis lissage
à 2 pôles ; course apprise automatiquement (0 et 100 % exacts même si la pédale n'envoie que 2 à 116), zones mortes aux butées.

## Presets d'usine

1. Classic Pickle · 2. Siamese Dream · 3. Smooth Low Fuzz · 4. Sputter Zone · 5. Open Crunch · 6. Scooped Power Chords ·
7. Dark Wool · 8. Bright Lead · 9. Amp Pusher · 10. Bass Pickle · 11. Doom Sludge · 12. 7 Words (Deftones, intro de *7 Words*)

## Fichiers

- `PickleDSP.h` : moteur audio (sans JUCE, testé par `tests/test_dsp.cpp`) ; `PickleCircuit.h` : étages à transistor et tone stack au niveau des composants.
- `PluginProcessor.*`, `PluginEditor.*` : plugin JUCE 8.0.15.
- `background.jpg`, `toggle_strip.png`, `glow_yellow.png`, `Poppins-Medium.ttf` : visuel intégré.
- `tools/make_background.py` : prépare le fond à partir du visuel fourni (repères des potards et leviers effacés, LED jaune éteinte, écran et barre de presets vidés), rend les leviers des inverseurs, calcule le calque de la LED et écrit `Layout.h` ; `tools/make_background_v0.py` : premier design (abandonné).
- `tools/render/` : compile la vraie fenêtre sous Linux et exporte des captures.
- `assets/` : kit Studio Wiga (logo, jacks, inverseurs, footswitch d'Echoflex), polices Barlow Condensed et Poppins (SIL Open Font License).

## Historique

- v1.1.0 : clic droit > Plugin size > Default size (new windows) : taille par défaut des nouvelles fenêtres au choix, comme dans Octopus
- v1.2.0 : fuzz refaite au niveau des composants (`PickleCircuit.h` : 2N3904, 1N914, condensateurs et tone stack réels, résolution à chaque échantillon) : plus de pompage ni de compression artificielle, plus de gain, grave réduit avant l'écrêtage (fuzz définie) ; CRUNCH = 8,2 k en série avec les diodes ; « PICKLE » allumé exactement aligné sur les lettres éteintes
- v1.2.1 : plus de définition, moins de bave : liaisons des étages d'écrêtage 150 / 100 nF (au lieu de 220 / 150 nF), condensateur Miller 330 pF (au lieu de 470 pF)
- v1.2.2 : preset 12 « 7 Words » (son de l'intro de *7 Words*, Deftones)
- v1.2.3 : « PICKLE » allumé dessiné à partir d'une copie allumée du fond, mise à l'échelle exactement comme le fond : plus aucun décalage entre allumé et éteint, quelles que soient la taille de la fenêtre et l'échelle d'affichage de Windows
