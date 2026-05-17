/*
 * ============================================================
 *  FILE  : eint0.c  (Node A)
 *  MCU   : LPC2129
 * ============================================================
 *
 *  PURPOSE:
 *    Configure External Interrupt 0 (EINT0) on pin P0.16 to detect
 *    a button press for the LEFT INDICATOR toggle.
 *
 *  HOW IT WORKS:
 *    - P0.16 is configured as EINT0 via PINSEL1.
 *    - EINT0 is set to trigger on a FALLING EDGE (button press pulls LOW).
 *    - When triggered, the ISR sets global flag1 = 1.
 *    - Main loop detects flag1, toggles the left indicator state,
 *      and transmits CAN message ID 0x212.
 *
 *  REGISTERS USED:
 *    PINSEL1  — Pin function select for P0.16–P0.31 (2 bits per pin)
 *    EXTMODE  — Selects edge (1) or level (0) trigger for each EINT
 *    EXTPOLAR — Selects polarity: 0 = falling edge / low level
 *    EXTINT   — External Interrupt Flag register (write 1 to clear)
 *    VICIntSelect  — 0 = IRQ, 1 = FIQ for each VIC source
 *    VICVectCntl1  — Priority slot 1: [4:0]=source, [5]=enable
 *    VICVectAddr1  — ISR address for priority slot 1
 *    VICIntEnable  — Enable bit for each interrupt source
 *
 *  VIC SOURCE NUMBER:
 *    EINT0 = source 14 (refer LPC2129 User Manual, Table VIC sources)
 * ============================================================
 */

#include "header.h"

extern u8 flag1;   /* Shared with Amain.c — set here, cleared in main loop  */

/* ─────────────────────────────────────────────────────────────────────────
 *  EINT0_Handler()  — External Interrupt 0 ISR
 *
 *  Called automatically by VIC when a falling edge occurs on P0.16.
 *  Must clear the EXTINT flag before returning, otherwise the interrupt
 *  fires again immediately (infinite re-entry).
 * ───────────────────────────────────────────────────────────────────────── */
void EINT0_Handler(void) __irq   /* __irq saves/restores CPSR for ARM7      */
{
    flag1 = 1;          /* Signal main loop: left indicator button pressed   */

    EXTINT |= 1;        /* Clear EINT0 flag: bit-0 of EXTINT register
                           Write 1 to clear (writing 0 has no effect)       */

    VICVectAddr = 0;    /* Acknowledge interrupt to VIC (required for ARM7)
                           Tells VIC the ISR is done; enables next interrupt */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  config_vic_for_eint0()
 *  Register EINT0_Handler into VIC priority slot 1.
 *
 *  VIC has 16 vectored slots (slot 0 = highest priority).
 *  We use slot 1 for EINT0 (left indicator).
 * ───────────────────────────────────────────────────────────────────────── */
void config_vic_for_eint0(void)
{
    VICIntSelect = 0;               /* All interrupts → IRQ mode (not FIQ)   */

    /*
     * VICVectCntl1 — Vectored Interrupt Control for slot 1:
     *   [4:0] = 14   → interrupt source 14 = EINT0
     *   [5]   = 1    → enable this vectored slot
     * Combined: 14 | (1<<5) = 14 | 32 = 46 = 0x2E
     */
    VICVectCntl1 = 14 | (1 << 5);  /* Assign EINT0 source to VIC slot 1    */

    /* Store the ISR function address in the corresponding vector register   */
    VICVectAddr1 = (u32)EINT0_Handler;  /* VIC jumps here on EINT0 trigger  */

    /* Enable EINT0 in the global VIC interrupt enable register              */
    VICIntEnable |= (1 << 14);     /* Bit-14 corresponds to EINT0 source    */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  config_eint0()
 *  Configure P0.16 as EINT0 and set falling-edge trigger mode.
 * ───────────────────────────────────────────────────────────────────────── */
void config_eint0(void)
{
    EXTPOLAR = 0;       /* EXTPOLAR bit-0 = 0 → falling edge (or low level)
                           0 = active low / falling edge for EINT0           */

    EXTMODE |= 1;       /* EXTMODE bit-0 = 1 → EDGE sensitive (not level)
                           1 = edge trigger; 0 = level trigger               */

    /*
     * PINSEL1 controls P0.16–P0.31 pin functions (2 bits per pin):
     *   P0.16 → PINSEL1[1:0] = 01 → selects EINT0 function
     * 0x00000001 sets only bits [1:0] = 01
     */
    PINSEL1 |= 1;       /* P0.16 → EINT0 alternate function                 */
}
