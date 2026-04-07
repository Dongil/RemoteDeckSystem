#pragma once
#include <Arduino.h>

// Relay outputs
constexpr uint8_t PIN_RELAY1    = 25;
constexpr uint8_t PIN_RELAY2    = 26;

// PC LED input (inverted logic: LOW = PC ON)
constexpr uint8_t PIN_PCLED     = 4;

// General purpose inputs
constexpr uint8_t PIN_GPIO1     = 12;
constexpr uint8_t PIN_GPIO2     = 14;
constexpr uint8_t PIN_GPIO3     = 15;

// Status LEDs
constexpr uint8_t PIN_STATUS1   = 32;  // System ready
constexpr uint8_t PIN_STATUS2   = 33;  // Network connected

// RS485 UART2
constexpr uint8_t PIN_RS485_RX  = 21;
constexpr uint8_t PIN_RS485_TX  = 22;
constexpr uint32_t RS485_BAUD   = 9600;

// W5500 Ethernet SPI
constexpr uint8_t PIN_ETH_CS    = 5;
constexpr uint8_t PIN_ETH_SCK   = 18;
constexpr uint8_t PIN_ETH_MISO  = 19;
constexpr uint8_t PIN_ETH_MOSI  = 23;
constexpr uint8_t PIN_ETH_INT   = 17;

// Network ports
constexpr uint16_t WEB_PORT       = 5050;
constexpr uint16_t UDP_DISC_PORT  = 5051;
constexpr uint16_t WOL_PORT       = 9;
