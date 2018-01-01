/**
 * An example Qth-based client.
 *
 * Blinks the onboard LED at a rate specified via a Qth property
 * (blinky/period). Also exposes a Qth event which allows the LED to be
 * manually toggled (blinky/toggle).
 *
 * This program is designed for an ESP8266 which connects via WiFi but could be
 * adapted as required.
 */

#include <stdlib.h>
#include <ESP8266WiFi.h>

#include "Qth.h"


// Update these with values suitable for your network.
const char* wifiSsid = "Cubit";
const char* wifiPassword = "yes pwnt";

const char* qthServer = "192.168.1.1";
const char* qthClientId = "esp8266-led-blinker";

// Define the Qth client (specifying that it use the WiFi connection for comms)
WiFiClient wifiClient;
Qth::QthClient qth(
  qthServer,
  wifiClient,
  qthClientId,  /* Unique client name */
  "A blinking LED on an ESP8266." /* Client description */);

// Defined later. A callback when the toggle event is received.
void onToggleEvent(const char *topic, const char *json);

// Define our two Qth values: an event and a property.
//
// A StoredProperty is a convenience API for defining properties which takes
// care of setting an initial value in Qth and retaining a copy of the last
// value received from Qth for use at a later time.
Qth::StoredProperty period("blinky/period", "3000", "Blinking toggle interval in ms.");
// Our event will simply call a callback function. (The 'false' argument
// specifies that this is a many-to-one Event rather than one-to-many Event.)
Qth::Event toggle("blinky/toggle", onToggleEvent, "Toggle the LED, now!", false);

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifiSsid);

  WiFi.begin(wifiSsid, wifiPassword);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
  Serial.begin(9600);
  
  // Register the Qth property and event (or more accurately in this case,
  // ensure they are registered once we finally connect.
  qth.registerProperty(&period);
  qth.registerEvent(&toggle);
  
  // ... and watch them both (otherwise we won't receive any values or events).
  qth.watchProperty(&period);
  qth.watchEvent(&toggle);
  
  setup_wifi();
}

void onToggleEvent(const char *topic, const char *json) {
  digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));
}

void loop() {
  // Run the Qth mainloop (which also handles automatic
  // reconnection/re-registration).
  qth.loop();
  
  // Blink the LED at the user defined rate
  static unsigned long last_toggle = 0;
  unsigned long now = millis();
  if (now - last_toggle > atol(period.get())) {
    digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));
    
    last_toggle = now;
  }
}
