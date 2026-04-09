#include "BleMidiTransport.h"

#if defined(ARDUINO)

#include <NimBLEDevice.h>
#include <Arduino.h>

namespace {

constexpr size_t kBleIngressCap = 512;
uint8_t g_bleIngress[kBleIngressCap];
size_t g_bleHead = 0;
size_t g_bleTail = 0;
size_t g_bleCount = 0;
bool g_bleConnected = false;
uint32_t g_bleRxBytesTotal = 0;
uint32_t g_bleDropTimestamp = 0;
uint32_t g_bleDropInvalidData = 0;
uint32_t g_bleDropTruncated = 0;

portMUX_TYPE g_bleIngressMux = portMUX_INITIALIZER_UNLOCKED;

constexpr const char* kMidiServiceUuid = "03B80E5A-EDE8-4B33-A751-6CE34EC4C700";
constexpr const char* kMidiCharUuid = "7772E5DB-3868-4112-A1A9-F2669D106BF3";

uint8_t midiDataByteCount(uint8_t status) {
  const uint8_t hi = static_cast<uint8_t>(status & 0xF0);
  switch (hi) {
    case 0xC0:
    case 0xD0:
      return 1;
    case 0x80:
    case 0x90:
    case 0xA0:
    case 0xB0:
    case 0xE0:
      return 2;
    default:
      break;
  }
  if (status == 0xF2) return 2;  // Song Position
  if (status >= 0xF8) return 0;  // realtime single byte
  return 0;
}

void pushByteUnsafe(uint8_t b) {
  if (g_bleCount >= kBleIngressCap) {
    g_bleHead = (g_bleHead + 1) % kBleIngressCap;
    g_bleCount--;
  }
  g_bleIngress[g_bleTail] = b;
  g_bleTail = (g_bleTail + 1) % kBleIngressCap;
  g_bleCount++;
}

// Decode BLE-MIDI packet into raw MIDI bytes.
void decodeBleMidiPacket(const uint8_t* in, size_t len) {
  if (!in || len < 2) return;
  // Byte 0 is BLE-MIDI header (timestamp high bits), ignore for parser feed.
  size_t i = 1;
  uint8_t runningStatus = 0;
  while (i < len) {
    // Timestamp low byte (0b1xxxxxxx) usually prefixes each message.
    if ((in[i] & 0x80) == 0) {
      g_bleDropTimestamp++;
      i++;
      continue;
    }
    i++;
    if (i >= len) break;

    uint8_t status = 0;
    uint8_t need = 0;
    const uint8_t b = in[i];

    if (b >= 0xF8) {
      // Realtime can appear standalone.
      pushByteUnsafe(b);
      i++;
      continue;
    }

    if (b & 0x80) {
      status = b;
      need = midiDataByteCount(status);
      if (status < 0xF0) {
        runningStatus = status;
      } else if (status != 0xF8) {
        // Clear running status for non-channel statuses.
        runningStatus = 0;
      }
      i++;
    } else {
      // Running status message: current byte is first data byte.
      if (runningStatus == 0) {
        i++;
        continue;
      }
      status = runningStatus;
      need = midiDataByteCount(status);
    }

    if (need == 0) {
      g_bleDropTimestamp++;
      i++;
      continue;
    }
    if (i + need > len) {
      g_bleDropTruncated++;
      break;
    }
    bool complete = true;
    uint8_t data[2] = {0, 0};
    for (uint8_t d = 0; d < need; ++d) {
      const uint8_t db = in[i + d];
      if (db & 0x80) {
        g_bleDropInvalidData++;
        complete = false;
        break;
      }
      data[d] = db;
    }
    if (!complete) {
      i++;
      continue;
    }
    pushByteUnsafe(status);
    for (uint8_t d = 0; d < need; ++d) {
      pushByteUnsafe(data[d]);
    }
    i += need;
  }
}

class BleMidiCharCallbacks : public NimBLECharacteristicCallbacks {
 public:
  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
    (void)connInfo;
    std::string v = pCharacteristic->getValue();
    if (v.empty()) return;
    portENTER_CRITICAL(&g_bleIngressMux);
    g_bleRxBytesTotal = static_cast<uint32_t>(g_bleRxBytesTotal + static_cast<uint32_t>(v.size()));
    decodeBleMidiPacket(reinterpret_cast<const uint8_t*>(v.data()), v.size());
    portEXIT_CRITICAL(&g_bleIngressMux);
  }
};

class BleMidiServerCallbacks : public NimBLEServerCallbacks {
 public:
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    (void)pServer;
    (void)connInfo;
    g_bleConnected = true;
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    (void)pServer;
    (void)connInfo;
    (void)reason;
    g_bleConnected = false;
    NimBLEDevice::startAdvertising();
  }
};

BleMidiCharCallbacks g_charCallbacks;
BleMidiServerCallbacks g_serverCallbacks;
bool g_bleInitDone = false;

}  // namespace

void bleMidiInit() {
  if (g_bleInitDone) return;
  NimBLEDevice::init("M5Chord MIDI IN");
  NimBLEServer* server = NimBLEDevice::createServer();
  if (!server) return;
  server->setCallbacks(&g_serverCallbacks);
  NimBLEService* service = server->createService(kMidiServiceUuid);
  if (!service) return;
  NimBLECharacteristic* ch = service->createCharacteristic(
      kMidiCharUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR |
                         NIMBLE_PROPERTY::NOTIFY);
  if (!ch) return;
  ch->setCallbacks(&g_charCallbacks);
  ch->setValue(std::string());
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  if (adv) {
    adv->addServiceUUID(kMidiServiceUuid);
    adv->start();
  }
  g_bleInitDone = true;
}

void bleMidiPoll() {}

bool bleMidiConnected() { return g_bleConnected; }

uint32_t bleMidiRxBytesTotal() {
  uint32_t n = 0;
  portENTER_CRITICAL(&g_bleIngressMux);
  n = g_bleRxBytesTotal;
  portEXIT_CRITICAL(&g_bleIngressMux);
  return n;
}

void bleMidiResetRxBytes() {
  portENTER_CRITICAL(&g_bleIngressMux);
  g_bleRxBytesTotal = 0;
  g_bleDropTimestamp = 0;
  g_bleDropInvalidData = 0;
  g_bleDropTruncated = 0;
  portEXIT_CRITICAL(&g_bleIngressMux);
}

uint32_t bleMidiDecodeDropTotal() {
  uint32_t n = 0;
  portENTER_CRITICAL(&g_bleIngressMux);
  n = g_bleDropTimestamp + g_bleDropInvalidData + g_bleDropTruncated;
  portEXIT_CRITICAL(&g_bleIngressMux);
  return n;
}

uint32_t bleMidiDecodeDropTimestampTotal() {
  uint32_t n = 0;
  portENTER_CRITICAL(&g_bleIngressMux);
  n = g_bleDropTimestamp;
  portEXIT_CRITICAL(&g_bleIngressMux);
  return n;
}

uint32_t bleMidiDecodeDropInvalidDataTotal() {
  uint32_t n = 0;
  portENTER_CRITICAL(&g_bleIngressMux);
  n = g_bleDropInvalidData;
  portEXIT_CRITICAL(&g_bleIngressMux);
  return n;
}

uint32_t bleMidiDecodeDropTruncatedTotal() {
  uint32_t n = 0;
  portENTER_CRITICAL(&g_bleIngressMux);
  n = g_bleDropTruncated;
  portEXIT_CRITICAL(&g_bleIngressMux);
  return n;
}

extern "C" size_t m5ChordBleMidiRead(uint8_t* dst, size_t cap) {
  if (!dst || cap == 0) return 0;
  size_t n = 0;
  portENTER_CRITICAL(&g_bleIngressMux);
  while (n < cap && g_bleCount > 0) {
    dst[n++] = g_bleIngress[g_bleHead];
    g_bleHead = (g_bleHead + 1) % kBleIngressCap;
    g_bleCount--;
  }
  portEXIT_CRITICAL(&g_bleIngressMux);
  return n;
}

#else

void bleMidiInit() {}
void bleMidiPoll() {}
bool bleMidiConnected() { return false; }
uint32_t bleMidiRxBytesTotal() { return 0; }
void bleMidiResetRxBytes() {}
uint32_t bleMidiDecodeDropTotal() { return 0; }
uint32_t bleMidiDecodeDropTimestampTotal() { return 0; }
uint32_t bleMidiDecodeDropInvalidDataTotal() { return 0; }
uint32_t bleMidiDecodeDropTruncatedTotal() { return 0; }

#endif
