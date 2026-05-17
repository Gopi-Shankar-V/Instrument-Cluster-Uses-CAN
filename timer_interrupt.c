/*
 * ============================================================
 *  FILE  : timer_interrupt.c  (Node A)
 *  MCU   : LPC2129
 * ============================================================
 *
 *  PURPOSE:
 *    Configure Timer 1 to generate a periodic interrupt every 1 second.
 *    On each interrupt, the ISR sets global flag = 1.
 *    Main loop detects flag and:
 *      1. Reads speed ADC value     → sends CAN ID 0x215
 *      2. Reads engine temp ADC     → sends CAN ID 0x216
 *      3. Polls horn button P0.20   → sends CAN ID 0x217 on change
 *
 *  HOW TIMER1 GENERATES 1-SECOND INTERRUPTS:
 *    - PCLK = 60 MHz (when VPBDIV = 1)
 *    - Prescaler Register T1PR = PCLK - 1 = 59,999,999
 *    - Timer Counter T1TC increments every (T1PR+1) PCLK cycles = 1 second
 *    - Match Register T1MR0 = 1 → match fires when T1TC reaches 1
 *    - T1MCR = 3 → on match: generate interrupt (bit-0) AND reset TC (bit-1)
 *    - Result: interrupt fires exactly every 1 second
 *
 *  REGISTERS USED:
 *    T1MCR   — Match Control Register: bits [1:0] for MR0
 *              bit-0 = MR0I (interrupt on match), bit-1 = MR0R (reset on match)
 *    T1PC    — Prescale Counter (read-only while running, clear by writing 0)
 *    T1PR    — Prescale Register (divider)
 *    T1TC    — Timer Counter
 *    T1MR0   — Match Register 0 (comparison value)
 *    T1TCR   — Timer Control Register: bit-0=enable, bit-1=reset
 *    T1IR    — Interrupt Register: write 1 to bit-0 to clear MR0 match flag
 *
 *  VIC SOURCE NUMBER:
 *    Timer 1 = source 5
 * ============================================================
 */

#include "header.h"

/* PCLK frequency lookup table indexed by VPBDIV value */
static const int pclk_mhz[] = {15, 60, 30, 15, 15};

extern u8 flag;    /* Shared with Amain.c — set in ISR, cleared in main loop */

/* ─────────────────────────────────────────────────────────────────────────
 *  TIMER1_Handler()  — Timer 1 Match Interrupt Service Routine
 *
 *  Fires every 1 second when T1TC matches T1MR0.
 *  Timer resets automatically (T1MCR bit-1 = MR0R set).
 * ───────────────────────────────────────────────────────────────────────── */
void TIMER1_Handler(void) __irq   /* __irq: CPSR saved/restored by compiler  */
{
    flag = 1;           /* Tell main loop: 1 second elapsed → sample ADC    */

    T1IR = 1;           /* Clear Timer1 MR0 interrupt flag
                           T1IR bit-0 = MR0 interrupt flag
                           Write 1 to clear (writing 0 has no effect)       */

    VICVectAddr = 0;    /* Signal VIC that ISR is complete                   */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  config_vic_for_timer1()
 *  Register TIMER1_Handler in VIC priority slot 2.
 * ───────────────────────────────────────────────────────────────────────── */
void config_vic_for_timer1(void)
{
    VICIntSelect = 0;              /* All sources → IRQ (bit=0), not FIQ     */

    /*
     * VICVectCntl2 — slot 2:
     *   [4:0] = 5  → source 5 = Timer1
     *   [5]   = 1  → enable this vectored slot
     * Combined: 5 | (1<<5) = 5 | 32 = 37 = 0x25
     */
    VICVectCntl2 = 5 | (1 << 5);  /* Timer1 source into VIC slot 2          */

    VICVectAddr2 = (u32)TIMER1_Handler; /* ISR address for slot 2            */

    VICIntEnable |= (1 << 5);     /* Enable Timer1 interrupt (bit-5) in VIC */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  en_timer1_interrupt()
 *  Configure Timer1 for a 1-second match period and start it.
 *
 *  FORMULA:
 *    T1PR  = PCLK - 1       → TC increments once per second
 *    T1MR0 = 1              → match fires after 1 TC tick = 1 second
 *    T1MCR = 3              → interrupt + reset TC on MR0 match
 * ───────────────────────────────────────────────────────────────────────── */
void en_timer1_interrupt(void)
{
    unsigned int pclk;

    /*
     * T1MCR = 3:
     *   bit-0 (MR0I) = 1 → Generate interrupt when T1TC == T1MR0
     *   bit-1 (MR0R) = 1 → Reset T1TC to 0 after match (auto-reload)
     * This makes Timer1 repeat every T1MR0+1 TC ticks indefinitely.
     */
    T1MCR = 3;              /* Interrupt on MR0 match AND reset counter       */

    /* Compute PCLK in Hz from lookup table */
    pclk = pclk_mhz[VPBDIV] * 1000000;  /* e.g. VPBDIV=1 → 60,000,000 Hz  */

    T1PC  = 0;              /* Clear prescale counter (start fresh)           */

    T1PR  = pclk - 1;       /* Prescale: TC ticks once every pclk PCLK cycles
                                = once per second at 60 MHz PCLK             */

    T1TC  = 0;              /* Reset timer counter to 0                      */

    T1MR0 = 1;              /* Match value: interrupt fires when T1TC = 1
                                Since TC resets on match, period = 1 second  */

    T1TCR = 1;              /* bit-0 = 1: Enable (start) Timer 1             */
}
