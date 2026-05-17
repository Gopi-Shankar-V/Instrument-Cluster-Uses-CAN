/*
 * ============================================================
 *  PROJECT : ARM-Based Real-Time Digital Automotive Instrument
 *            Cluster with CAN Protocol Integration
 *  FILE    : header.h  (Node A — Sensor / Input Node)
 *  MCU     : LPC2129 (ARM7TDMI-S, 60 MHz)
 *  AUTHOR  : Your Name
 * ============================================================
 *
 *  NODE A RESPONSIBILITY:
 *    - Reads analog sensors (speed via ADC ch2, engine-temp via ADC ch1)
 *    - Reads digital switches via External Interrupts:
 *        EINT0 (P0.16) → Left  Indicator toggle
 *        EINT1 (P0.14) → Right Indicator toggle
 *        EINT2 (P0.15) → Head-Light   toggle
 *        EINT2-alt     → Horn / Buzzer (NEW — added button on EINT3 or
 *                        a 4th switch handled via GPIO polling)
 *    - Transmits all data to Node B over CAN2 bus
 *
 *  CAN MESSAGE IDs USED:
 *    0x212  → Left  Indicator  (byteA: 0x02=ON , 0x03=OFF)
 *    0x213  → Head Light       (byteA: 0x04=ON , 0x05=OFF)
 *    0x214  → Right Indicator  (byteA: 0x06=ON , 0x07=OFF)
 *    0x215  → Speed ADC value  (byteA: raw 10-bit ADC result, 2 bytes)
 *    0x216  → Engine Temp ADC  (byteA: raw 10-bit ADC result, 2 bytes)
 *    0x217  → Horn / Buzzer    (byteA: 0x08=PRESS, 0x09=RELEASE) ← NEW
 * ============================================================
 */

#ifndef HEADER_H        /* Include-guard: prevents double inclusion */
#define HEADER_H

#include <lpc21xx.h>    /* LPC2129 peripheral register definitions (SFR map) */

/* ─── Portable type aliases ─────────────────────────────────────────────── */
typedef unsigned char       u8;   /* 8-bit  unsigned — used for bytes, chars  */
typedef unsigned int        u32;  /* 32-bit unsigned — used for register vals  */
typedef unsigned short int  u16;  /* 16-bit unsigned — used for ADC results    */

/* ─── ADC "DONE" flag ───────────────────────────────────────────────────── */
/*  ADDR (ADC Data Register) bit-31 = DONE flag.
 *  When DONE=1, the conversion is complete and the result is valid.        */
#define DONE  ((ADDR >> 31) & 1)

/* ─── CAN2 Global Status Register convenience macros ───────────────────── */
/*  C2GSR bit-3 = TCS (Transmit Channel Status) — 1 means TX buffer free.
 *  C2GSR bit-0 = RBS (Receive Buffer Status)   — 1 means a message is waiting. */
#define TCS2  (C2GSR & 8)   /* Transmit complete / buffer free check */
#define RBS2  (C2GSR & 1)   /* Receive buffer has data check          */

/* ─── CAN message structure ─────────────────────────────────────────────── */
/*  One CAN frame carries: identifier, RTR flag, data-length code, 8 bytes.
 *  We split the 8-byte payload into byteA (bytes 0-3) and byteB (bytes 4-7)
 *  because LPC2129 CAN data registers are 32-bit wide.                     */
typedef struct CAN2_MSG
{
    u32 id;     /* 11-bit standard CAN identifier (e.g. 0x212)   */
    u32 rtr;    /* Remote Transmission Request: 0=data, 1=remote  */
    u32 dlc;    /* Data Length Code: number of valid data bytes   */
    u32 byteA;  /* Data bytes [0..3] packed in a 32-bit word      */
    u32 byteB;  /* Data bytes [4..7] packed in a 32-bit word      */
} CAN2;

/* ─── Delay function prototypes (defined in delay.c) ───────────────────── */
extern void delay_sec(unsigned int sec); /* Blocking delay in whole seconds  */
extern void delay_ms (unsigned int ms);  /* Blocking delay in milliseconds    */

/* ─── ADC function prototypes (defined in ADC.c) ───────────────────────── */
extern void adc_init(void);              /* Configure PINSEL + ADCR register  */
extern u32  adc_read(u8 ch_num);         /* Start conversion & return result  */

/* ─── CAN driver prototypes (defined in CAN-driver.c) ──────────────────── */
extern void can2_init(void);             /* Set baud-rate, mode, acceptance filter */
extern void can2_tx(CAN2 v);             /* Transmit one CAN frame            */
extern void can2_rx(CAN2 *ptr);          /* Polling receive of one CAN frame  */

/* ─── External Interrupt prototypes (eint0/1/2/3.c) ────────────────────── */
extern void config_vic_for_eint0(void);  /* VIC slot setup for EINT0 (P0.16) */
extern void config_eint0(void);          /* Edge/polarity config for EINT0    */

extern void config_vic_for_eint1(void);  /* VIC slot setup for EINT1 (P0.14) */
extern void config_eint1(void);          /* Edge/polarity config for EINT1    */

extern void config_vic_for_eint2(void);  /* VIC slot setup for EINT2 (P0.15) */
extern void config_eint2(void);          /* Edge/polarity config for EINT2    */

/* ─── Timer interrupt prototypes (timer_interrupt.c) ───────────────────── */
extern void config_vic_for_timer1(void); /* VIC slot for Timer1 match IRQ     */
extern void en_timer1_interrupt(void);   /* Load MR0, prescaler, start Timer1 */

/* ─── Horn / Buzzer GPIO definitions (Node A side — NEW) ────────────────── */
/*  Horn button  : P0.20 (GPIO input, active-LOW, pulled up externally)
 *  The horn is polled inside the Timer1 ISR every 1 second; for faster
 *  response wire it to EINT3 and add a similar handler like eint0.c.      */
#define HORN_BTN_PIN   (1 << 20)                    /* P0.20 bit mask        */
#define HORN_BTN_PRESSED  (!(IOPIN0 & HORN_BTN_PIN))/* 1 when pin is LOW      */

#endif /* HEADER_H */
