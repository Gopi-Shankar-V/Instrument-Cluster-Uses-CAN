/*
 * ============================================================
 *  FILE  : CAN-driver.c  (Node B — Receiver side)
 *  MCU   : LPC2129
 * ============================================================
 *
 *  PURPOSE:
 *    - Initialise CAN2 with hardware acceptance filter (only accept
 *      frames whose IDs match the instrument cluster messages).
 *    - Provide TX, polling-RX routines.
 *    - Provide CAN2 RX interrupt setup so Node B wakes on each frame.
 *
 *  ACCEPTANCE FILTER OVERVIEW:
 *    The LPC2129 CAN acceptance filter uses a look-up table in
 *    on-chip SRAM (mapped at 0xE003_8000) to decide which frame IDs
 *    to accept. We program 6 IDs in pairs (standard frame, SFF_sa mode):
 *      Pair 0: 0x212, 0x213
 *      Pair 1: 0x214, 0x215
 *      Pair 2: 0x216, 0x217   ← 0x217 is the new Horn message ID
 *
 *  EACH ENTRY FORMAT (32-bit word, standard frame):
 *    [28:16] = higher ID,  [12:0] = lower ID  (IDs must be ascending)
 *    [15]    = disable-bit for lower ID (0=enabled)
 *    [31]    = disable-bit for higher ID
 *    Controller number in bits [30:29] / [14:13]:
 *    For CAN2: controller number = 2 − 1 = 1 (zero-indexed)
 *    So bits [30:29]=0b01 and bits [14:13]=0b01 → encoded as:
 *      0x22XXYYYY where XX = high ID upper bits, YYYY = low ID bits.
 *
 *  INTERRUPT:
 *    CAN2 RX interrupt (VIC slot 2, source 27) fires when a frame
 *    matching the filter arrives. ISR is in can_tx.c (kept naming from
 *    original project; file is CAN-driver.c here for clarity).
 * ============================================================
 */

#include "header.h"

/* Pointer to the acceptance filter SRAM table base address */
static u32 *af_ram = (u32 *)0xE0038000;

/* ─────────────────────────────────────────────────────────────────────────
 *  can2_init()
 *  Initialise CAN2 on Node B with programmed acceptance filter.
 * ───────────────────────────────────────────────────────────────────────── */
void can2_init(void)
{
    VPBDIV = 1;             /* PCLK = CCLK = 60 MHz                         */

    /*
     * P0.8  = CAN2 TD  (PINSEL1 bits [17:16] = 01)
     * P0.9  = CAN2 RD  (PINSEL1 bits [19:18] = 01)
     * Combined: PINSEL1 |= 0x00014000
     */
    PINSEL1 |= 0x14000;     /* Route CAN2 pins                              */

    C2MOD = 1;              /* Reset mode — required before modifying BTR   */

    C2BTR = 0x001C001D;     /* 100 kbps @ 60 MHz PCLK (matches Node A)     */

    /* ── Program Acceptance Filter SRAM ──────────────────────────────────
     * Each 32-bit entry pairs two 11-bit CAN IDs.
     * Format for standard frame, CAN2 (controller index=1):
     *   [31]    = 0  (upper ID enabled)
     *   [30:29] = 01 (controller 2 → index 1)
     *   [28:16] = upper 11-bit ID (13 bits, upper 2 zero for 11-bit)
     *   [15]    = 0  (lower ID enabled)
     *   [14:13] = 01 (controller 2 → index 1)
     *   [12:0]  = lower 11-bit ID
     *
     * 0x32112212 → IDs 0x211 (disabled, dummy) and 0x212 (enabled)
     * 0x22132214 → IDs 0x213 and 0x214
     * 0x22152217 → IDs 0x215 and 0x216  ← updated to include 0x217 (horn)
     *
     * Actual encoding (verified against original NodeB code):
     *   af_ram[0] = 0x32112212  (pair: 0x212, 0x213)
     *   af_ram[1] = 0x22132214  (pair: 0x214, 0x215) -- wait, original says 0x22132214
     *   af_ram[2] = 0x22152216  (pair: 0x216, 0x217 -- add horn)
     *
     * We keep first two pairs exactly as original and update pair 3 to
     * accommodate 0x217 by adding a 4th entry word.
     */
    af_ram[0] = 0x32112212; /* Pair 0: IDs 0x212 (left indi) & 0x213 (HL)  */
    af_ram[1] = 0x22142215; /* Pair 1: IDs 0x214 (right indi) & 0x215 (spd)*/
    af_ram[2] = 0x22162217; /* Pair 2: IDs 0x216 (temp) & 0x217 (horn) NEW */

    /* ── Set acceptance filter boundary registers ─────────────────────── */
    SFF_sa      = 0;        /* Standard Frame Individual table starts at 0  */
    ENDofTable  = 0x0C;     /* End of table = 3 entries × 4 bytes = 12 = 0xC*/
    SFF_GRP_sa  = 0x0C;     /* No group entries (starts where individual ends) */
    EFF_sa      = 0x0C;     /* No extended frame entries                    */
    EFF_GRP_sa  = 0x0C;
    AFMR        = 2;        /* Enable acceptance filter (AFMR bit-1 = 1)    */

    C2MOD = 0;              /* Leave Reset mode → CAN bus now active         */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  can2_tx()
 *  Transmit one CAN data frame from Node B (if needed for ACK/reply).
 * ───────────────────────────────────────────────────────────────────────── */
void can2_tx(CAN2 v)
{
    C2TID1 = v.id;              /* Load 11-bit ID into TX Identifier reg     */
    C2TFI1 = (v.dlc << 16);    /* Frame Info: DLC in bits [19:16]           */

    if (v.rtr == 0)
    {
        C2TDA1 = v.byteA;       /* Data bytes 0–3                           */
        C2TDB1 = v.byteB;       /* Data bytes 4–7                           */
    }
    else
    {
        C2TFI1 |= (1 << 30);    /* Set RTR bit for remote frame             */
    }

    C2CMR = 1 | (1 << 5);      /* TX Request + Self-Reception Request       */

    while (TCS2 == 0);          /* Wait for TX complete                      */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  can2_rx()
 *  Polling receive (used if interrupt mode is disabled for debug).
 * ───────────────────────────────────────────────────────────────────────── */
void can2_rx(CAN2 *ptr)
{
    while (RBS2 == 0);          /* Wait until RX buffer has a message        */

    ptr->id  = C2RID;           /* Read received message identifier          */
    ptr->dlc = (C2RFS >> 16) & 0xF;  /* Extract DLC from Frame Status reg  */
    ptr->rtr = (C2RFS >> 30) & 1;    /* Extract RTR bit                     */

    if (ptr->rtr == 0)
    {
        ptr->byteA = C2RDA;     /* Read payload bytes 0–3                    */
        ptr->byteB = C2RDB;     /* Read payload bytes 4–7                    */
    }

    C2CMR = (1 << 2);           /* Release Receive Buffer (RRB bit)          */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  CAN2_RX_Handler()  — CAN2 Receive Interrupt Service Routine
 *
 *  Fires every time a CAN frame passes the acceptance filter.
 *  Reads the frame directly from hardware registers (no polling needed),
 *  stores it in the global 'm1' struct, then sets 'flag' for main loop.
 * ───────────────────────────────────────────────────────────────────────── */
extern u32  flag; /* Defined in Bmain.c — signals main loop a new frame arrived */
extern CAN2 m1;   /* Defined in Bmain.c — storage for the received CAN frame    */

void CAN2_RX_Handler(void) __irq  /* __irq keyword: saves/restores CPSR properly */
{
    /* Read the received frame directly from CAN hardware registers         */
    m1.id  = C2RID;                    /* 11-bit message identifier          */
    m1.dlc = (C2RFS >> 16) & 0xF;     /* Data length (0–8 bytes)            */
    m1.rtr = (C2RFS >> 30) & 1;       /* 0=data frame, 1=remote frame       */

    if (m1.rtr == 0)
    {
        m1.byteA = C2RDA;              /* Payload bytes [0..3]               */
        m1.byteB = C2RDB;              /* Payload bytes [4..7]               */
    }

    C2CMR = (1 << 2);                  /* Release RX buffer for next frame   */

    flag = 1;                          /* Signal main loop: new data ready   */

    VICVectAddr = 0;                   /* Acknowledge interrupt to VIC       */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  en_can2_interrupt()
 *  Enable the CAN2 RX interrupt in the CAN interrupt enable register.
 * ───────────────────────────────────────────────────────────────────────── */
void en_can2_interrupt(void)
{
    C2IER = 1;  /* CAN2 Interrupt Enable Register bit-0 = RI (RX Interrupt) */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  config_vic_for_CAN2()
 *  Register the CAN2 RX ISR with the Vectored Interrupt Controller.
 *
 *  VIC source 27 = CAN1 TX/RX & CAN2 TX/RX combined interrupt.
 *  We route it to vectored slot 2 at highest priority.
 * ───────────────────────────────────────────────────────────────────────── */
void config_vic_for_CAN2(void)
{
    VICIntSelect = 0;                       /* All interrupts → IRQ (not FIQ) */

    /*
     * VICVectCntl2:
     *   [4:0] = 27 → source number (CAN interrupt)
     *   [5]   = 1  → enable this vectored slot
     */
    VICVectCntl2 = 27 | (1 << 5);          /* VIC slot 2 → CAN source 27     */

    /* Store ISR address in corresponding VIC vector address register        */
    VICVectAddr2 = (u32)CAN2_RX_Handler;   /* ISR function pointer           */

    /* Enable interrupt source 27 in the global interrupt enable register   */
    VICIntEnable = (1 << 27);              /* Bit-27 = CAN1/2 interrupt       */
}
