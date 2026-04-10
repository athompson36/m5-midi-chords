#include "UsbMidiTransport.h"

#if defined(ARDUINO)
#include <Arduino.h>
#include <USB.h>
#include <string.h>
#include "esp32-hal-tinyusb.h"
#include "class/midi/midi_device.h"
#endif

namespace {

bool g_usbReady = false;

#if defined(ARDUINO) && CONFIG_TINYUSB_ENABLED
uint8_t g_usbMidiEpOut = 0;
uint8_t g_usbMidiEpIn = 0;
bool g_usbMidiIfaceEnabled = false;

uint16_t loadMidiDescriptor(uint8_t* dst, uint8_t* itf) {
  if (!dst || !itf) return 0;
  if (g_usbMidiEpOut == 0 || g_usbMidiEpIn == 0) return 0;
  const uint8_t strIndex = tinyusb_add_string_descriptor("TinyUSB MIDI");
  uint8_t descriptor[TUD_MIDI_DESC_LEN] = {
      TUD_MIDI_DESCRIPTOR(*itf, strIndex, static_cast<uint8_t>(0x00U | g_usbMidiEpOut),
                          static_cast<uint8_t>(0x80U | g_usbMidiEpIn), 64)};
  *itf = static_cast<uint8_t>(*itf + 2U);
  memcpy(dst, descriptor, TUD_MIDI_DESC_LEN);
  return TUD_MIDI_DESC_LEN;
}

struct UsbMidiInterfaceRegistrar {
  UsbMidiInterfaceRegistrar() {
    g_usbMidiEpOut = tinyusb_get_free_duplex_endpoint();
    if (g_usbMidiEpOut == 0) return;
    g_usbMidiEpIn = g_usbMidiEpOut;
    g_usbMidiIfaceEnabled =
        tinyusb_enable_interface(USB_INTERFACE_MIDI, TUD_MIDI_DESC_LEN, loadMidiDescriptor) == ESP_OK;
  }
};

UsbMidiInterfaceRegistrar g_usbMidiRegistrar;
#endif

}  // namespace

void usbMidiInit() {
#if defined(ARDUINO)
  if (!USB) {
    USB.begin();
  }
#if CONFIG_TINYUSB_ENABLED && CONFIG_TINYUSB_MIDI_ENABLED
  g_usbReady = g_usbMidiIfaceEnabled;
  Serial.printf("[USB-MIDI] TinyUSB path: epOut=%u epIn=%u ifaceEnabled=%d usbReady=%d\n",
                (unsigned)g_usbMidiEpOut, (unsigned)g_usbMidiEpIn,
                (int)g_usbMidiIfaceEnabled, (int)g_usbReady);
#else
  g_usbReady = true;
  Serial.println("[USB-MIDI] Fallback serial path (TinyUSB MIDI not enabled at compile time)");
#endif
#else
  g_usbReady = false;
#endif
}

void usbMidiPoll() {}

bool usbMidiReady() { return g_usbReady; }

bool usbMidiHostConnected() {
#if defined(ARDUINO)
#if CONFIG_TINYUSB_ENABLED && CONFIG_TINYUSB_MIDI_ENABLED
  return g_usbReady && tud_mounted();
#else
  return false;
#endif
#else
  return false;
#endif
}

size_t usbMidiWrite(const uint8_t* bytes, size_t len) {
#if defined(ARDUINO)
  if (!bytes || len == 0 || !g_usbReady) return 0;
#if CONFIG_TINYUSB_ENABLED && CONFIG_TINYUSB_MIDI_ENABLED
  return static_cast<size_t>(tud_midi_stream_write(0, bytes, static_cast<uint32_t>(len)));
#else
  return Serial.write(bytes, len);
#endif
#else
  (void)bytes;
  (void)len;
  return 0;
#endif
}

size_t usbMidiRead(uint8_t* dst, size_t cap) {
#if defined(ARDUINO)
  if (!dst || cap == 0 || !g_usbReady) return 0;
#if CONFIG_TINYUSB_ENABLED && CONFIG_TINYUSB_MIDI_ENABLED
  return static_cast<size_t>(tud_midi_stream_read(dst, static_cast<uint32_t>(cap)));
#else
  size_t n = 0;
  while (n < cap && Serial.available() > 0) {
    const int rb = Serial.read();
    if (rb < 0) break;
    dst[n++] = static_cast<uint8_t>(rb);
  }
  return n;
#endif
#else
  (void)dst;
  (void)cap;
  return 0;
#endif
}

extern "C" size_t m5ChordUsbMidiRead(uint8_t* dst, size_t cap) { return usbMidiRead(dst, cap); }
