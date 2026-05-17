/*
 * ============================================================
 *  PROJECT : ARM-Based Real-Time Digital Automotive Instrument
 *            Cluster with CAN Protocol Integration
 *  FILE    : header.h  (Node B — Display / Output Node)
 *  MCU     : LPC2129
 * ============================================================
 *
 *  NODE B RESPONSIBILITY:
 *    - Receives CAN frames from Node A via CAN2 RX interrupt
 *    - Displays data on 16×2 LCD in 4-bit mode:
 *
 *        ROW 1: ←       ☀       →
 *               [Left]  [Sun]  [Right]   ← CGRAM custom chars
 *                Col0   Col8   Col15
 *
 *        ROW 2: 123km/h    45.6°
 *               Speed      Temperature
 *
 *    - Drives LED indicators via GPIO:
 *        P0.17 = Left  Indicator LED  (LI)
 *        P0.18 = Head Light LED       (HL)
 *        P0.19 = Right Indicator LED  (RI)
 *    - Drives Buzzer/Horn via GPIO:   (NEW)
 *        P0.21 = Buzzer / Horn output (active HIGH)
 *
 *  CAN MESSAGE IDs RECEIVED:
 *    0x212  → Left  Indicator  (byteA: 0x02=ON,  0x03=OFF)
 *    0x213  → Head Light       (byteA: 0x04=ON,  0x05=OFF)
 *    0x214  → Right Indicator  (byteA: 0x06=ON,  0x07=OFF)
 *    0x215  → Speed ADC value  (byteA: 0–1023 raw)
 *    0x216  → Engine Temp ADC  (byteA: 0–1023 raw)
 *    0x217  → Horn             (byteA: 0x08=PRESS, 0x09=RELEASE) ← NEW
 *
 *  LCD PIN MAPPING (4-bit mode, Port 1):
 *    P1.16 = D4   P1.20 = D4 upper nibble
 *    P1.17 = RS   P1.18 = RW   P1.19 = EN
 *    (See lcd4-bit-driver.c for exact bit assignments)
 * ============================================================
 */

#ifndef HEADER_H
#define HEADER_H

#include <lpc21xx.h>   /* LPC2129 SFR definitions                            */

/* ─── Portable type aliases ─────────────────────────────────────────────── */
typedef unsigned char       u8;
typedef unsigned int        u32;
typedef unsigned short int  u16;

/* ─── ADC DONE flag ─────────────────────────────────────────────────────── */
#define DONE  ((ADDR >> 31) & 1)

/* ─── CAN2 status macros ─────────────────────────────────────────────────── */
#define TCS2  (C2GSR & 8)   /* TX complete check (C2GSR bit-3)              */
#define RBS2  (C2GSR & 1)   /* RX buffer has data (C2GSR bit-0)             */

/* ─── GPIO pin assignments on Port 0 (Node B output pins) ──────────────── */
#define HL  (1 << 18)        /* P0.18 — Head Light LED (active LOW)          */
#define LI  (1 << 17)        /* P0.17 — Left  Indicator LED (active LOW)     */
#define RI  (1 << 19)        /* P0.19 — Right Indicator LED (active LOW)     */
#define BUZ (1 << 21)        /* P0.21 — Buzzer / Horn output (active HIGH)   */ /* NEW */

/* ─── CAN message structure ─────────────────────────────────────────────── */
typedef struct CAN2_MSG
{
    u32 id;     /* 11-bit standard CAN identifier                           */
    u32 rtr;    /* Remote Transmission Request flag                         */
    u32 dlc;    /* Data Length Code                                         */
    u32 byteA;  /* Data bytes [0..3]                                        */
    u32 byteB;  /* Data bytes [4..7]                                        */
} CAN2;

/* ─── LCD CGRAM custom character indices ─────────────────────────────────── */
/*  Written by lcd_cgram() into LCD character generator RAM.
 *  Access by sending the index (0,1,2) as LCD data.                        */
#define LCD_CHAR_LEFT_ARROW   0   /* ← custom glyph at CGRAM slot 0        */
#define LCD_CHAR_SUN          1   /* ☀ custom glyph at CGRAM slot 1        */
#define LCD_CHAR_RIGHT_ARROW  2   /* → custom glyph at CGRAM slot 2        */

/* ─── LCD DDRAM addresses ────────────────────────────────────────────────── */
/*  Row 1 starts at 0x80 (cursor position command = 0x80 + col)
 *  Row 2 starts at 0xC0 (cursor position command = 0xC0 + col)            */
#define LCD_ROW1(col)  (0x80 + (col))   /* Row 1 column position command    */
#define LCD_ROW2(col)  (0xC0 + (col))   /* Row 2 column position command    */

/* ─── Function prototypes ───────────────────────────────────────────────── */

/* Delay (delay.c) */
extern void delay_sec(unsigned int sec);
extern void delay_ms (unsigned int ms);

/* LCD driver (lcd4-bit-driver.c) */
extern void lcd_init(void);                  /* Initialise LCD in 4-bit mode */
extern void lcd_cmd(unsigned char cmd);      /* Send command byte to LCD     */
extern void lcd_data(unsigned char data);    /* Send data byte to LCD        */
extern void lcd_string(unsigned char *ptr);  /* Send null-terminated string  */
extern void lcd_integer(int num);            /* Display an integer           */
extern void lcd_float_adc(float num);        /* Display float (1 decimal)    */
extern void lcd_float(float num);            /* Display float (multi decimal)*/
extern void lcd_cgram(void);                 /* Load custom chars into CGRAM */

/* CAN driver (CAN-driver.c in NodeB) */
extern void can2_init(void);                 /* Init CAN2 with acceptance filter */
extern void can2_tx(CAN2 v);                 /* Transmit one frame           */
extern void can2_rx(CAN2 *ptr);              /* Polling receive              */
extern void config_vic_for_CAN2(void);       /* VIC setup for CAN2 RX IRQ   */
extern void en_can2_interrupt(void);         /* Enable CAN2 RX interrupt     */

#endif /* HEADER_H */
