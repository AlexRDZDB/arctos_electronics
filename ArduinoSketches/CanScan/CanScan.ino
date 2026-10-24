/*
 * CanScan
 *
 * CAN bus diagnostic for the MCP2515 + MKS SERVO42D setup (same wiring as MotorOnOff).
 *   1. Checks the Nano can talk to the MCP2515 over SPI.
 *   2. Tries every crystal (8/16 MHz) x bitrate combination and reports which one
 *      gets its frames ACKed. Any powered, correctly wired node ACKs regardless of ID,
 *      so "no ACK" everywhere = wiring/power/termination problem.
 *   3. With a working combination, polls CAN IDs 1..MAX_ID_SCAN and reports which reply.
 *
 * Open a serial monitor at 115200 baud. The scan runs at startup and again on any line sent.
 * Library: autowp-mcp2515 (see ../README.md)
 */
#include <SPI.h>
#include <mcp2515.h>

const uint8_t  CAN_CS_PIN  = 10;
const uint16_t MAX_ID_SCAN = 16;

MCP2515 mcp2515(CAN_CS_PIN);

const CAN_CLOCK clocks[]     = { MCP_8MHZ, MCP_16MHZ };
const char *clockNames[]     = { "8MHz", "16MHz" };
const CAN_SPEED speeds[]     = { CAN_1000KBPS, CAN_500KBPS, CAN_250KBPS, CAN_125KBPS };
const char *speedNames[]     = { "1000K", "500K", "250K", "125K" };

// MKS frame with checksum = (ID + bytes) & 0xFF. 0x30 = read encoder value.
bool sendRead(uint16_t id) {
  struct can_frame f;
  f.can_id  = id;
  f.can_dlc = 2;
  f.data[0] = 0x30;
  f.data[1] = (id + 0x30) & 0xFF;
  return mcp2515.sendMessage(&f) == MCP2515::ERROR_OK;
}

bool initCan(CAN_SPEED speed, CAN_CLOCK clock) {
  mcp2515.reset();
  if (mcp2515.setBitrate(speed, clock) != MCP2515::ERROR_OK) return false;
  return mcp2515.setNormalMode() == MCP2515::ERROR_OK;
}

void scanIds() {
  Serial.println(F("  Scanning IDs..."));
  bool found = false;
  struct can_frame rx;
  while (mcp2515.readMessage(&rx) == MCP2515::ERROR_OK) {}

  for (uint16_t id = 1; id <= MAX_ID_SCAN; id++) {
    sendRead(id);
    unsigned long t0 = millis();
    while (millis() - t0 < 50) {
      if (mcp2515.readMessage(&rx) == MCP2515::ERROR_OK) {
        Serial.print(F("  -> Reply from CAN ID 0x"));
        Serial.print(rx.can_id, HEX);
        Serial.print(F(" data:"));
        for (uint8_t i = 0; i < rx.can_dlc; i++) {
          Serial.print(' ');
          if (rx.data[i] < 0x10) Serial.print('0');
          Serial.print(rx.data[i], HEX);
        }
        Serial.println();
        found = true;
      }
    }
  }
  if (!found) Serial.println(F("  No replies (ACKs seen, but no ID 1..MAX_ID_SCAN answered)"));
}

void runScan() {
  Serial.println(F("\n=== CAN scan ==="));

  if (!initCan(CAN_500KBPS, MCP_8MHZ)) {
    Serial.println(F("MCP2515 not responding over SPI -> check CS=D10, SCK=D13, SI=D11, SO=D12, 5V, GND"));
    return;
  }
  Serial.println(F("MCP2515 SPI OK"));

  bool anyAck = false;
  for (uint8_t c = 0; c < 2; c++) {
    for (uint8_t s = 0; s < 4; s++) {
      Serial.print(clockNames[c]);
      Serial.print(F(" @ "));
      Serial.print(speedNames[s]);
      Serial.print(F(": "));

      if (!initCan(speeds[s], clocks[c])) {
        Serial.println(F("unsupported"));
        continue;
      }
      sendRead(1);
      delay(100);
      uint8_t tec = mcp2515.errorCountTX();
      if (tec == 0) {
        Serial.println(F("ACK  <-- bus works with this crystal/bitrate"));
        anyAck = true;
        scanIds();
      } else {
        Serial.print(F("no ACK (TX errors="));
        Serial.print(tec);
        Serial.println(')');
      }
    }
  }

  if (!anyAck) {
    Serial.println(F("\nNo combination was ACKed: the driver is not on the bus."));
    Serial.println(F("Check: driver powered (12-24V), shared GND, CANH<->CANH / CANL<->CANL,"));
    Serial.println(F("120 ohm termination at both ends (~60 ohm across CANH-CANL, power off)."));
  }
  mcp2515.reset();
  Serial.println(F("=== done (send any line to rescan) ==="));
}

void setup() {
  Serial.begin(115200);
  delay(500);
  runScan();
}

void loop() {
  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    runScan();
  }
}
