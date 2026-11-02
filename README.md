# Strong Pickle (Studio Wiga) — VST3

Fuzz au format pédale simple (référence de format : Echoflex / Doorn), reproduction de la
**Way Huge Swollen Pickle Jumbo Fuzz MkIII** (WM41, série « Smalls »), fuzz de la famille Big Muff
connue pour son grave énorme, sa réserve de volume et ses crachotements (« sputter ») à mi-course.
Usage personnel. Windows 11 et macOS (binaire universel). Design original (aucune image de la pédale d'origine).

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
| Circuit | Big Muff modifié : étage d'entrée, deux étages d'écrêtage à diodes, tone stack passif, étage de sortie | Même chaîne, modélisée étage par étage, écrêtage sur-échantillonné x4 |
| SUSTAIN | Quantité de fuzz ; gain total 18 à 62 dB (fiche Way Huge) ; fuzz lisse vers 8 – 10 h, crachotements vers midi, mur de fuzz au-delà | 18 → 62 dB mesurés en petits signaux (`tests/test_dsp.cpp`) ; le gain monte vite en début de course ; la charge du condensateur de liaison par la jonction base-émetteur décale la polarisation (crachotements et « bloom » de l'attaque, maximum vers midi) |
| LOUDNESS | Volume de sortie, réserve énorme | Courbe audio, 0 = coupé, midi ≈ niveau du bypass, +12 dB en haut de course (limiteur de sécurité à 0 dBFS) |
| FILTER | Du sombre / gras au brillant / fin (tone stack passe-bas / passe-haut) | Mélange passe-bas 470 Hz / passe-haut 1,15 kHz, creux de médiums au centre ; à gauche le grave gonfle, à droite il s'amincit et le grain ressort |
| SCOOP (inverseur) | Creuse les médiums | Bas = voix classique (creux léger) ; haut = creux d'environ −8 dB vers 760 Hz (filtre à midi) |
| CRUNCH (inverseur) | Change l'intensité de la compression | Bas = classique : diodes franches, fuzz lisse et compressée ; haut = diodes freinées (résistance série) : l'étage va jusqu'à ses rails, fuzz plus ouverte et granuleuse, ~ +5 dB |
| Les deux inverseurs en bas | La Swollen Pickle « trois potards » classique | Preset 1 *Classic Pickle* |
| Grave « jumbo » | Condensateurs de liaison plus gros qu'une Big Muff | Liaisons à 18 – 22 Hz + bosse de 2,5 dB à 95 Hz |
| Footswitch / LED | True bypass, LED bleue | Footswitch d'Echoflex (bascule à l'appui), LED bleue en fondu, la façade s'éclaire autour |
| Noise gate (ajout) | — | Clic droit > *Noise gate* : Off (défaut, comme la pédale) / Light / Medium / Strong |

Sources : manuel Way Huge WM41 (Dunlop), fiches revendeurs (Reverb, Guitar Center, Musik Store), test Bonedo
(Swollen Pickle MkIII), test du MkII sur basse (forum Seymour Duncan : rôle de Sustain, Filter, Scoop, Crunch),
analyse du Big Muff (Electrosmash), Effects Layouts (Swollen Pickle Mk2).
Note : les textes officiels se contredisent sur le sens du SCOOP (« creuse » ou « garde » les médiums) ; le manuel
et le test Bonedo disent qu'il creuse : c'est ce qui est reproduit.

## Interface

- Fenêtre ouverte à **493 px de haut** (391 × 493), impossible de la réduire en dessous, proportions bloquées, jusqu'à 150 %. Clic droit > *Window size* : 60 % (défaut) / 75 % / 100 %.
- **Barre de presets** sous la ligne d'état : flèches = preset précédent / suivant, clic sur le nom = menu (presets d'usine, presets utilisateur 1 à 9).
- **Ligne d'état** : réglage de la commande que l'on bouge (potards en %, inverseurs), police Poppins ; apparaît quand un réglage bouge, puis s'efface.
- Potards : seul le trait blanc tourne, les reflets du chapeau restent fixes (lumière en haut à gauche). Double-clic = midi.
- Inverseurs : ceux du kit Studio Wiga (FreakOut), image intermédiaire ~55 ms pendant la bascule ; clic = bascule, molette possible.
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
| Program Change 1 à 11 | Presets d'usine |

Footswitch MIDI à accrochage (127 = on / 0 = off, par défaut) ou momentané (bascule à chaque appui) : clic droit > Settings.
Pédale d'expression sans saccades : rampe vers chaque nouvelle valeur sur l'intervalle mesuré entre deux CC, puis lissage
à 2 pôles ; course apprise automatiquement (0 et 100 % exacts même si la pédale n'envoie que 2 à 116), zones mortes aux butées.

## Presets d'usine

1. Classic Pickle · 2. Siamese Dream · 3. Smooth Low Fuzz · 4. Sputter Zone · 5. Open Crunch · 6. Scooped Power Chords ·
7. Dark Wool · 8. Bright Lead · 9. Amp Pusher · 10. Bass Pickle · 11. Doom Sludge

## Fichiers

- `PickleDSP.h` : moteur audio (sans JUCE, testé par `tests/test_dsp.cpp`).
- `PluginProcessor.*`, `PluginEditor.*` : plugin JUCE 8.0.15.
- `background.jpg`, `toggle_strip.png`, `glow_blue.png`, `Poppins-Medium.ttf` : visuel intégré.
- `tools/make_background.py` : génère le fond, l'inverseur, le calque de la LED et `Layout.h`.
- `tools/render/` : compile la vraie fenêtre sous Linux et exporte des captures.
- `assets/` : kit Studio Wiga (logo, jacks, inverseurs, footswitch d'Echoflex), polices Barlow Condensed et Poppins (SIL Open Font License).
