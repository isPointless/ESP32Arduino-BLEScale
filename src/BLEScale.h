#pragma once
#include <Arduino.h>
#include "NimBLEDevice.h"

enum class ScaleType : uint8_t {
    UNKNOWN,
    BOOKOO,
    ACAIA_NEW,
    ACAIA_OLD,
    FELIC
};

class BLEScale {
public:
    BLEScale(bool debug = false);

    // Main loop handler
    bool manage(bool connect, bool maintain, String mac = "");
    void disconnect();

    // Control
    bool tare();
    bool startTimer();
    bool resetTimer();
    bool stopTimer();

    // Data access
    bool newWeightAvailable();
    float getWeight();              // grams
    unsigned long getTimer();       // seconds
    int getBatteryLevel();          // %

    // Connection info
    String connected_name;
    String connected_mac;

private:
    NimBLEClient* _pClient = nullptr;
    NimBLERemoteCharacteristic* _pChrREAD = nullptr;
    NimBLERemoteCharacteristic* _pChrWRITE = nullptr;
    ScaleType _type = ScaleType::UNKNOWN;

    std::vector<uint8_t> dataBuffer;

    static void cleanupJunkData(std::vector<uint8_t>& dataBuffer);
    static std::pair<uint8_t, uint8_t> checksumAcaia(const uint8_t* payload, size_t len);
    bool decodeAndHandleAcaia();             // consumes framed packets from dataBuffer
    void handleAcaiaEventPayload(const uint8_t* payload, size_t len);
    void handleAcaiaStatusPayload(const uint8_t* payload, size_t len);

    bool _connected = false;
    bool _isConnecting = false;
    bool doConnect = false;
    bool _debug = false;

    float _weight = 0.f;
    bool _weightAvailable = false;
    unsigned long _timer = 0;
    int _batteryLevel = -1;

    String connectMac;
    uint32_t lastHeartbeat = 0;

    // The discovered device we want to connect to
    NimBLEAdvertisedDevice* advDevice = nullptr;

    // Connection
    bool connectToServer();
    void identifyScaleType(NimBLEClient* client);

    // Notify handler
    static void notifyCB(NimBLERemoteCharacteristic* chr, uint8_t* data,
                         size_t length, bool isNotify, void* ctx);
    void handleNotification(uint8_t* data, size_t length);

    // ---- ACAIA ----
    void performHandshakeAcaia(bool oldVersion);
    void sendMessageAcaia(uint8_t msgType, const uint8_t* payload, size_t length, bool waitResponse = false);
    // void sendIdAcaia();
    void sendNotificationRequestAcaia();
    void sendHeartbeatAcaia();
    float decodeWeightAcaia(const uint8_t* payload, size_t len);
    unsigned long decodeTimeAcaia(const uint8_t* payload, size_t len);
    float decodeWeightAcaiaEvent(const uint8_t* w);
    unsigned long decodeTimeAcaiaEvent(const uint8_t* t);

    // ---- BOOKOO ----
    void performHandshakeBookoo();
    void sendMessageBookoo(const uint8_t* payload, size_t length,
                           bool waitResponse = false);
    void sendNotificationRequestBookoo();
    void sendHeartbeatBookoo();
    float decodeWeightBookoo(const uint8_t* data, size_t len);

    // ---- FELICITA ----
    void performHandshakeFelicita();
    int32_t parseWeightFelicita(const uint8_t* data, size_t length);

    // Common helper
    void subscribeNotifications(NimBLERemoteCharacteristic* chr);

    // ---- Callbacks ----
    class ClientCallbacks : public NimBLEClientCallbacks {
        BLEScale* _parent;
    public:
        ClientCallbacks(BLEScale* parent) : _parent(parent) {}
        void onConnect(NimBLEClient* pClient) override;
        void onDisconnect(NimBLEClient* pClient, int reason) override;
    };

    class ScanCallbacks : public NimBLEScanCallbacks {
        BLEScale* _parent;
    public:
        ScanCallbacks(BLEScale* parent) : _parent(parent) {}
        void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override;
        void onScanEnd(const NimBLEScanResults& results, int reason) override;
    };

    ClientCallbacks _clientCallbacks{this};
    ScanCallbacks _scanCallbacks{this};
};
