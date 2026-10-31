#include "PadRaw.hpp"

#if defined(__ANDROID__)

bool PadTouchpadDown(int) { return false; } // su Android si leggera' dagli eventi di input del sistema

#elif defined(__EMSCRIPTEN__)

// Nel browser i joystick arrivano dalla Gamepad API, con la stessa numerazione di raylib
#include <emscripten/html5.h>

// Nella mappatura standard dei browser il touchpad PlayStation e' il tasto 17
constexpr int TOUCHPAD_BUTTON = 17;

bool PadTouchpadDown(int pad) {
    EmscriptenGamepadEvent state;
    if (pad < 0 || emscripten_get_gamepad_status(pad, &state) != EMSCRIPTEN_RESULT_SUCCESS) return false;
    return state.connected && state.numButtons > TOUCHPAD_BUTTON && state.digitalButton[TOUCHPAD_BUTTON];
}

#else

// raylib su PC usa GLFW, e numera i joystick come lui: si possono leggere direttamente
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

// Nei controller PlayStation (DualShock 4, DualSense) visti da Windows il touchpad e' il tasto 13
constexpr int TOUCHPAD_BUTTON = 13;

bool PadTouchpadDown(int pad) {
    if (pad < 0 || !glfwJoystickPresent(pad)) return false;
    int count = 0;
    const unsigned char* buttons = glfwGetJoystickButtons(pad, &count);
    return buttons && count > TOUCHPAD_BUTTON && buttons[TOUCHPAD_BUTTON] == GLFW_PRESS;
}

#endif

// ---------------------------------------------------------------------------
// Dita sullo schermo
// ---------------------------------------------------------------------------
#if defined(__EMSCRIPTEN__)

// raylib nel browser perde il conto delle dita quando se ne stacca una in mezzo alle altre:
// ascoltiamo anche noi gli eventi della pagina e teniamo l'elenco giusto
#include <emscripten/html5.h>
#include <vector>

static std::vector<RawTouch> touches;
static bool listening = false;

static EM_BOOL OnTouch(int type, const EmscriptenTouchEvent* e, void*) {
    double cssW = 1, cssH = 1;
    emscripten_get_element_css_size("#canvas", &cssW, &cssH);
    const float sx = GetScreenWidth() / (float)cssW, sy = GetScreenHeight() / (float)cssH;
    const bool ending = type == EMSCRIPTEN_EVENT_TOUCHEND || type == EMSCRIPTEN_EVENT_TOUCHCANCEL;

    std::vector<RawTouch> now;
    for (int i = 0; i < e->numTouches; ++i) {
        const EmscriptenTouchPoint& t = e->touches[i];
        if (ending && t.isChanged) continue; // questo dito si e' appena staccato
        now.push_back({ (int)t.identifier, { (float)t.targetX * sx, (float)t.targetY * sy } });
    }
    touches = now;
    return 1; // niente zoom o scorrimento della pagina
}

static void Listen() {
    if (listening) return;
    listening = true;
    emscripten_set_touchstart_callback("#canvas", nullptr, 1, OnTouch);
    emscripten_set_touchmove_callback("#canvas", nullptr, 1, OnTouch);
    emscripten_set_touchend_callback("#canvas", nullptr, 1, OnTouch);
    emscripten_set_touchcancel_callback("#canvas", nullptr, 1, OnTouch);
}

int RawTouchCount() {
    Listen();
    return (int)touches.size();
}

RawTouch RawTouchAt(int i) { return touches[i]; }

#else

// Altrove (Android, schermi touch su PC) basta raylib
int RawTouchCount() { return GetTouchPointCount(); }
RawTouch RawTouchAt(int i) { return { GetTouchPointId(i), GetTouchPosition(i) }; }

#endif
