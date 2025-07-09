#pragma once

#include "Arduino.h"
#include "NimBLEDevice.h"

#define SERVICE_GENERIC        "0ffe"
#define SERVICE_OLD            "1820"
#define WRITE_CHAR_OLD_VERSION "2a80"
#define READ_CHAR_OLD_VERSION  "2a80"
#define WRITE_CHAR_NEW_VERSION "49535343-8841-43f4-a8d4-ecbe34729bb3"
#define READ_CHAR_NEW_VERSION  "49535343-1e4d-4bd9-ba61-23c647249616"
#define WRITE_CHAR_GENERIC     "ff12"
#define READ_CHAR_GENERIC      "ff11"
#define HEARTBEAT_PERIOD_MS     2750
#define MAX_PACKET_PERIOD_GENERIC_MS 500 
#define MAX_PACKET_PERIOD_ACAIA_MS 2000

enum scale_type{
    UNKNOWN = -1,
    OLD,    // Lunar (pre-2021)
    NEW,    // Lunar (2021), Pyxis
    GENERIC // Felicita Arc, etc
};

class BLEScale {
public:
    BLEScale(bool debug);
    bool manage(bool connect=true, bool maintain=true, String mac = "");

    String connected_mac;
    String connected_name;

    bool isConnected();
    bool _isConnecting;
    void disconnect();

    bool tare();
    bool startTimer();
    bool stopTimer();
    bool resetTimer();
    bool heartbeat();

    bool heartbeatRequired();

    bool newWeightAvailable();
    float getWeight();
    
private:
    NimBLERemoteCharacteristic* _pChrREAD;
    NimBLERemoteCharacteristic* _pChrWRITE;
    NimBLEClient* _pClient = nullptr;

    static BLEScale* instance;
    static void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
    bool connectToServer();
    bool isScaleName(const std::string& name);
    String connectMac;

    volatile float _currentWeight;
    volatile bool _connected;

    unsigned long _lastPacket;
    unsigned long _packetPeriod;
    unsigned long _lastHeartBeat;
    bool _newWeightPacket;
    

    static const NimBLEAdvertisedDevice* advDevice;
    static bool doConnect;
    static uint32_t scanTimeMs;

    scale_type          _type;
    bool                _debug; 
    bool                _isScanning;


    class ClientCallbacks : public NimBLEClientCallbacks {
        BLEScale* _parent;
    public:
        ClientCallbacks(BLEScale* parent) : _parent(parent) {}
        void onConnect(NimBLEClient* pClient) override {
            if(instance->_debug) Serial.printf("Connected\n");
            instance->_connected = true;
        }
        void onDisconnect(NimBLEClient* pClient, int reason) override {
            if(instance->_debug) Serial.printf("%s Disconnected, reason = %d - Starting scan\n", pClient->getPeerAddress().toString().c_str(), reason);
            instance->_connected = false;
        }


    };

    class ScanCallbacks : public NimBLEScanCallbacks {
        BLEScale* _parent;
    public:
        ScanCallbacks(BLEScale* parent) : _parent(parent) {}
        void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
            if(instance->_debug) Serial.printf("Advertised Device found: %s\n", advertisedDevice->toString().c_str());
            std::string devName = advertisedDevice->getName();
            std::string devAddr = advertisedDevice->getAddress();
            if(instance->connectMac == "") {
                if (_parent->isScaleName(devName)) {
                    if(instance->_debug) Serial.printf("Found scale by name: %s\n", devName.c_str());
                    _parent->_isConnecting = true;
                    NimBLEDevice::getScan()->stop();
                    // Set in parent
                    _parent->advDevice = advertisedDevice;
                    _parent->doConnect = true;
                    
                }
            } else { 
                if (devAddr == instance->connectMac.c_str()) { 
                    if(instance->_debug) Serial.printf("Found scale by addr: %s\n", devAddr.c_str());
                    _parent->_isConnecting = true;
                    NimBLEDevice::getScan()->stop();
                    // Set in parent
                    _parent->advDevice = advertisedDevice;
                    _parent->doConnect = true;
                }
            }
        }
        void onScanEnd(const NimBLEScanResults& results, int reason) override {
            if(instance->_debug) Serial.printf("Scan Ended, reason: %d, device count: %d; Restarting scan\n", reason, results.getCount());
        }
    };

    ClientCallbacks clientCallbacks;
    ScanCallbacks scanCallbacks;

};