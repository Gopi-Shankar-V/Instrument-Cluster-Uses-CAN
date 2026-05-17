/*
 * ============================================================
 *  FILE  : delay.c  (Node B — identical to Node A delay.c)
 *  MCU   : LPC2129 @ 60 MHz
 * ============================================================
 *
 *  Both nodes use Timer 0 for software blocking delays.
 *  Timer 0 is free on Node B (not used for any interrupt).
 *  See NodeA/delay.c for detailed explanation.
 * ============================================================
 */

#include "header.h"

/* PCLK frequency lookup: index = VPBDIV value */
static const int pclk_mhz[] = {15, 60, 30, 15, 15};

/* ─────────────────────────────────────────────────────────────────────────
 *  delay_sec()  — Blocking delay, whole seconds, using Timer 0
 * ───────────────────────────────────────────────────────────────────────── */
void delay_sec(unsigned int sec)
{
    unsigned int pclk;

    pclk = pclk_mhz[VPBDIV] * 1000000; /* PCLK in Hz (60 000 000 typically) */

    T0PC  = 0;          /* Clear prescale counter                            */
    T0PR  = pclk - 1;   /* TC increments once per second                     */
    T0TC  = 0;          /* Reset the timer counter                           */
    T0TCR = 1;          /* Start Timer 0                                     */

    while (T0TC < sec); /* Busy-wait loop                                    */

    T0TCR = 0;          /* Stop Timer 0                                      */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  delay_ms()  — Blocking delay in milliseconds, using Timer 0
 * ───────────────────────────────────────────────────────────────────────── */
void delay_ms(unsigned int ms)
{
    unsigned int pclk;

    pclk = pclk_mhz[VPBDIV] * 1000;    /* PCLK in kHz (60 000 typically)    */

    T0PC  = 0;          /* Clear prescale counter                            */
    T0PR  = pclk - 1;   /* TC increments once per millisecond                */
    T0TC  = 0;          /* Reset the timer counter                           */
    T0TCR = 1;          /* Start Timer 0                                     */

    while (T0TC < ms);  /* Busy-wait loop                                    */

    T0TCR = 0;          /* Stop Timer 0                                      */
}
