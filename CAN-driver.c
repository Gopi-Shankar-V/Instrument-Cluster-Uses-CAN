/*
 * ============================================================
 *  FILE  : CAN-driver.c  (Node A — Transmitter side)
 *  MCU   : LPC2129
 * ============================================================
 *
 *  PURPOSE:
 *    Initialise CAN2 controller, provide transmit and receive routines.
 *    Node A is the primary TRANSMITTER. It sends sensor data and
 *    switch states to Node B.
 *
 *  CAN BUS CONFIGURATION:
 *    PCLK   = 60 MHz  (VPBDIV = 1)
 *    Baud   = 100 kbps
 *    C2BTR  = 0x001C001D
 *      [9:0]  BRP  = 0x1D = 29  → time-quantum = PCLK/(BRP+1) = 2 MHz
 *      [22:16] TSEG1 = 0x1C = 28-16 = ... (see silicon manual for encoding)
 *      Effective: 100 kbps on a short bus (≤ 40m with proper termination)
 *
 *  PINS:
 *    CAN2 RD (receive data) = P0.9  → PINSEL1[19:18] = 01
 *    CAN2 TD (transmit data)= P0.8  → PINSEL1[17:16] = 01 (0x14000 combined)
 *
 *  ACCEPTANCE FILTER:
 *    AFMR = 0x00 → bypass filter: ALL messages accepted (promiscuous mode)
 *    Used on Node A because it may also need to receive ACK/status frames.
 * ============================================================
 */

#include "header.h"

/* ─────────────────────────────────────────────────────────────────────────
 *  can2_init()
 *  1. Set PCLK = CCLK (full speed)
 *  2. Map CAN2 pins via PINSEL1
 *  3. Enter Reset mode, configure baud rate, exit Reset mode
 * ───────────────────────────────────────────────────────────────────────── */
void can2_init(void)
{
    VPBDIV = 1;             /* PCLK = CCLK = 60 MHz (no peripheral clock divide) */

    /*
     * PINSEL1 bits [19:16] control P0.8 and P0.9:
     *   [17:16] = 01 → P0.8  = CAN2 TD (transmit)
     *   [19:18] = 01 → P0.9  = CAN2 RD (receive)
     * Combined mask: 0x00014000 (bits 18 and 16 set)
     */
    PINSEL1 |= 0x14000;     /* Route P0.8/P0.9 to CAN2 peripheral           */

    C2MOD = 1;              /* Enter Reset mode — required before changing BTR */

    AFMR = 0;               /* Acceptance Filter: bypass/off → accept all IDs */

    /*
     * C2BTR — Bus Timing Register
     *   [9:0]   BRP    = 0x1D (29) → CAN clock = PCLK/(29+1) = 2 MHz
     *   [19:16] TSEG1  = TSEG1 encoding (number of time quanta in segment 1)
     *   [22:20] TSEG2  = TSEG2 encoding
     *   [23]    SAM    = 0 (sample once)
     *   [25:24] SJW    = 0 (1 TQ resync jump width)
     * Value 0x001C001D achieves ~100 kbps at 60 MHz PCLK.
     */
    C2BTR = 0x001C001D;     /* Set CAN baud rate to 100 kbps                */

    C2MOD = 0;              /* Enter Operating mode — CAN bus now active      */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  can2_tx()
 *  Load a CAN frame into TX buffer 1 and trigger transmission.
 *
 *  @param v  CAN2 struct with id, rtr, dlc, byteA, byteB filled in
 * ───────────────────────────────────────────────────────────────────────── */
void can2_tx(CAN2 v)
{
    /* Step 1: Write the 11-bit message identifier into TID register         */
    C2TID1 = v.id;          /* TX Identifier register — standard 11-bit ID  */

    /* Step 2: Write Frame Info — DLC placed in bits [19:16] of C2TFI1      */
    C2TFI1 = (v.dlc << 16); /* [19:16]=DLC, [30]=RTR, [31]=FF(0=standard)  */

    if (v.rtr == 0)
    {
        /* Data frame: load payload into TX Data registers                   */
        C2TDA1 = v.byteA;   /* Bytes [0..3] of the payload (LSB first)      */
        C2TDB1 = v.byteB;   /* Bytes [4..7] of the payload                  */
    }
    else
    {
        /* Remote frame: set RTR bit (bit-30) in the Frame Info register     */
        C2TFI1 |= (1 << 30);/* RTR=1 → this is a request, no data bytes     */
    }

    /*
     * C2CMR — CAN Command Register (write-only):
     *   Bit-0  = TR  (Transmit Request)  — 1 = request transmission
     *   Bit-5  = SRR (Self Reception Req)— 1 = also receive our own msg
     *   Writing (1 | (1<<5)) = 0x21 starts TX on buffer 1.
     */
    C2CMR = 1 | (1 << 5);  /* Send: Transmit Request + Self Reception       */

    /* Step 3: Wait until transmission is complete                           */
    while (TCS2 == 0);      /* TCS2 = C2GSR bit-3; 0 while TX in progress   */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  can2_rx()
 *  Polling receive — waits until a frame arrives, reads it, releases buffer.
 *
 *  @param ptr  Pointer to a CAN2 struct that will be filled with received data
 * ───────────────────────────────────────────────────────────────────────── */
void can2_rx(CAN2 *ptr)
{
    /* Wait until the Receive Buffer Status bit is set (a message arrived)  */
    while (RBS2 == 0);      /* RBS2 = C2GSR bit-0; 0 = no message waiting   */

    /* Read the identifier of the received message                          */
    ptr->id  = C2RID;       /* CAN2 RX Identifier Register                  */

    /* Extract DLC from Frame Information Register bits [19:16]             */
    ptr->dlc = (C2RFS >> 16) & 0xF; /* Mask 4-bit DLC field                */

    /* Extract RTR bit from Frame Information Register bit [30]             */
    ptr->rtr = (C2RFS >> 30) & 1;   /* 0=data frame, 1=remote frame        */

    if (ptr->rtr == 0)
    {
        /* Data frame: read the 8 payload bytes (as two 32-bit words)       */
        ptr->byteA = C2RDA;  /* RX Data Register A: bytes [3:0]             */
        ptr->byteB = C2RDB;  /* RX Data Register B: bytes [7:4]             */
    }

    /* Release the receive buffer so the controller can accept next frame   */
    C2CMR = (1 << 2);       /* Bit-2 = RRB (Release Receive Buffer)         */
}
