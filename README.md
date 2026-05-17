# 🚗 ARM-Based Real-Time Digital Automotive Instrument Cluster

A **bare-metal embedded C project** implementing a fully functional **Real-Time Digital Automotive Instrument Cluster** on the **NXP LPC2129 (ARM7TDMI-S)** microcontroller using the **CAN (Controller Area Network) protocol** for inter-node communication.

The system uses **two LPC2129 nodes** communicating over a CAN bus at **100 kbps** — one node reads sensors and switch inputs, the other drives a **16×2 LCD cluster display** with custom characters and GPIO indicator LEDs.

---

## 🏗️ System Architecture

```
┌─────────────────────────────────┐         ┌─────────────────────────────────┐
│         NODE A — Sensor Node    │         │       NODE B — Display Node     │
│         (Transmitter)           │         │       (Receiver)                │
│                                 │  CAN    │                                 │
│  ┌──────────┐  ┌─────────────┐  │ 100kbps │  ┌──────────┐  ┌────────────┐   │
│  │ ADC      │  │ EINT0/1/2   │  │◄───────►│  │ CAN2 RX  │  │ 16×2 LCD   │   │
│  │ Speed    │  │ Indicators  │  │         │  │ ISR      │  │ Custom     │   │
│  │ Temp     │  │ Head Light  │  │         │  │          │  │ CGRAM      │   │
│  └────┬─────┘  └──────┬──────┘  │         │  └────┬─────┘  └─────┬──────┘   │
│       │               │         │         │       │               │         │
│  ┌────▼───────────────▼──────┐  │         │  ┌────▼───────────────▼──────┐  │
│  │     Timer1 ISR            │  │         │  │  Bmain.c — Frame decoder  │  │
│  │  (1-sec ADC sample flag)  │  │         │  │  LED GPIO + LCD update    │  │
│  └────────────┬──────────────┘  │         │  └───────────────────────────┘  │
│               │                 │         │                                 │
│  ┌────────────▼──────────────┐  │         │                                 │
│  │   CAN2 TX (Amain.c)       │  │         │                                 │
│  │   Sends CAN frames        │  │         │                                 │
│  └───────────────────────────┘  │         │                                 │
└─────────────────────────────────┘         └─────────────────────────────────┘
```

---

## 📁 Project Structure

```
Instrument-Cluster-Uses-CAN/
│
├── Node-A/  (Root level)              ← Sensor & Input Node (Transmits over CAN)
│   ├── Amain.c                        # Main loop — polls flags, sends CAN frames
│   ├── header.h                       # Shared types, macros, function prototypes
│   ├── ADC.c                          # 10-bit ADC driver (Speed ch2, Temp ch1)
│   ├── CAN-driver.c                   # CAN2 init, TX, polling RX
│   ├── eint0.c                        # EINT0 ISR → Left Indicator flag
│   ├── eint1.c                        # EINT1 ISR → Right Indicator flag
│   ├── eint2.c                        # EINT2 ISR → Head Light flag
│   ├── timer_interrupt.c              # Timer1 ISR → 1-second ADC sample flag
│   └── delay.c                        # Timer0 blocking delay (ms and sec)
│
├── Node-B/                            ← Display & Output Node (Receives CAN)
│   ├── Bmain.c                        # Main — processes CAN frames, drives LCD/GPIO
│   ├── header.h                       # Shared types, macros, prototypes
│   ├── CAN-driver.c                   # CAN2 init (acceptance filter), TX, RX ISR, VIC
│   ├── lcd4-bit-driver.c              # HD44780 4-bit LCD driver with CGRAM chars
│   └── delay.c                        # Timer0 blocking delay
│
└── ARM_Cluster_Presentation.pptx      # Project presentation slides
```

---

## ✨ Features

- 🚦 **Left & Right Turn Indicators** — blinking at 250ms via EINT ISRs
- 💡 **Head Light Control** — solid ON/OFF via EINT ISR
- 🏎️ **Real-Time Speed Display** — 10-bit ADC sampling via Timer1 ISR (1 sec), displayed in km/h on LCD
- 🌡️ **Engine Temperature Display** — 10-bit ADC on separate channel, displayed in °C on LCD
- 📡 **CAN Protocol Communication** — 100 kbps inter-node messaging, no shared memory
- 🖥️ **16×2 HD44780 LCD** — custom CGRAM characters (arrow symbols, headlight icon)
- 💡 **GPIO Indicator LEDs** — independent active-LOW LEDs for each vehicle function
- ⚡ **Interrupt-Driven Architecture** — EINT0/1/2 for buttons + Timer1 for periodic ADC + CAN2 RX ISR

---

## 🛠️ Tech Stack

| Component | Details |
|---|---|
| **Microcontroller** | NXP LPC2129 (ARM7TDMI-S @ 60 MHz) |
| **Language** | Embedded C (bare-metal, no RTOS) |
| **Protocol** | CAN 2.0A — 100 kbps |
| **Display** | HD44780 16×2 LCD (4-bit mode, CGRAM custom chars) |
| **IDE / Toolchain** | Keil MDK (ARM MDK), LPC2129 Flash Tool |
| **Interrupts** | EINT0, EINT1, EINT2 (falling-edge) + Timer1 ISR + CAN2 RX ISR |
| **ADC** | 10-bit successive approximation, channels 1 & 2 |
| **Crystal** | 10 MHz external (60 MHz PLL output) |

---

## 📡 CAN Message Table

All communication between Node A and Node B happens exclusively over CAN frames:

| CAN ID | Source | Payload (byteA) | Meaning |
|--------|--------|-----------------|---------|
| `0x212` | Node A | `0x02` | Left Indicator — **ON** |
| `0x212` | Node A | `0x03` | Left Indicator — **OFF** |
| `0x213` | Node A | `0x04` | Head Light — **ON** |
| `0x213` | Node A | `0x05` | Head Light — **OFF** |
| `0x214` | Node A | `0x06` | Right Indicator — **ON** |
| `0x214` | Node A | `0x07` | Right Indicator — **OFF** |
| `0x215` | Node A | `0–1023` (u32) | Speed ADC raw value |
| `0x216` | Node A | `0–1023` (u32) | Engine Temperature ADC raw value |

> Node B CAN2 acceptance filter is configured to receive only these IDs.

---

## 🖥️ LCD Display Layout (16×2)

```
Col:  0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15
      ─────────────────────────────────────────────────
ROW1: ←                    ☀                         →
      ^                    ^                          ^
    Col 0               Col 8                      Col 15
  (Left Indicator)   (Head Light)            (Right Indicator)

ROW2: 1  2  3  k  m  /  h           4  5  .  6  °  C
      ^                             ^
    Col 0                        Col 10
  (Speed in km/h)           (Temperature in °C)
```

### Custom CGRAM Characters

| Slot | Symbol | Function |
|------|--------|----------|
| 0 | `←` | Left indicator arrow |
| 1 | `☀` | Head light / sun symbol |
| 2 | `→` | Right indicator arrow |

**Behaviour:**
- `←` and `→` blink at **250ms intervals** when the respective indicator is active
- `☀` appears **solid** when head light is ON, cleared when OFF
- Speed and temperature values update every **1 second** (Timer1 ISR tick)

---

## 📌 GPIO Pin Mapping

### Node A — Input Pins (Port 0)

| Pin | Function | Method |
|-----|----------|--------|
| `P0.16` | Left Indicator Button | EINT0 falling-edge ISR |
| `P0.14` | Right Indicator Button | EINT1 falling-edge ISR |
| `P0.15` | Head Light Button | EINT2 falling-edge ISR |
| `P0.28` | Temperature Sensor (ADC1) | ADC channel 1 |
| `P0.29` | Speed Sensor (ADC2) | ADC channel 2 |

### Node B — Output Pins (Port 0)

| Pin | Define | Direction | Function |
|-----|--------|-----------|----------|
| `P0.17` | `LI` | OUTPUT | Left Indicator LED (active LOW) |
| `P0.18` | `HL` | OUTPUT | Head Light LED (active LOW) |
| `P0.19` | `RI` | OUTPUT | Right Indicator LED (active LOW) |

### Node B — LCD Pins (Port 1, 4-bit mode)

| LCD Signal | LPC Pin | IODIR1 Bit |
|------------|---------|------------|
| RS | P1.17 | bit 17 |
| RW | P1.18 | bit 18 (write-only, tied LOW) |
| EN | P1.19 | bit 19 |
| D4 | P1.20 | bit 20 |
| D5 | P1.21 | bit 21 |
| D6 | P1.22 | bit 22 |
| D7 | P1.23 | bit 23 |

> `PINSEL2 = 0` required to use P1.16–P1.31 as GPIO.
> D0–D3 → GND (unused in 4-bit mode).

---

## 📄 File Descriptions

### Node A

**`Amain.c` — Main Application Loop**
Polls interrupt flags set by EINT and Timer ISRs. On each flag, constructs and transmits the appropriate CAN frame using the CAN2 driver.

**`ADC.c` — 10-bit ADC Driver**
Configures and reads ADC channels 1 (temperature) and 2 (speed). Returns raw 10-bit values (0–1023) for transmission over CAN.

**`CAN-driver.c` — CAN2 Bus Driver**
Initializes CAN2 at 100 kbps, implements TX and polling RX. Configures bit timing registers based on 60 MHz PCLK.

**`eint0.c` — Left Indicator ISR**
Falling-edge EINT0 handler on P0.16. Toggles left indicator flag and sets the CAN transmit trigger.

**`eint1.c` — Right Indicator ISR**
Falling-edge EINT1 handler on P0.14. Toggles right indicator flag and sets the CAN transmit trigger.

**`eint2.c` — Head Light ISR**
Falling-edge EINT2 handler on P0.15. Toggles head light flag and sets the CAN transmit trigger.

**`timer_interrupt.c` — Timer1 ISR**
Fires every 1 second. Sets the ADC sample flag, triggering Amain.c to read speed and temperature ADC values and send CAN frames 0x215 and 0x216.

**`delay.c` — Blocking Delay**
Timer0-based delay functions in milliseconds and seconds. Used for debounce and LCD timing.

---

### Node B

**`Bmain.c` — Main Receiver & Display Logic**
Processes incoming CAN frames from the RX ISR. Decodes each CAN ID and updates LCD display and GPIO LED outputs accordingly. Handles the 250ms indicator blink timing.

**`CAN-driver.c` — CAN2 Driver with Acceptance Filter & ISR**
Initialises CAN2 with a hardware acceptance filter to accept only the required CAN IDs. Implements the CAN2 RX ISR and VIC (Vectored Interrupt Controller) setup.

**`lcd4-bit-driver.c` — HD44780 LCD Driver**
Full 4-bit mode LCD driver for the HD44780 controller. Includes CGRAM character definition for the three custom symbols (left arrow, sun, right arrow), cursor positioning, string/character write, and display clear functions.

**`delay.c` — Blocking Delay**
Same Timer0-based delay as Node A. Used for LCD initialisation timing and indicator blink intervals.

---

## 🔧 Keil MDK Project Setup

### Node A

Add the following files to the Keil project:
```
Amain.c
delay.c
ADC.c
CAN-driver.c
eint0.c
eint1.c
eint2.c
timer_interrupt.c
```

Target Settings:
```
Device      : LPC2129
XTAL        : 10 MHz
Optimization: Level 1 (-O1)
```

---

### Node B

Add the following files to the Keil project:
```
Bmain.c
delay.c
lcd4-bit-driver.c
CAN-driver.c
```

Target Settings:
```
Device      : LPC2129
XTAL        : 10 MHz
Optimization: Level 1 (-O1)
```

---

## 🚀 Getting Started

### Hardware Required

- 2× LPC2129 development boards
- CAN transceiver modules (e.g., MCP2551 or TJA1050) — one per node
- 120Ω termination resistors at both ends of the CAN bus
- HD44780 16×2 LCD module
- Push buttons for indicator and headlight inputs
- Potentiometers or sensors for speed and temperature ADC inputs
- LEDs for Node B indicator outputs
- Keil uVision IDE installed on PC
- LPC2129 Flash Tool (Flash Magic or Keil built-in)

### Connections

```
Node A CAN TX (P0.1) ──► MCP2551 TXD ──► CAN Bus H/L
Node A CAN RX (P0.0) ◄── MCP2551 RXD ◄── CAN Bus H/L

Node B CAN TX (P0.1) ──► MCP2551 TXD ──► CAN Bus H/L
Node B CAN RX (P0.0) ◄── MCP2551 RXD ◄── CAN Bus H/L

120Ω between CANH and CANL at both node ends.
```

### Build & Flash

```
1. Open Keil uVision
2. Create/Open project for Node A → add Node A source files → Build (F7)
3. Flash Node_A.hex to first LPC2129 board
4. Create/Open project for Node B → add Node B source files → Build (F7)
5. Flash Node_B.hex to second LPC2129 board
6. Connect both boards to CAN bus via transceivers
7. Power both boards → system starts automatically
```

---

## 🧠 Key Concepts Demonstrated

| Concept | Implementation |
|---|---|
| **CAN Protocol (2.0A)** | 100 kbps inter-node messaging, acceptance filter, RX ISR |
| **Bare-Metal Interrupt Handling** | EINT0/1/2 falling-edge ISRs + Timer1 periodic ISR + CAN2 RX ISR |
| **10-bit ADC** | Channel 1 & 2 polling, raw-to-display value conversion |
| **VIC (Vectored Interrupt Controller)** | Priority-based interrupt routing for CAN2 and Timers |
| **HD44780 LCD (4-bit mode)** | CGRAM custom chars, cursor addressing, nibble-mode write |
| **Register-Level Peripheral Programming** | Direct CANAFMR, CANTFD, ADCR, T1MR0, EXTMODE, VICVect registers |
| **Modular Firmware Architecture** | Separate driver files per peripheral, shared header.h |
| **Event-Driven Design** | ISR sets flags, main loop polls and acts — no blocking in ISR |

---

## 📊 CAN Bus Configuration

```c
// Bit timing for 100 kbps @ 60 MHz PCLK
// Prescaler = 20, TSEG1 = 14, TSEG2 = 5, SJW = 1
// Bit time = (1 + 14 + 5) × (20/60MHz) = 20 × 333ns = 6.66μs → ~150 kbps
// (Exact values tuned in CAN-driver.c for target 100 kbps)
```

---

## 👤 Author

**Gopi Shankar V**
GitHub: [@Gopi-Shankar-V](https://github.com/Gopi-Shankar-V)

---

## 📄 License

This project is open source and available for educational and reference purposes.
