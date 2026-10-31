// Menu iniziale: il GameMaster scende dal cielo e si apposta fuori dalla finestra, poi
// Nuova partita / Scegli partita (tre salvataggi) / Impostazioni.
#include <algorithm>
#include <bit>
#include <string>
#include "Cutscene.hpp"
#include "Game.hpp"
#include "Ui.hpp"

#if defined(__EMSCRIPTEN__)
static constexpr bool WEB = true;  // nel browser niente "Esci" e niente schermo intero dal menu
#else
static constexpr bool WEB = false;
#endif

enum { MAIN_NEW, MAIN_LOAD, MAIN_SETTINGS, MAIN_QUIT, MAIN_COUNT };
enum { SET_CONTROLLER, SET_CONTROLS, SET_FULLSCREEN, SET_SENSITIVITY, SET_TIPS, SET_TITLES, SET_BACK, SET_COUNT };

static int MainCount() { return WEB ? MAIN_COUNT - 1 : MAIN_COUNT; }
static int SettingsItem(int index) { // nel browser la voce "schermo intero" non c'e'
    if (WEB && index >= SET_FULLSCREEN) return index + 1;
    return index;
}
static int SettingsCount() { return WEB ? SET_COUNT - 1 : SET_COUNT; }

void Game::OpenTitle() {
    static bool firstTime = true;
    state = State::Title;
    titleTime = firstTime ? 0 : TitleScene::MENU; // la discesa dal cielo solo all'avvio del gioco
    firstTime = false;
    titlePage = TitlePage::Main;
    titleIndex = 0;
    runActive = false;
    activeTip = -1;
    pauseShowControls = false;
    for (int i = 0; i < SAVE_SLOTS; ++i) slotInfo[i] = ReadSlotInfo(i);
    if (cursorLocked) { EnableCursor(); cursorLocked = false; }
}

// Si chiude il menu e comincia il sogno (nuova partita nello slot scelto, oppure quella appena caricata)
void Game::StartDream(bool newRun) {
    if (newRun) {
        runActive = true;
        NewRun(); // salva subito il primo piano nello slot
    }
    runActive = true;
    introNewRun = newRun;
    cutsceneTime = 0;
    state = State::Intro;
}

void Game::UpdateTitle(float dt) {
    titleTime += dt;
    if (titleTime < TitleScene::MENU) { // l'animazione si puo' saltare
        if (input.ConfirmPressed() || input.BackPressed()) titleTime = TitleScene::MENU;
        return;
    }
    if (pauseShowControls) {
        if (input.BackPressed() || input.ConfirmPressed()) pauseShowControls = false;
        return;
    }
    auto move = [&](int count) {
        if (input.UpPressed()) titleIndex = (titleIndex + count - 1) % count;
        if (input.DownPressed()) titleIndex = (titleIndex + 1) % count;
    };

    switch (titlePage) {
        case TitlePage::Main:
            move(MainCount());
            if (!input.ConfirmPressed()) return;
            if (titleIndex == MAIN_NEW || titleIndex == MAIN_LOAD) {
                slotsForNew = titleIndex == MAIN_NEW;
                titlePage = TitlePage::Slots;
                titleIndex = 0;
                // Per una nuova partita propone il primo salvataggio libero
                if (slotsForNew)
                    for (int i = SAVE_SLOTS - 1; i >= 0; --i) if (!slotInfo[i].used) titleIndex = i;
            } else if (titleIndex == MAIN_SETTINGS) {
                titlePage = TitlePage::Settings;
                titleIndex = 0;
            } else {
                quit = true;
            }
            return;
        case TitlePage::Slots: {
            move(SAVE_SLOTS + 1);
            if (input.BackPressed()) { titlePage = TitlePage::Main; titleIndex = slotsForNew ? MAIN_NEW : MAIN_LOAD; return; }
            if (!input.ConfirmPressed()) return;
            if (titleIndex == SAVE_SLOTS) { titlePage = TitlePage::Main; titleIndex = 0; return; }
            if (slotsForNew) {
                if (slotInfo[titleIndex].used) { confirmSlot = titleIndex; titlePage = TitlePage::Confirm; titleIndex = 1; return; }
                slot = titleIndex;
                StartDream(true);
            } else if (slotInfo[titleIndex].used && LoadRun(titleIndex)) {
                StartDream(false);
            } else {
                ShowToast("Questo salvataggio e' vuoto: inizia una nuova partita");
            }
            return;
        }
        case TitlePage::Confirm:
            move(2);
            if (input.BackPressed() || (input.ConfirmPressed() && titleIndex == 1)) { titlePage = TitlePage::Slots; titleIndex = confirmSlot; return; }
            if (input.ConfirmPressed()) {
                slot = confirmSlot;
                DeleteRun(slot);
                StartDream(true);
            }
            return;
        case TitlePage::Settings:
            UpdateSettings();
            return;
        case TitlePage::Titles:
            if (input.BackPressed() || input.ConfirmPressed()) { titlePage = TitlePage::Settings; titleIndex = SET_TITLES - (WEB ? 1 : 0); }
            return;
    }
}

void Game::UpdateSettings() {
    const int count = SettingsCount();
    if (input.UpPressed()) titleIndex = (titleIndex + count - 1) % count;
    if (input.DownPressed()) titleIndex = (titleIndex + 1) % count;
    const int item = SettingsItem(titleIndex);
    if (item == SET_SENSITIVITY && (input.LeftPressed() || input.RightPressed())) {
        profile.lookSensitivity += input.RightPressed() ? 0.1f : -0.1f;
        profile.lookSensitivity = std::clamp(profile.lookSensitivity, 0.4f, 2.5f);
        SaveProfile();
        return;
    }
    if (input.BackPressed()) { titlePage = TitlePage::Main; titleIndex = MAIN_SETTINGS; return; }
    if (!input.ConfirmPressed()) return;
    switch (item) {
        case SET_CONTROLLER: OpenControllerMenu(""); break;
        case SET_CONTROLS: pauseShowControls = true; break;
        case SET_FULLSCREEN: ToggleFullscreenMode(); break;
        case SET_SENSITIVITY:
            profile.lookSensitivity = profile.lookSensitivity >= 2.45f ? 0.4f : profile.lookSensitivity + 0.1f;
            SaveProfile();
            break;
        case SET_TIPS: profile.tips = !profile.tips; SaveProfile(); break;
        case SET_TITLES: titlePage = TitlePage::Titles; break;
        default: titlePage = TitlePage::Main; titleIndex = MAIN_SETTINGS; break;
    }
}

void Game::DrawSettings(Rectangle area) const {
    float y = area.y;
    for (int k = 0; k < SettingsCount(); ++k) {
        int item = SettingsItem(k);
        const char* label = "";
        std::string status;
        Color statusColor = GRAY;
        switch (item) {
            case SET_CONTROLLER: label = "Controller e joystick"; break;
            case SET_CONTROLS: label = "Comandi"; break;
            case SET_FULLSCREEN:
                label = "Schermo intero";
                status = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) ? "SI" : "NO";
                break;
            case SET_SENSITIVITY:
                label = "Sensibilita' della visuale";
                status = TextFormat("< x%.1f >", profile.lookSensitivity);
                statusColor = SKYBLUE;
                break;
            case SET_TIPS:
                label = "Consigli del GameMaster";
                status = profile.tips ? "SI" : "NO";
                statusColor = profile.tips ? GREEN : GRAY;
                break;
            case SET_TITLES:
                label = "Titoli sbloccati";
                status = TextFormat("%d / %d", std::popcount(profile.titles), VICTORY_COUNT);
                statusColor = GOLD;
                break;
            default: label = "Indietro"; break;
        }
        DrawMenuItem({ area.x, y, area.width, 48 }, label, k == titleIndex, true, status.c_str(), statusColor);
        y += 54;
    }
}

void Game::DrawTitle() const {
    DrawTitleScene(titleTime);
    if (titleTime < TitleScene::MENU) return;
    if (pauseShowControls) { DrawControlsPage(); return; }

    Rectangle area = TitleMenuArea();
    const float w = std::min(460.0f, area.width - 20), x = area.x + 10;
    Rectangle list{ x, area.y, w, area.height };
    DrawRectangleRounded({ x - 14, area.y - 16, w + 28, area.height + 10 }, 0.06f, 8, Fade({ 10, 10, 18, 255 }, 0.72f));

    switch (titlePage) {
        case TitlePage::Main: {
            static const char* labels[] = { "Nuova partita", "Scegli partita", "Impostazioni", "Esci dal gioco" };
            static const char* descs[] = { "Il GM ti aspetta: si parte dal primo piano", "Riprendi uno dei tre salvataggi",
                                           "Controller, comandi, sensibilita', titoli", nullptr };
            float y = list.y;
            for (int i = 0; i < MainCount(); ++i) {
                float h = descs[i] ? 70.0f : 52.0f;
                DrawMenuItem({ x, y, w, h }, labels[i], i == titleIndex, true, "", GRAY, descs[i]);
                y += h + 10;
            }
            if (profile.bestFloor > 0)
                DrawText(TextFormat("Record: piano %d   Vittorie: %d", profile.bestFloor, profile.victories), (int)x, (int)y + 6, 16, Fade(WHITE, 0.6f));
            break;
        }
        case TitlePage::Slots: {
            DrawText(slotsForNew ? "Dove salvo la nuova partita?" : "Quale partita riprendi?", (int)x, (int)list.y - 4, 22, WHITE);
            float y = list.y + 30;
            for (int i = 0; i < SAVE_SLOTS; ++i) {
                const SlotInfo& s = slotInfo[i];
                std::string status = s.used ? TextFormat("Piano %d", s.floor) : "vuota";
                std::string desc = s.used ? TextFormat("%s - %d:%02d di gioco, %d nemici", GetDistrict(s.district).city,
                                                       (int)s.runTime / 60, (int)s.runTime % 60, s.kills)
                                          : (slotsForNew ? "Inizia qui" : "Niente da riprendere");
                DrawMenuItem({ x, y, w, 70 }, TextFormat("Partita %d", i + 1), i == titleIndex, slotsForNew || s.used,
                             status.c_str(), s.used ? GOLD : GRAY, desc.c_str());
                y += 80;
            }
            DrawMenuItem({ x, y, w, 48 }, "Indietro", titleIndex == SAVE_SLOTS, true);
            break;
        }
        case TitlePage::Confirm: {
            DrawTextWrapped(TextFormat("La partita %d e' arrivata al piano %d. Vuoi davvero ricominciare da capo?", confirmSlot + 1,
                                       slotInfo[confirmSlot].floor), { x, list.y, w, 90 }, 22, WHITE);
            DrawMenuItem({ x, list.y + 100, w, 52 }, "Si', ricomincia da capo", titleIndex == 0, true);
            DrawMenuItem({ x, list.y + 162, w, 52 }, "No, torna indietro", titleIndex == 1, true);
            break;
        }
        case TitlePage::Settings:
            DrawText("Impostazioni", (int)x, (int)list.y - 4, 22, WHITE);
            DrawSettings({ x, list.y + 30, w, list.height - 30 });
            break;
        case TitlePage::Titles: {
            DrawText("Titoli: vinci una run completando una delle sfide del GM", (int)x, (int)list.y - 4, 16, WHITE);
            float y = list.y + 24;
            for (int i = 0; i < VICTORY_COUNT; ++i) {
                bool got = (profile.titles >> i) & 1;
                DrawText(got ? VictoryChallenge(i).prize : "???", (int)x, (int)y, 18, got ? GOLD : GRAY);
                DrawText(VictoryChallenge(i).desc, (int)x + 14, (int)y + 20, 13, Fade(WHITE, got ? 0.7f : 0.45f));
                y += 41;
            }
            break;
        }
    }
    if (!input.UsingTouch())
        DrawTextCentered("Su/Giu scegli  |  Invio o A conferma  |  Esc o B indietro", GetScreenWidth() / 2, GetScreenHeight() - 30, 16, Fade(WHITE, 0.6f));
}
