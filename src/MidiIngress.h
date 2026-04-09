#pragma once

#include <stddef.h>
#include <stdint.h>

enum class MidiSource : uint8_t { Usb = 0, Ble = 1, Din = 2 };

enum class MidiEventType : uint8_t {
  None = 0,
  NoteOff,
  NoteOn,
  PolyAftertouch,
  ControlChange,
  ProgramChange,
  ChannelAftertouch,
  PitchBend,
  Clock,
  Start,
  Continue,
  Stop,
  SongPosition
};

struct MidiEvent {
  MidiSource source = MidiSource::Usb;
  MidiEventType type = MidiEventType::None;
  uint8_t channel = 0;  // 0..15 for channel events
  uint8_t data1 = 0;
  uint8_t data2 = 0;
  uint16_t value14 = 0;  // Pitch bend / SPP payload
};

class MidiIngressParser {
 public:
  void reset();
  bool feedByte(uint8_t b, MidiSource src, MidiEvent* out);

 private:
  uint8_t runningStatus_ = 0;
  uint8_t pendingStatus_ = 0;
  uint8_t dataBuf_[2] = {0, 0};
  uint8_t dataCount_ = 0;
};

bool midiEventIsChannelVoice(const MidiEvent& ev);
bool midiEventIsRealtime(const MidiEvent& ev);
uint8_t midiSourceToRoute(MidiSource src);  // 1=USB, 2=Bluetooth, 3=DIN

/// Poll USB MIDI bytes (currently USB CDC serial stream) and emit one event at a time.
bool midiIngressPollUsb(MidiIngressParser& parser, MidiEvent* out);
/// BLE ingress placeholder (safe no-op until transport byte source is wired).
bool midiIngressPollBle(MidiIngressParser& parser, MidiEvent* out);
/// DIN ingress poller. Reads optional transport hook bytes then parses queued data.
bool midiIngressPollDin(MidiIngressParser& parser, MidiEvent* out);
/// Adapter feed helpers for transport integrations.
void midiIngressEnqueueBleBytes(const uint8_t* bytes, size_t len);
void midiIngressEnqueueDinBytes(const uint8_t* bytes, size_t len);
/// Queue-overflow counters (oldest-byte drops while enqueuing).
uint32_t midiIngressBleQueueDropTotal();
uint32_t midiIngressDinQueueDropTotal();
void midiIngressResetQueueDropTotals();
/// Optional BLE transport hook: implement this symbol in a BLE module to provide raw MIDI bytes.
/// Return number of bytes written to `dst` (max `cap`). Default weak implementation returns 0.
extern "C" size_t m5ChordBleMidiRead(uint8_t* dst, size_t cap);
/// Optional USB transport hook: implement this symbol in a USB module to provide raw MIDI bytes.
/// Return number of bytes written to `dst` (max `cap`). Default weak implementation returns 0.
extern "C" size_t m5ChordUsbMidiRead(uint8_t* dst, size_t cap);
/// Optional DIN transport hook: implement this symbol in a DIN/UART module to provide raw MIDI bytes.
/// Return number of bytes written to `dst` (max `cap`). Default weak implementation returns 0.
extern "C" size_t m5ChordDinMidiRead(uint8_t* dst, size_t cap);

/// Test helper: parse a byte buffer as USB source and return first event produced.
bool midiIngressFeedUsbBytes(MidiIngressParser& parser, const uint8_t* bytes, size_t len, MidiEvent* out);
/// Test/helper: parse byte buffer for an explicit source and return first event produced.
bool midiIngressFeedBytes(MidiIngressParser& parser, MidiSource src, const uint8_t* bytes, size_t len,
                         MidiEvent* out);
