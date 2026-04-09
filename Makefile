# Convenience targets (PlatformIO is the source of truth; see platformio.ini).
# Prefer project venv when present: python3 -m venv .venv && pip install -r requirements.txt
ifeq ($(wildcard .venv/bin/pio),)
  PIO = pio
else
  PIO = .venv/bin/pio
endif

.PHONY: build test native upload monitor clean

build:
	$(PIO) run -e m5stack-cores3

native:
	$(PIO) test -e native

test: native build

upload:
	$(PIO) run -e m5stack-cores3 -t upload

monitor:
	$(PIO) device monitor -b 115200

clean:
	$(PIO) run -t clean
