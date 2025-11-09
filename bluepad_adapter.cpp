#include <Bluepad32.h>
#include "src/emu.h"

static ControllerPtr controllers[BP32_MAX_GAMEPADS];
static bool lastUp[BP32_MAX_GAMEPADS], lastDown[BP32_MAX_GAMEPADS];
static bool lastLeft[BP32_MAX_GAMEPADS], lastRight[BP32_MAX_GAMEPADS];
static bool lastA[BP32_MAX_GAMEPADS], lastB[BP32_MAX_GAMEPADS];
static bool lastStart[BP32_MAX_GAMEPADS], lastSelect[BP32_MAX_GAMEPADS];
static bool lastMenu[BP32_MAX_GAMEPADS];

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
    // D‑pad bits: 0x01=Up, 0x02=Down, 0x04=Right, 0x08=Left:contentReference[oaicite:1]{index=1}.
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

    // emit key events on transitions
    if (up    != lastUp[idx])    { gui_key(82, up    ? 1 : 0, 0); lastUp[idx]    = up;    }
    if (down  != lastDown[idx])  { gui_key(81, down  ? 1 : 0, 0); lastDown[idx]  = down;  }
    if (right != lastRight[idx]) { gui_key(79, right ? 1 : 0, 0); lastRight[idx] = right; }
    if (left  != lastLeft[idx])  { gui_key(80, left  ? 1 : 0, 0); lastLeft[idx]  = left;  }
    if (a     != lastA[idx])     { gui_key(gui_is_visible() ? 40 : 225, a ? 1 : 0, 0); lastA[idx] = a; }
    if (b     != lastB[idx])     { gui_key(gui_is_visible() ? 58 : 226, b ? 1 : 0, 0); lastB[idx] = b; }
    if (start != lastStart[idx]) { gui_key(40,  start? 1 : 0, 0); lastStart[idx] = start; }
    if (select!= lastSelect[idx]){ gui_key(43,  select?1 : 0, 0); lastSelect[idx]= select;}
    if (menuBtn!= lastMenu[idx]) { gui_key(58,  menuBtn?1:0, 0);  lastMenu[idx]  = menuBtn;}
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
            lastStart[i] = lastSelect[i] = lastMenu[i] = false;
        }
    }
}
