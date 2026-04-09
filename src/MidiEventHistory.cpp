#include "MidiEventHistory.h"

#include <string.h>

void MidiEventHistory::clear() {
  memset(ring_, 0, sizeof(ring_));
  head_ = 0;
  count_ = 0;
}

void MidiEventHistory::push(uint32_t nowMs, const MidiEvent& ev) {
  ring_[head_].atMs = nowMs;
  ring_[head_].source = ev.source;
  ring_[head_].type = ev.type;
  ring_[head_].channel = ev.channel;
  ring_[head_].data1 = ev.data1;
  ring_[head_].data2 = ev.data2;
  ring_[head_].value14 = ev.value14;

  head_ = (head_ + 1) % kCapacity;
  if (count_ < kCapacity) {
    count_++;
  }
}

size_t MidiEventHistory::size() const {
  return count_;
}

bool MidiEventHistory::getNewestFirst(size_t idx, MidiEventHistoryItem* out) const {
  if (!out || idx >= count_) return false;
  size_t p = (head_ + kCapacity - 1 - idx) % kCapacity;
  *out = ring_[p];
  return true;
}
