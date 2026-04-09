#include "BleMidiTransport.h"

#if defined(ARDUINO)

#include <NimBLEDevice.h>
#include <Arduino.h>
#include <string.h>

namespace {

constexpr size_t kBleIngressCap = 512;
uint8_t g_bleIngress[kBleIngressCap];
size_t g_bleHead = 0;
size_t g_bleTail = 0;
size_t g_bleCount = 0;
bool g_bleConnected = false;
NimBLECharacteristic* g_bleMidiChar = nullptr;
NimBLEServer* g_bleServer = nullptr;
uint32_t g_bleRxBytesTotal = 0;
uint32_t g_bleDropTimestamp = 0;
uint32_t g_bleDropInvalidData = 0;
uint32_t g_bleDropTruncated = 0;

portMUX_TYPE g_bleIngressMux = portMUX_INITIALIZER_UNLOCKED;

constexpr const char* kMidiServiceUuid = "03B80E5A-EDE8-4B33-A751-6CE34EC4C700";
constexpr const char* kMidiCharUuid = "7772E5DB-3868-4112-A1A9-F2669D106BF3";
constexpr const char* kDeviceName = "M5Chord MIDI";

// ── TX staging buffer ──────────────────────────────────────────────
// All outgoing MIDI bytes are appended here by bleMidiWrite() and
// flushed as efficiently-packed BLE-MIDI packets by bleMidiFlush().
constexpr size_t kBleTxStageCap = 256;
uint8_t g_bleTxStage[kBleTxStageCap];
size_t g_bleTxStageLen = 0;

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
  if (status == 0xF2) return 2;
  if (status >= 0xF8) return 0;
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

void decodeBleMidiPacket(const uint8_t* in, size_t len) {
  if (!in || len < 2) return;
  size_t i = 1;
  uint8_t runningStatus = 0;
  while (i < len) {
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
        runningStatus = 0;
      }
      i++;
    } else {
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

static void startAdvertisingWithName() {
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  if (!adv) return;
  adv->setName(kDeviceName);
  adv->addServiceUUID(kMidiServiceUuid);
  adv->enableScanResponse(true);
  adv->setMinInterval(32);
  adv->setMaxInterval(64);
  adv->start();
}

class BleMidiServerCallbacks : public NimBLEServerCallbacks {
 public:
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    (void)pServer;
    g_bleConnected = true;
    // Request comfortable connection parameters: 15-30ms interval, 0 latency, 4s timeout.
    if (g_bleServer) {
      g_bleServer->updateConnParams(connInfo.getConnHandle(), 12, 24, 0, 400);
    }
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    (void)pServer;
    (void)connInfo;
    (void)reason;
    g_bleConnected = false;
    g_bleTxStageLen = 0;
    startAdvertisingWithName();
  }
};

BleMidiCharCallbacks g_charCallbacks;
BleMidiServerCallbacks g_serverCallbacks;
bool g_bleInitDone = false;

}  // namespace

void bleMidiInit() {
  if (g_bleInitDone) return;
  NimBLEDevice::init(kDeviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEServer* server = NimBLEDevice::createServer();
  if (!server) return;
  g_bleServer = server;
  server->setCallbacks(&g_serverCallbacks);
  NimBLEService* service = server->createService(kMidiServiceUuid);
  if (!service) return;
  NimBLECharacteristic* ch = service->createCharacteristic(
      kMidiCharUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR |
                         NIMBLE_PROPERTY::NOTIFY);
  if (!ch) return;
  g_bleMidiChar = ch;
  ch->setCallbacks(&g_charCallbacks);
  ch->setValue(std::string());
  service->start();
  startAdvertisingWithName();
  g_bleInitDone = true;
}

void bleMidiPoll() {}

size_t bleMidiWrite(const uint8_t* bytes, size_t len) {
  if (!bytes || len == 0) return 0;
  const size_t room = kBleTxStageCap - g_bleTxStageLen;
  const size_t copy = (len < room) ? len : room;
  memcpy(g_bleTxStage + g_bleTxStageLen, bytes, copy);
  g_bleTxStageLen += copy;
  return copy;
}

void bleMidiFlush() {
  if (g_bleTxStageLen == 0 || !g_bleInitDone || !g_bleMidiChar || !g_bleConnected) {
    g_bleTxStageLen = 0;
    return;
  }

  const uint8_t* bytes = g_bleTxStage;
  const size_t len = g_bleTxStageLen;
  g_bleTxStageLen = 0;

  uint8_t pkt[20];
  size_t p = 0;
  size_t consumed = 0;
  uint8_t runningStatus = 0;

  const uint16_t t = static_cast<uint16_t>(millis() & 0x1FFF);
  const uint8_t th = static_cast<uint8_t>(0x80U | ((t >> 7) & 0x3FU));
  const uint8_t tl = static_cast<uint8_t>(0x80U | (t & 0x7FU));

  pkt[p++] = th;

  while (consumed < len) {
    const uint8_t b0 = bytes[consumed];
    uint8_t status = 0;
    uint8_t need = 0;
    uint8_t data0 = 0;
    uint8_t data1 = 0;

    if (b0 >= 0xF8U) {
      status = b0;
      need = 0;
      consumed += 1;
    } else if (b0 & 0x80U) {
      status = b0;
      need = midiDataByteCount(status);
      if (status < 0xF0U) {
        runningStatus = status;
      } else {
        runningStatus = 0;
      }
      if (consumed + 1U + need > len) break;
      if (need >= 1) data0 = static_cast<uint8_t>(bytes[consumed + 1] & 0x7F);
      if (need >= 2) data1 = static_cast<uint8_t>(bytes[consumed + 2] & 0x7F);
      consumed += static_cast<size_t>(1U + need);
    } else {
      if (runningStatus == 0) {
        consumed += 1;
        continue;
      }
      status = runningStatus;
      need = midiDataByteCount(status);
      if (need == 0) {
        consumed += 1;
        continue;
      }
      if (consumed + need > len) break;
      data0 = static_cast<uint8_t>(bytes[consumed] & 0x7F);
      if (need >= 2) data1 = static_cast<uint8_t>(bytes[consumed + 1] & 0x7F);
      consumed += static_cast<size_t>(need);
    }

    // Each BLE-MIDI message: timestamp_low + status + data bytes
    const size_t msgBleLen = static_cast<size_t>(1U + 1U + need);
    if (p + msgBleLen > sizeof(pkt)) {
      // Flush current packet and start a new one.
      g_bleMidiChar->setValue(pkt, p);
      g_bleMidiChar->notify();
      p = 0;
      pkt[p++] = th;
    }
    pkt[p++] = tl;
    pkt[p++] = status;
    if (need >= 1) pkt[p++] = data0;
    if (need >= 2) pkt[p++] = data1;
  }
  if (p > 1) {
    g_bleMidiChar->setValue(pkt, p);
    g_bleMidiChar->notify();
  }
}

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
size_t bleMidiWrite(const uint8_t* bytes, size_t len) {
  (void)bytes;
  (void)len;
  return 0;
}
void bleMidiFlush() {}
bool bleMidiConnected() { return false; }
uint32_t bleMidiRxBytesTotal() { return 0; }
void bleMidiResetRxBytes() {}
uint32_t bleMidiDecodeDropTotal() { return 0; }
uint32_t bleMidiDecodeDropTimestampTotal() { return 0; }
uint32_t bleMidiDecodeDropInvalidDataTotal() { return 0; }
uint32_t bleMidiDecodeDropTruncatedTotal() { return 0; }

#endif
