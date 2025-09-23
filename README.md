## A library to connect and use BLE scales like the Acaia Lunar, Bookoo Themis mini on ESP32 Arduino.

This library is based on previous work by tatemazer (AcaiaArduinoBLE) but uses a modified version of NimBLE-Arduino instead of ArduinoBLE, which is much less prone to crashing the ESP32.

Bookoo Themis mini is performing excellent with frequent weight updates. Acaia Lunar too, but with less updates.

The "NEW" type Acaia (My Pearl S is "NEW" Type) is working properly, however there are still some issues with the checksum. It is currently partly bypassed :(
The "OLD" type Acaia (My Lunar 2021+ from 2024 w USB C is this) is working flawless. 
Felicita ARC is working excellent with frequent weight updates like Bookoo

Unfortunately the Acaia & Felicita scales do not support setting the timer. I do not know the instructions for this, feel free to find out.

An example sketch is added. 

#### The whole NimBLE library is added, because it has been modified to skip MTU Exchange: 

In NimBLEClient.cpp line 912 is added:
```
bool NimBLEClient::exchangeMTU() {
    int rc = ble_gattc_exchange_mtu(m_connHandle, NimBLEClient::exchangeMTUCb, this);
    if(rc == 2) rc = 0; // << ADDED >>
    if (rc != 0) {
        NIMBLE_LOGE(LOG_TAG, "MTU exchange error; rc=%d %s", rc, NimBLEUtils::returnCodeToString(rc));
        m_lastErr = rc;
        return false;
    }

    return true;
} // exchangeMTU
```
