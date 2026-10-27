#include "link.h"
#include <Arduino.h>
#include <cstdlib>
#include <vector>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"  // NimBLE 2's headers; src/ builds with -Werror
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#include <NimBLEDevice.h>
#pragma GCC diagnostic pop
#include "../util/elmmon.h"
#include "../util/obd.h"
#include "../util/obdble.h"

namespace sniff {
namespace link {
static constexpr uint32_t SCAN_MS = 4000, CONNECT_MS = 5000;
static NimBLEClient* client = nullptr;
static NimBLERemoteCharacteristic* tx = nullptr;  // commands to the adapter (write)
static bool txResponse = true;                    // write with response (no write-without)
static obdble::Inbox inbox;                       // replies, filled by notifications
static portMUX_TYPE inboxMux = portMUX_INITIALIZER_UNLOCKED;
static elmmon::Lines lines;

// Notifications from the adapter (NimBLE's host task): appended to the inbox.
static void onNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    portENTER_CRITICAL(&inboxMux);
    inbox.push(data, len);
    portEXIT_CRITICAL(&inboxMux);
}

static uint16_t uuid16(const NimBLEUUID& u) {
    if (u.bitSize() != 16) return 0;
    return static_cast<uint16_t>(strtoul(u.toString().c_str(), nullptr, 16));  // "0xfff0"
}

bool connected() { return client && client->isConnected() && tx; }

static bool findAdapter(NimBLEAddress* addr) {
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(nullptr);  // the Xbox library sets its own again for its next scan
    scan->setActiveScan(true);        // names often come in the scan response
    NimBLEScanResults found = scan->getResults(SCAN_MS, false);
    bool match = false;
    for (int i = 0; i < found.getCount() && !match; ++i) {
        const NimBLEAdvertisedDevice* d = found.getDevice(static_cast<uint32_t>(i));
        const std::string nm = d->getName();
        bool serial = false;  // advertises a clone's serial service
        for (int k = 0; k < static_cast<int>(d->getServiceUUIDCount()); ++k)
            serial = serial || obdble::serialService(uuid16(d->getServiceUUID(static_cast<uint8_t>(k))));
        if (nm.empty() && !serial) continue;
        Serial.printf("[link] seen \"%s\" %s%s\n", nm.c_str(), d->getAddress().toString().c_str(),
                      serial ? " (serial service)" : "");
        match = obd::looksLikeAdapter(nm.c_str()) || serial;
        if (match) *addr = d->getAddress();
    }
    scan->clearResults();
    return match;
}

// Connects, finds the serial characteristics (logged, for unknown clones) and subscribes.
static bool open(const NimBLEAddress& addr) {
    client = NimBLEDevice::createClient();
    if (!client) return false;
    client->setConnectTimeout(CONNECT_MS);
    tx = nullptr;
    if (!client->connect(addr)) {
        Serial.println("[link] connect failed");
        return false;
    }
    std::vector<obdble::Char> chars;
    std::vector<NimBLERemoteCharacteristic*> refs;
    for (NimBLERemoteService* svc : client->getServices(true)) {
        for (NimBLERemoteCharacteristic* c : svc->getCharacteristics(true)) {
            chars.push_back({uuid16(svc->getUUID()), uuid16(c->getUUID()), c->canWrite(), c->canWriteNoResponse(),
                             c->canNotify(), c->canIndicate()});
            refs.push_back(c);
            Serial.printf("[link] %s/%s%s%s%s%s\n", svc->getUUID().toString().c_str(), c->getUUID().toString().c_str(),
                          c->canWrite() ? " w" : "", c->canWriteNoResponse() ? " wn" : "", c->canNotify() ? " n" : "",
                          c->canIndicate() ? " i" : "");
        }
    }
    const obdble::Pick pk = obdble::pick(chars.data(), static_cast<int>(chars.size()));
    if (pk.write < 0 || pk.notify < 0) {
        Serial.println("[link] no serial service on this device");
        return false;
    }
    NimBLERemoteCharacteristic* rx = refs[pk.notify];
    if (!rx->subscribe(rx->canNotify(), onNotify)) {
        Serial.println("[link] subscribe failed");
        return false;
    }
    tx = refs[pk.write];
    txResponse = !tx->canWriteNoResponse();
    return true;
}

void close() {
    tx = nullptr;
    if (!client) return;
    if (client->isConnected()) {
        client->disconnect();
        for (int i = 0; i < 30 && client->isConnected(); ++i) delay(100);
    }
    NimBLEDevice::deleteClient(client);
    client = nullptr;
}

bool tryConnect() {
    close();
    NimBLEAddress addr;
    if (!findAdapter(&addr)) return false;
    if (!open(addr)) {
        close();  // a fresh client next time (feat/obd's car test: reconnecting a closing link failed)
        return false;
    }
    portENTER_CRITICAL(&inboxMux);
    inbox.clear();
    portEXIT_CRITICAL(&inboxMux);
    lines.clear();
    Serial.printf("[link] adapter connected %s\n", addr.toString().c_str());
    return true;
}

bool send(const char* cmd) {
    if (!obd::allowed(cmd)) {
        Serial.printf("[link] refused %.*s\n", static_cast<int>(strcspn(cmd ? cmd : "", "\r")), cmd ? cmd : "");
        return false;
    }
    if (!connected()) return false;
    return tx->writeValue(reinterpret_cast<const uint8_t*>(cmd), strlen(cmd), txResponse);
}

bool readLine(char* out, size_t cap, uint32_t waitMs) {
    const uint32_t t0 = millis();
    for (;;) {
        char chunk[64];
        size_t n = 0;
        portENTER_CRITICAL(&inboxMux);
        while (n < sizeof chunk && inbox.pop(&chunk[n])) n++;
        portEXIT_CRITICAL(&inboxMux);
        if (n) lines.push(chunk, n);
        if (lines.next(out, cap)) return true;
        if (millis() - t0 >= waitMs) return false;
        delay(2);
    }
}
}
}
