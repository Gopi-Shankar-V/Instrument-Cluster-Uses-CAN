/*
 * ============================================================
 *  FILE  : delay.c  (Node A)
 *  MCU   : LPC2129 @ 60 MHz (VPBDIV=1 → PCLK = CCLK = 60 MHz)
 * ============================================================
 *
 *  PURPOSE:
 *    Provides two blocking delay functions using Timer 0.
 *    "Blocking" means the CPU spins (polls) until the time
 *    expires — no interrupts involved.
 *
 *  HOW LPC2129 TIMER WORKS (brief):
 *    Timer counter TC increments every (PR+1) PCLK cycles.
 *    Prescale Register (PR) divides PCLK before TC increments.
 *    By setting PR = PCLK - 1, TC increments exactly once per second.
 *    By setting PR = PCLK/1000 - 1, TC increments once per millisecond.
 *
 *  REGISTER MAP USED:
 *    T0PC  — Timer 0 Prescale Counter (read-only while running)
 *    T0PR  — Timer 0 Prescale Register  (write the divider value)
 *    T0TC  — Timer 0 Timer Counter      (the actual tick counter)
 *    T0TCR — Timer 0 Timer Control Reg  (bit0=enable, bit1=reset)
 *    VPBDIV — Peripheral clock divider  (0=¼, 1=1, 2=½ of CCLK)
 * ============================================================
 */

#include "header.h"

/* ─── PCLK lookup table ─────────────────────────────────────────────────── */
/*  VPBDIV register can hold 0,1,2 — maps to PCLK divisors 4,1,2 of CCLK.
 *  Crystal = 10 MHz, PLL × 6 → CCLK = 60 MHz.
 *  Array index = VPBDIV value → PCLK in MHz.
 *  Index:  0      1      2      (3 & 4 duplicated for safety)
 *  Value: 15MHz  60MHz  30MHz                                              */
static const int pclk_mhz[] = {15, 60, 30, 15, 15};

/* ─────────────────────────────────────────────────────────────────────────
 *  delay_sec()
 *  Busy-wait for exactly 'sec' whole seconds.
 *
 *  TRICK: PR = pclk - 1 means TC ticks once per second.
 *  So we wait until TC reaches 'sec'.
 * ───────────────────────────────────────────────────────────────────────── */
void delay_sec(unsigned int sec)
{
    unsigned int pclk;

    /* Read PCLK frequency (in Hz) from lookup table indexed by VPBDIV */
    pclk = pclk_mhz[VPBDIV] * 1000000;  /* e.g. VPBDIV=1 → 60,000,000 Hz */

    T0PC  = 0;          /* Reset prescale counter (start clean)            */
    T0PR  = pclk - 1;   /* TC increments every pclk PCLK cycles = 1 sec   */
    T0TC  = 0;          /* Reset timer counter to zero                      */
    T0TCR = 1;          /* Bit-0=1: Enable (start) Timer 0                 */

    while (T0TC < sec); /* Busy-wait: spin until TC reaches desired seconds */

    T0TCR = 0;          /* Bit-0=0: Stop Timer 0 (conserve power)          */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  delay_ms()
 *  Busy-wait for exactly 'ms' milliseconds.
 *
 *  TRICK: PR = (pclk/1000) - 1 means TC ticks once per millisecond.
 * ───────────────────────────────────────────────────────────────────────── */
void delay_ms(unsigned int ms)
{
    unsigned int pclk;

    /* pclk in kHz (×1000) so that PR gives 1 ms per TC tick */
    pclk = pclk_mhz[VPBDIV] * 1000;     /* e.g. VPBDIV=1 → 60,000 (kHz)  */

    T0PC  = 0;          /* Reset prescale counter                          */
    T0PR  = pclk - 1;   /* TC increments every pclk PCLK cycles = 1 ms    */
    T0TC  = 0;          /* Reset timer counter                              */
    T0TCR = 1;          /* Start Timer 0                                   */

    while (T0TC < ms);  /* Spin until TC reaches desired milliseconds       */

    T0TCR = 0;          /* Stop Timer 0                                     */
}
