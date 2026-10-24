/*
 * MotorOnOff
 *
 * Receives "1" or "0" (one per line) over USB serial from the ROS 2
 * ArduinoTestNode and turns a NEMA 17 driven by an MKS SERVO42D on/off
 * over CAN, via an MCP2515 module wired to the Nano's SPI bus.
 *
 * Wiring (MCP2515 -> Nano):
 *   VCC -> 5V, GND -> GND, CS -> D10, SO -> D12, SI -> D11, SCK -> D13, INT -> D2 (unused)
 *   CANH/CANL -> MKS SERVO42D CANH/CANL (120 ohm termination at each end of the bus)
 *
 * Library: autowp-mcp2515 (see ../README.md)
 */
#include <SPI.h>
#include <mcp2515.h>

// ---------------- Configuration ----------------
const uint8_t  CAN_CS_PIN    = 10;
const CAN_SPEED CAN_BITRATE  = CAN_500KBPS;   // MKS SERVO42D default CAN rate
const CAN_CLOCK MCP_CRYSTAL  = MCP_8MHZ;      // most blue MCP2515 modules use 8 MHz; change to MCP_16MHZ if yours says 16.000
const uint16_t MOTOR_CAN_ID  = 0x01;          // MKS SERVO42D default slave address

const uint16_t MOTOR_RPM     = 30;            // fixed low speed (0-3000 RPM)
const uint8_t  MOTOR_ACCEL   = 2;             // 0-255, 0 = no ramp
const bool     MOTOR_DIR_CW  = false;         // direction bit

const unsigned long SERIAL_BAUD = 115200;
// ------------------------------------------------

MCP2515 mcp2515(CAN_CS_PIN);
bool motorOn = false;

// MKS SERVO42D CAN frame: [cmd, data..., crc], crc = (CAN ID + all bytes) & 0xFF
bool sendMksCommand(const uint8_t *data, uint8_t len) {
  struct can_frame frame;
  frame.can_id  = MOTOR_CAN_ID;
  frame.can_dlc = len + 1;

  uint16_t crc = MOTOR_CAN_ID;
  for (uint8_t i = 0; i < len; i++) {
    frame.data[i] = data[i];
    crc += data[i];
  }
  frame.data[len] = crc & 0xFF;

  MCP2515::ERROR err = mcp2515.sendMessage(&frame);
  if (err != MCP2515::ERROR_OK) {
    // 2 = ERROR_ALLTXBUSY (no node ACKed earlier frames), 4 = ERROR_FAILTX
    Serial.print(F("CAN error "));
    Serial.print(err);
    Serial.print(F(", EFLG=0x"));
    Serial.println(mcp2515.getErrorFlags(), HEX);
  }
  return err == MCP2515::ERROR_OK;
}

// 0xF3: enable (1) / disable (0) the motor driver
bool setEnable(bool enable) {
  uint8_t cmd[] = { 0xF3, (uint8_t)(enable ? 0x01 : 0x00) };
  return sendMksCommand(cmd, sizeof(cmd));
}

// 0x82: work mode. 5 = SR_vFOC (bus-controlled, closed-loop FOC)
bool setWorkMode(uint8_t mode) {
  uint8_t cmd[] = { 0x82, mode };
  return sendMksCommand(cmd, sizeof(cmd));
}

// 0xF6: speed mode. byte1 = dir(bit7) | speed[11:8], byte2 = speed[7:0], byte3 = accel
// Speed 0 stops the motor (ramped down by accel; accel 0 = immediate stop).
bool runSpeed(uint16_t rpm, uint8_t accel, bool dirCw) {
  uint8_t cmd[] = {
    0xF6,
    (uint8_t)((dirCw ? 0x80 : 0x00) | ((rpm >> 8) & 0x0F)),
    (uint8_t)(rpm & 0xFF),
    accel
  };
  return sendMksCommand(cmd, sizeof(cmd));
}

void setMotor(bool on) {
  bool ok = on ? runSpeed(MOTOR_RPM, MOTOR_ACCEL, MOTOR_DIR_CW)
               : runSpeed(0, MOTOR_ACCEL, MOTOR_DIR_CW);
  motorOn = on;
  Serial.print(on ? F("Motor ON") : F("Motor OFF"));
  Serial.println(ok ? F("") : F(" (CAN send failed)"));
}

void setup() {
  Serial.begin(SERIAL_BAUD);

  mcp2515.reset();
  // A failure here means the Nano can't talk to the MCP2515 over SPI (check wiring/power)
  if (mcp2515.setBitrate(CAN_BITRATE, MCP_CRYSTAL) != MCP2515::ERROR_OK ||
      mcp2515.setNormalMode() != MCP2515::ERROR_OK) {
    Serial.println(F("MCP2515 init failed (check SPI wiring)"));
  }

  delay(100);
  setWorkMode(5);   // SR_vFOC so the driver accepts bus commands
  delay(50);
  setEnable(true);
  delay(50);
  setMotor(false);

  Serial.println(F("Ready: send 1 (on) or 0 (off)"));
}

void loop() {
  static char buf[12];
  static uint8_t idx = 0;

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (idx > 0) {
        buf[idx] = '\0';
        long value = atol(buf);
        idx = 0;
        if (value == 1)      setMotor(true);
        else if (value == 0) setMotor(false);
        else { Serial.print(F("Ignored value: ")); Serial.println(value); }
      }
    } else if (idx < sizeof(buf) - 1) {
      buf[idx++] = c;
    }
  }

  // Drain any replies from the driver so the MCP2515 RX buffers don't fill up
  struct can_frame rx;
  while (mcp2515.readMessage(&rx) == MCP2515::ERROR_OK) {}
}
