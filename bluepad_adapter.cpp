/*
 * Bluepad32 integration for esp_8_bit
 *
 * This module adds support for modern wireless gamepads, including
 * Xbox Wireless controllers, by leveraging the Bluepad32 library.  When
 * enabled, it polls connected gamepads and maps their inputs to the
 * GUI scancode values used throughout the esp_8_bit emulator.  This
 * allows users to navigate the on‑screen menus and control games using
 * a modern Bluetooth gamepad instead of a classic HID keyboard or Wii
 * remote.
 */

#include "bluepad_adapter.h"
#include <Bluepad32.h>

// gui_key is defined in gui.cpp as an extern "C" function.  Declare it
// here so we can call it from C++ without needing to include the whole
// GUI implementation.
extern "C" void gui_key(int keycode, int pressed, int mods);

// Track the previous state of each button per gamepad so we can detect
// changes and fire events only on transitions.  Bluepad32 exposes up to
// BP32_MAX_GAMEPADS concurrent controllers.
static bool lastUp[BP32_MAX_GAMEPADS]    = {false};
static bool lastDown[BP32_MAX_GAMEPADS]  = {false};
static bool lastLeft[BP32_MAX_GAMEPADS]  = {false};
static bool lastRight[BP32_MAX_GAMEPADS] = {false};
static bool lastA[BP32_MAX_GAMEPADS]     = {false};
static bool lastStart[BP32_MAX_GAMEPADS] = {false};

// Initialise Bluepad32.  This sets up the Bluetooth stack and prepares
// the library to accept new gamepad connections.  Once this function
// returns, the ESP32 will begin advertising and accept pairing requests
// from supported controllers.  See the Bluepad32 documentation for
// information about supported models and pairing procedures.
void bluepad_setup() {
    BP32.init();
}

// Helper to process a single gamepad.  Given the gamepad index and
// corresponding Bluepad32 GamepadPtr, this function reads the current
// button and axis states, compares them against the previous state, and
// invokes gui_key() with appropriate SDL scancodes when changes occur.
static void process_gamepad(int index, GamepadPtr gp) {
    // Use either the d‑pad or analogue stick for directional input.  The
    // axes return values in the range [-1,1]; treat values beyond ±0.5
    // as directional presses.  D‑pad booleans take precedence.
    bool up    = gp->dpadUp()    || (gp->axisY() < -0.5f);
    bool down  = gp->dpadDown()  || (gp->axisY() >  0.5f);
    bool left  = gp->dpadLeft()  || (gp->axisX() < -0.5f);
    bool right = gp->dpadRight() || (gp->axisX() >  0.5f);
    bool a     = gp->a();     // primary button – map to Enter
    bool start = gp->start(); // start button – map to F1 (toggle GUI)

    // Emit key events on state transitions.  The GUI expects keycode,
    // pressed flag (1=down,0=up) and a mods bitmask which we leave at 0.
    if (up != lastUp[index]) {
        gui_key(82, up ? 1 : 0, 0);    // up arrow
        lastUp[index] = up;
    }
    if (down != lastDown[index]) {
        gui_key(81, down ? 1 : 0, 0);  // down arrow
        lastDown[index] = down;
    }
    if (right != lastRight[index]) {
        gui_key(79, right ? 1 : 0, 0); // right arrow
        lastRight[index] = right;
    }
    if (left != lastLeft[index]) {
        gui_key(80, left ? 1 : 0, 0);  // left arrow
        lastLeft[index] = left;
    }
    if (a != lastA[index]) {
        gui_key(40, a ? 1 : 0, 0);     // Enter key
        lastA[index] = a;
    }
    if (start != lastStart[index]) {
        gui_key(58, start ? 1 : 0, 0); // F1 key toggles GUI
        lastStart[index] = start;
    }
}

// Poll connected gamepads and translate their inputs into GUI events.
// Should be called regularly from the main loop.  It calls BP32.update()
// internally to refresh the state of each gamepad.  After updating, it
// iterates over all possible gamepads and processes each connected one.
void bluepad_update() {
    // Update the internal state of Bluepad32.  This must be called
    // regularly to maintain Bluetooth connections and read incoming
    // reports.
    BP32.update();
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        GamepadPtr gp = BP32.getGamepad(i);
        if (gp && gp->isConnected()) {
            process_gamepad(i, gp);
        } else {
            // If the gamepad disconnects, clear any lingering pressed
            // states so that releases are sent once when reconnection
            // occurs.
            lastUp[i] = lastDown[i] = lastLeft[i] = lastRight[i] = false;
            lastA[i] = lastStart[i] = false;
        }
    }
}
