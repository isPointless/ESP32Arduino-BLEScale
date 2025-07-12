A library to connect and use BLE scales like the Acaia Lunar, Bookoo Themis mini on ESP32 Arduino.

This library is based on previous work by tatemazer (AcaiaArduinoBLE) but uses a modified version of NimBLE-Arduino instead of ArduinoBLE, which is much less prone to crashing the ESP32.

Bookoo Themis mini is performing excellent with frequent weight updates. Acaia Lunar too, but with less updates.

The "NEW" type Acaia (My Pearl S is "NEW" Type) aren't yet working with weight updates. Feel free to debug this. Something is not working regarding Notify.
The "OLD" type Acaia (My Lunar 2021+ from 2024 w USB C is this) is working flawless. 

Other scale types have NOT been tested. These might differ regarding services (these are not discovered but manually put in) & regarding writeWithoutResponse from the Bookoo (only "generic" scale currently tested). Felicita Arc will follow soon.

An example sketch is added. 

