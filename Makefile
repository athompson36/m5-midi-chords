# Convenience targets (PlatformIO is the source of truth; see platformio.ini).
.PHONY: build test native upload monitor clean

build:
	pio run -e m5stack-cores3

native:
	pio test -e native

test: native build

upload:
	pio run -e m5stack-cores3 -t upload

monitor:
	pio device monitor -b 115200

clean:
	pio run -t clean
