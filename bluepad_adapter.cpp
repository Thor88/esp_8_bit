#include <Bluepad32.h>
#include "src/config.h"
#include "src/emu.h"

static ControllerPtr controllers[BP32_MAX_GAMEPADS];
static bool lastUp[BP32_MAX_GAMEPADS], lastDown[BP32_MAX_GAMEPADS];
static bool lastLeft[BP32_MAX_GAMEPADS], lastRight[BP32_MAX_GAMEPADS];
static bool lastA[BP32_MAX_GAMEPADS], lastB[BP32_MAX_GAMEPADS];
static bool lastStart[BP32_MAX_GAMEPADS], lastSelect[BP32_MAX_GAMEPADS];
static bool lastMenu[BP32_MAX_GAMEPADS], lastRefresh[BP32_MAX_GAMEPADS];

void onConnectedController(ControllerPtr ctl) {
    controllers[ctl->index()] = ctl;
}
void onDisconnectedController(ControllerPtr ctl) {
    controllers[ctl->index()] = nullptr;
}

void bluepad_setup() {
    BP32.setup(&onConnectedController, &onDisconnectedController);
}

void process_controller(int idx, ControllerPtr ctl) {
    // D-pad bits: 0x01=Up, 0x02=Down, 0x04=Right, 0x08=Left
    uint8_t dpad = ctl->dpad();
    bool up    = (dpad & 0x01);
    bool down  = (dpad & 0x02);
    bool right = (dpad & 0x04);
    bool left  = (dpad & 0x08);
    bool a     = ctl->a();
    bool b     = ctl->b();
    bool start = ctl->miscStart();
    bool select = ctl->miscSelect();
    bool menuBtn = ctl->y();
    bool refresh = ctl->x();

    // emit key events on transitions
    if (up    != lastUp[idx])    { gui_key(KEYCODE_UP,    up    ? 1 : 0, 0); lastUp[idx]    = up;    }
    if (down  != lastDown[idx])  { gui_key(KEYCODE_DOWN,  down  ? 1 : 0, 0); lastDown[idx]  = down;  }
    if (right != lastRight[idx]) { gui_key(KEYCODE_RIGHT, right ? 1 : 0, 0); lastRight[idx] = right; }
    if (left  != lastLeft[idx])  { gui_key(KEYCODE_LEFT,  left  ? 1 : 0, 0); lastLeft[idx]  = left;  }
    if (a     != lastA[idx])     { gui_key(gui_is_visible() ? KEYCODE_START : KEYCODE_A_EMU, a ? 1 : 0, 0); lastA[idx] = a; }
    if (b     != lastB[idx])     { gui_key(gui_is_visible() ? KEYCODE_MENU_TOGGLE : KEYCODE_B_EMU, b ? 1 : 0, 0); lastB[idx] = b; }
    if (start != lastStart[idx]) { gui_key(KEYCODE_START,  start? 1 : 0, 0); lastStart[idx] = start; }
    if (select!= lastSelect[idx]){ gui_key(KEYCODE_SELECT, select?1 : 0, 0); lastSelect[idx]= select;}
    if (menuBtn!= lastMenu[idx]) { gui_key(KEYCODE_MENU_TOGGLE, menuBtn?1:0, 0);  lastMenu[idx]  = menuBtn;}
    if (refresh!= lastRefresh[idx]) { gui_key(KEYCODE_REFRESH, refresh?1:0, 0);  lastRefresh[idx]  = refresh;}
}

void bluepad_update() {
    BP32.update();
    for (int i = 0; i < BP32_MAX_GAMEPADS; ++i) {
        ControllerPtr ctl = controllers[i];
        if (ctl && ctl->isConnected()) {
            process_controller(i, ctl);
        } else {
            // clear previous state on disconnect
            lastUp[i] = lastDown[i] = lastLeft[i] = lastRight[i] = false;
            lastA[i] = lastB[i] = false;
            lastStart[i] = lastSelect[i] = lastMenu[i] = lastRefresh[i] = false;
        }
    }
}

