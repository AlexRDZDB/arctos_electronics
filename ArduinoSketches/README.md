# Arduino Sketches

## MotorOnOff

Receives `1` / `0` lines over USB serial (115200 baud) from the ROS 2 `ArduinoTestNode`
and moves a NEMA 17 on an **MKS SERVO42D** to 180° (`1`) or back to 0° (`0`) over CAN through
an **MCP2515** SPI module. The shaft position at power-up is treated as 0°.

### Required library

| Library | Author | Used for |
|---|---|---|
| `autowp-mcp2515` (a.k.a. *arduino-mcp2515*) | autowp | MCP2515 CAN controller (`#include <mcp2515.h>`) |

`SPI.h` ships with the Arduino AVR core and needs no installation.

#### Install with the Arduino IDE (2.x)
1. Open **Tools → Manage Libraries…** (or the Library Manager icon on the left).
2. Search for `mcp2515`.
3. Install **autowp-mcp2515** by *autowp*.
   - Do **not** install "MCP_CAN_lib" (coryjfowler) — it is a different API.

#### Install with arduino-cli
```bash
arduino-cli lib install "autowp-mcp2515"
```

#### Manual install
1. Download the ZIP from https://github.com/autowp/arduino-mcp2515
2. Arduino IDE → **Sketch → Include Library → Add .ZIP Library…** and select the ZIP.

### Board settings
- Board: **Arduino Nano**
- Processor: **ATmega328P** — or **ATmega328P (Old Bootloader)** for most clone Nanos (try this if upload fails)
- Port: `/dev/ttyUSB0` (CH340/FTDI) or `/dev/ttyACM0`

Compile/upload from the CLI:
```bash
arduino-cli compile --fqbn arduino:avr:nano ArduinoSketches/MotorOnOff
arduino-cli upload  --fqbn arduino:avr:nano:cpu=atmega328old -p /dev/ttyUSB0 ArduinoSketches/MotorOnOff
```
> Close the ROS node (or stop the container) before uploading — only one program can hold the serial port.

### Wiring

| MCP2515 | Nano |
|---|---|
| VCC | 5V |
| GND | GND |
| CS  | D10 |
| SO (MISO) | D12 |
| SI (MOSI) | D11 |
| SCK | D13 |
| INT | D2 (not used) |

MCP2515 `CANH`/`CANL` → MKS SERVO42D `CANH`/`CANL`. Terminate the bus with 120 Ω at both ends
(the MCP2515 module has a jumper `J1` for its end). Power the SERVO42D from its own 12–24 V supply
and share GND.

### Configuration (top of `MotorOnOff.ino`)
- `MCP_CRYSTAL` — check the crystal on your MCP2515 module: `8.000` → `MCP_8MHZ`, `16.000` → `MCP_16MHZ`.
- `CAN_BITRATE` — must match the driver's **CanRate** menu setting (default 500K).
- `MOTOR_CAN_ID` — must match the driver's **CanID** menu setting (default 01).
- `MOTOR_RPM` — fixed speed, default 30 RPM.
- `MOTOR_MICROSTEPS` — must match the driver's **MStep** menu setting (default 16), otherwise 180° will be off.
- `MOTOR_DIR_CW` — flip to reverse which way the 0 → 180° move turns.

### MKS SERVO42D notes
- On startup the sketch sets work mode to **SR_vFOC** (`0x82 05`) and enables the driver (`0xF3 01`).
  You can also set *Mode → SR_vFOC* on the driver's OLED menu.
- Calibrate the motor once from the driver menu (**Cal**) with the motor unloaded before first use.
- Moves use the relative position command `0xFD` (pulses). 180° = 200 × MStep / 2 pulses
  (1600 at MStep 16).

## CanScan (diagnostic)

Upload `CanScan/` with the same wiring to check the bus. It reports whether the MCP2515 responds
over SPI, which crystal/bitrate combination gets ACKed by the driver, and which CAN ID replies.
Read the output from inside the container (the ROS node must not be running):
```bash
docker exec -it arctos_arduino python3 -m serial.tools.miniterm /dev/ttyUSB0 115200
```
Press Enter to rescan, `Ctrl+]` to quit.

Note: a wrong CAN ID does **not** cause `CAN error 2` — every node ACKs every valid frame.
Error 2 means no node ACKed at all (wiring, power, termination or bitrate/crystal mismatch).

### Troubleshooting `CAN send failed`
The sketch prints `CAN error <n>, EFLG=0x..`. Error 2 or 4 means no device on the bus acknowledged the frame. Check the following:
- The SERVO42D is powered (12–24 V), and its GND is shared with the Nano.
- CANH goes to CANH and CANL goes to CANL. They are not swapped.
- Both ends of the bus have a 120 Ω terminator (the MCP2515 `J1` jumper plus one at the driver).
- `MCP_CRYSTAL` matches the crystal on the MCP2515 module (8 vs 16 MHz). If it's wrong, the bitrate will be wrong.
- The driver's CanRate menu setting matches `CAN_BITRATE`.

`MCP2515 init failed` means the Nano can't talk to the module over SPI. Check the wiring and the 5 V supply to the module.

### Testing without ROS
Open the Serial Monitor at 115200 baud with "Newline" line ending and send `1` or `0`.
