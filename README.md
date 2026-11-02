# ProjectAsobi (遊び)

Roguelike action in prima persona, in C++ con [raylib](https://www.raylib.com/).

**L'ambientazione:** un labirinto enorme e' precipitato sulle citta' del mondo. Ogni piano e' una zona
di una delle 16 citta' del giro del mondo (vedi sotto): le strade, le strisce pedonali e i marciapiedi
sono ancora li', ma i muri di pietra del labirinto le tagliano
a caso. Auto schiacciate o divise in due dai muri, alberi piegati, cestini rovesciati, distributori a terra,
facciate di negozi e case rimaste incastrate nella pietra. In alto si vede il cielo e, all'orizzonte, il
profilo della citta'. Il fuoco c'e' solo dove c'e' qualcosa che puo' bruciare (auto, cestini, bancarelle).

Combatti le creature leggendarie di ogni paese, raccogli artefatti e armi, completa le sfide del GameMaster e trova il torii per passare al piano successivo.

## La storia

*Il GameMaster si annoiava, cosi' un giorno scese sulla terra per insinuarsi nei sogni del povero malcapitato.*

- **Menu iniziale** (all'avvio): il GameMaster, uno stickman bianco e luminoso, scende dal cielo, atterra
  davanti alla casa del malcapitato e si apposta sul davanzale. Dentro, il ragazzo e' ancora sveglio col
  telefono. Compare il menu: **Nuova partita**, **Scegli partita** (3 salvataggi), **Impostazioni**.
- **Il sogno**: scelta la partita, il ragazzo sbadiglia, spegne la luce e si addormenta; il GM entra dalla
  finestra e gli si infila nella testa. Lui sogna di girare il mondo... e il labirinto precipita.
  Il GM si presenta, spiega le regole (e le sfide) e sparisce. Riprendendo un salvataggio il sogno e' corto.
  Invio / A per andare avanti, Esc / B per saltare.
- **Tutorial**: niente lezioni all'inizio. Il GM compare di persona (in un riquadro, senza fermare il gioco)
  solo se sembri in difficolta' con un comando: fermo da troppo, mai girato, sotto attacco senza attaccare,
  bloccato contro un ostacolo basso, accanto a un oggetto senza raccoglierlo, colpito spesso senza scattare,
  due armi mai cambiate, menu di partita mai aperto, oppure perso nel piano. Appena ci riesci si complimenta
  e sparisce. Ogni consiglio torna al massimo una volta.
- **Morte**: il labirinto sfuma nel rosso, e il malcapitato si sveglia di soprassalto nel suo letto.
  Dalla finestra, il GM saluta: "A stanotte!". Poi il resoconto del sogno. La run e' persa e il salvataggio si cancella.
- **Vittoria**: completa una delle 3 sfide "vittoria" della run e il GM scoppia a ridere: si e' divertito.
  Ti da' un titolo, e il ragazzo si sveglia contento.

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
| Attacca e mira | Stick di tiro **ATTACCA**: finche' il pollice e' appoggiato spari; spostandolo la visuale gira a destra e a sinistra e guarda in alto e in basso, col mirino sempre al centro. Piccoli spostamenti = mira precisa, tutto di lato = giro veloce |
| Girati senza sparare | Trascina il dito dove non ci sono comandi (sensibilita' alta: basta un angolino) |
| Sistemare i comandi | **Modifica comandi touch** (impostazioni del menu iniziale o menu di pausa) |
| Salta / scatto / cambia arma / raccogli | **SALTA**, **SCATTO**, **ARMA**, **PRENDI** |
| Menu di partita / menu di gioco | **BORSA** e il tasto pausa, in alto a destra |
| Nei menu | Frecce a sinistra, **OK** e **INDIETRO** a destra |
| Nelle animazioni | Tocca ovunque per andare avanti, **SALTA** per saltarle |

Con il touch l'inventario non e' in basso (li' ci sono i comandi): si vede nel menu **BORSA**.

**Modifica comandi touch**: si trascina un comando per spostarlo e toccandolo lo si sceglie. Nel pannello:
grandezza (tutti i comandi, anche BORSA e pausa in alto), sensibilita' (i due stick) e stick **fisso**
(sta dove l'hai messo) o **mobile** (parte da dove appoggi il dito nella sua meta' di schermo).
C'e' anche la sensibilita' della **visuale libera**: dove non ci sono comandi trascinare sposta la
visuale senza sparare (uno stick mobile invece compare dove si appoggia il dito, dal suo lato dello schermo).
**Ripristina** rimette tutto com'era, **Fatto** salva nel profilo (riga `touch=`).
La disposizione di partenza sta nella tabella `CTL` in `src/Touch.cpp`, quella dei menu in `BUTTONS`.

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
| **Oni** | Il guardiano del torii in **Giappone**. Clavata, carica (se finisce contro un muro resta stordito), massi se sali su qualcosa, schiva i colpi. Sotto meta' vita diventa furioso e usa lo schianto a terra. Lascia sempre un artefatto, spesso un'arma e dei cuori |

Goblin e robot sono i servitori del GM: li trovi in tutto il mondo (in Giappone di piu').
Ogni paese ha poi le sue **creature leggendarie** e il suo **guardiano del torii** (in grassetto):

| Paese | Creatura | Come si comporta |
|---|---|---|
| Stati Uniti | **Wendigo** | Scatti fulminei; i morsi lo curano. Ferito, corre a divorare i nemici caduti per rimettersi in forze: colpiscilo mentre mangia |
| Stati Uniti | Mothman | Vola alto in cerchio, si vedono solo gli occhi rossi. Se ti fissa a lungo ti da' il **Presagio** (danni subiti x1.5). Si colpisce quando scende in picchiata, o saltando |
| Stati Uniti | Jersey Devil | Mini-boss: balza addosso (lo schianto si evita saltando). A meta' vita urla e sveglia tutti |
| Italia | **Badalisc** | Neutrale: se ne sta vicino al torii a spettegolare e puoi passare. Se lo attacchi si offende: rotola rimbalzando e sputa pettegolezzi a ventaglio |
| Italia | Gnefri | Folletti nascosti sotto le macerie: saltano fuori tutti insieme quando passi (imboscata) |
| Italia | Uomo Nero | Un'ombra che ti segue alle spalle e fa salire la **paura**. Si materializza solo con la paura alta, e piu' paura hai piu' e' forte. Se ti calmi torna nell'ombra |
| Germania | **Der Grossmann** | L'Uomo Alto: se lo guardi si ferma, poi sparisce e ricompare alle tue spalle. Quando non lo guardi arriva in fretta e allunga le braccia. Vicino a lui lo schermo si disturba |
| Germania | Nachzehrer | Morto famelico, lento: da vicino ti prosciuga la vita e se la prende |
| Germania | Wolpertinger | Lepri cornute in branco: mordono e scappano |
| Francia | **Bestia del Gevaudan** | Lupo enorme che balza addosso. Le armi normali non la scalfiscono: servono l'**argento** (Fucile di Chastel, che il GM ti lascia in giro) o il **fuoco** |
| Francia | Dahu | Innocuo, sa girare solo a sinistra. Se riesci a toccarlo ti regala un artefatto |
| Francia | Matagot | Gatto-spirito: sparisce e ricompare intorno a te sparando sfere. Sconfitto lascia due artefatti... ma il **patto di sangue** costa 15 vita |
| Inghilterra | **Black Shuck** | Il cane nero sorveglia la sua brughiera (la stanza del torii): se ci entri ti insegue ovunque, senza tregua |
| Inghilterra | Spring-heeled Jack | Balzi enormi e fiamme blu. In aria non lo prendi coi colpi bassi: aspetta che atterri o salta anche tu |
| Inghilterra | Berretto Rosso | Ogni volta che ti ferisce diventa piu' forte |
| Spagna | **El Cuco** | Travestito da sacco vicino al torii. Ti trova dal rumore: se **stai fermo e non attacchi** perde le tue tracce. Ogni tanto ricompare alle tue spalle |
| Spagna | Dip | Cane nero zoppo: il morso ti fa **sanguinare** e lo cura |
| Spagna | Santa Compana | Processione di anime con le candele che va di stanza in stanza. Non si combatte: se ci finisci in mezzo, la maledizione ti prosciuga per 25 secondi |

**Paura** (Italia e Spagna): sale quando hai poca vita, quando vieni colpito e quando qualcosa ti osserva
dal buio; scende sconfiggendo nemici e raccogliendo cuori. La barra viola e' sotto le armi.

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
| Revolver Colt | Far West | Colpi rapidi e precisi che trapassano un nemico |
| Fucile a pompa | Texas | Sei pallettoni a ventaglio: devastante da vicino |
| Mazza da baseball | New York | Colpo largo e pesante, spinge lontano e stordisce un attimo |
| Tomahawk | Grandi Pianure | Ascia da lancio che rimbalza e trapassa |
| Gladio | Roma | Spada corta, colpi rapidi |
| Balestra genovese | Genova | Lenta, ma i dardi trapassano 3 nemici |
| Lancia della Giostra | Arezzo | Affondo lunghissimo |
| Fuochi di Piedigrotta | Napoli | Tre fuochi d'artificio a ventaglio che incendiano |
| Fioretto | Parigi | Affondi lunghi e rapidissimi |
| Fucile di Chastel | Gevaudan | Pallottole d'**argento**: l'unica arma normale che ferisce la Bestia |
| Boule de petanque | Marsiglia | Bocce pesanti che rimbalzano 3 volte |
| Tappo di Champagne | Reims | Il tappo rimbalza e poi esplode in schiuma (niente fuoco) |
| Zweihander | Solingen | Spadone lento che taglia tutto davanti |
| Morgenstern | Norimberga | Mazza ferrata: ogni colpo stordisce |
| Boccale dell'Oktoberfest | Monaco | Esplode in schiuma di birra |
| Boller di Capodanno | Berlino | Petardi a raffica |
| Arco lungo inglese | Agincourt | Frecce velocissime che attraversano 3 nemici |
| Ombrello di Londra | Londra | Rapido, e ferma i proiettili |
| Freccette da pub | Pub inglesi | Tre freccette a ventaglio |
| Mazza da cricket | Lord's, Londra | **Rispedisce al mittente** i colpi nemici |
| Espada toledana | Toledo | Spada equilibrata e affilata |
| Navaja | Albacete | Coltello cortissimo ma velocissimo |
| Banderillas | Siviglia | Due aste che trapassano |
| Petardi delle Fallas | Valencia | Rimbalzano una volta e poi esplodono in fiamme |

Di solito trovi le armi del paese in cui ti trovi. Al primo piano ce n'e' una accanto alla partenza;
poi una per piano, e a volte la lascia un nemico.

**Armi leggendarie** (solo come premio delle sfide del GM): Kusanagi (il fendente lancia un'onda di vento),
Excalibur (d'argento, ti cura a ogni nemico colpito), Durlindana (d'argento, rimanda indietro i colpi),
Tizona (paralizza dalla paura), Balmung (colpisce tutto intorno, anche oltre i muri),
Martello di John Henry (ogni colpo fa tremare la terra).

## Il giro del mondo

Ogni run il GM mischia 16 citta' e ogni piano ne pesca una. Ogni citta' ha il suo cielo, il suo profilo
all'orizzonte, le sue facciate incastrate nei muri, i suoi alberi e i suoi oggetti per strada.

| Paese | Citta' (piano) |
|---|---|
| Giappone | Kyoto (Fushimi Inari), Nara (parco dei cervi), Osaka (Dotonbori), Tokyo (Shibuya) |
| Stati Uniti | New York (Times Square), New Orleans (French Quarter), Chicago (Lincoln Park), Las Vegas (la Strip) |
| Italia | Roma (Fori Imperiali), Venezia (Campo San Polo), Napoli (Spaccanapoli), Firenze (Piazza della Signoria) |
| Francia | Parigi (Montmartre) |
| Germania | Monaco di Baviera (Marienplatz d'inverno, con la neve) |
| Inghilterra | Londra (Westminster nella nebbia) |
| Spagna | Siviglia (Barrio de Santa Cruz) |

Le mappe non sono tutte squadrate: stanze rettangolari, a L, tonde o piazze grandi, corridoi che piegano,
piu' strade per arrivare nello stesso posto, nicchie, brecce nei muri sottili e muri di altezze diverse.

## Le sfide del GameMaster

Nel menu di partita, scheda **Sfide**.

- **Premi** (valgono sempre): 10 sfide, ognuna regala un pezzo raro (le 6 armi leggendarie o 4 artefatti
  rari: Orologio di Greenwich, Cornicello napoletano, Acchiappasogni, Giglio d'oro). Es.: 100 nemici,
  3 guardiani, un piano senza farti colpire, acchiappare il Dahu, 10 nemici in 10 secondi, piano 10...
- **Vittoria**: a ogni run il GM ne sceglie 3 su 10. Completane una e la run e' **vinta**, con un titolo
  che resta per sempre (si vedono in Impostazioni > Titoli). Es.: sconfiggi 500 mostri, sconfiggi un
  guardiano potenziato, arriva al piano 100 senza subire ne' infliggere danni, visita tutte le 16 citta',
  sconfiggi il guardiano di ogni paese, piano 15 in meno di 25 minuti...

## Salvataggi e impostazioni

- **3 salvataggi**. Il gioco salva da solo all'inizio di ogni piano; dal menu di gioco c'e' anche
  "Salva e torna al menu". Riprendendo, si riparte dall'inizio di quel piano.
- Se muori (o vinci) la run finisce e il salvataggio si libera.
- Su PC i salvataggi sono nella cartella `salvataggi` accanto al gioco; nel browser restano nella memoria
  del sito (sullo stesso iPad e con lo stesso browser).
- **Impostazioni**: controller, comandi, schermo intero, sensibilita' della visuale, consigli del GM, titoli.

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
| `src/Dungeon.hpp/.cpp` | Generazione del labirinto, strade della citta' sotto, disegno 3D di suolo, muri, facciate di ogni paese e torii |
| `src/World.cpp` | Le 16 citta' (colori, facciate, alberi...) e il giro del mondo di ogni run |
| `src/Legends.cpp` | Dati di tutti i nemici e comportamento delle creature leggendarie; paura e maledizioni |
| `src/EnemyDraw.cpp` | Disegno 3D di tutti i nemici |
| `src/Challenges.hpp/.cpp` | Le sfide del GameMaster, premi, vittoria, titoli |
| `src/Save.cpp` | Salvataggi (3 partite) e profilo; su PC file, nel browser localStorage |
| `src/Title.cpp` | Menu iniziale, scelta del salvataggio, impostazioni |
| `src/Props.hpp/.cpp` | Oggetti della citta' distrutta: dove nascono, collisioni, salti, fuoco, disegno |
| `src/Sky.hpp/.cpp` | Cielo e profilo della citta' all'orizzonte |
| `src/Weapons.hpp/.cpp` | Le armi: caratteristiche, modellini a terra, arma in mano |
| `src/Artifacts.hpp/.cpp` | Artefatti, sinergie e calcolo delle statistiche |
| `src/Input.hpp/.cpp` | Tastiera, mouse e joystick (collegamento, zona morta, comandi, nomi dei tasti) |
| `src/PadRaw.hpp/.cpp` | Input che raylib non vede o non legge bene: touchpad PlayStation (GLFW o browser), dita sullo schermo |
| `src/Touch.hpp/.cpp` | Comandi touch: stick per muoversi, stick di tiro, visuale libera, tasti a schermo (tabelle `CTL` e `BUTTONS`) ed editor dei comandi |
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

Non serve farlo a mano: a ogni salvataggio sul ramo `ipad-web` GitHub compila da solo la versione
web e la pubblica sul ramo `gh-pages` (il sito). Lo fa `.github/workflows/web.yml`; si vede in **Actions**.

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
