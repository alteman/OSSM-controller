#include <Arduino.h>
#include <SPIFFS.h>

#include "ard_timing.h"

#include "mywifi.h"
#include "mystepper.h"
#include "ws.h"

// Setup
// -----

void setup()
{
    /*
        Start communications
        --------------------
    */

    // start serial
    Serial.begin(SERIAL_BAUDRATE);
    Serial.println("ESP32 OSSM Firmware Start");

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS error");
        return;
    }
    mywifi_init();

    mystepper_motion_update();
    
    Serial.println("motion_update");

    ws_start_server();
}

void loop()
{
    mystepper_loop();
}
