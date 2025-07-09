#include <Arduino.h>
#include "BLEScale.h"

byte IDENTIFY[20]               = { 0xef, 0xdd, 0x0b, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x9a, 0x6d };
byte HEARTBEAT[7]               = { 0xef, 0xdd, 0x00, 0x02, 0x00, 0x02, 0x00 };
byte NOTIFICATION_REQUEST[14]   = { 0xef, 0xdd, 0x0c, 0x09, 0x00, 0x01, 0x01, 0x02, 0x02, 0x05, 0x03, 0x04, 0x15, 0x06 };
byte START_TIMER[7]             = { 0xef, 0xdd, 0x0d, 0x00, 0x00, 0x00, 0x00 };
byte STOP_TIMER[7]              = { 0xef, 0xdd, 0x0d, 0x00, 0x02, 0x00, 0x02 };
byte RESET_TIMER[7]             = { 0xef, 0xdd, 0x0d, 0x00, 0x01, 0x00, 0x01 };
byte TARE_ACAIA[6]              = { 0xef, 0xdd, 0x04, 0x00, 0x00, 0x00 };
byte TARE_GENERIC[6]            = { 0x03, 0x0a, 0x01, 0x00, 0x00, 0x08 };
byte START_TIMER_GENERIC[6]     = { 0x03, 0x0a, 0x04, 0x00, 0x00, 0x0a };
byte STOP_TIMER_GENERIC[6]      = { 0x03, 0x0a, 0x05, 0x00, 0x00, 0x0d };
byte RESET_TIMER_GENERIC[6]     = { 0x03, 0x0a, 0x06, 0x00, 0x00, 0x0c };

const NimBLEAdvertisedDevice* BLEScale::advDevice = nullptr;
bool BLEScale::doConnect = false;
uint32_t BLEScale::scanTimeMs = 2000;
BLEScale* BLEScale::instance = nullptr;


BLEScale::BLEScale(bool debug)
    : clientCallbacks(this),
      scanCallbacks(this)
    { 
    instance = this;
    NimBLEAdvertisedDevice* advDevice;
    _newWeightPacket = false;
    _debug = debug;
    _currentWeight = 0;
    _connected = false;
    _packetPeriod = 0;
    _isScanning = false;
    doConnect = false;
    scanTimeMs = 2000;
    connectMac = "";
    connected_mac = "";
    connected_name = "";
}

//This will be called continuously to manage the connection.
bool BLEScale::manage(bool connect, bool maintain, String mac) { 
    static unsigned long lastCall;

    if(lastCall + 200 > millis()) return false;
    lastCall = millis();

    NimBLEScan* pScan = NimBLEDevice::getScan();
    /**
     * Active scan will gather scan response data from advertisers
     *  but will use more energy from both devices
     */
    pScan->setActiveScan(false);
    pScan->setScanCallbacks(&scanCallbacks, false);

    if(_pClient != nullptr) {
        if(!_pClient->isConnected()) { 
            _connected = false;
        } else _connected = true;
    }
    
    if(mac != "") { 
        connectMac = mac;
    }

    if(!pScan->isScanning() && _connected == false && _isConnecting == false && connect == true) { 
        pScan->start(scanTimeMs);
        if(instance->_debug) Serial.println("Scanning for peripherals");
    }
    
    if(maintain == true) {
        if(heartbeatRequired()) { 
            heartbeat();
            Serial.println("Beat!");
        }
    }

    if(maintain == false && connect == false) disconnect();

    bool response = false;

    if (doConnect) {
        doConnect = false;
        /** Found a device we want to connect to, do it now */
        if (connectToServer()) {
            if(instance->_debug) Serial.println("Success! we should now be getting notifications, scanning for more!");
            _connected = true;
            response = true;
        } else {
            if(instance->_debug) Serial.println("Failed to connect");
            response = false;
            _connected = false;
        }
    }
    return response;
}
    
/** Notification / Indication receiving handler callback */
void BLEScale::notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if(instance->_debug) {
        std::string str  = (isNotify == true) ? "Notification" : "Indication";
        str             += " from ";
        str             += pRemoteCharacteristic->getClient()->getPeerAddress().toString();
        str             += ": Service = " + pRemoteCharacteristic->getRemoteService()->getUUID().toString();
        str             += ", Characteristic = " + pRemoteCharacteristic->getUUID().toString();
        str             += ", Value = " + std::string((char*)pData, length);
        Serial.printf("%s\n", str.c_str());
    }

    instance->_connected = true;
    static const float pow10[] = {1, 10, 100, 1000, 10000, 100000}; // precomputed powers

    //GENERIC
    if (instance->_type == GENERIC && length == 20) {
        int32_t raw = (pData[7] << 16) | (pData[8] << 8) | pData[9];
        if (pData[6] == 45) {
            raw = -raw;
        }
        instance->_currentWeight = raw / 100.0f;
        instance->_newWeightPacket = true;
        if(instance->_debug) Serial.println(instance->_currentWeight);
    }
    //OLD
    if (instance->_type == OLD && (length == 10 || length == 14)) {
        uint16_t rawWeight = ((pData[3] & 0xff) << 8) | (pData[2] & 0xff);
        byte scaleIndex = pData[6];
        float scale = (scaleIndex < sizeof(pow10) / sizeof(pow10[0])) ? pow10[scaleIndex] : 1.0;
        instance->_currentWeight = rawWeight / scale * ((pData[7] & 0x02) ? -1 : 1);
        instance->_newWeightPacket = true;
        if(instance->_debug) Serial.println(instance->_currentWeight);
    }
    //NEW (DOES NOT CONNECT??!)
    if (instance->_type == NEW && (length == 13 || length == 17) && pData[4] == 0x05) {
        uint16_t rawWeight = ((pData[6] & 0xff) << 8) | (pData[5] & 0xff);
        byte scaleIndex = pData[9];
        float scale = (scaleIndex < sizeof(pow10) / sizeof(pow10[0])) ? pow10[scaleIndex] : 1.0;
        instance->_currentWeight = rawWeight / scale * ((pData[10] & 0x02) ? -1 : 1);
        instance->_newWeightPacket = true;
        if(instance->_debug) Serial.println(instance->_currentWeight);
    }
    
}

/** Handles the provisioning of clients and connects / interfaces with the server */
bool BLEScale::connectToServer() {
    NimBLEClient* pClient = nullptr;

    /** Check if we have a client we should reuse first **/
    if (NimBLEDevice::getCreatedClientCount()) {
        /**
         *  Special case when we already know this device, we send false as the
         *  second argument in connect() to prevent refreshing the service database.
         *  This saves considerable time and power.
         */
        pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
        if (pClient) {
            if (!pClient->connect(advDevice, false)) {
                if(instance->_debug) Serial.println(instance->_currentWeight); Serial.printf("Reconnect failed\n");
                return false;
            }
            if(instance->_debug) Serial.println(instance->_currentWeight); Serial.printf("Reconnected client\n");
        } else {
            /**
             *  We don't already have a client that knows this device,
             *  check for a client that is disconnected that we can use.
             */
            pClient = NimBLEDevice::getDisconnectedClient();
        }
    }

    /** No client to reuse? Create a new one. */
    if (!pClient) {
        if (NimBLEDevice::getCreatedClientCount() >= 3) {
            if(instance->_debug) Serial.println(instance->_currentWeight); Serial.printf("Max clients reached - no more connections available\n");
            return false;
        }

        pClient = NimBLEDevice::createClient();
        

        if(instance->_debug) Serial.printf("New client created\n");

        pClient->setClientCallbacks(&clientCallbacks, false);
        /**
         *  Set initial connection parameters:
         *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
         *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
         *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 150 * 10ms = 1500ms timeout
         */
        pClient->setConnectionParams(12, 24, 0, 400);

        /** Set how long we are willing to wait for the connection to complete (milliseconds), default is 30000. */
        pClient->setConnectTimeout(500);

        if(instance->_debug) Serial.printf("Trying to connect. Address type: %d\n", advDevice->getAddressType());

        delay(1000);
        if (!pClient->connect(advDevice, advDevice->getAddressType())) {
            if(instance->_debug) Serial.printf("Failed to connect (address type issue)\n");
            if(!pClient->connect(advDevice, !advDevice->getAddressType())) { 
            NimBLEDevice::deleteClient(pClient);
            return false;
            }
        }
        _connected = true;

        // for (auto &service : pClient->getServices()) {
        //     Serial.printf("Found Service: %s\n", service->getUUID().toString().c_str());
        // }
    }

    if (!pClient->isConnected()) {
        if (!pClient->connect(advDevice)) {
            if(instance->_debug) Serial.printf("Failed to connect\n");
            return false;
        }
    } else _pClient = pClient; // save client for later use in member var

    if(instance->_debug) Serial.printf("Connected to: %s RSSI: %d\n", pClient->getPeerAddress().toString().c_str(), pClient->getRssi());
    
    if(instance->_debug) Serial.println("Getting services:");

    NimBLERemoteService*        pSvc = nullptr;
    _pChrREAD = nullptr;
    _pChrWRITE = nullptr;

    //First try GENERIC
    pSvc = pClient->getService(SERVICE_GENERIC);
    if (pSvc) {
        if(instance->_debug) Serial.printf("Found scale type GENERIC");
        _type = GENERIC;
        _pChrREAD = pSvc->getCharacteristic(READ_CHAR_GENERIC);
        _pChrWRITE = pSvc->getCharacteristic(WRITE_CHAR_GENERIC);
    }

    //Try OLD
    pSvc = pClient->getService(SERVICE_OLD);
    if (pSvc) {
        if(instance->_debug) Serial.printf("Found scale type OLD");
        _type = OLD;
        _pChrREAD = pSvc->getCharacteristic(READ_CHAR_OLD_VERSION);
        _pChrWRITE = pSvc->getCharacteristic(WRITE_CHAR_OLD_VERSION);
    }

    //Try NEW (NO IDEA what service?!)

    if (_pChrREAD) {
        if (_pChrREAD->canRead()) {
            if(instance->_debug) Serial.printf("%s Value: %s\n", _pChrREAD->getUUID().toString().c_str(), _pChrREAD->readValue().c_str());
            if(instance->_debug) Serial.println("Succes pChrREAD -> canRead()");
        }

        if (_pChrREAD->canWrite()) {
            if(instance->_debug) Serial.println("Succes pChrREAD -> canWrite()");
        }

        if (_pChrREAD->canNotify()) {
            if (!_pChrREAD->subscribe(true, notifyCB)) {
                pClient->disconnect();
                _connected = false;
                _isConnecting = false;
                return false;
            }
        } else if (_pChrREAD->canIndicate()) {
            /** Send false as first argument to subscribe to indications instead of notifications */
            if (!_pChrREAD->subscribe(false, notifyCB)) {
                pClient->disconnect();
                _connected = false;
                _isConnecting = false;
                return false;
            }
        }
    } else {
        if(instance->_debug) Serial.printf("pChr = nullptr");
    }


    if (!_pChrREAD || !_pChrWRITE) {
        if(instance->_debug) Serial.println("Could not find required characteristics!");
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        _connected = false;
        _isConnecting = false;
        return false;
    }

    //Handshake
    bool ok = true;
    if (_type == OLD || _type == NEW) {
        if (!_pChrWRITE->writeValue(IDENTIFY, 20, false)) ok = false;
    }
    if (!_pChrWRITE->writeValue(NOTIFICATION_REQUEST, 14, false)) ok = false;

    if (!ok) {
        if(instance->_debug) Serial.println("Handshake failed!");
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        _connected = false;
        _isConnecting = false;
        return false;
    }

    _isConnecting = false;
    instance->connected_name = advDevice->getName().c_str();
    instance->connected_mac = advDevice->getAddress().toString().c_str();

    if(instance->_debug) Serial.println("Connected and handshake complete!");
    return true;
}

bool BLEScale::isScaleName(const std::string& name) {
    if (name.length() < 5) return false;
    std::string nameShort = name.substr(0, 5);
    return nameShort == "CINCO" ||
           nameShort == "ACAIA" ||
           nameShort == "PYXIS" ||
           nameShort == "LUNAR" ||
           nameShort == "PEARL" ||
           nameShort == "PROCH" ||
           nameShort == "BOOKO";
}


bool BLEScale::isConnected() { 
    return _connected;
}

void BLEScale::disconnect() { 
    NimBLEScan* pScan = NimBLEDevice::getScan();
    if(_pClient == nullptr) return;
    else {
        pScan->stop();
        _pClient->disconnect();
        NimBLEDevice::deleteClient(_pClient);
        _connected = false;
        _isConnecting = false;
    }
}

bool BLEScale::tare() { 
    if(_connected == false) return false;
    if(_pClient == nullptr) return false;
    else {
        if (!_pChrWRITE || !_pChrWRITE->canWrite()) {
            if(instance->_debug) Serial.println("WRITE characteristic is invalid!");
            _connected = false;
            return false;
        }

        if (_pChrWRITE->writeValue((_type == GENERIC ? TARE_GENERIC : TARE_ACAIA), 6)){
            if(instance->_debug) Serial.println("tare write successful");
            return true;
        } else {
            _connected = false;
            if(instance->_debug) Serial.println("tare write failed");
            return false;
        }
    }
    return false;
}

bool BLEScale::startTimer() { 
    if(_connected == false) return false;
    if(_pClient == nullptr) return false;
    else {
        if (!_pChrWRITE || !_pChrWRITE->canWrite()) {
            if(instance->_debug) Serial.println("WRITE characteristic is invalid or not writable!");
            _connected = false;
            return false;
        }

        if(_pChrWRITE->writeValue((_type == GENERIC ? START_TIMER_GENERIC : START_TIMER),
                            (_type == GENERIC ? 6                   : 7))){
            if(instance->_debug) Serial.println("start timer write successful");
            return true;
        }else{
            _connected = false;
            if(instance->_debug) Serial.println("start timer write failed");
            return false;
        }
    }
    return false;
}

bool BLEScale::stopTimer() { 
    if(_connected == false) return false;
    if(_pClient == nullptr) return false;
    else {
        if (!_pChrWRITE || !_pChrWRITE->canWrite()) {
            if(instance->_debug) Serial.println("WRITE characteristic is invalid or not writable!");
            _connected = false;
            return false;
        }


        if(_pChrWRITE->writeValue((_type == GENERIC ? STOP_TIMER_GENERIC : STOP_TIMER),
                            (_type == GENERIC ? 6                  : 7 ))){
            if(instance->_debug) Serial.println("stop timer write successful");
            return true;
        }else{
            _connected = false;
            if(instance->_debug) Serial.println("stop timer write failed");
            return false;
        }
    }
    return false;
}

bool BLEScale::resetTimer() { 
     if(_connected == false) return false;
     if(_pClient == nullptr) return false;
     else {
        if (!_pChrWRITE || !_pChrWRITE->canWrite()) {
            if(instance->_debug) Serial.println("WRITE characteristic is invalid or not writable!");
            _connected = false;
            return false;
        }

        if(_pChrWRITE->writeValue((_type == GENERIC ? RESET_TIMER_GENERIC : RESET_TIMER),
                            (_type == GENERIC ? 6                  : 7 ))){
            if(instance->_debug) Serial.println("reset timer write successful");
            return true;
        }else{
            _connected = false;
            if(instance->_debug) Serial.println("reset timer write failed");
            return false;
        }
    }
    return false;
}

bool BLEScale::newWeightAvailable() { 
    if(_newWeightPacket == true && _connected == true) { 
        return true;
    } 
    return false;
}

float BLEScale::getWeight() { 
    _newWeightPacket = false;
    if(_connected == false) return 0;
    return _currentWeight;
}

bool BLEScale::heartbeatRequired() { 
    if(_type == OLD || _type == NEW){
        return (millis() - _lastHeartBeat) > HEARTBEAT_PERIOD_MS;
    }else{
        return 0;
    }
}

bool BLEScale::heartbeat() { 
    if(_connected == false) return false;
    if(_pClient == nullptr) return false;

    _lastHeartBeat = millis();

    if (!_pChrWRITE || _pChrWRITE->canWrite()) {
        if(instance->_debug) Serial.println("WRITE characteristic is invalid or not writable!");
        _connected = false;
        return false;
    }

    if(_pChrWRITE->writeValue(HEARTBEAT, 7)){
        return true;
    }
}