/*
 * ============================================================
 *  FILE  : eint2.c  (Node A)
 *  MCU   : LPC2129
 * ============================================================
 *
 *  PURPOSE:
 *    Configure External Interrupt 2 (EINT2) on pin P0.15 to detect
 *    a button press for the HEAD LIGHT toggle.
 *
 *  HOW IT WORKS:
 *    - P0.15 is configured as EINT2 via PINSEL0 (P0.0–P0.15).
 *    - EINT2 triggers on FALLING EDGE (active-low button).
 *    - ISR sets global flag3 = 1.
 *    - Main loop detects flag3, toggles head-light state,
 *      and sends CAN message ID 0x213.
 *
 *  NOTE ON PINSEL:
 *    P0.15 → PINSEL0 bits [31:30]:
 *    EINT2 on P0.15 → PINSEL0[31:30] = 10 → value 0x80000000
 *
 *  VIC SOURCE NUMBER:
 *    EINT2 = source 16
 * ============================================================
 */

#include "header.h"

extern u8 flag3;   /* Shared with Amain.c — set here, cleared in main loop  */

/* ─────────────────────────────────────────────────────────────────────────
 *  EINT2_Handler()  — External Interrupt 2 ISR
 *
 *  Fires on falling edge of P0.15 (head-light toggle button pressed).
 * ───────────────────────────────────────────────────────────────────────── */
void EINT2_Handler(void) __irq
{
    flag3 = 1;          /* Signal main loop: head-light button pressed       */

    EXTINT |= 4;        /* Clear EINT2 flag: bit-2 of EXTINT register
                           (decimal 4 = bit-2; write 1 to clear the flag)   */

    VICVectAddr = 0;    /* Acknowledge the VIC controller                    */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  config_vic_for_eint2()
 *  Register EINT2_Handler into VIC priority slot 3.
 * ───────────────────────────────────────────────────────────────────────── */
void config_vic_for_eint2(void)
{
    /*
     * Note: VICIntSelect not re-written here to avoid overwriting EINT0/1.
     * VICIntSelect is a global register — the whole word is set once in
     * config_vic_for_eint0(). Writing 0 again here is safe as all
     * sources should stay as IRQ.
     */

    /*
     * VICVectCntl3 — slot 3:
     *   [4:0] = 16 → interrupt source 16 = EINT2
     *   [5]   = 1  → enable this slot
     */
    VICVectCntl3 = 16 | (1 << 5);  /* EINT2 source into VIC slot 3          */

    VICVectAddr3 = (u32)EINT2_Handler; /* ISR address for slot 3            */

    VICIntEnable |= (1 << 16);     /* Enable EINT2 (bit-16) in VIC          */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  config_eint2()
 *  Configure P0.15 as EINT2 and set falling-edge trigger.
 * ───────────────────────────────────────────────────────────────────────── */
void config_eint2(void)
{
    EXTPOLAR = 0;       /* Bit-2 = 0 → EINT2 active on falling edge / LOW   */

    EXTMODE |= 4;       /* EXTMODE bit-2 = 1 → EINT2 is EDGE triggered
                           (bit-2 = decimal 4)                               */

    /*
     * PINSEL0 bits [31:30] = 10 → P0.15 = EINT2
     * 0x80000000 = bit-31 set → [31:30] = 10
     */
    PINSEL0 |= 0x80000000;  /* Route P0.15 to EINT2 alternate function      */
}
