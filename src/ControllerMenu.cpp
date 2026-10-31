// Menu Controller: scegliere, collegare (Bluetooth, cavo, pennetta) e provare i controller dal gioco.
#include <algorithm>
#include <string>
#include "Game.hpp"
#include "PadRaw.hpp"
#include "Ui.hpp"

// Voci della pagina Devices: 0 = tastiera e mouse, 1..MAX_PADS = joystick, poi "nuovo" e "torna"
constexpr int ITEM_NEW_PAD = Input::MAX_PADS + 1;
constexpr int DEVICE_ITEMS = Input::MAX_PADS + 3;

// Voci della pagina NewController
enum { METHOD_BLUETOOTH, METHOD_CABLE, METHOD_DONGLE, METHOD_BACK, METHOD_COUNT };

static const Color BORDER = { 90, 90, 100, 255 };

static int Wrap(int value, int count) {
    return (value % count + count) % count;
}

// ---------------------------------------------------------------------------
// Navigazione tra le pagine
// ---------------------------------------------------------------------------

void Game::OpenControllerMenu(const std::string& message) {
    if (state != State::ControllerMenu) menuReturnState = state;
    state = State::ControllerMenu;
    GoToPage(MenuPage::Devices);
    menuMessage = message;
}

void Game::CloseControllerMenu() {
    link->Cancel();
    state = menuReturnState;
}

void Game::GoToPage(MenuPage page) {
    if (menuPage == MenuPage::Bluetooth && page != MenuPage::Bluetooth) link->Cancel();
    menuPage = page;
    pageIndex = 0;
    menuMessage.clear();
    if (page == MenuPage::Devices) menuIndex = input.PadConnected() ? input.ConnectedPad() + 1 : 0;
}

// Ricorda quali joystick ci sono gia', per riconoscere quello appena collegato o abbinato
void Game::SnapshotPads() {
    for (int i = 0; i < Input::MAX_PADS; ++i) padsBefore[i] = IsGamepadAvailable(i);
}

int Game::NewlyAvailablePad() const {
    for (int i = 0; i < Input::MAX_PADS; ++i)
        if (IsGamepadAvailable(i) && !padsBefore[i]) return i;
    return -1;
}

void Game::ConnectNewPad(int pad) {
    input.Connect(pad);
    GoToPage(MenuPage::Devices);
    ShowToast(TextFormat("Joystick collegato: %s", GetGamepadName(pad)));
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void Game::UpdateControllerMenu() {
    // F1 o il tasto menu del joystick chiudono (Esc invece torna indietro di una pagina)
    if (input.MenuPressed() || input.PadMenuButtonPressed()) {
        CloseControllerMenu();
        return;
    }
    switch (menuPage) {
        case MenuPage::Devices:       UpdateDevicesPage(); break;
        case MenuPage::NewController: UpdateNewControllerPage(); break;
        case MenuPage::Bluetooth:     UpdateBluetoothPage(); break;
        case MenuPage::Wired:         UpdateWiredPage(); break;
    }
}

void Game::UpdateDevicesPage() {
    if (input.UpPressed()) menuIndex = Wrap(menuIndex - 1, DEVICE_ITEMS);
    if (input.DownPressed()) menuIndex = Wrap(menuIndex + 1, DEVICE_ITEMS);
    if (input.BackPressed()) {
        CloseControllerMenu();
        return;
    }
    if (!input.ConfirmPressed()) return;

    if (menuIndex == 0) {
        input.Disconnect();
        ShowToast("Controlli: tastiera e mouse");
    } else if (menuIndex <= Input::MAX_PADS) {
        int p = menuIndex - 1;
        if (IsGamepadAvailable(p)) {
            input.Connect(p);
            menuMessage.clear();
            ShowToast(TextFormat("Joystick collegato: %s", GetGamepadName(p)));
        } else {
            menuMessage = TextFormat("Nessun joystick nello slot %d: usa \"Collega un nuovo joystick\"", p + 1);
        }
    } else if (menuIndex == ITEM_NEW_PAD) {
        GoToPage(MenuPage::NewController);
    } else {
        CloseControllerMenu();
    }
}

void Game::UpdateNewControllerPage() {
    if (input.UpPressed()) pageIndex = Wrap(pageIndex - 1, METHOD_COUNT);
    if (input.DownPressed()) pageIndex = Wrap(pageIndex + 1, METHOD_COUNT);
    if (input.BackPressed()) {
        GoToPage(MenuPage::Devices);
        menuIndex = ITEM_NEW_PAD;
        return;
    }
    if (!input.ConfirmPressed()) return;

    switch (pageIndex) {
        case METHOD_BLUETOOTH:
            if (!link->PairingSupported()) {
                menuMessage = link->Message().empty() ? "La ricerca Bluetooth non e' disponibile su questo sistema"
                                                      : link->Message();
            } else if (!link->BluetoothAvailable()) {
                menuMessage = "Questo dispositivo non ha il Bluetooth (o e' spento): usa il cavo USB o una pennetta";
            } else {
                GoToPage(MenuPage::Bluetooth);
                SnapshotPads();
                link->StartScan();
            }
            break;
        case METHOD_CABLE:
        case METHOD_DONGLE:
            wiredIsDongle = pageIndex == METHOD_DONGLE;
            GoToPage(MenuPage::Wired);
            SnapshotPads();
            break;
        default:
            GoToPage(MenuPage::Devices);
            menuIndex = ITEM_NEW_PAD;
            break;
    }
}

void Game::UpdateBluetoothPage() {
    // Appena Windows rende disponibile il controller abbinato, il gioco lo collega da solo
    int pad = NewlyAvailablePad();
    if (pad >= 0) {
        ConnectNewPad(pad);
        return;
    }
    if (input.BackPressed()) {
        GoToPage(MenuPage::NewController);
        pageIndex = METHOD_BLUETOOTH;
        return;
    }

    int count = (int)link->Found().size();
    if (count > 0) {
        if (input.UpPressed()) pageIndex = Wrap(pageIndex - 1, count);
        if (input.DownPressed()) pageIndex = Wrap(pageIndex + 1, count);
        pageIndex = std::clamp(pageIndex, 0, count - 1);
    }
    if (!input.ConfirmPressed()) return;

    switch (link->Status()) {
        case LinkStatus::Scanning:
            if (count > 0) link->Pair(pageIndex);
            break;
        case LinkStatus::Failed:
        case LinkStatus::Idle:
            link->StartScan(); // riprova
            break;
        default:
            break;
    }
}

void Game::UpdateWiredPage() {
    int pad = NewlyAvailablePad();
    if (pad >= 0) {
        ConnectNewPad(pad);
        return;
    }
    if (input.BackPressed() || input.ConfirmPressed()) {
        GoToPage(MenuPage::NewController);
        pageIndex = wiredIsDongle ? METHOD_DONGLE : METHOD_CABLE;
    }
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

// Pannello che mostra in tempo reale stick e tasti premuti: serve a verificare che il joystick funzioni
static void DrawPadTester(int pad, Rectangle r) {
    DrawRectangleRec(r, { 20, 18, 26, 255 });
    DrawRectangleLinesEx(r, 1, BORDER);
    DrawText("Prova joystick", (int)r.x + 16, (int)r.y + 12, 20, WHITE);
    if (pad < 0 || !IsGamepadAvailable(pad)) {
        DrawText("Seleziona un joystick disponibile", (int)r.x + 16, (int)r.y + 44, 16, GRAY);
        return;
    }

    auto drawStick = [pad](Vector2 c, int axisX, int axisY, const char* name) {
        Vector2 v{ GetGamepadAxisMovement(pad, axisX), GetGamepadAxisMovement(pad, axisY) };
        DrawCircleLines((int)c.x, (int)c.y, 50, GRAY);
        DrawCircleV({ c.x + v.x * 50, c.y + v.y * 50 }, 10, GOLD);
        DrawTextCentered(name, (int)c.x, (int)c.y + 60, 16, LIGHTGRAY);
    };
    drawStick({ r.x + 110, r.y + 120 }, GAMEPAD_AXIS_LEFT_X, GAMEPAD_AXIS_LEFT_Y, "Stick sinistro");
    drawStick({ r.x + 310, r.y + 120 }, GAMEPAD_AXIS_RIGHT_X, GAMEPAD_AXIS_RIGHT_Y, "Stick destro");

    // I nomi seguono la famiglia del joystick (es. Triangolo/Quadrato su PlayStation)
    static const int buttons[] = {
        GAMEPAD_BUTTON_RIGHT_FACE_DOWN, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, GAMEPAD_BUTTON_RIGHT_FACE_LEFT,
        GAMEPAD_BUTTON_RIGHT_FACE_UP,   GAMEPAD_BUTTON_MIDDLE_RIGHT,     GAMEPAD_BUTTON_LEFT_TRIGGER_1,
        GAMEPAD_BUTTON_RIGHT_TRIGGER_1, GAMEPAD_BUTTON_LEFT_TRIGGER_2,   GAMEPAD_BUTTON_RIGHT_TRIGGER_2,
        GAMEPAD_BUTTON_MIDDLE_LEFT,     GAMEPAD_BUTTON_LEFT_FACE_UP,     GAMEPAD_BUTTON_LEFT_FACE_DOWN,
        GAMEPAD_BUTTON_LEFT_FACE_LEFT,  GAMEPAD_BUTTON_LEFT_FACE_RIGHT,  GAMEPAD_BUTTON_MIDDLE,
    };
    Input::PadFamily family = Input::FamilyOf(pad);
    int i = 0;
    auto chip = [&](const char* name, bool down) {
        Rectangle c{ r.x + 16 + (i % 5) * 78.0f, r.y + 220 + (i / 5) * 36.0f, 70, 30 };
        int size = MeasureText(name, 16) > 64 ? 12 : 16;
        DrawRectangleRec(c, down ? GOLD : Color{ 40, 40, 50, 255 });
        DrawTextCentered(name, (int)(c.x + c.width / 2), (int)c.y + (size == 16 ? 7 : 9), size, down ? BLACK : LIGHTGRAY);
        ++i;
    };
    for (int b : buttons) chip(Input::ButtonName(family, b), IsGamepadButtonDown(pad, b));
    // Il touchpad non e' tra i tasti standard: lo legge PadRaw
    if (family == Input::PadFamily::PlayStation) chip("Touchpad", PadTouchpadDown(pad));
}

void Game::DrawControllerMenu() const {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.88f));

    const char* title = "Controller";
    const char* subtitle = "Premi START o A su un joystick per collegarlo al volo";
    const char* hint = "Su/Giu scegli  |  Invio o A conferma  |  Esc o B indietro";
    switch (menuPage) {
        case MenuPage::Devices: break;
        case MenuPage::NewController:
            title = "Collega un nuovo joystick";
            subtitle = "Come vuoi collegarlo?";
            break;
        case MenuPage::Bluetooth:
            title = "Bluetooth";
            subtitle = "Metti il controller in modalita' abbinamento, poi sceglilo dalla lista";
            hint = "Su/Giu scegli  |  Invio o A abbina (o riprova)  |  Esc o B indietro";
            break;
        case MenuPage::Wired:
            title = wiredIsDongle ? "Pennetta o ricevitore" : "Cavo USB";
            subtitle = "Il gioco riconosce il joystick appena Windows lo rileva";
            hint = "Esc o B indietro";
            break;
    }
    DrawTextCentered(title, sw / 2, 36, 40, WHITE);
    DrawTextCentered(subtitle, sw / 2, 86, 18, LIGHTGRAY);
    if (!menuMessage.empty()) DrawTextCentered(menuMessage.c_str(), sw / 2, 114, 18, ORANGE);

    switch (menuPage) {
        case MenuPage::Devices:       DrawDevicesPage(); break;
        case MenuPage::NewController: DrawNewControllerPage(); break;
        case MenuPage::Bluetooth:     DrawBluetoothPage(); break;
        case MenuPage::Wired:         DrawWiredPage(); break;
    }
    DrawTextCentered(hint, sw / 2, sh - 50, 18, Fade(WHITE, 0.7f));
}

void Game::DrawDevicesPage() const {
    const float x = 100, w = 580, h = 52;
    float y = 160;
    for (int i = 0; i < DEVICE_ITEMS; ++i, y += h + 8) {
        Rectangle r{ x, y, w, h };
        bool selected = i == menuIndex;
        if (i == 0) {
            bool inUse = !input.PadConnected();
            DrawMenuItem(r, "Tastiera e mouse", selected, true, inUse ? "IN USO" : "", GREEN);
        } else if (i <= Input::MAX_PADS) {
            int p = i - 1;
            bool available = IsGamepadAvailable(p);
            std::string label = TextFormat("Joystick %d: ", p + 1);
            label += available ? std::string(GetGamepadName(p)).substr(0, 30) : "nessuno";
            const char* status = input.ConnectedPad() == p ? "COLLEGATO" : available ? "disponibile" : "";
            DrawMenuItem(r, label.c_str(), selected, available, status, input.ConnectedPad() == p ? GREEN : SKYBLUE);
        } else if (i == ITEM_NEW_PAD) {
            DrawMenuItem(r, "+ Collega un nuovo joystick...", selected, true);
        } else {
            DrawMenuItem(r, menuReturnState == State::Paused ? "Torna al menu" : "Torna al gioco", selected, true);
        }
    }

    // Il pannello di prova mostra il joystick selezionato, o quello collegato
    int testPad = (menuIndex >= 1 && menuIndex <= Input::MAX_PADS) ? menuIndex - 1 : input.ConnectedPad();
    DrawPadTester(testPad, { 720, 160, 420, 376 });
}

void Game::DrawNewControllerPage() const {
    const char* btStatus = "pronto";
    Color btColor = GREEN;
    if (!link->PairingSupported()) { btStatus = "non supportato"; btColor = GRAY; }
    else if (!link->BluetoothAvailable()) { btStatus = "Bluetooth assente"; btColor = ORANGE; }

    struct Method { const char* label; const char* desc; };
    const Method methods[METHOD_COUNT] = {
        { "Bluetooth (senza fili)", "Cerca e abbina un controller: PlayStation, Xbox, Switch Pro..." },
        { "Cavo USB", "Collega il joystick con il cavo: il gioco lo riconosce da solo" },
        { "Pennetta o ricevitore", "Ricevitore del controller (es. adattatore Xbox) o pennetta Bluetooth USB" },
        { "Indietro", nullptr },
    };
    const float x = 290, w = 700;
    float y = 160;
    for (int i = 0; i < METHOD_COUNT; ++i) {
        float h = methods[i].desc ? 70.0f : 52.0f;
        bool enabled = i != METHOD_BLUETOOTH || (link->PairingSupported() && link->BluetoothAvailable());
        DrawMenuItem({ x, y, w, h }, methods[i].label, i == pageIndex, enabled,
                     i == METHOD_BLUETOOTH ? btStatus : "", btColor, methods[i].desc);
        y += h + 10;
    }
}

void Game::DrawBluetoothPage() const {
    // Colonna sinistra: istruzioni e stato
    const int lx = 80;
    int ly = 160;
    DrawText("Modalita' abbinamento:", lx, ly, 20, WHITE);
    ly += 32;
    const char* steps[] = {
        "PlayStation: tieni premuti PS + Share/Create",
        "  finche' la luce lampeggia veloce",
        "Xbox: accendilo e tieni premuto il tasto",
        "  di abbinamento in alto (vicino all'USB)",
        "Switch Pro: tieni premuto il tasto sync",
        "  in alto, accanto alla porta USB-C",
        "Altri: guarda il manuale (\"pairing\")",
    };
    for (const char* s : steps) {
        DrawText(s, lx, ly, 16, LIGHTGRAY);
        ly += 22;
    }

    ly += 20;
    Color statusColor = SKYBLUE;
    std::string status = link->Message();
    switch (link->Status()) {
        case LinkStatus::Scanning:
        case LinkStatus::Pairing:
            status += std::string((int)(GetTime() * 2) % 4, '.');
            break;
        case LinkStatus::Paired: statusColor = GREEN; break;
        case LinkStatus::Failed:
            statusColor = ORANGE;
            status += "  (Invio o A per riprovare)";
            break;
        default: break;
    }
    DrawRectangle(lx - 10, ly - 8, 500, 64, { 20, 18, 26, 255 });
    DrawText(status.substr(0, 52).c_str(), lx, ly, 18, statusColor);
    if (status.size() > 52) DrawText(status.substr(52, 52).c_str(), lx, ly + 24, 18, statusColor);

    // Colonna destra: dispositivi trovati
    const float rx = 620, rw = 560, rh = 46;
    float ry = 160;
    DrawText("Dispositivi vicini", (int)rx, (int)ry, 20, WHITE);
    ry += 32;
    const auto& found = link->Found();
    if (found.empty()) {
        DrawText("Nessun dispositivo ancora: tieni il controller in abbinamento", (int)rx, (int)ry, 16, GRAY);
        return;
    }
    const int visible = 8;
    int first = std::clamp(pageIndex - visible + 1, 0, std::max(0, (int)found.size() - visible));
    for (int i = first; i < (int)found.size() && i < first + visible; ++i, ry += rh + 6) {
        const NearbyController& c = found[i];
        std::string label = c.name.substr(0, 32);
        const char* kind = c.looksLikeGamepad ? (c.lowEnergy ? "controller - LE" : "controller")
                                              : (c.lowEnergy ? "altro - LE" : "altro");
        DrawMenuItem({ rx, ry, rw, rh }, label.c_str(), i == pageIndex, c.looksLikeGamepad, kind,
                     c.looksLikeGamepad ? GOLD : GRAY);
    }
}

void Game::DrawWiredPage() const {
    int sw = GetScreenWidth();
    int y = 170;
    auto line = [&](const char* text, Color color, int size = 20) {
        DrawTextCentered(text, sw / 2, y, size, color);
        y += size + 14;
    };
    if (wiredIsDongle) {
        line("1. Inserisci la pennetta o il ricevitore in una porta USB", WHITE);
        line("2. Accendi il joystick e, se serve, premi il tasto di abbinamento del ricevitore", WHITE);
        line("Se e' una pennetta Bluetooth generica, dopo averla inserita usa l'opzione Bluetooth", LIGHTGRAY, 16);
    } else {
        line("1. Collega il joystick a questo dispositivo con il cavo USB", WHITE);
        line("2. Se il joystick ha un interruttore, accendilo", WHITE);
        line("Usa un cavo dati: alcuni cavi servono solo a ricaricare", LIGHTGRAY, 16);
    }

    y += 30;
    std::string waiting = "In attesa del joystick" + std::string((int)(GetTime() * 2) % 4, '.');
    line(waiting.c_str(), SKYBLUE, 28);

    // Suggerimenti utili mentre si aspetta
    y += 10;
    for (int i = 0; i < Input::MAX_PADS; ++i) {
        if (IsGamepadAvailable(i) && padsBefore[i] && input.ConnectedPad() != i) {
            line(TextFormat("C'e' gia' un joystick pronto (%s): premi A su di esso", GetGamepadName(i)), GOLD, 16);
            break;
        }
    }
    if (wiredIsDongle && link->PairingSupported() && link->BluetoothAvailable())
        line("Bluetooth rilevato: torna indietro e scegli \"Bluetooth\" per abbinare il controller", GREEN, 16);
}
