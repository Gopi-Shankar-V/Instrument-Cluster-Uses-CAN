/*
 * ============================================================
 *  FILE  : Bmain.c  (Node B — Display & Output Node)
 *  MCU   : LPC2129
 * ============================================================
 *
 *  DISPLAY LAYOUT ON 16×2 LCD:
 *
 *  ┌────────────────────────────────────┐
 *  │ Col: 0123456789012345              │
 *  │ ROW1: ←       ☀       →           │
 *  │       ^       ^       ^            │
 *  │      col0   col8   col15           │
 *  │                                    │
 *  │ ROW2: 123km/h    45.6°             │
 *  │       ^          ^                 │
 *  │      col0       col10              │
 *  └────────────────────────────────────┘
 *
 *  Row 1 shows indicator arrows (← →) and head-light (☀) status.
 *  Row 2 shows speed in km/h and engine temperature in °C.
 *
 *  GPIO OUTPUT PINS (Port 0):
 *    P0.17 = Left  Indicator LED  (LI)  — active LOW (IOCLR turns ON)
 *    P0.18 = Head  Light   LED   (HL)  — active LOW
 *    P0.19 = Right Indicator LED  (RI)  — active LOW
 *    P0.21 = Buzzer / Horn output (BUZ) — active HIGH (IOSET turns ON) NEW
 *
 *  CAN MESSAGES HANDLED:
 *    ID 0x212 → Left  Indicator toggle (0x02=ON / 0x03=OFF)
 *    ID 0x213 → Head Light      toggle (0x04=ON / 0x05=OFF)
 *    ID 0x214 → Right Indicator toggle (0x06=ON / 0x07=OFF)
 *    ID 0x215 → Speed ADC value        (raw 0–1023 → converted to km/h %)
 *    ID 0x216 → Engine Temp ADC        (raw 0–1023 → °C via LM35 formula)
 *    ID 0x217 → Horn / Buzzer          (0x08=BEEP ON / 0x09=BEEP OFF) NEW
 * ============================================================
 */

#include "header.h"

/* ─── Global variables — shared with ISR in CAN-driver.c ───────────────── */
CAN2 m1;     /* Latest received CAN frame — filled by CAN2_RX_Handler ISR   */
u32  flag;   /* Set to 1 by CAN ISR; cleared in main loop after processing   */

/* ─────────────────────────────────────────────────────────────────────────
 *  display_init_row1()
 *  Draw the static skeleton of Row 1 at startup:
 *    ←       ☀       →
 *  The arrow and sun characters will be blank until signals arrive.
 *  We pre-fill spaces so the layout is predictable.
 * ───────────────────────────────────────────────────────────────────────── */
static void display_init_row1(void)
{
    lcd_cmd(LCD_ROW1(0));       /* Move cursor to Row 1, Column 0           */
    lcd_data(' ');              /* Col 0: space (left arrow shown when active)*/
    lcd_string("       ");      /* Cols 1–7: spaces                          */
    lcd_data(' ');              /* Col 8: space (sun/headlight when active)  */
    lcd_string("       ");      /* Cols 9–15: spaces                         */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  display_init_row2()
 *  Draw the static skeleton of Row 2 at startup.
 *  Speed and temperature slots show '---' until first CAN data arrives.
 * ───────────────────────────────────────────────────────────────────────── */
static void display_init_row2(void)
{
    lcd_cmd(LCD_ROW2(0));       /* Move cursor to Row 2, Column 0            */
    lcd_string("---km/h   --.-"); /* Placeholder text fills all 16 columns   */
    lcd_data(0xDF);             /* 0xDF = ° symbol in LCD ROM character set   */
    lcd_data('C');
}

/* ─────────────────────────────────────────────────────────────────────────
 *  main()
 * ───────────────────────────────────────────────────────────────────────── */
int main(void)
{
    u8  f1 = 0;     /* Left  indicator blink toggle state: 0=OFF, 1=BLINKING */
    u8  f2 = 0;     /* Right indicator blink toggle state: 0=OFF, 1=BLINKING */
    u32 speed, per; /* Raw ADC speed and converted percentage                 */
    float eng_val, vol, temp_c; /* Temperature conversion intermediates      */

    /* ── Initialise peripherals ─────────────────────────────────────────── */
    can2_init();                /* CAN2 @ 100 kbps, acceptance filter for IDs */
    config_vic_for_CAN2();      /* Register CAN2 ISR in VIC                   */
    en_can2_interrupt();        /* Enable CAN2 RX interrupt (C2IER bit-0)     */

    lcd_init();                 /* Init 16×2 LCD in 4-bit mode                */
    lcd_cgram();                /* Load ←, ☀, → custom chars into CGRAM      */

    /* ── Configure GPIO outputs ─────────────────────────────────────────── */
    /*
     * HL  = P0.18, LI = P0.17, RI = P0.19, BUZ = P0.21
     * IODIR0: set these pins as OUTPUT (1 = output)
     * IOSET0: drive them HIGH initially (LEDs OFF since active-low;
     *          Buzzer OFF since active-HIGH needs explicit IOCLR or LOW)
     */
    IODIR0 |= HL | LI | RI | BUZ;  /* Configure as outputs                  */
    IOSET0  = HL | LI | RI;         /* LEDs OFF (active low = IOSET = OFF)   */
    IOCLR0  = BUZ;                   /* Buzzer OFF (active high = IOCLR = OFF) */

    /* ── Draw initial LCD frame skeleton ────────────────────────────────── */
    display_init_row1();        /* Row 1: ←  [spaces]  ☀  [spaces]  →       */
    display_init_row2();        /* Row 2: ---km/h   --.-°C                   */

    /* ── Main event loop ─────────────────────────────────────────────────── */
    while (1)
    {
        /* ════════════════════════════════════════════════════════════════
         *  CAN FRAME RECEIVED — process flag set by CAN ISR
         * ════════════════════════════════════════════════════════════════ */
        l:                          /* Label for goto after blink loop (kept
                                       from original code for reference)    */
        if (flag)
        {
            flag = 0;               /* Clear flag; ISR may set it again any time */

            /* ── ID 0x215 — Speed sensor data ────────────────────────── */
            if (m1.id == 0x215)
            {
                speed = m1.byteA;       /* Raw ADC result: 0 to 1023         */
                per   = ((float)speed / 1023.0f) * 200; /* Map 0–1023 → 0–200 km/h */

                lcd_cmd(LCD_ROW2(0));   /* Cursor to Row 2, Col 0            */
                lcd_integer(per);       /* Display speed, e.g. "123"         */
                lcd_string("km/h  ");   /* Unit label + spaces to clear old  */
            }

            /* ── ID 0x216 — Engine Temperature ───────────────────────── */
            if (m1.id == 0x216)
            {
                eng_val = (float)m1.byteA;       /* Raw ADC 0–1023           */
                vol     = (eng_val * 3.3f) / 1023.0f; /* ADC count → Volts  */
                temp_c  = (vol - 0.5f) / 0.01f;  /* LM35: 10mV/°C, 500mV@0°C*/

                lcd_cmd(LCD_ROW2(10));  /* Cursor to Row 2, Col 10           */
                lcd_float_adc(temp_c);  /* Display "45.6"                    */
                lcd_data(0xDF);         /* 0xDF = ° degree symbol in LCD ROM */
                lcd_data('C');          /* Append 'C' → "45.6°C"            */
            }

            /* ── ID 0x213 — Head Light ─────────────────────────────────── */
            if (m1.id == 0x213)
            {
                if ((m1.byteA & 0xF) == 0x04)      /* Head light ON command  */
                {
                    lcd_cmd(LCD_ROW1(8));            /* Col 8: sun position   */
                    lcd_data(LCD_CHAR_SUN);          /* Display ☀ custom char */
                    IOCLR0 = HL;                     /* Turn on Head Light LED */
                }
                if ((m1.byteA & 0xF) == 0x05)      /* Head light OFF command */
                {
                    lcd_cmd(LCD_ROW1(8));            /* Col 8                 */
                    lcd_data(' ');                   /* Erase sun symbol       */
                    IOSET0 = HL;                     /* Turn off Head Light LED */
                }
            }

            /* ── ID 0x212 — Left Indicator ─────────────────────────────── */
            if (m1.id == 0x212)
            {
                if ((m1.byteA & 0xF) == 0x02)  /* Left indicator ON          */
                {
                    f1 = 0;             /* Ensure right blink is stopped      */
                    f2 = 1;             /* Set left blink flag active         */
                }
                if ((m1.byteA & 0xF) == 0x03)  /* Left indicator OFF         */
                {
                    f2 = 0;             /* Stop left blink                    */
                    lcd_cmd(LCD_ROW1(0));
                    lcd_data(' ');      /* Erase left arrow from Row 1        */
                    IOSET0 = LI;        /* Turn off Left Indicator LED        */
                }
            }

            /* ── ID 0x214 — Right Indicator ────────────────────────────── */
            if (m1.id == 0x214)
            {
                if ((m1.byteA & 0xF) == 0x06)  /* Right indicator ON         */
                {
                    f2 = 0;             /* Ensure left blink is stopped       */
                    f1 = 1;             /* Set right blink flag active        */
                }
                if ((m1.byteA & 0xF) == 0x07)  /* Right indicator OFF        */
                {
                    f1 = 0;             /* Stop right blink                   */
                    lcd_cmd(LCD_ROW1(15));
                    lcd_data(' ');      /* Erase right arrow from Row 1       */
                    IOSET0 = RI;        /* Turn off Right Indicator LED       */
                }
            }

            /* ── ID 0x217 — Horn / Buzzer ──── NEW ─────────────────────── */
            if (m1.id == 0x217)
            {
                if ((m1.byteA & 0xF) == 0x08)  /* Horn PRESSED               */
                {
                    IOSET0 = BUZ;       /* Drive P0.21 HIGH → Buzzer ON      */
                    /*
                     * Optional: display "HORN" on LCD Row 1 center area
                     * Uncomment if a visual horn indicator is desired:
                     *
                     * lcd_cmd(LCD_ROW1(4));
                     * lcd_string("HORN");
                     */
                }
                if ((m1.byteA & 0xF) == 0x09)  /* Horn RELEASED              */
                {
                    IOCLR0 = BUZ;       /* Drive P0.21 LOW  → Buzzer OFF     */
                    /*
                     * lcd_cmd(LCD_ROW1(4));
                     * lcd_string("    ");   // Clear "HORN" text
                     */
                }
            }

        } /* end if(flag) */

        /* ════════════════════════════════════════════════════════════════
         *  LEFT INDICATOR BLINK (f2 = 1 while blinking)
         *  Blink ← at Col 0 of Row 1 at 250 ms period.
         *  Also blinks the physical LED on P0.17.
         * ════════════════════════════════════════════════════════════════ */
        if (f2)
        {
            while (flag == 0)               /* Blink until next CAN frame    */
            {
                lcd_cmd(LCD_ROW1(0));       /* Cursor to Row 1, Col 0        */
                lcd_data(LCD_CHAR_LEFT_ARROW); /* Show ← custom char         */
                IOCLR0 = LI;               /* Turn ON Left Indicator LED     */
                delay_ms(250);             /* ON for 250 ms                  */

                lcd_cmd(LCD_ROW1(0));
                lcd_data(' ');             /* Erase ← (blank = LED off visual)*/
                IOSET0 = LI;              /* Turn OFF Left Indicator LED     */
                delay_ms(250);            /* OFF for 250 ms                  */
            }
            goto l;                        /* Re-check flag immediately       */
        }

        /* ════════════════════════════════════════════════════════════════
         *  RIGHT INDICATOR BLINK (f1 = 1 while blinking)
         *  Blink → at Col 15 of Row 1 at 250 ms period.
         * ════════════════════════════════════════════════════════════════ */
        if (f1)
        {
            while (flag == 0)
            {
                lcd_cmd(LCD_ROW1(15));      /* Cursor to Row 1, Col 15       */
                lcd_data(LCD_CHAR_RIGHT_ARROW); /* Show → custom char        */
                IOCLR0 = RI;               /* Turn ON Right Indicator LED    */
                delay_ms(250);

                lcd_cmd(LCD_ROW1(15));
                lcd_data(' ');             /* Erase →                        */
                IOSET0 = RI;
                delay_ms(250);
            }
            goto l;
        }

    } /* end while(1) */

    return 0;
}
