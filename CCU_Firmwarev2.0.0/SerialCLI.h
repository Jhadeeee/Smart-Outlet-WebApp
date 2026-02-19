/*
 * SerialCLI.h
 * ------------
 * Debug serial interface for controlling Smart Outlets
 * via the Serial Monitor. Provides a menu-driven CLI with
 * single-key commands, two-step input, device selection,
 * AT passthrough, and raw hex mode.
 *
 * This is an optional debug tool — can be removed for production.
 */

#ifndef SERIAL_CLI_H
#define SERIAL_CLI_H

#include <Arduino.h>
#include "OutletManager.h"

class SerialCLI {
public:
    // Constructor takes a reference to the OutletManager
    SerialCLI(OutletManager& manager);

    // Print help menu and "Listening" message
    void begin();

    // Must be called in loop() — reads Serial input, dispatches commands
    void update();

    // Print the formatted help menu
    void printHelp();

private:
    OutletManager& _manager;

    // Two-step input state (Features #16-19)
    bool    _waitingForData;
    uint8_t _pendingCmd;

    // Handle a single-line input (Features #15, #20-23, #25, #27-28)
    void _handleInput(const String& input);

    // Handle the second step of a two-step command (Features #16-18)
    void _handleDataInput(const String& input);
};

#endif // SERIAL_CLI_H
