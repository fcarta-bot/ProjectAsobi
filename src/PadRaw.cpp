#include "PadRaw.hpp"

#if defined(__ANDROID__)

bool PadTouchpadDown(int) { return false; } // su Android si leggera' dagli eventi di input del sistema

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
