#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x303A
#define USB_PID 0x1001
#define USB_MANUFACTURER "M5Stack"
#define USB_PRODUCT "CoreS3"
#define USB_SERIAL ""

#define LED_BUILTIN 37
#define BUILTIN_LED LED_BUILTIN

static const uint8_t TX = 43;
static const uint8_t RX = 44;
#define TX1 TX
#define RX1 RX

static const uint8_t SDA = 12;
static const uint8_t SCL = 11;

static const uint8_t SS = 10;
static const uint8_t MOSI = 37;
static const uint8_t MISO = 35;
static const uint8_t SCK = 36;

#endif
