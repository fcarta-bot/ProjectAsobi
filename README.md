# ProjectAsobi (遊び)

Roguelike action in prima persona, in C++ con [raylib](https://www.raylib.com/).

**L'ambientazione:** un labirinto enorme e' precipitato sulle citta' del Giappone. Ogni piano e' una zona
di una citta' (Fushimi Inari a Kyoto, il parco dei cervi di Nara, Dotonbori a Osaka, Shibuya a Tokyo):
le strade, le strisce pedonali e i marciapiedi sono ancora li', ma i muri di pietra del labirinto le tagliano
a caso. Auto schiacciate o divise in due dai muri, alberi piegati, cestini rovesciati, distributori a terra,
facciate di negozi e case rimaste incastrate nella pietra. In alto si vede il cielo e, all'orizzonte, il
profilo della citta'. Il fuoco c'e' solo dove c'e' qualcosa che puo' bruciare (auto, cestini, bancarelle).

Combatti yokai, raccogli artefatti e armi, trova il torii per passare al piano successivo.

## La storia

*Il GameMaster si annoiava, cosi' un giorno scese sulla terra per insinuarsi nei sogni del povero malcapitato.*

- **Intro** (all'avvio del gioco): il GameMaster, uno stickman bianco e luminoso, scende dal cielo, compare
  ai piedi del letto del malcapitato e gli entra nella testa. Lui sogna il Giappone... e il labirinto
  precipita sulla citta'. Poi il GM si presenta, spiega le regole e sparisce.
  Invio / A per andare avanti, Esc / B per saltarla.
- **Tutorial**: niente lezioni all'inizio. Il GM compare di persona (in un riquadro, senza fermare il gioco)
  solo se sembri in difficolta' con un comando: fermo da troppo, mai girato, sotto attacco senza attaccare,
  bloccato contro un ostacolo basso, accanto a un oggetto senza raccoglierlo, colpito spesso senza scattare,
  due armi mai cambiate, menu di partita mai aperto, oppure perso nel piano. Appena ci riesci si complimenta
  e sparisce. Ogni consiglio torna al massimo una volta.
- **Morte**: il labirinto sfuma nel rosso, e il malcapitato si sveglia di soprassalto nel suo letto.
  "Era solo un incubo..." Dalla finestra, il GM saluta. "...o forse no." Poi il resoconto del sogno.

## Comandi

| Azione | Tastiera e mouse | Joystick (nomi Xbox / PlayStation) |
|---|---|---|
| Avanti / indietro / laterale | WASD (o frecce su e giu') | Stick sinistro |
| Girati | Mouse / frecce sinistra e destra | Stick destro |
| Attacca (dove punta il mirino) | Tasto sinistro (tenuto) | RT o RB / R2 o R1 |
| Salta | Spazio | A / X (croce) |
| Scatto (invulnerabile per un attimo) | Shift, nella direzione in cui ti muovi | Frecce, nella direzione della freccia (o LB / L1) |
| Raccogli / attraversa il torii | E | X / Quadrato |
| Cambia arma | Q / rotellina | Y / Triangolo |
| **Menu di partita** (inventario, armi, mappa, statistiche) | Tab / I | **Options** / Menu |
| **Menu di gioco** (riprendi, comandi, controller, nuova run, esci) | Esc / P | **Touchpad** (anche Share / Visualizza e tasto PS / Xbox) |
| Menu Controller | F1 (o dal menu di gioco) | dal menu di gioco |
| Schermo intero / finestra | F11 o Alt+Invio (anche dal menu di gioco) | dal menu di gioco |
| Inventario pieno: scegli | 1-6, oppure frecce + Invio | Su/Giu + A |
| Inventario pieno: lascia a terra | 6 / Q / Esc | B / Cerchio |

Nel gioco i tasti del joystick sono chiamati col nome del **tuo** controller (PlayStation, Xbox o Nintendo):
lo vedi nella riga in basso e in **Pausa > Comandi**.

In partita il mouse viene "catturato" per girare la visuale; nei menu torna il puntatore normale.

### Touch (iPad)

Appena si tocca lo schermo compaiono i comandi touch. Spariscono se si usa la tastiera o un joystick.

| Azione | Come |
|---|---|
| Muoviti | Appoggia il dito nella meta' sinistra e trascinalo (stick virtuale) |
| Girati | Trascina il dito nella meta' destra |
| Attacca | Tieni premuto **ATTACCA**. Trascinandolo ti giri, cosi' attacchi e miri insieme |
| Salta / scatto / cambia arma / raccogli | **SALTA**, **SCATTO**, **ARMA**, **PRENDI** |
| Menu di partita / menu di gioco | **BORSA** e il tasto pausa, in alto a destra |
| Nei menu | Frecce a sinistra, **OK** e **INDIETRO** a destra |
| Nelle animazioni | Tocca ovunque per andare avanti, **SALTA** per saltarle |

Con il touch l'inventario non e' in basso (li' ci sono i comandi): si vede nel menu **BORSA**.
La disposizione dei tasti sta tutta nella tabella `BUTTONS` in `src/Touch.cpp`.

### Il menu di partita

Si apre con **Options** (o Tab) e si sfoglia con L1 / R1 o le frecce. Quattro schede:

| Scheda | Cosa mostra |
|---|---|
| Inventario | I 5 artefatti con descrizione, gli elementi (quanti ne mancano a 3) e tutte le sinergie, quelle attive in oro |
| Armi | Le due armi con i valori reali, artefatti compresi: danno, cadenza, proiettili, trapasso, rimbalzi... |
| Mappa | Solo le zone del piano che hai **visto**, con te, il torii e gli oggetti a terra. Non mostra i bordi ne' quanto e' grande il piano: si inquadra su cio' che conosci e si allarga man mano che esplori. I nemici non compaiono |
| Statistiche | Tempo, nemici sconfitti per tipo, danno inflitto e subito, artefatti raccolti, e tutte le caratteristiche del personaggio |

### Se ti perdi

Cambiare piano non e' obbligatorio: puoi esplorare quanto vuoi. Ma se vaghi a lungo senza scoprire
niente di nuovo (dopo 2 minuti sul piano, 45 secondi senza caselle nuove; o comunque dopo 5 minuti)
in alto compare una **freccia verso il torii**. Segue la strada vera, intorno a muri e oggetti,
e resta fino al piano successivo. I tempi si regolano in cima a `src/Guide.cpp`.

### Il salto

- Scavalca tronchi caduti, cestini e distributori a terra; sali sulle auto (e cammina sopra).
- In aria (o in piedi su un'auto) i nemici corpo a corpo non ti toccano, e i colpi dei tengu ti passano sotto.
- Lo schianto a terra dell'oni si evita saltando.

## Nemici

Tutti girano intorno a muri e oggetti per raggiungerti. Si accorgono di te solo se ti **vedono**: hanno
un campo visivo (gli puoi passare alle spalle) e i muri li bloccano. Quando ti notano compare un **!**.
Se li semini, il loro **interesse** cala: prima vanno dove ti hanno visto l'ultima volta, poi si stufano
e tornano al loro posto. Piu' sei veloce di loro (lo scatto aiuta), prima si arrendono.
Ogni attacco ha un **preavviso** (si alzano, lampeggiano, compare un segno a terra): e' il momento di schivare.

| Nemico | Come si comporta |
|---|---|
| **Goblin** | Sciami da 7 a 20. Ti circondano e mordono da vicino. Se lo sciame scende **sotto 7** scappa e si unisce a un altro sciame; se supera **20**, i compagni eliminano i piu' deboli e si spartiscono la loro forza (diventano piu' grossi e piu' forti) |
| **Robot** | Squadre miste da 3 a 7, dirette dal **GameMaster**: pattugliano le stanze, ti accerchiano da lati diversi (assaltatori, arancioni) mentre gli artiglieri (azzurri) si piazzano a distanza e sparano raffiche. Se ti perdono si dividono per cercarti. Non scappano mai |
| **Tengu** | Da solo. Ti tiene a distanza, si sposta di lato e spara anticipando i tuoi movimenti. Se ti avvicini scappa; se lo metti all'angolo spara a ventaglio |
| **Oni** | Il **boss** del piano: fa la guardia al torii. Clavata, carica (se finisce contro un muro resta stordito), massi se sali su qualcosa, schiva i colpi. Sotto meta' vita diventa furioso e usa lo schianto a terra. Lascia sempre un artefatto, spesso un'arma e dei cuori |

## Armi

Se ne portano **due**: con Q / Triangolo passi dall'una all'altra. Se prendi un'arma con le mani piene,
quella che stai usando resta a terra al suo posto. Gli artefatti potenziano tutte le armi (danno, cadenza,
proiettili in piu', bruciatura, trapasso...).

| Arma | Tributo | Come funziona |
|---|---|---|
| Ofuda | Santuari shinto | Talismani di carta: l'arma di partenza, equilibrata |
| Katana | Seki | Fendente ravvicinato fortissimo, colpisce tutto davanti a te e **respinge i colpi nemici** |
| Yumi | Sanjusangen-do, Kyoto | Arco lungo: lento, potente, le frecce trapassano 2 nemici |
| Fionda pachinko | Sale pachinko di Osaka | Raffica di biglie d'acciaio che **rimbalzano sui muri** |
| Razzi Tamaya | Fuochi di Sumida, Tokyo | Esplodono in fuochi d'artificio, danno ad area, **incendiano** cio' che e' infiammabile |

Al primo piano trovi un'arma accanto alla partenza; poi ce n'e' una per piano, e a volte la lascia un nemico.

## Il fuoco

- Brucia solo cio' che e' infiammabile: auto (soprattutto quelle tagliate dai muri), cestini dei
  rifiuti combustibili, bancarelle di Osaka, panchine di legno.
- I razzi Tamaya incendiano gli oggetti infiammabili vicini all'esplosione.
- Il fuoco scotta: a te e ai nemici. Attirarli vicino a un'auto in fiamme e' una tattica.

## Collegare un joystick

Tutto si fa dal gioco: **Pausa > Controller e joystick** (o F1), poi **+ Collega un nuovo joystick...**:

| Metodo | Cosa fa il gioco |
|---|---|
| **Bluetooth** | Cerca i controller vicini (classici e Bluetooth LE), li elenca e li abbina. Appena Windows lo attiva, il gioco lo collega da solo |
| **Cavo USB** | Aspetta il joystick e lo collega appena viene inserito |
| **Pennetta o ricevitore** | Come il cavo, per i ricevitori dei controller (es. adattatore Xbox). Se inserisci una pennetta Bluetooth generica, il gioco se ne accorge e abilita l'opzione Bluetooth |

Se il dispositivo non ha il Bluetooth, il gioco lo segnala e propone cavo o pennetta.
Un joystick gia' presente si collega anche al volo premendo **START/Options** (che apre subito il menu) o **A/croce**.

Nel menu Controller il pannello **Prova joystick** mostra in tempo reale stick e tasti premuti.
Se il joystick si stacca durante la partita, il gioco si mette in pausa e apre il menu.

### Piattaforme

La ricerca e l'abbinamento stanno in `src/link/`, con un'implementazione per piattaforma:

| Piattaforma | Stato |
|---|---|
| Windows | `ControllerLink_Windows.cpp`: API moderna di Windows (WinRT), Bluetooth classico e LE |
| Android | da scrivere quando porteremo il gioco su Android (l'API Bluetooth di Android permette l'abbinamento dall'app) |
| Console / iOS / browser | `ControllerLink_Unsupported.cpp`: i controller li abbina il sistema, il gioco li rileva e basta |

## Il cuore del gioco: artefatti e sinergie

- L'inventario ha **5 posti**. Quando e' pieno devi scegliere cosa **scartare per sempre**.
- Ogni artefatto ha 1-2 **elementi**: Fuoco, Vento, Spirito, Acciaio, Fortuna.
- **3 artefatti con lo stesso elemento** attivano una sinergia (es. Fuoco x3 = *Hanabi*).
- Alcune **coppie specifiche** attivano combo (es. Shuriken di Iga + Ventaglio Sensu = *Tempesta di Iga*).
- Nella schermata di scelta vedi in anteprima quali sinergie guadagni (verde) o perdi (rosso).

## Struttura del codice

| File | Contenuto |
|---|---|
| `src/main.cpp` | Finestra e ciclo principale (nel browser il ciclo lo fa il browser) |
| `src/Game.hpp/.cpp` | Logica di gioco: giocatore (salto, scatto), colpi, armi, telecamera, disegno dei nemici, HUD |
| `src/EnemyAI.cpp` | IA dei nemici: vista e interesse, movimento, sciami di goblin, comportamento di ogni tipo |
| `src/GameMaster.hpp/.cpp` | Il "regista": sceglie e piazza i nemici di ogni piano e comanda le squadre di robot |
| `src/Nav.hpp/.cpp` | Navigazione: percorsi A* e mappa delle distanze dal giocatore, per non sbattere nei muri |
| `src/Dungeon.hpp/.cpp` | Labirinto, distretti, strade della citta' sotto il labirinto, disegno 3D di suolo, muri e torii |
| `src/Props.hpp/.cpp` | Oggetti della citta' distrutta: dove nascono, collisioni, salti, fuoco, disegno |
| `src/Sky.hpp/.cpp` | Cielo e profilo della citta' all'orizzonte |
| `src/Weapons.hpp/.cpp` | Le armi: caratteristiche, modellini a terra, arma in mano |
| `src/Artifacts.hpp/.cpp` | Artefatti, sinergie e calcolo delle statistiche |
| `src/Input.hpp/.cpp` | Tastiera, mouse e joystick (collegamento, zona morta, comandi, nomi dei tasti) |
| `src/PadRaw.hpp/.cpp` | Input che raylib non vede o non legge bene: touchpad PlayStation (GLFW o browser), dita sullo schermo |
| `src/Touch.hpp/.cpp` | Comandi touch: stick virtuale, trascinare per girarsi, tasti a schermo (tabella `BUTTONS`) |
| `src/PauseMenu.cpp` | Menu di gioco (pausa) e pagina dei comandi |
| `src/MatchMenu.cpp` | Menu di partita: inventario, armi, mappa esplorata, statistiche della run |
| `src/Guide.cpp` | Freccia che indica la strada per il torii quando ti perdi |
| `src/Cutscene.hpp/.cpp` | Animazioni della storia: il GameMaster, l'intro, il risveglio |
| `src/Tutorial.cpp` | I consigli del GameMaster quando il giocatore e' in difficolta' |
| `src/ControllerMenu.cpp` | Menu Controller: elenco, nuovo joystick (Bluetooth / cavo / pennetta), prova tasti |
| `src/link/` | Ricerca e abbinamento dei controller, un file per piattaforma |
| `src/Ui.hpp` | Piccole funzioni di disegno condivise |
| `web/` | Versione browser: pagina per iPad (`shell.html`) e script di compilazione (`build-web.ps1`) |

La logica resta su una mappa 2D in pixel (caselle da 32): il 3D e' solo il modo di disegnarla.
Nel mondo 3D una casella misura 1 unita' e `To3D()` converte le posizioni. Le quote (salti, altezza
degli oggetti, dei colpi) sono in caselle.

Per aggiungere un artefatto: aggiungi un valore all'enum `ArtifactId` e una riga nella lista
in `AllArtifacts()`, **nello stesso ordine**. Per le armi vale lo stesso con `WeaponId` e `AllWeapons()`.

## Compilare

Apri la cartella in **CLion** e premi Run (configurazione `ProjectAsobi`).
Al primo caricamento CMake scarica e compila raylib da solo: serve la connessione internet
e ci vuole circa un minuto.

### Versione iPad (browser)

Un'app iPad vera si compila solo su un Mac con Xcode. Al suo posto il gioco si compila per il
**browser** con Emscripten. Su iPad si apre da Safari e con **Condividi > Aggiungi alla schermata Home**
diventa un'icona che si apre a tutto schermo, come un'app.

1. Installa Emscripten una volta sola, in `C:\Users\flavi\emsdk`
   (`git clone https://github.com/emscripten-core/emsdk`, poi `emsdk install latest` e `emsdk activate latest`).
2. Da PowerShell, nella cartella del progetto: `.\web\build-web.ps1`
3. In `build-web/` trovi `index.html`, `index.js` e `index.wasm`: vanno messi su un sito
   (ad esempio GitHub Pages o itch.io). Aprire `index.html` con doppio clic non funziona,
   perche' il browser vuole un indirizzo web.

Nel browser il menu di gioco non ha "Schermo intero" ne' "Esci": li gestisce la pagina.
L'iPad va tenuto in orizzontale. Va bene anche un joystick abbinato all'iPad via Bluetooth.

## Idee per i prossimi passi

- Modelli, texture e animazioni al posto delle forme geometriche
- Luci (lanterne, insegne, fuochi) che illuminano davvero
- Altre citta' (e citta' del mondo, come previsto all'inizio)
- GameMaster: profilo del giocatore salvato tra le sessioni e difficolta' che si adatta
- Negozio tra un piano e l'altro
- Effetti sonori e musica
- Minimappa delle zone gia' esplorate
