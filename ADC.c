/*
 * ============================================================
 *  FILE  : ADC.c  (Node A)
 *  MCU   : LPC2129
 * ============================================================
 *
 *  PURPOSE:
 *    Initialise the on-chip 10-bit ADC and read individual channels.
 *
 *  CHANNEL ASSIGNMENTS:
 *    ADC Channel 1 (AD0.1, P0.28) → Engine Temperature sensor (LM35)
 *    ADC Channel 2 (AD0.2, P0.29) → Speed sensor / potentiometer
 *
 *  LPC2129 ADC BASICS:
 *    - 10-bit successive-approximation ADC
 *    - Reference = VREF = 3.3V (or VDD), so 1023 counts = full scale
 *    - ADCR  — ADC Control Register (select channel, clock, start)
 *    - ADDR  — ADC Data Register    (result in bits [15:6], DONE in bit-31)
 *
 *  ADCR BIT FIELDS:
 *    [7:0]  SEL  — channel select bitmask (bit-n = select channel n)
 *    [15:8] CLKDIV — (PCLK / (CLKDIV+1)) must be ≤ 4.5 MHz for accuracy
 *    [21]   BURST — 1=continuous, 0=single conversion
 *    [26:24] START — 001 = start conversion now
 * ============================================================
 */

#include "header.h"

/* ─────────────────────────────────────────────────────────────────────────
 *  adc_init()
 *  Configure the PINSEL registers to connect P0.28 and P0.29 to the ADC
 *  peripheral, then set up ADCR for software-triggered, single conversion.
 * ───────────────────────────────────────────────────────────────────────── */
void adc_init(void)
{
    /*
     * PINSEL1 controls P0.16–P0.31 pin functions (2 bits per pin).
     * AD0.1 is on P0.28 → PINSEL1[25:24] = 01
     * AD0.2 is on P0.29 → PINSEL1[27:26] = 01
     * AD0.4 is on P0.30 → not used here but mask includes it.
     *
     * 0x15400000 sets bits [28:24]:
     *   [25:24]=01 → P0.28 = AD0.1
     *   [27:26]=01 → P0.29 = AD0.2
     *   [29:28]=01 → P0.30 = AD0.4  (not used but safe)
     */
    PINSEL1 |= 0x15400000;  /* Route P0.28,29 to ADC peripheral pins      */

    /*
     * ADCR = 0x00200400
     *  [7:0]  SEL   = 0x00 — no channel selected yet (selected per read)
     *  [15:8] CLKDIV= 0x04 — ADC clock = PCLK / 5 = 60M/5 = 12MHz
     *                         (spec says ≤ 4.5 MHz, so set div higher in
     *                          production; 0x0D gives 60M/14 ≈ 4.28 MHz)
     *  [21]   BURST = 0    — software-triggered single conversion
     *  [26:24] START= 0    — no start yet; triggered in adc_read()
     *
     * NOTE: 0x00200400 = bits [13:8]=0x04 (CLKDIV) + bit[21]=1 (PDN=power down OFF)
     *       Actually bit-21 in ADCR is PDN (Power Down): 1=ADC operational.
     */
    ADCR = 0x00200400;      /* Power up ADC, set clock divider, burst off   */
}

/* ─────────────────────────────────────────────────────────────────────────
 *  adc_read()
 *  Perform a single software-triggered conversion on the given channel.
 *
 *  @param ch_num  Channel number: 1 for engine temp, 2 for speed
 *  @return        10-bit result (0–1023)
 * ───────────────────────────────────────────────────────────────────────── */
u32 adc_read(u8 ch_num)
{
    u32 result = 0;

    /* Step 1: Select the channel by setting the corresponding bit in SEL[7:0] */
    ADCR |= (1 << ch_num);   /* e.g. ch_num=2 → set bit-2 → select AD0.2  */

    /* Step 2: Start conversion by writing 001 to START field [26:24]       */
    ADCR |= (1 << 24);       /* Bit-24 = START[0] = 1 → "start now"        */

    /* Step 3: Poll the DONE flag (ADDR bit-31) until conversion finishes   */
    while (DONE == 0);       /* Busy-wait; typically ~25 ADC clock cycles   */

    /* Step 4: Clear the START bit to avoid repeated conversions            */
    ADCR ^= (1 << 24);       /* XOR clears bit-24 → stop trigger            */

    /* Step 5: De-select the channel (clear channel bit)                    */
    ADCR ^= (1 << ch_num);   /* XOR clears the channel selection bit        */

    /* Step 6: Extract the 10-bit result from ADDR[15:6]                    */
    /*         ADDR layout: [5:0]=reserved, [15:6]=RESULT, [31]=DONE        */
    result = (ADDR >> 6) & 0x3FF;   /* Shift right 6, mask 10 bits          */

    return result;           /* Returns value 0 to 1023                     */
}
