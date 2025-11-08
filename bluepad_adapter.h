// Adapter to integrate Bluepad32 gamepad support into the esp_8_bit project.
//
// This header declares two functions that must be called from the Arduino
// sketch: bluepad_setup() to initialise the Bluepad32 stack, and
// bluepad_update() to poll connected gamepads and translate their inputs
// into GUI key events understood by the emulator.

#pragma once

// Initialise the Bluepad32 subsystem.  Should be called once from setup().
void bluepad_setup();

// Poll connected gamepads and generate GUI key events.  Call this from the
// main loop alongside hid_update().
void bluepad_update();
