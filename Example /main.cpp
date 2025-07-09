#include <Arduino.h>

//The timeout has been set very tightely for "generic" scales. The Bookoo Themis Mini handles 500ms. If you have issues with your scale giving a timeout use, increase before you call the header.
//#define MAX_PACKET_PERIOD_GENERIC_MS 1000
#include "BLEScale.h"

//true doesnt work...
#define DEBUG true

bool scaleConnected = false;
unsigned long lastTare = 0;

BLEScale scale(DEBUG);

//AcaiaArduinoBLE scale(DEBUG);
uint8_t goalWeight = 100;      // Goal Weight to be read from EEPROM

void setup() {
  delay(2000);
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("Scale Interface test");
  NimBLEDevice::init("GbWService");
}

void loop() {
  if(scale.manage(true, true) == true) { 
    Serial.println("connected to name: " + scale.connected_name);
    Serial.println("connected to addr: " + scale.connected_mac);
  }

  // always call newWeightAvailable to actually receive the datapoint from the scale,
  // otherwise getWeight() will return stale data
  if (scale.newWeightAvailable()) {
    Serial.println(scale.getWeight());
  }

  // To test ; tare the scale every 10 seconds
  // This is not necessary, but useful for testing purposes
  if(lastTare + 10000 < millis()) { 
    lastTare = millis();
    //there is a limit to the amount of commands you can send within a timeframe. The library does not handle this. YMMV 
    //note that "tare" temporary locks up the BOOKOO weight untill its stabilized. 
    //Acaia tares almost instantly, even when not stable. Note this may cause the tare not to be zeroed properly when the weight is not stable.
    scale.tare();
    delay(50);
    scale.stopTimer();
    delay(50);
    scale.resetTimer();
    delay(50);
    scale.startTimer();

  }
}