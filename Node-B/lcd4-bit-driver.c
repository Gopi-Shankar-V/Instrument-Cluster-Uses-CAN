/*
 * ============================================================
 *  FILE  : lcd4-bit-driver.c  (Node B)
 *  MCU   : LPC2129
 * ============================================================
 *
 *  PURPOSE:
 *    Drive a standard HD44780-compatible 16×2 character LCD
 *    in 4-BIT interface mode using Port 1 of LPC2129.
 *
 *  LCD PIN → LPC2129 PORT 1 MAPPING:
 *  ┌───────────┬──────────┬────────────────────────────────────┐
 *  │ LCD Pin   │ P1.xx    │ Description                        │
 *  ├───────────┼──────────┼────────────────────────────────────┤
 *  │ RS        │ P1.17    │ Register Select: 0=cmd, 1=data     │
 *  │ RW        │ P1.18    │ Read/Write:      0=write (always)  │
 *  │ EN        │ P1.19    │ Enable:  pulse HIGH→LOW to latch   │
 *  │ D4        │ P1.20    │ Data bit 4 (lower nibble sent 1st) │
 *  │ D5        │ P1.21    │ Data bit 5                         │
 *  │ D6        │ P1.22    │ Data bit 6                         │
 *  │ D7        │ P1.23    │ Data bit 7                         │
 *  └───────────┴──────────┴────────────────────────────────────┘
 *  NOTE: D0–D3 of the LCD are tied to GND (4-bit mode).
 *        IODIR1 mask = 0x00FE0000 (P1.17–P1.23 set as output).
 *        "0xfe<<16" = 0x00FE0000 in the original code.
 *
 *  4-BIT COMMUNICATION SEQUENCE:
 *    1. Clear data lines (D4–D7 = 0)
 *    2. Put the upper nibble of byte onto D4–D7
 *    3. Set RS (1=data / 0=command)
 *    4. Pulse EN HIGH then LOW  → LCD latches upper nibble
 *    5. Repeat for lower nibble
 *
 *  CGRAM CUSTOM CHARACTERS (loaded at startup by lcd_cgram()):
 *    Slot 0 — Left  Arrow  ←
 *    Slot 1 — Sun symbol   ☀
 *    Slot 2 — Right Arrow  →
 *    These are displayed using lcd_data(0), lcd_data(1), lcd_data(2).
 *
 *  DISPLAY LAYOUT:
 *    Row 1: ← (col0)   ☀ (col8)   → (col15)
 *    Row 2: 123km/h        45.6°C
 *           (col0)          (col10)
 * ============================================================
 */

#include "header.h"

/* ─────────────────────────────────────────────────────────────────────────
 *  lcd_data()
 *  Send one data byte to LCD (RS=1 means LCD receives it as a character).
 *  Sends upper nibble first, then lower nibble.
 * ───────────────────────────────────────────────────────────────────────── */
void lcd_data(unsigned char data)
{
    /* ── Send UPPER nibble (bits [7:4]) ──────────────────────────────────── */
    IOCLR1 = 0xFE << 16;           /* Clear P1.17–P1.23 (RS,RW,EN,D4–D7)   */
    IOSET1 = (data & 0xF0) << 16;  /* Place upper nibble [7:4] onto D4–D7   */
                                    /* e.g. data=0xAB → 0xA0 → bits P1.20–23 */
    IOSET1 = 1 << 17;              /* RS = 1 → tells LCD this is DATA byte   */
    IOCLR1 = 1 << 18;              /* RW = 0 → write mode                   */
    IOSET1 = 1 << 19;              /* EN = 1 → rising edge: LCD ready to latch*/
    delay_ms(2);                   /* Wait for LCD to process (≥450 ns min)  */
    IOCLR1 = 1 << 19;              /* EN = 0 → falling edge: LCD latches data */

    /* ── Send LOWER nibble (bits [3:0]) ──────────────────────────────────── */
    IOCLR1 = 0xFE << 16;           /* Clear data and control lines again     */
    IOSET1 = (data & 0x0F) << 20;  /* Place lower nibble [3:0] onto D4–D7   */
                                    /* Shift left 20 places: bit0→P1.20, etc. */
    IOSET1 = 1 << 17;              /* RS = 1 → data byte                    */
    IOCLR1 = 1 << 18;              /* RW = 0 → write mode                   */
    IOSET1 = 1 << 19;              /* EN = 1 → rising edge                   */
    delay_ms(2);                   /* Hold time                              */
    IOCLR1 = 1 << 19;              /* EN = 0 → falling edge: lower nibble latched */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  lcd_cmd()
 *  Send one command byte to LCD (RS=0 means LCD interprets it as a command).
 *  Commands include: set cursor, clear display, set entry mode, etc.
 * ───────────────────────────────────────────────────────────────────────── */
void lcd_cmd(unsigned char cmd)
{
    /* ── Send UPPER nibble of command ────────────────────────────────────── */
    IOCLR1 = 0xFE << 16;           /* Clear all control & data pins          */
    IOSET1 = (cmd & 0xF0) << 16;   /* Upper nibble of command onto D4–D7     */
    IOCLR1 = 1 << 17;              /* RS = 0 → COMMAND (not data)            */
    IOCLR1 = 1 << 18;              /* RW = 0 → write                         */
    IOSET1 = 1 << 19;              /* EN = 1 → rising edge                   */
    delay_ms(2);
    IOCLR1 = 1 << 19;              /* EN = 0 → latch command upper nibble     */

    /* ── Send LOWER nibble of command ────────────────────────────────────── */
    IOCLR1 = 0xFE << 16;
    IOSET1 = (cmd & 0x0F) << 20;   /* Lower nibble onto D4–D7               */
    IOCLR1 = 1 << 17;              /* RS = 0 → still command                */
    IOCLR1 = 1 << 18;              /* RW = 0                                */
    IOSET1 = 1 << 19;              /* EN pulse high                          */
    delay_ms(2);
    IOCLR1 = 1 << 19;              /* EN low → latch                        */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  lcd_init()
 *  Initialise LCD controller in 4-bit mode with 2 display lines.
 *
 *  Sequence (per HD44780 4-bit initialisation procedure):
 *    0x02 → Return home / set 4-bit mode (special init nibble)
 *    0x28 → Function Set: 4-bit, 2 lines, 5×8 dot font
 *    0x0E → Display ON, cursor ON, blink OFF
 *    0x0C → Display ON, cursor OFF, blink OFF (clean display mode)
 *    0x01 → Clear Display and return cursor to home
 * ───────────────────────────────────────────────────────────────────────── */
void lcd_init(void)
{
    IODIR1  = 0xFE << 16;  /* P1.17–P1.23 configured as OUTPUT              */
    PINSEL2 = 0;            /* P1.16–P1.31 = GPIO (not JTAG/trace)           */

    lcd_cmd(0x02);          /* 4-bit mode initialisation sequence            */
    lcd_cmd(0x28);          /* 2-line, 5×8 dots, 4-bit interface            */
    lcd_cmd(0x0E);          /* Display ON, cursor visible                    */
    lcd_cmd(0x0C);          /* Display ON, cursor OFF (for final use)        */
    lcd_cmd(0x01);          /* Clear display, cursor home                    */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  lcd_string()
 *  Send a null-terminated string to the LCD character by character.
 * ───────────────────────────────────────────────────────────────────────── */
void lcd_string(unsigned char *ptr)
{
    while (*ptr)            /* Loop until null terminator '\0'               */
    {
        lcd_data(*ptr);     /* Send each character                           */
        ptr++;              /* Advance pointer to next character             */
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 *  lcd_integer()
 *  Display a signed integer on the LCD.
 *  Extracts digits in reverse, then prints in correct order.
 * ───────────────────────────────────────────────────────────────────────── */
void lcd_integer(int num)
{
    int a[15], i;

    if (num == 0)
    {
        lcd_data('0');      /* Special case: zero digit                      */
        return;
    }
    if (num < 0)
    {
        lcd_data('-');      /* Print minus sign for negative numbers         */
        num = -num;         /* Work with positive magnitude                  */
    }

    i = 0;
    while (num > 0)
    {
        a[i] = (num % 10) + '0'; /* Extract last digit, convert to ASCII    */
        num  = num / 10;          /* Remove last digit                       */
        i++;
    }
    for (i = i - 1; i >= 0; i--)
    {
        lcd_data(a[i]);     /* Print digits in correct order (most→least significant) */
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 *  lcd_float_adc()
 *  Display a float with 1 decimal place. Used for temperature display.
 *  Example: 45.6 → displays "45.6"
 * ───────────────────────────────────────────────────────────────────────── */
void lcd_float_adc(float num)
{
    int temp, a[15], i, j, c = 0, k;

    if (num < 0)
    {
        lcd_data('-');
        num = -num;
    }

    temp = (int)(num * 10);       /* Scale up by 10 to capture 1 decimal    */
    j    = (int)num;              /* Integer part                            */

    /* Count digits in the integer part */
    for (c = 0, k = (int)num; k; c++, k /= 10);

    i = 0;
    if (temp == 0)
    {
        a[i] = 0;
        i++;
    }
    else
    {
        while (temp)
        {
            a[i] = temp % 10;     /* Extract each digit                     */
            temp  = temp / 10;
            i++;
        }
    }

    lcd_integer(j);               /* Print integer part (e.g. "45")         */
    lcd_data('.');                /* Print decimal point                     */
    lcd_integer(a[0]);            /* Print 1 decimal digit (e.g. "6")        */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  lcd_float()
 *  Display a float with multiple decimal places (general purpose).
 * ───────────────────────────────────────────────────────────────────────── */
void lcd_float(float num)
{
    int temp, k = 1000000;

    temp = (int)num;
    if (temp < 0)
    {
        lcd_data('-');
        temp = -temp;
        num  = -num;
    }

    lcd_integer(temp);            /* Print integer portion                   */
    lcd_data('.');

    temp = (int)((num - temp) * k); /* Fractional part scaled up            */
    k    = k / 10;

    while (k > temp)              /* Pad leading zeros after decimal         */
    {
        lcd_data('0');
        k /= 10;
    }
    lcd_integer(temp);            /* Print fractional digits                 */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  lcd_cgram()
 *  Load three custom 5×8 pixel characters into LCD CGRAM.
 *
 *  CGRAM address 0x40 = start of custom character 0 (8 bytes per char).
 *
 *  CHARACTER BITMAP LAYOUT (each u8 = one row, 5 bits used):
 *
 *  Slot 0 — Left Arrow ←       Slot 1 — Sun ☀        Slot 2 — Right Arrow →
 *  ─────────────────────        ─────────────────        ─────────────────────
 *  00000  0x00                  00000  0x00              00000  0x00
 *  00100  0x04                  01110  0x0E              00100  0x04
 *  01000  0x08                  00000  0x00              00010  0x02
 *  11111  0x1F                  11111  0x1F              11111  0x1F
 *  11111  0x1F                  10001  0x11              11111  0x1F
 *  01000  0x08                  10001  0x11              00010  0x02
 *  00100  0x04                  01110  0x0E              00100  0x04
 *  00000  0x00                  00000  0x00              00000  0x00
 * ───────────────────────────────────────────────────────────────────────── */
void lcd_cgram(void)
{
    /*
     * Flat array: 3 characters × 8 rows = 24 bytes.
     * Bytes  0– 7 → CGRAM slot 0 (Left Arrow)
     * Bytes  8–15 → CGRAM slot 1 (Sun)
     * Bytes 16–23 → CGRAM slot 2 (Right Arrow)
     */
    unsigned char bitmaps[] = {
        /* Slot 0: Left Arrow ← */
        0x00, 0x04, 0x08, 0x1F,
        0x1F, 0x08, 0x04, 0x00,

        /* Slot 1: Sun ☀ */
        0x00, 0x0E, 0x00, 0x1F,
        0x11, 0x11, 0x0E, 0x00,

        /* Slot 2: Right Arrow → */
        0x00, 0x04, 0x02, 0x1F,
        0x1F, 0x02, 0x04, 0x00
    };

    unsigned char i;

    lcd_cmd(0x40);              /* Set CGRAM address to 0 (char slot 0, row 0) */

    for (i = 0; i < 24; i++)   /* Write all 24 bitmap rows                  */
    {
        lcd_data(bitmaps[i]);   /* Each lcd_data() call advances CGRAM ptr by 1 */
    }

    lcd_cmd(0x80);              /* Return to DDRAM (normal display mode)     */
                                /* Without this, subsequent lcd_data() would
                                   write into CGRAM instead of the display!  */
}
