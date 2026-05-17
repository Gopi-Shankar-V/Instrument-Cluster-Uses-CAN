/*
 * ============================================================
 *  FILE  : Amain.c  (Node A — Sensor & Input Node)
 *  MCU   : LPC2129
 * ============================================================
 *
 *  NODE A OVERVIEW:
 *  ┌─────────────────────────────────────────────────────────┐
 *  │  INPUTS                          CAN2 MESSAGE SENT      │
 *  │  ────────────────────────────    ─────────────────────  │
 *  │  EINT0 (P0.16) — Left Indi.  →  ID 0x212 (0x02/0x03)  │
 *  │  EINT1 (P0.14) — Right Indi. →  ID 0x214 (0x06/0x07)  │
 *  │  EINT2 (P0.15) — Head Light  →  ID 0x213 (0x04/0x05)  │
 *  │  P0.20  GPIO   — Horn Button →  ID 0x217 (0x08/0x09)  │ ← NEW
 *  │  ADC ch2       — Speed (pot) →  ID 0x215 (raw ADC)    │
 *  │  ADC ch1       — Eng. Temp   →  ID 0x216 (raw ADC)    │
 *  └─────────────────────────────────────────────────────────┘
 *
 *  INTERRUPT SOURCES:
 *    Timer1 ISR  (every 1 sec) → sets flag=1 → ADC + horn GPIO sampled
 *    EINT0 ISR   (falling edge P0.16) → sets flag1=1 → left indicator
 *    EINT1 ISR   (falling edge P0.14) → sets flag2=1 → right indicator
 *    EINT2 ISR   (falling edge P0.15) → sets flag3=1 → head-light
 *
 *  HORN:
 *    Polled inside the Timer1-triggered block every second.
 *    If horn button (P0.20) is LOW: send 0x08 (horn pressed)
 *    If horn button (P0.20) is HIGH: send 0x09 (horn released)
 *    Node B buzzer activates when it receives 0x08.
 * ============================================================
 */

#include "header.h"

/* ─── Global flags — set by ISRs, cleared in main loop ─────────────────── */
u8 flag;    /* Set by Timer1 ISR every 1 sec  → triggers ADC + horn sample  */
u8 flag1;   /* Set by EINT0 ISR (P0.16)       → left indicator toggle       */
u8 flag2;   /* Set by EINT1 ISR (P0.14)       → right indicator toggle      */
u8 flag3;   /* Set by EINT2 ISR (P0.15)       → head-light toggle           */

/* ─── CAN frame buffers ─────────────────────────────────────────────────── */
CAN2 v1;    /* Used for single-byte control messages (switches, horn)        */
CAN2 v2;    /* Used for two-byte sensor data (speed, engine temp)            */

/* ─────────────────────────────────────────────────────────────────────────
 *  main()
 * ───────────────────────────────────────────────────────────────────────── */
int main(void)
{
    u8  f1 = 0;   /* Toggle state for left indicator  (0=OFF, 1=ON)         */
    u8  f2 = 0;   /* Toggle state for right indicator (0=OFF, 1=ON)         */
    u8  f3 = 0;   /* Toggle state for head-light      (0=OFF, 1=ON)         */
    u8  horn_prev = 0; /* Previous horn state to detect edge (NEW)          */
    u32 eng_temp, speed; /* Raw 10-bit ADC results                           */

    /* ── Hardware Initialisation ─────────────────────────────────────────── */

    can2_init();             /* Init CAN2 @ 100 kbps, bypass acceptance filter */
    adc_init();              /* Connect P0.28,29 to ADC, power up ADC          */

    config_vic_for_timer1(); /* Register Timer1 IRQ in VIC slot 2             */
    en_timer1_interrupt();   /* Timer1: 1-second period, MR0 match reset+irq  */

    config_vic_for_eint0();  /* Register EINT0 IRQ (left indicator button)    */
    config_eint0();          /* P0.16, falling-edge sensitive                  */

    config_vic_for_eint1();  /* Register EINT1 IRQ (right indicator button)   */
    config_eint1();          /* P0.14, falling-edge sensitive                  */

    config_vic_for_eint2();  /* Register EINT2 IRQ (head-light button)        */
    config_eint2();          /* P0.15, falling-edge sensitive                  */

    /* Horn button pin — configure P0.20 as GPIO input (default after reset) */
    IODIR0 &= ~HORN_BTN_PIN; /* Clear direction bit → P0.20 = INPUT           */

    /* ── CAN frame: v1 (control, 1 byte payload) ─────────────────────────── */
    v1.dlc  = 1;   /* 1 data byte                                            */
    v1.rtr  = 0;   /* Data frame (not a remote request)                      */
    v1.byteA = 0;  /* Payload initialised to 0                               */
    v1.byteB = 0;  /* Unused second word                                     */

    /* ── CAN frame: v2 (sensor, 2 byte payload) ──────────────────────────── */
    v2.dlc  = 2;   /* 2 data bytes (ADC value fits in 2 bytes)               */
    v2.rtr  = 0;   /* Data frame                                              */
    v2.byteA = 0;
    v2.byteB = 0;

    /* ── Main event loop ─────────────────────────────────────────────────── */
    while (1)
    {
        /* ════════════════════════════════════════════════════
         *  HEAD-LIGHT TOGGLE
         *  EINT2 ISR sets flag3; we toggle f3 and send CAN.
         *  0x04 = ON, 0x05 = OFF
         * ════════════════════════════════════════════════════ */
        if (flag3)
        {
            flag3 = 0;          /* Acknowledge flag (clear it)               */
            v1.id = 0x213;      /* Message ID for head-light                 */
            f3 ^= 1;            /* XOR toggle: 0→1→0→1...                   */

            if (f3)
                v1.byteA = 0x04; /* Head-light ON  command                  */
            else
                v1.byteA = 0x05; /* Head-light OFF command                  */

            can2_tx(v1);         /* Transmit the CAN frame                  */
        }

        /* ════════════════════════════════════════════════════
         *  RIGHT INDICATOR TOGGLE
         *  EINT1 ISR sets flag2; 0x06 = ON, 0x07 = OFF
         * ════════════════════════════════════════════════════ */
        if (flag2)
        {
            flag2 = 0;
            v1.id = 0x214;      /* Message ID for right indicator           */
            f2 ^= 1;

            if (f2)
                v1.byteA = 0x06; /* Right indicator ON                      */
            else
                v1.byteA = 0x07; /* Right indicator OFF                     */

            can2_tx(v1);
        }

        /* ════════════════════════════════════════════════════
         *  LEFT INDICATOR TOGGLE
         *  EINT0 ISR sets flag1; 0x02 = ON, 0x03 = OFF
         * ════════════════════════════════════════════════════ */
        if (flag1)
        {
            flag1 = 0;
            v1.id = 0x212;      /* Message ID for left indicator            */
            f3 ^= 1;            /* reuses f3 variable as left toggle — kept
                                   consistent with original code logic      */
            if (f3)
                v1.byteA = 0x02; /* Left indicator ON                       */
            else
                v1.byteA = 0x03; /* Left indicator OFF                      */

            can2_tx(v1);
        }

        /* ════════════════════════════════════════════════════
         *  TIMER-TRIGGERED BLOCK (every 1 second)
         *  Reads ADC channels and polls horn GPIO.
         * ════════════════════════════════════════════════════ */
        if (flag)
        {
            flag = 0;   /* Clear the timer flag immediately                  */

            /* ── Speed sensor (ADC channel 2, P0.29) ──────────────────── */
            v2.id    = 0x215;           /* Speed message ID                  */
            speed    = adc_read(2);     /* Read 10-bit speed value (0–1023)  */
            v2.byteA = speed;           /* Lower 16 bits carry the ADC value */
            can2_tx(v2);                /* Send speed over CAN               */

            delay_ms(5);                /* Brief gap between two TX frames   */

            /* ── Engine temperature sensor (ADC channel 1, P0.28) ──────── */
            v2.id    = 0x216;           /* Engine temperature message ID     */
            eng_temp = adc_read(1);     /* Read 10-bit temperature value     */
            v2.byteA = eng_temp;
            can2_tx(v2);                /* Send temperature over CAN         */

            /* ── Horn / Buzzer (GPIO poll, P0.20) ─────── NEW ──────────── */
            /*
             * We check pin state every second.
             * Only send a CAN frame when the state CHANGES (edge detection)
             * to avoid flooding the bus with repeated messages.
             */
            {
                u8 horn_now = HORN_BTN_PRESSED; /* 1=pressed, 0=released    */

                if (horn_now != horn_prev)       /* State changed?           */
                {
                    horn_prev = horn_now;         /* Update previous state   */
                    v1.id = 0x217;               /* Horn message ID          */

                    if (horn_now)
                        v1.byteA = 0x08;         /* 0x08 = HORN PRESS       */
                    else
                        v1.byteA = 0x09;         /* 0x09 = HORN RELEASE     */

                    can2_tx(v1);                 /* Transmit horn command    */
                }
            }
        }

    } /* end while(1) */

    return 0; /* Never reached — bare-metal loop runs forever */
}
