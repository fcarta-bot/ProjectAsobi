# ProjectAsobi: istruzioni per Claude

Roguelike in prima persona in C++20 + raylib 5.5 (scaricata da CMake con FetchContent).
Cosa fa il gioco, i comandi e la mappa dei file sono nel `README.md`: leggilo prima di lavorare.

## Lavorare con l'utente
- Parla in **italiano**. L'utente programma "un po'": spiega le scelte in modo semplice.
- Commenti nel codice in italiano, brevi, nello stile dei file esistenti (senza accenti: `e'`, `piu'`).
- Ogni funzionalita' nuova va anche nel `README.md` (comandi, tabelle, struttura del codice).
- Obiettivo futuro: app per **Windows, Android, console**. Il codice dipendente dalla piattaforma
  sta in `src/link/` (abbinamento controller) e `src/PadRaw.cpp` (tasti che raylib non vede).
- A volte l'utente modifica gli stessi file da **PyCharm** (altro assistente): rileggi un file prima di
  modificarlo se potrebbe essere cambiato. Non toccare `UserAuthenticationRoute.txt`.

## Compilare
Toolchain: MinGW, CMake e Ninja inclusi in CLion 2026.2. Da PowerShell:
```
$env:PATH = "C:\Users\flavi\AppData\Local\Programs\CLion 2\bin\mingw\bin;C:\Users\flavi\AppData\Local\Programs\CLion 2\bin\cmake\win\x64\bin;C:\Users\flavi\AppData\Local\Programs\CLion 2\bin\ninja\win\x64;" + $env:PATH
cmake --build cmake-build-debug --target ProjectAsobi
```
- Eseguibile: `cmake-build-debug/ProjectAsobi.exe` (configurazione Run di CLion: `ProjectAsobi`).
- "cannot open output file ProjectAsobi.exe: Permission denied" = il gioco e' aperto (spesso lanciato
  dall'utente da CLion, processo figlio di `winpty-agent.exe`): **non chiuderlo**, avvisa l'utente.
- File nuovi in `src/` vanno aggiunti a `add_executable` in `CMakeLists.txt`.
- Linker: niente `-static` completo (rompe col MinGW di CLion); si usa `-static-libgcc -static-libstdc++`.
- Connettore CLion (MCP): `execute_run_configuration` con `waitForExit=true` va in errore, usa `false`.

## Architettura in breve
- La logica sta su una **mappa 2D in pixel** (casella = `TILE` = 32 px); il 3D e' solo disegno:
  1 casella = 1 unita' 3D, `To3D()` converte. Quote (salti, altezze, colpi) in caselle.
- `Game` (Game.cpp) e' il centro; parti di `Game` stanno in file separati: `EnemyAI.cpp`, `PauseMenu.cpp`
  (menu di gioco), `MatchMenu.cpp` (menu di partita + mappa esplorata), `Guide.cpp`, `Tutorial.cpp`,
  `ControllerMenu.cpp`. `GameMaster` (regista dei nemici) e `Nav` (A* e mappa delle distanze) sono classi a se'.
- Stati in `enum class State` (Intro, Playing, InventoryFull, GameOver, Paused, MatchMenu, ControllerMenu).
- Artefatti: l'ordine di `ArtifactId` deve corrispondere a `AllArtifacts()`; idem `WeaponId` / `AllWeapons()`.
- Tasti menu del joystick: **Options/START = menu di partita**, **touchpad/Share/PS = menu di gioco**.
  Il touchpad PS (tasto grezzo 13) si legge da GLFW in `PadRaw.cpp`. Nomi dei tasti: `Input::Btn()`.

## Trappole di raylib gia' incontrate
- raylib disegna a blocchi: prima di cambiare culling o depth mask chiama `rlDrawRenderBatchActive()`.
- `DrawTriangle` 2D si vede solo con i vertici in un verso: in dubbio disegnalo nei due versi (vedi `Tri()`).
- `TextFormat` ha pochi buffer a rotazione: usa il risultato subito, non salvarne tanti in un array.
- I colori di scena passano da `ApplyFog()`; il colore della foschia e' `District::horizon`.

## Provare il gioco
Nessun test automatico: si avvia l'exe e si fanno screenshot con uno script PowerShell nella scratchpad
(`SetProcessDPIAware`, `keybd_event` **con scan code**, flag "esteso" per le frecce, `CopyFromScreen`
sull'area client della finestra). Per arrivare a una situazione precisa si puo' aggiungere un gancio
temporaneo con `getenv("ASOBI_TEST_...")` marcato `// TEMP-TEST`: va **sempre rimosso** dopo
(controlla con una ricerca di `TEMP-TEST` e `getenv` in `src/`).
