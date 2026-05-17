=======================================================================
  ARM-Based Real-Time Digital Automotive Instrument Cluster
  CAN Protocol Integration — LPC2129 Bare-Metal Code
=======================================================================

MCU       : LPC2129 (ARM7TDMI-S @ 60 MHz)
Toolchain : Keil MDK / LPC2129 flash tool
CAN Speed : 100 kbps

-----------------------------------------------------------------------
  PROJECT STRUCTURE
-----------------------------------------------------------------------

NodeA/                    ← Sensor & Input Node (transmits over CAN)
  header.h                  Common types, macros, prototypes
  Amain.c                   Main loop: polls flags, sends CAN frames
  delay.c                   Timer0 blocking delay (ms and sec)
  ADC.c                     10-bit ADC driver (speed ch2, temp ch1)
  CAN-driver.c              CAN2 init, TX, polling RX
  eint0.c                   EINT0 ISR → Left Indicator flag
  eint1.c                   EINT1 ISR → Right Indicator flag
  eint2.c                   EINT2 ISR → Head Light flag
  timer_interrupt.c         Timer1 ISR → 1-second ADC sample flag

NodeB/                    ← Display & Output Node (receives CAN)
  header.h                  Common types, macros, prototypes + BUZ pin
  Bmain.c                   Main: processes CAN frames, drives LCD/GPIO
  delay.c                   Timer0 blocking delay
  lcd4-bit-driver.c         HD44780 4-bit LCD driver with CGRAM chars
  CAN-driver.c              CAN2 init (with acceptance filter), TX, RX,
                            CAN2 RX ISR, VIC setup

-----------------------------------------------------------------------
  CAN MESSAGE TABLE
-----------------------------------------------------------------------

  ID    | Source  | Payload (byteA) | Meaning
 -------+---------+-----------------+-------------------------------
  0x212 | Node A  | 0x02 / 0x03    | Left  Indicator ON / OFF
  0x213 | Node A  | 0x04 / 0x05    | Head  Light     ON / OFF
  0x214 | Node A  | 0x06 / 0x07    | Right Indicator ON / OFF
  0x215 | Node A  | 0–1023 (u32)   | Speed ADC raw value
  0x216 | Node A  | 0–1023 (u32)   | Engine Temp ADC raw value
  0x217 | Node A  | 0x08 / 0x09    | Horn PRESS / RELEASE  ← NEW

-----------------------------------------------------------------------
  LCD DISPLAY LAYOUT (16×2)
-----------------------------------------------------------------------

  Col:  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
  ROW1: ←               ☀               →
        ^               ^               ^
       Col0           Col8           Col15
       (Left          (Head          (Right
       Indicator)     Light)         Indicator)

  ROW2: 1 2 3 k m / h       4 5 . 6 ° C
        ^                   ^
       Col0               Col10
       (Speed km/h)    (Temperature °C)

  ← : CGRAM slot 0 (custom left arrow)
  ☀ : CGRAM slot 1 (custom sun / headlight symbol)
  → : CGRAM slot 2 (custom right arrow)

  Left/Right arrows BLINK at 250ms when indicator is active.
  ☀ appears solid when headlight is ON.

-----------------------------------------------------------------------
  GPIO PINS — Node B (Port 0)
-----------------------------------------------------------------------

  Pin    | Define | Direction | Function
 --------+--------+-----------+-------------------------------
  P0.17  | LI     | OUTPUT    | Left  Indicator LED (active LOW)
  P0.18  | HL     | OUTPUT    | Head  Light LED     (active LOW)
  P0.19  | RI     | OUTPUT    | Right Indicator LED (active LOW)
  P0.21  | BUZ    | OUTPUT    | Buzzer / Horn       (active HIGH) ← NEW

-----------------------------------------------------------------------
  GPIO PINS — Node A (Port 0, inputs)
-----------------------------------------------------------------------

  Pin    | Function          | Method
 --------+-------------------+-----------------------------
  P0.16  | Left Indi. Button | EINT0 falling-edge ISR
  P0.14  | Right Indi. Button| EINT1 falling-edge ISR
  P0.15  | Head Light Button | EINT2 falling-edge ISR
  P0.20  | Horn Button       | GPIO polled in Timer1 ISR  ← NEW
  P0.28  | Temp sensor (ADC1)| ADC channel 1
  P0.29  | Speed sensor(ADC2)| ADC channel 2

-----------------------------------------------------------------------
  LCD CONNECTIONS (Node B, Port 1 — 4-bit mode)
-----------------------------------------------------------------------

  LCD Signal | LPC P1.xx | IODIR1 bit
 ------------+-----------+-----------
  RS         | P1.17     | bit 17
  RW         | P1.18     | bit 18 (tied LOW for write-only)
  EN         | P1.19     | bit 19
  D4         | P1.20     | bit 20
  D5         | P1.21     | bit 21
  D6         | P1.22     | bit 22
  D7         | P1.23     | bit 23
  D0–D3 → GND (unused in 4-bit mode)

  PINSEL2 = 0  required to use P1.16–P1.31 as GPIO.

-----------------------------------------------------------------------
  HOW THE HORN WORKS (NEW FEATURE)
-----------------------------------------------------------------------

  Node A:
    - P0.20 wired to a momentary push-button (NO, active LOW).
    - Timer1 fires every 1 second and polls IOPIN0 bit-20.
    - On state change (press/release), CAN frame ID=0x217 is sent.
    - byteA = 0x08 on press, 0x09 on release.

  Node B:
    - CAN2 acceptance filter includes 0x217.
    - When ISR receives 0x217:
        0x08 → IOSET0 |= BUZ  (P0.21 HIGH → buzzer ON)
        0x09 → IOCLR0 |= BUZ  (P0.21 LOW  → buzzer OFF)
    - Buzzer is a simple 5V piezo/transistor-driven buzzer.

  For faster horn response (< 1 sec), connect the button to EINT3
  (P0.20 supports EINT3) and add an eint3.c handler following the
  same pattern as eint0.c.

-----------------------------------------------------------------------
  KEIL PROJECT SETUP — Node A
-----------------------------------------------------------------------

  Add these files to the project:
    Amain.c, delay.c, ADC.c, CAN-driver.c,
    eint0.c, eint1.c, eint2.c, timer_interrupt.c

  Target settings:
    Device:  LPC2129
    XTAL:    10 MHz
    Optimization: Level 1 (-O1)

-----------------------------------------------------------------------
  KEIL PROJECT SETUP — Node B
-----------------------------------------------------------------------

  Add these files:
    Bmain.c, delay.c, lcd4-bit-driver.c, CAN-driver.c

  Target settings:
    Device:  LPC2129
    XTAL:    10 MHz
    Optimization: Level 1 (-O1)

=======================================================================
