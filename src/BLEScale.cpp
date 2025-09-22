#include "BLEScale.h"

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------
BLEScale::BLEScale(bool debug /*= false*/)
: _clientCallbacks(this),
  _scanCallbacks(this) {
    _debug = debug;
    // NimBLEDevice::init(""); // Initialize NimBLE
    // NimBLEDevice::setPower(ESP_PWR_LVL_P9); // optional
}

// -----------------------------------------------------------------------------
// Manage connection
// -----------------------------------------------------------------------------
bool BLEScale::manage(bool connect, bool maintain, String mac) {
    static unsigned long lastCall;

    if (lastCall + 50 > millis()) return false;
    lastCall = millis();
    NimBLEScan* pScan = NimBLEDevice::getScan();

    static bool scanConfigured = false;
    if (!scanConfigured) {
        pScan->setActiveScan(false);
        pScan->setScanCallbacks(&_scanCallbacks, false);
        scanConfigured = true;
    }

    _connected = (_pClient != nullptr) && _pClient->isConnected();
    connectMac = mac.length() ? mac : "";
    

    if (!_connected && !_isConnecting && connect && !pScan->isScanning()) {
        pScan->start(5000); // scan 5 seconds
    }

    if (maintain && _connected) {
        if (_type == ScaleType::ACAIA_NEW || _type == ScaleType::ACAIA_OLD) {
            sendHeartbeatAcaia();
        } 
    }

    if (!maintain && !connect) {
        disconnect();
    }

    if (doConnect && connect) { //doConnect is set in scan call back onresult
        doConnect = false;
        /** Found a device we want to connect to, do it now */
        if (connectToServer()) {
            if(_debug) Serial.println("Success! we should now be getting notifications");
            _connected = true;
            _isConnecting = false;
            return true;
        } else {
            if(_debug) Serial.println("Failed to connect");
            _connected = false;
            _isConnecting = false;
            return false;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// Disconnect
// -----------------------------------------------------------------------------
void BLEScale::disconnect() {
    if (_pClient && _pClient->isConnected()) {
        _pClient->disconnect();
    }

    _pChrREAD = _pChrWRITE = nullptr;
    if (advDevice) { delete advDevice; advDevice = nullptr; } // or unique_ptr.reset()
    _type = ScaleType::UNKNOWN;
    _connected = false;
    _isConnecting = false;
}

// -----------------------------------------------------------------------------
// Public API commands
// -----------------------------------------------------------------------------
bool BLEScale::tare() {
    if (!_connected || !_pChrWRITE) return false;
    if (_type == ScaleType::ACAIA_OLD) {
        byte TARE_ACAIA[6]              = { 0xef, 0xdd, 0x04, 0x00, 0x00, 0x00 };
        if(_pChrWRITE) if(_pChrWRITE->writeValue(TARE_ACAIA, 6, false)) return true;
        return false;
    } 

    if (_type == ScaleType::ACAIA_NEW) {
        byte TARE_REQUEST[6] = { 0xef, 0xdd, 0x04, 0x00, 0x00, 0x00 };
        if(_pChrWRITE) if(_pChrWRITE->writeValue(TARE_REQUEST, 6, true)) return true;
        return false;
    } 
    
    if (_type == ScaleType::BOOKOO) {
        uint8_t payload[6] = {0x03,0x0a,0x01,0x00,0x00,0x08};
        sendMessageBookoo(payload, sizeof(payload));
        return true;
    }
    if (_type == ScaleType::FELIC) {
        uint8_t tareCmd[] = {0x54};
        if(_pChrWRITE) if(_pChrWRITE->writeValue(tareCmd, sizeof(tareCmd), true)) return true;
        return false;
    }
    return false;
}

bool BLEScale::startTimer() {
    if (!_connected || !_pChrWRITE) return false;
    if (_type == ScaleType::ACAIA_NEW || _type == ScaleType::ACAIA_OLD) {
        return false;
        // uint8_t payload[] = {0x07};
        // sendMessageAcaia(0x0C, payload, 1);
        // return true;
    } else if (_type == ScaleType::BOOKOO) {
        uint8_t payload[6] = {0x03,0x0a,0x04,0x00,0x00,0x0a};
        sendMessageBookoo(payload, sizeof(payload));
        return true;
    } else if (_type == ScaleType::FELIC) {
        // uint8_t StartCmd[] = {0x50};
        // if(_pChrWRITE) if(_pChrWRITE->writeValue(StartCmd, sizeof(StartCmd), true)) return true;
        return false;
    }
    return false;
}

bool BLEScale::resetTimer() {
    if (!_connected || !_pChrWRITE) return false;
    if (_type == ScaleType::ACAIA_NEW || _type == ScaleType::ACAIA_OLD) {
        // uint8_t payload[] = {0x08};
        // sendMessageAcaia(0x0C, payload, 1);
        return false;
    } else if (_type == ScaleType::BOOKOO) {
        uint8_t payload[6] = {0x03,0x0a,0x06,0x00,0x00,0x0c};
        sendMessageBookoo(payload, sizeof(payload));
        return true;
    } else if (_type == ScaleType::FELIC) {
        // uint8_t cmd[7] = {0xef,0xdd,0x0d,0x00,0x01,0x00,0x01}; //this is acaia placeholder
        // _pChrWRITE->writeValue(cmd, 7, true);
        return false;
    }
    return false;
}

bool BLEScale::stopTimer() {
    if (!_connected || !_pChrWRITE) return false;
    if (_type == ScaleType::ACAIA_NEW || _type == ScaleType::ACAIA_OLD) {
        // uint8_t payload[] = {0x09};
        // sendMessageAcaia(0x0C, payload, 1); // does not work
        return false;
    } else if (_type == ScaleType::BOOKOO) {
        uint8_t payload[6] = {0x03,0x0a,0x05,0x00,0x00,0x0d};
        sendMessageBookoo(payload, sizeof(payload));
        return true;
    } else if (_type == ScaleType::FELIC) {
        // uint8_t cmd[7] = {0xef,0xdd,0x0d,0x00,0x02,0x00,0x02}; //this is acaia stuff
        // _pChrWRITE->writeValue(cmd, 7, true);
        return false;
    }
    return false;
}

// -----------------------------------------------------------------------------
// Data access
// -----------------------------------------------------------------------------
bool BLEScale::newWeightAvailable() {
    bool available = _weightAvailable;
    _weightAvailable = false;
    return available;
}

float BLEScale::getWeight() { return _weight; }
unsigned long BLEScale::getTimer() { return _timer; }
int BLEScale::getBatteryLevel() { return _batteryLevel; }


// -----------------------------------------------------------------------------
// ConnectToServer (private)
// -----------------------------------------------------------------------------
bool BLEScale::connectToServer() {
    if (!advDevice) return false;

    // Try to reuse an existing client first
    NimBLEClient* client = nullptr;

    if (NimBLEDevice::getCreatedClientCount()) {
        // If we already know this device, reuse that client and skip DB refresh
        client = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
        if (client) {
            if (!client->connect(advDevice, /*refresh=*/false)) {
                if (_debug) { Serial.println("Reconnect failed"); }
                return false;
            }
            if (_debug) { Serial.println("Reconnected existing client"); }
        } else {
            // Otherwise grab any disconnected client to reuse
            client = NimBLEDevice::getDisconnectedClient();
        }
    }

    // No reusable client? Create a new one.
    if (!client) {
        // Optional safety: limit concurrent clients (tune as needed)
        if (NimBLEDevice::getCreatedClientCount() >= 3) {
            if (_debug) { Serial.println("Max clients reached - no more connections available"); }
            return false;
        }

        // (Ensure NimBLEDevice::init(...) was called elsewhere, e.g., in setup())

        client = NimBLEDevice::createClient();
        if (!client) {
            Serial.println("createClient() returned nullptr (init missing or OOM)");
            return false;
        }

        // MTU is global; set once early in your app if possible.
        NimBLEDevice::setMTU(23);

        client->setClientCallbacks(&_clientCallbacks, false);
        client->setConnectionParams(12, 24, 0, 400);
        client->setConnectTimeout(450);

        if (_debug) { Serial.println("New client created"); }
    } else {
        // Ensure callbacks are set even when reusing a client
        client->setClientCallbacks(&_clientCallbacks, false);
    }

    if (_debug) {
        Serial.printf("Trying to connect. AddrType=%d\n", advDevice->getAddressType());
    }

    // Prefer the advertised-device overload; lets NimBLE decide address type
    if (!client->connect(advDevice)) {
        if (_debug) { Serial.println("Connect failed with advertised-device overload"); }

        // As a fallback, try explicit address & type (do not invert with !)
        if (!client->connect(advDevice->getAddress(), advDevice->getAddressType())) {
            if (_debug) { Serial.println("Connect failed (explicit type). Deleting client."); }
            NimBLEDevice::deleteClient(client);
            return false;
        }
    }

    // We are connected; keep the handle
    _pClient = client;

    // Identify by service UUID
    if (_pClient->getService("49535343-fe7d-4ae5-8fa9-9fafd205e455")) {
        _type = ScaleType::ACAIA_NEW;
        performHandshakeAcaia(false);
    } else if (_pClient->getService("1820")) {
        _type = ScaleType::ACAIA_OLD;
        performHandshakeAcaia(true);
    } else if (_pClient->getService("0FFE")) {
        _type = ScaleType::BOOKOO;
        performHandshakeBookoo();
    } else if (_pClient->getService("FFE0")) {
        _type = ScaleType::FELIC;
        performHandshakeFelicita();
    } else {
        _type = ScaleType::UNKNOWN;
    }

    if (_type == ScaleType::UNKNOWN) return false;

    // Safer: assign via temporary std::strings (avoid any dangling pointers)
    {
        std::string tmpName; 
        if(_type == ScaleType::FELIC) tmpName = "Felicita";
        else if(advDevice) tmpName = advDevice->getName();
        connected_name = tmpName.c_str();
    }
    {
        
        if(_pClient) { 
            std::string tmpMac = _pClient->getPeerAddress().toString();
            connected_mac = tmpMac.c_str();
        }
    }

    if (_debug) {
        Serial.print("Connected to: "); Serial.println(connected_name.c_str());
        Serial.print("with addr: ");   Serial.println(connected_mac.c_str());
    }

    return true;
}

// -----------------------------------------------------------------------------
// NotifyCB
// -----------------------------------------------------------------------------
void BLEScale::notifyCB(NimBLERemoteCharacteristic* chr, uint8_t* data, size_t len, bool isNotify, void* ctx) {
    BLEScale* self = static_cast<BLEScale*>(ctx);
    if (!self) return;

    switch (self->_type) {
        case ScaleType::BOOKOO: {
            float w = self->decodeWeightBookoo(data, len);
            self->_weight = w;
            self->_weightAvailable = true;
            break;
        }
        case ScaleType::ACAIA_NEW: {
            Serial.println("ACAIA NEW");
            self->handleNotification(data, len); // already handles weight + timer
            break;
        }
        case ScaleType::ACAIA_OLD: {
            if(self->_debug) Serial.println("ACAIA OLD");
            self->handleNotification(data, len); // already handles weight + timer
            break;
        }
        case ScaleType::FELIC: {
            self->handleNotification(data, len); // already handles weight + timer
            break;
        }
        default:
            break;
    }
}

void BLEScale::handleNotification(uint8_t* data, size_t length) {
    if (_type == ScaleType::ACAIA_NEW) {
        // accumulate and drain
        dataBuffer.insert(dataBuffer.end(), data, data + length);
        bool more = true;
        while (more) more = decodeAndHandleAcaia();
        return;
    } 
    
    static const float pow10[] = {1, 10, 100, 1000, 10000, 100000}; // precomputed powers
    if (_type == ScaleType::ACAIA_OLD) { 
        if((length == 10 || length == 14)) {
            uint16_t rawWeight = ((data[3] & 0xff) << 8) | (data[2] & 0xff);
            byte scaleIndex = data[6];
            float scale = (scaleIndex < sizeof(pow10) / sizeof(pow10[0])) ? pow10[scaleIndex] : 1.0;
            _weight = rawWeight / scale * ((data[7] & 0x02) ? -1 : 1);
            _weightAvailable = true;
            if(_debug) Serial.println(_weight);
        }
    }
    else if (_type == ScaleType::BOOKOO) {
        _weight = decodeWeightBookoo(data, length);
        _weightAvailable = true;
    } else if (_type == ScaleType::FELIC) {
        if(length < 18) return; 
        int32_t w = parseWeightFelicita(data, length);
        _weight = (float)w / 100.0f;
        _weightAvailable = true;
    }
}

void BLEScale::cleanupJunkData(std::vector<uint8_t>& buf) {
    int start = 0;
    // advance until we find EF DD adjacent
    while (start < (int)buf.size() - 1 &&
           !(buf[start] == 0xEF && buf[start + 1] == 0xDD)) {
        start++;
    }
    if (start > 0) buf.erase(buf.begin(), buf.begin() + start);

    // if we ended with a trailing single EF (no DD yet), keep it;
    // if it's not EF, clear to avoid infinite growth
    if (buf.size() == 1 && buf[0] != 0xEF) buf.clear();
}

// -----------------------------------------------------------------------------
// Acaia helpers
// -----------------------------------------------------------------------------
void BLEScale::performHandshakeAcaia(bool oldVersion) {
    if (oldVersion) {
        // ACAIA OLD (svc 0x1820, chr 0x2a80 does notify + write-no-response)
        NimBLERemoteService* svc = _pClient->getService("1820");
        if (!svc) {
            if (_debug) Serial.println("ACAIA OLD service not found");
            return;
        }

        NimBLERemoteCharacteristic* chr = svc->getCharacteristic("2a80");
        if (!chr) {
            if (_debug) Serial.println("ACAIA OLD char 2a80 not found");
            return;
        }

        // Use the same characteristic for read (notify) and write (no response)
        _pChrREAD  = chr;
        _pChrWRITE = chr;

        if (_pChrREAD->canWriteNoResponse()) {
            if(_debug) Serial.println("Succes pChrREAD -> canWriteNoResponse()");
        }

        if (_pChrREAD->canNotify()) {
            subscribeNotifications(_pChrREAD);
            if (_debug) Serial.println("Subscribed to ACAIA OLD notifications.");
        } else {
            if (_debug) Serial.println("ACAIA OLD: 0x2a80 cannot notify");
        }

        if (!_pChrWRITE->canWriteNoResponse()) {
            if (_debug) Serial.println("ACAIA OLD: 0x2a80 does not support write-no-response");
            // We keep going for notifications, but heartbeat/tare will be skipped.
        }

        byte IDENTIFY_OLD[20]               = { 0xef, 0xdd, 0x0b, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x9a, 0x6d };
        byte NOTIFICATION_REQUEST[14]   = { 0xef, 0xdd, 0x0c, 0x09, 0x00, 0x01, 0x01, 0x02, 0x02, 0x05, 0x03, 0x04, 0x15, 0x06 };

        _pChrWRITE->writeValue(IDENTIFY_OLD, 20, false);
        _pChrWRITE->writeValue(NOTIFICATION_REQUEST, 14, false);
        return;
    }

    // ACAIA NEW (svc 4953..., read + write distinct characteristics)
    NimBLERemoteService* svc = _pClient->getService("49535343-fe7d-4ae5-8fa9-9fafd205e455");
    if (!svc) {
        if (_debug) Serial.println("ACAIA NEW service not found");
        return;
    }

    _pChrREAD  = svc->getCharacteristic("49535343-1e4d-4bd9-ba61-23c647249616");
    _pChrWRITE = svc->getCharacteristic("49535343-8841-43f4-a8d4-ecbe34729bb3");

    if (_pChrREAD) {
        subscribeNotifications(_pChrREAD);
    } else { 
        if (_debug) Serial.println("ACAIA NEW read characteristic not found");
        return;
    }
    delay(10); // for some reason acaia NEW charachteristics not always found without?? 
    if (!_pChrWRITE) {
        if (_debug) Serial.println("ACAIA NEW write characteristic not found");
        return;
    }

    Serial.println("Sending ID and notification request...");

    byte IDENTIFY_NEW[20]       = { 0xef, 0xdd, 0x0b, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x68, 0x3b };
    _pChrWRITE->writeValue(IDENTIFY_NEW, 20, true);
    //HB contains notification request
}


void BLEScale::sendMessageAcaia(uint8_t msgType, const uint8_t* payload, size_t length, bool waitResponse) {
    uint8_t header[3] = {0xEF, 0xDD, msgType};
    uint8_t len = (uint8_t)length;
    uint16_t checksum = 0;
    for (size_t i = 0; i < length; i++) checksum += payload[i];
    std::vector<uint8_t> msg;
    msg.insert(msg.end(), header, header+3);
    msg.push_back(len);
    msg.insert(msg.end(), payload, payload+length);
    msg.push_back(checksum & 0xFF);
    msg.push_back((checksum >> 8) & 0xFF);
    if(_pChrWRITE) _pChrWRITE->writeValue(msg.data(), msg.size(), !waitResponse);
}

void BLEScale::sendHeartbeatAcaia() {
    unsigned long now = millis();
    if (now - lastHeartbeat < 2750) {
        return; // avoid spamming
    }

    if (_type == ScaleType::ACAIA_OLD) {
        // --- OLD ACAIA HEARTBEAT ---
        uint8_t hbOld[7] = {0xef, 0xdd, 0x00, 0x02, 0x00, 0x02, 0x00};

        if (_pChrWRITE) {
            bool ok = _pChrWRITE->writeValue(hbOld, sizeof(hbOld), false);
            if (_debug) {
                Serial.printf("Sent ACAIA OLD heartbeat [%s]\n", ok ? "OK" : "FAIL");
            }
        } else {
            if (_debug) Serial.println("ACAIA OLD heartbeat skipped, WRITE char invalid");
        }

        lastHeartbeat = now;
        return;
    }

    if (_type == ScaleType::ACAIA_NEW) {
        // --- NEW ACAIA HEARTBEAT ---
        byte HEARTBEAT_SYSTEM[7]        = { 0xef, 0xdd, 0x00, 0x02, 0x00, 0x02, 0x00 };
        byte NOTIFICATION_REQUEST[14]   = { 0xef, 0xdd, 0x0c, 0x09, 0x00, 0x01, 0x01, 0x02, 0x02, 0x05, 0x03, 0x04, 0x15, 0x06 };

        if (_pChrWRITE) {
            int score = 0;
            if(_pChrWRITE->writeValue(HEARTBEAT_SYSTEM, sizeof(HEARTBEAT_SYSTEM), true)) score++;
            if(_pChrWRITE->writeValue(NOTIFICATION_REQUEST, sizeof(NOTIFICATION_REQUEST), true)) score++;
            if (_debug) {
                Serial.printf("Sent ACAIA NEW heartbeat [%s]\n", score == 2? "OK" : "FAIL");
            }
        } else {
            if (_debug) Serial.println("ACAIA OLD heartbeat skipped, WRITE char invalid");
        }

        lastHeartbeat = now;
        return;
    }
}

float BLEScale::decodeWeightAcaiaEvent(const uint8_t* w) {
    // 6-byte layout:
    // w[0..1]=uint16 le, w[4]=scaling(1=/10,2=/100,3=/1000,4=/10000), w[5] bit1 sign
    float value = float((w[1] << 8) | w[0]);
    uint8_t scaling = w[4];

    switch (scaling) {
        case 1: value /= 10.f; break;
        case 2: value /= 100.f; break;
        case 3: value /= 1000.f; break;
        case 4: value /= 10000.f; break;
        default: return 0.f;
    }
    if (w[5] & 0x02) value = -value;
    return value;
}

unsigned long BLEScale::decodeTimeAcaiaEvent(const uint8_t* t) {
    // mm, ss, tenths — we return whole seconds like your API
    return (unsigned long)(t[0] * 60 + t[1]);
}

// checksum per Acaia protocol: two running sums over payload bytes
std::pair<uint8_t, uint8_t> BLEScale::checksumAcaia(const uint8_t* payload, size_t len) {
    uint8_t cksum1 = 0;
    uint8_t cksum2 = 0;

    for (size_t i = 0; i < len; i++) {
        if (i % 2 == 0) {
            cksum1 += payload[i];
        } else {
            cksum2 += payload[i];
        }
    }
    return { static_cast<uint8_t>(cksum2), static_cast<uint8_t>(cksum1) };
}

bool BLEScale::decodeAndHandleAcaia() {
    static constexpr size_t HEADER_LEN   = 3;  // EF DD type
    static constexpr size_t CHECKSUM_LEN = 2; 
    static constexpr size_t MIN_FRAME    = HEADER_LEN + 1 + CHECKSUM_LEN; // len byte + checksums

    cleanupJunkData(dataBuffer);
    if (dataBuffer.size() < MIN_FRAME) return false;

    // Need the length byte to know full frame size
    if (dataBuffer.size() < 4) return false;
    uint8_t payloadLen = dataBuffer[3];

    // Frame = EF DD [type] [len] [payload(len bytes)] [ck1][ck2]
    const size_t frameLen = HEADER_LEN + 1 + payloadLen + CHECKSUM_LEN;
    if (dataBuffer.size() < frameLen) return false; // wait for more

    // Quick sanity: cap absurd lengths to resync faster
    if (payloadLen > 64) {
        dataBuffer.erase(dataBuffer.begin()); // drop 1 byte and rescan
        return dataBuffer.size() >= MIN_FRAME;
    }

    const uint8_t* frame   = dataBuffer.data();
    const uint8_t* payload = frame + 4;

    // Verify checksum (both bytes)
    auto ck = checksumAcaia(payload, payloadLen);
    const uint8_t c1 = frame[frameLen - 2];
    const uint8_t c2 = frame[frameLen - 1];

    if (ck.second != c1) {
        if (_debug) {
            Serial.printf("Checksum fail: calc[%02X %02X] recv[%02X %02X]\n", ck.first, ck.second, c1, c2);
        }
        dataBuffer.erase(dataBuffer.begin());
        return dataBuffer.size() >= MIN_FRAME;
    }

    // Top-level message type
    uint8_t msgType = frame[2];

    // Dispatch
    if (msgType == 0x0C /* EVENT */) {
        handleAcaiaEventPayload(payload, payloadLen);
    } else if (msgType == 0x02 /* STATUS */) {
        handleAcaiaStatusPayload(payload, payloadLen);
    } else {
        if (_debug) {
            Serial.printf("Unknown msgType %02X len=%u\n", msgType, payloadLen);
        }
    }

    // consume the frame and see if more complete packets remain
    dataBuffer.erase(dataBuffer.begin(), dataBuffer.begin() + frameLen);
    return dataBuffer.size() >= MIN_FRAME;
}

void BLEScale::handleAcaiaEventPayload(const uint8_t* p, size_t len) {
    if (len < 2) return;
    uint8_t eventType  = p[0];
    const uint8_t* e = p + 1; // start of event data after [type]

    switch (eventType) {
        case 0x05: { // WEIGHT
            if (len < 2 + 6) return;
            float v = decodeWeightAcaiaEvent(e);
            _weight = v;
            _weightAvailable = true;
            if (_debug) Serial.printf("Weight: %.4f\n", v);
            break;
        }
        case 0x07: { // TIMER
            if (len < 2 + 3) return; // mm ss tenths if you want tenths
            _timer = decodeTimeAcaiaEvent(e);
            if (_debug) Serial.printf("Timer: %lus\n", _timer);
            break;
        }
        case 0x06: { // BATTERY sometimes appears via STATUS, but keep for completeness
            if (len >= 3) _batteryLevel = (int)(e[0] & 0x7F);
            break;
        }
        default:
            if (_debug) Serial.printf("Unknown event %02X\n", eventType);
            break;
    }
}

void BLEScale::handleAcaiaStatusPayload(const uint8_t* p, size_t len) {
    if (len < 3) return;
    // p[1] battery with 0x7F mask, p[2] units (2=grams,5=oz)
    _batteryLevel = (int)(p[1] & 0x7F);
    if (_debug) {
        const char* u = (p[2] == 2 ? "g" : (p[2] == 5 ? "oz" : "?"));
        Serial.printf("Status: batt=%d%% units=%s\n", _batteryLevel, u);
    }
}

// -----------------------------------------------------------------------------
// Bookoo helpers
// -----------------------------------------------------------------------------
void BLEScale::performHandshakeBookoo() {
    NimBLERemoteService* svc = _pClient->getService("0FFE");
    if (!svc) { if(_debug) Serial.println("BOOKOO service not found"); return; }

    _pChrREAD  = svc->getCharacteristic("FF11");
    _pChrWRITE = svc->getCharacteristic("FF12");
    if(!_pChrREAD || !_pChrWRITE) { if(_debug) Serial.println("BOOKOO chars missing"); return; }

    subscribeNotifications(_pChrREAD);
    sendNotificationRequestBookoo();
}

void BLEScale::sendMessageBookoo(const uint8_t* payload, size_t length, bool waitResponse) {
    if(_pChrWRITE) _pChrWRITE->writeValue(payload, length, !waitResponse);
}
void BLEScale::sendNotificationRequestBookoo() {
    uint8_t payload1[] = {0x02,0x00};
    sendMessageBookoo(payload1, sizeof(payload1));
    uint8_t payload2[] = {0x00};
    sendMessageBookoo(payload2, sizeof(payload2));
}

void BLEScale::sendHeartbeatBookoo() {
    uint8_t hb[] = {0,0,0,0,0,0};
    sendMessageBookoo(hb, sizeof(hb));
}

float BLEScale::decodeWeightBookoo(const uint8_t* data, size_t len) {
    if (len < 10) return 0.0f;  // need at least 10 bytes

    int32_t raw = (data[7] << 16) | (data[8] << 8) | data[9];
    if (data[6] == 45) {
        raw = -raw;
    }

    float weight = raw / 100.0f;
    if (_debug) {
        Serial.printf("Bookoo weight decoded: %.2fg\n", weight);
    }
    return weight;
}

// -----------------------------------------------------------------------------
// Felicita helpers
// -----------------------------------------------------------------------------
void BLEScale::performHandshakeFelicita() {
    NimBLERemoteService* svc = _pClient->getService("FFE0");
    if (!svc) { if(_debug) Serial.println("FELICITA service not found"); return; }

    _pChrREAD  = svc->getCharacteristic("FFE1");
    _pChrWRITE = svc->getCharacteristic("FFE1");
    if(!_pChrREAD || !_pChrWRITE) { if(_debug) Serial.println("FELICITA chars missing"); return; }

    subscribeNotifications(_pChrREAD);
    // IDENTIFY + NOTIFICATION_REQUEST
    uint8_t IDENTIFY[20] = {0xef,0xdd,0x0b,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x30,0x31,0x32,0x33,0x34,0x9a,0x6d};
    _pChrWRITE->writeValue(IDENTIFY, 20, true);
    uint8_t NOTIFICATION_REQUEST[14] = {0xef,0xdd,0x0c,0x09,0x00,0x01,0x01,0x02,0x02,0x05,0x03,0x04,0x15,0x06};
    _pChrWRITE->writeValue(NOTIFICATION_REQUEST, 14, true);
}

int32_t BLEScale::parseWeightFelicita(const uint8_t* data, size_t length) {
    // Expect ASCII digits in positions 3..8 and optional '-' in position 2.
    // Bounds were already checked by caller (length >= 18).
    const bool isNegative = (data[2] == 0x2D);

    // Validate that each is '0'..'9' (avoid bitwise tricks which can misclassify)
    for (int i = 3; i <= 8; ++i) {
        if (data[i] < '0' || data[i] > '9') {
            return 0;
        }
    }

    // Direct digit expansion
    int32_t v =
        (data[3] - '0') * 100000 +
        (data[4] - '0') * 10000  +
        (data[5] - '0') * 1000   +
        (data[6] - '0') * 100    +
        (data[7] - '0') * 10     +
        (data[8] - '0');

    return isNegative ? -v : v;
}

// -----------------------------------------------------------------------------
// Common helper
// -----------------------------------------------------------------------------
void BLEScale::subscribeNotifications(NimBLERemoteCharacteristic* chr) {
    if (chr && chr->canNotify()) {
        chr->subscribe(true, [this](NimBLERemoteCharacteristic* /*c*/, uint8_t* data,
                                    size_t len, bool /*isNotify*/) {
            this->handleNotification(data, len);
        });
    }
}

// -----------------------------------------------------------------------------
// ClientCallbacks implementation
// -----------------------------------------------------------------------------
void BLEScale::ClientCallbacks::onConnect(NimBLEClient* pClient) {
    if (_parent) {
        _parent->_connected = true;
        _parent->_isConnecting = false;
        _parent->_pClient = pClient;
        Serial.println("Client connected");
    }
}

void BLEScale::ClientCallbacks::onDisconnect(NimBLEClient* pClient, int reason) {
    if (_parent) {
        _parent->_connected = false;
        _parent->_isConnecting = false;
        _parent->_pClient = nullptr;
        Serial.printf("Client disconnected, reason=%d\n", reason);
        _parent->_type = ScaleType::UNKNOWN;
        _parent->_pChrREAD = _parent->_pChrWRITE = nullptr;
        if (_parent->advDevice) { delete _parent->advDevice; _parent->advDevice = nullptr; } // or unique_ptr.reset()
    }
}

// -----------------------------------------------------------------------------
// ScanCallbacks implementation
// -----------------------------------------------------------------------------
void BLEScale::ScanCallbacks::onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
    if (!_parent) return;
    std::string devName = advertisedDevice->getName();
    std::string devAddr = advertisedDevice->getAddress().toString(); // ensure std::string

    // Always print found device if debugging
    if (_parent->_debug) {
        Serial.printf("Advertised Device found: %s\n", advertisedDevice->toString().c_str());
    }

    // Match by MAC if provided
    if (_parent->connectMac.length() > 0) {
        if (devAddr == _parent->connectMac.c_str()) {
            Serial.printf("Found scale by addr: %s\n", devAddr.c_str());
            NimBLEDevice::getScan()->stop();
            if (_parent->advDevice) { delete _parent->advDevice; _parent->advDevice = nullptr; } // or unique_ptr.reset()
            _parent->advDevice = new NimBLEAdvertisedDevice(*advertisedDevice);
            _parent->doConnect = true;
            _parent->_isConnecting = true;
            return;
        }
    } else {
        // Otherwise try by service UUIDs or name
        // 1) Try by advertised service UUID (mostly hidden felicita scale)
        bool hasFFE0 =
            advertisedDevice->haveServiceUUID() &&
            (advertisedDevice->isAdvertisingService(NimBLEUUID((uint16_t)0xFFE0)) ||
             advertisedDevice->isAdvertisingService(NimBLEUUID("0000FFE0-0000-1000-8000-00805F9B34FB")));

        if (hasFFE0 && devName == "") {
            Serial.println("Found scale by service UUID FFE0"); //FELICITA NAMELESS
            NimBLEDevice::getScan()->stop();
            if (_parent->advDevice) { delete _parent->advDevice; _parent->advDevice = nullptr; } // or unique_ptr.reset()
            _parent->advDevice = new NimBLEAdvertisedDevice(*advertisedDevice);
            _parent->doConnect = true;
            _parent->_isConnecting = true;
            return;
        }
        // 2) Find by name
        if (devName.find("ACAIA") != std::string::npos ||
            devName.find("PEARL") != std::string::npos ||
            devName.find("PYXIS") != std::string::npos ||
            devName.find("CINCO") != std::string::npos ||
            devName.find("LUNAR") != std::string::npos ||
            devName.find("PROCH") != std::string::npos ||
            devName.find("FELIC") != std::string::npos ||
            devName.find("BOOKOO_SC") != std::string::npos) {
            Serial.printf("Found scale by name: %s\n", devName.c_str());
            NimBLEDevice::getScan()->stop();
         
            if (_parent->advDevice) { delete _parent->advDevice; _parent->advDevice = nullptr; } // or unique_ptr.reset()

            _parent->advDevice = new NimBLEAdvertisedDevice(*advertisedDevice);
            _parent->doConnect = true;
            _parent->_isConnecting = true;

            if(_parent->_debug) Serial.print("End of OnResult -> Connecting?");
            return;
        }
    }
}

void BLEScale::ScanCallbacks::onScanEnd(const NimBLEScanResults& results, int reason) {
    if(_parent->_debug) Serial.printf("Scan ended. reason=%d, devices=%d\n", reason, results.getCount());
}
