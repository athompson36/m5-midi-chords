#pragma once

#include <stddef.h>
#include <stdint.h>

#include "MidiIngress.h"

struct MidiEventHistoryItem {
  uint32_t atMs = 0;
  MidiSource source = MidiSource::Usb;
  MidiEventType type = MidiEventType::None;
  uint8_t channel = 0;
  uint8_t data1 = 0;
  uint8_t data2 = 0;
  uint16_t value14 = 0;
};

class MidiEventHistory {
 public:
  static constexpr size_t kCapacity = 32;

  void clear();
  void push(uint32_t nowMs, const MidiEvent& ev);
  size_t size() const;
  bool getNewestFirst(size_t idx, MidiEventHistoryItem* out) const;

 private:
  MidiEventHistoryItem ring_[kCapacity]{};
  size_t head_ = 0;
  size_t count_ = 0;
};
