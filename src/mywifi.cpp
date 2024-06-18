#include "mywifi.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WifiManager.h>


bool mywifi_init() {

	WiFiManager mgr;
	Serial.print("Connecting to WiFi ..\n");
	if (!mgr.autoConnect()) {
        Serial.println("Failed to connect, rebooting in 5 sec");
		sleep(5);
        ESP.restart();
		for(;;);
    }
	Serial.println(WiFi.localIP());

    while(!MDNS.begin("cock")) {
        Serial.println("Starting mDNS...");
        delay(1000);
    }
    Serial.println("mdns/1");
    MDNS.addService("http","tcp",80);
    Serial.println("mdns/2");
    return true;
}