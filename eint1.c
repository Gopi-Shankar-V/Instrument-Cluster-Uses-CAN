/*
 * ============================================================
 *  FILE  : eint1.c  (Node A)
 *  MCU   : LPC2129
 * ============================================================
 *
 *  PURPOSE:
 *    Configure External Interrupt 1 (EINT1) on pin P0.14 to detect
 *    a button press for the RIGHT INDICATOR toggle.
 *
 *  HOW IT WORKS:
 *    - P0.14 is configured as EINT1 via PINSEL0 (P0.0–P0.15).
 *    - EINT1 triggers on FALLING EDGE (active-low button).
 *    - ISR sets global flag2 = 1.
 *    - Main loop detects flag2, toggles right indicator state,
 *      and sends CAN message ID 0x214.
 *
 *  NOTE ON PINSEL:
 *    PINSEL0 controls P0.0–P0.15 (2 bits per pin).
 *    PINSEL1 controls P0.16–P0.31 (2 bits per pin).
 *    P0.14 is in PINSEL0, bits [29:28].
 *    EINT1 on P0.14 → PINSEL0[29:28] = 10 → value 0x20000000
 *
 *  VIC SOURCE NUMBER:
 *    EINT1 = source 15
 * ============================================================
 */

#include "header.h"

extern u8 flag2;   /* Shared with Amain.c — set here, cleared in main loop  */

/* ─────────────────────────────────────────────────────────────────────────
 *  EINT1_Handler()  — External Interrupt 1 ISR
 *
 *  Fires on falling edge of P0.14 (right indicator button pressed).
 * ───────────────────────────────────────────────────────────────────────── */
void EINT1_Handler(void) __irq
{
    flag2 = 1;          /* Signal main loop: right indicator button pressed  */

    EXTINT |= 2;        /* Clear EINT1 flag: bit-1 of EXTINT register
                           Must write 1 to the corresponding bit to clear it */

    VICVectAddr = 0;    /* Acknowledge VIC — marks end of ISR execution      */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  config_vic_for_eint1()
 *  Register EINT1_Handler into VIC priority slot 0 (highest priority).
 * ───────────────────────────────────────────────────────────────────────── */
void config_vic_for_eint1(void)
{
    VICIntSelect = 0;               /* All interrupts as IRQ                 */

    /*
     * VICVectCntl0 — slot 0 (highest priority):
     *   [4:0] = 15 → interrupt source 15 = EINT1
     *   [5]   = 1  → enable slot
     */
    VICVectCntl0 = 15 | (1 << 5);  /* EINT1 source assigned to slot 0       */

    VICVectAddr0 = (u32)EINT1_Handler; /* ISR address for slot 0            */

    VICIntEnable |= (1 << 15);     /* Enable EINT1 (bit-15) in VIC          */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  config_eint1()
 *  Configure P0.14 as EINT1 and set falling-edge trigger.
 * ───────────────────────────────────────────────────────────────────────── */
void config_eint1(void)
{
    EXTPOLAR = 0;       /* Bit-1 = 0 → EINT1 active on falling edge / LOW   */

    EXTMODE |= 2;       /* EXTMODE bit-1 = 1 → EINT1 is EDGE triggered
                           (bit-1 = decimal 2)                               */

    /*
     * PINSEL0 bits [29:28] = 10 → P0.14 = EINT1
     * 0x20000000 = bit-29 set → [29:28] = 10
     */
    PINSEL0 |= 0x20000000;  /* Route P0.14 to EINT1 alternate function      */
}
