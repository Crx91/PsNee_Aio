#define SYSCLK_FREQ_48MHZ_HSI 48000000

#include "ch32fun.h"
#include <stdio.h>
#include "ws2812.h"  // WS2812b led custom lib
#include "Pal_Fix.h" // Schp_102 bios fix classic [onechip]
#include "Jap_Fix.h" // Jap bios fix [Jap_Bios_Unlocker]

// PsNee ch32v003f4p6 AIO version 2.1
//
// Beware to use the PSX 3.5V / 3.3V power, *NOT* 5V! The installation pictures include an example.
//
// Only for WCH CH32V003f4p6 mcu @ 48Mhz internal clock.
//
// Coded using Ch32fun library! https://github.com/cnlohr/ch32fun
//
// Changelog:
// v.1x -> Alpha
// v2.f -> First stable release
// v2.1 -> Little code & comnts refactoring
//
//
// PINOUT for wch ch32v003f4p6:
/*
                   ___ ___
   Bios_Ax (PD4) -|   U   |- (PD3) Bios_Dx
 WFCK_GATE (PD5) -|       |- (PD2) SUBQ
      DATA (PD6) -|       |- (PD1) [SWDIO]
   [Reset] (PD7) -|       |- (PC7) SQCK
   Bios_CE (PA1) -|       |- (PC6) reg_Bit0
     Speed (PA2) -|       |- (PC5) reg_Bit1
             GND -|       |- (PC4) Switch
   WS2812B (PD0) -|       |- (PC3) bios_Bit0
             VCC -|       |- (PC2) bios_Bit1
  VCD_mode (PC0) -|       |- (PC1) bios_Bit2
                   -------

[Reset] trigger on LOW, so LOW signal trigger chip reset!

L = Jumper set -> Set input Low (connect to GND)
H = Jumper NOT set -> Set input High (via internal MCU pullup)

reg_Bit table:

 Bit0 | Bit1 | Effect | reg_mode
 PC6  | PC5  |        |
-------------------------------
   H  |   H  | Error  |   0
   H  |   L  |  PAL   |   1
   L  |   H  |  USA   |   2
   L  |   L  |  JAP   |   3


bios_Bit table:

 Bit2 | Bit1 | Bit0 |  Effect   | bios_mode
  PC1 | PC2  | PC3  |           |
---------------------------------------
   H  |   H  |   H  | No Fix    |   0
   H  |   H  |   L  | Scph_102  |   1
   H  |   L  |   H  | Jap_Fix   |   2
   H  |   L  |   L  | FREE      |   3
   L  |   H  |   H  | FREE      |   4
   L  |   H  |   L  | FREE      |   5
   L  |   L  |   H  | FREE      |   6
   L  |   L  |   L  | FREE      |   7
   */


#define bits_delay 4000     // 250 bits/s (microseconds)
#define injections_delay 90 // 72 in oldcrow. PU-22+ work best with 80 to 100 (milliseconds)
#define HYSTERESIS_MAX 17

/* Global Variable */
uint8_t Xbit = 0;

//  Setup() detects which (of 2) injection methods this PSX board requires, then stores it in wfck_mode.
uint8_t wfck_mode = 0;
// Setup the modchip Region, Bios and VCD modes
uint8_t reg_mode = 0;  // default no region set. Unset = error loop
uint8_t bios_mode = 0; // default no bios patching enabled (with this rev i'm supporting only classic scph-102 biosfix)
uint8_t vcd_mode = 1;  // default for standard motherboards mode
uint8_t Switch = 1;    // default for switch var (1: Disabled; 0: Enabled and don't apply any bios patch)

// --- Prototypes (Forward declarations) ---
// These tell the compiler that the functions exist later in the code.
void logic_Standard(uint8_t isDataSector);
void logic_SCPH_5903(uint8_t isDataSector);

// Function pointer type definition for the console detection logic.
// This allows switching between 'Standard' and 'SCPH-5903' heuristics dynamically.
typedef void (*ConsoleLogicPtr)(uint8_t isDataSector);

// Global pointer holding the currently active logic function.
// Using a function pointer eliminates the need for repetitive 'if/else' checks in the main loop.
volatile ConsoleLogicPtr currentLogic = logic_Standard;

uint8_t hysteresis = 0;
uint8_t scbuf[12] = {0}; // SUBQ bit storage
uint16_t timeout_clock_counter = 0;
uint8_t bitbuf = 0;
uint8_t bitpos = 0;
uint8_t scpos = 0; // scbuf position

/*------------------------------------------------------------------------
  Function Modules_set

  This function sets the variables reg_mode, bios_mode and VCD _mode
  based on the jumpers setted on the x_bitsx pins

  reg_mode settings
  [00] == 0 Invalid setting
  [01] == 1 PAL
  [10] == 2 USA
  [11] == 3 JAP

  bios_mode setting
  [000] == 0 Don't apply any bios fix
  [001] == 1 Scph-102 [onechip]
  [010] == 2 Jap_Bios_Unlocker
  [011] == 3 NOT AVAIABLE
  [100] == 4 NOT AVAIABLE
  [101] == 5 NOT AVAIABLE
  [110] == 6 NOT AVAIABLE
  [111] == 7 NOT AVAIABLE
  ----------------------------------------------------------------------*/
void Modules_set()
{
  vcd_mode = funDigitalRead(PC0); // sets the VCD_mode based on pin status
  Switch = funDigitalRead(PC4);

  //-------------------- reg_bit mapping ---------------------------------
  if ((funDigitalRead(PC6)) == 0) // reg_bit0
  {
    reg_mode = reg_mode + 1;
  }

  if ((funDigitalRead(PC5)) == 0) // reg_bit1
  {
    reg_mode = reg_mode + 2;
  }

  //-------------------- bios_bit mapping ---------------------------------
  if ((funDigitalRead(PC3)) == 0) // bios_bit0
  {
    bios_mode = bios_mode + 1;
  }

  if ((funDigitalRead(PC2)) == 0) // bios_bit1
  {
    bios_mode = bios_mode + 2;
  }

  if ((funDigitalRead(PC1)) == 0) // bios_bit2
  {
    bios_mode = bios_mode + 4;
  }

  //-------------------- reg_mode set ---------------------------------
  switch (reg_mode)
  {
  case 0:
    while (1)
    {
      // error
      LED_SendColour(254, 0, 0); // Send RED
      Delay_Ms(500);
      LED_SendColour(0, 0, 0);
      Delay_Ms(500);
    }
    break;
  case 1:
    Xbit = 1; // PAL
    // Set ws2812b injection led GREEN
    led_r = 0;
    led_g = 200;
    led_b = 0;
    break;
  case 2:
    Xbit = 2; // USA
    // Set ws2812b injection led ORANGE
    led_r = 200;
    led_g = 200;
    led_b = 0;
    break;
  case 3:
    Xbit = 3; // JAP
    // Set ws2812b injection led BLUE
    led_r = 0;
    led_g = 0;
    led_b = 200;
    break;
  }

  //-------------------- vcd_mode set ---------------------------------
  switch (vcd_mode)
  {
  case 0:
    currentLogic = logic_SCPH_5903;
    break;
  case 1:
    currentLogic = logic_Standard;
    break;
  }

  // Sets done, put pinmode to input now.
  funPinMode(PC0, GPIO_CNF_IN_FLOATING);
  funPinMode(PC1, GPIO_CNF_IN_FLOATING);
  funPinMode(PC2, GPIO_CNF_IN_FLOATING);
  funPinMode(PC3, GPIO_CNF_IN_FLOATING);
  funPinMode(PC5, GPIO_CNF_IN_FLOATING);
  funPinMode(PC6, GPIO_CNF_IN_FLOATING);
}

/**************************************************************************************
 * Processes sector data for the SCPH-5903 (Dual-interface PS1) to differentiate
 * between PlayStation games and Video CDs (VCD).
 *
 * This heuristic uses an 'hysteresis' counter to stabilize disc detection:
 * - Increases when a PSX Lead-In or valid game sector is identified.
 * - Remains neutral/ignores VCD-specific Lead-In patterns.
 * - Decreases (fades out) when the data does not match known patterns.
 *
 *  isDataSector Boolean flag indicating if the current sector contains data.

**************************************************************************************/
void logic_SCPH_5903(uint8_t isDataSector)
{
  // Identify VCD Lead-In: Specific SCBUF patterns (0xA0/A1/A2) with sub-mode 0x02
  uint8_t isVcdLeadIn = isDataSector && scbuf[1] == 0x00 && scbuf[6] == 0x00 &&
                        (scbuf[2] == 0xA0 || scbuf[2] == 0xA1 || scbuf[2] == 0xA2) &&
                        (scbuf[3] == 0x02);

  // Identify PSX Lead-In: Same SCBUF patterns but different sub-mode (!= 0x02)
  uint8_t isPsxLeadIn = isDataSector && scbuf[1] == 0x00 && scbuf[6] == 0x00 &&
                        (scbuf[2] == 0xA0 || scbuf[2] == 0xA1 || scbuf[2] == 0xA2) &&
                        (scbuf[3] != 0x02);

  if (isPsxLeadIn)
  {
    hysteresis++;
  }
  else if (hysteresis > 0 && !isVcdLeadIn &&
           ((scbuf[0] == 0x01 || isDataSector) && scbuf[1] == 0x00 && scbuf[6] == 0x00))
  {
    hysteresis++; // Maintain/Increase confidence for valid non-VCD sectors
  }
  else if (hysteresis > 0)
  {
    hysteresis--; // Patterns stop matching
  }
}
/******************************************************************************************
 * Heuristic logic for standard PlayStation hardware (Non-VCD models).
 *
 * This function monitors disc sectors to identify genuine PlayStation discs:
 * 1. Checks for specific Lead-In markers (Point A0, A1, A2 or Track 01).
 * 2. Uses an incrementing 'hysteresis' counter to confirm disc validity.
 * 3. Includes a 'fade-out' mechanism to reduce the counter if valid patterns are lost,
 *    effectively filtering out noise or read errors.
 *
 *  isDataSector Boolean flag: true if the current sector is a data sector.

******************************************************************************************/

void logic_Standard(uint8_t isDataSector)
{
  // Detect specific Lead-In patterns
  if ((isDataSector && scbuf[1] == 0x00 && scbuf[6] == 0x00) &&
      (scbuf[2] == 0xA0 || scbuf[2] == 0xA1 || scbuf[2] == 0xA2 ||
       (scbuf[2] == 0x01 && (scbuf[3] >= 0x98 || scbuf[3] <= 0x02))))
  {
    hysteresis++;
  }
  // Maintain confidence if general valid sector markers are found
  else if (hysteresis > 0 &&
           ((scbuf[0] == 0x01 || isDataSector) && scbuf[1] == 0x00 && scbuf[6] == 0x00))
  {
    hysteresis++;
  }
  else if (hysteresis > 0)
  {
    hysteresis--;
  }
}

// *****************************************************************************
// Function: board_detection
// DESCRIPTION:
// Distinguishes motherboard generations (PU-7 through PU-22+) by analyzing
// the behavior of the WFCK signal.
//
// SIGNAL CHARACTERISTICS:
// - Legacy Boards (PU-7 to PU-20): WFCK acts as a static GATE signal.
//   It remains HIGH (continuous) during the region-check window.
// - Modern Boards (PU-22 or newer): WFCK is an oscillating clock signal
//   (Frequency-based).
//
// WFCK: __-----------------------  // CONTINUOUS (PU-7 .. PU-20)(GATE)
//
// WFCK: __-_-_-_-_-_-_-_-_-_-_-_-  // FREQUENCY  (PU-22 or newer)
//
// HISTORICAL CONTEXT:
// Traditionally, WFCK was referred to as the "GATE" signal. On early models,
// modchips functioned as a synchronized gate, pulling the signal LOW
// precisely when the region-lock data was being processed.
//
// FREQUENCY DATA:
// - Initial/Protection Phase: ~7.3 kHz.
// - Standard Data Reading: ~14.6 kHz.
//
// *****************************************************************************
void board_detection()
{
  wfck_mode = 0;           // Default: Legacy (GATE)
  uint8_t pulse_hits = 25; // We need to see 25 oscillations to confirm FREQUENCY mode
  uint16_t detectionWindow = 10000;
  Delay_Ms(300); // Wait for WFCK to stabilize (High on Legacy, Oscillation on Modern)

  while (--detectionWindow)
  {
    // LOGIC BASED ON YOUR ANALYSIS:
    // If WFCK is "CONTINUOUS" (Legacy), it stays HIGH. PIN_WFCK_READ will always be 1.
    // If WFCK is "FREQUENCY" (Modern), it will hit 0 (LOW) periodically.

    if ((funDigitalRead(PD5)) == 0)
    { // Wfck == 0  //Detect a LOW state (only possible in FREQUENCY mode)

      pulse_hits--; // Record one oscillation hit

      if (pulse_hits == 0)
      {
        wfck_mode = 1; // Confirmed: FREQUENCY mode (PU-22 or newer)
        return;        // Exit as soon as we are sure
      }

      // SYNC: Wait for the signal to go HIGH again.
      // This ensures we count each pulse of the "FREQUENCY" signal only once.

      while ((funDigitalRead(PD5) == 0) && detectionWindow > 0)
      {
        detectionWindow--;
      }
    }
  }
  // If the window expires without seeing enough LOW pulses, it remains wfck_mode = 0 (GATE)
}

// *****************************************************************************************
// Function: inject_SCEX
// DESCRIPTION:
// Injects SCEX data corresponding to a given region ('e' for Europe, 'a' for America,
// 'i' for Japan). This function is used for modulating the SCEX signal to bypass
// region-locking mechanisms.
//
// PARAMETERS:
// - region: A character ('e', 'a', or 'i') representing the target region.
//
// *****************************************************************************************
void inject_SCEX(const char region)
{
  // SCEX data patterns for different regions (SCEE: Europe, SCEA: America, SCEI: Japan)
  // Each array contains the specific bit sequence required to bypass region locking.
  static const uint8_t SCEEData[] = {
      0b01011001,
      0b11001001,
      0b01001011,
      0b01011101,
      0b11101010,
      0b00000010};

  static const uint8_t SCEAData[] = {
      0b01011001,
      0b11001001,
      0b01001011,
      0b01011101,
      0b11111010,
      0b00000010};

  static const uint8_t SCEIData[] = {
      0b01011001,
      0b11001001,
      0b01001011,
      0b01011101,
      0b11011010,
      0b00000010};

  // Select the appropriate data pointer based on the region character to avoid
  // repetitive conditional checks inside the high-timing-sensitive loop.
  const uint8_t *ByteSet = (region == 1) ? SCEEData : (region == 2) ? SCEAData
                                                                    : SCEIData;

  // Iterate through the 44 bits of the SCEX sequence
  for (uint8_t bit_counter = 0; bit_counter < 44; bit_counter++)
  {

    // Extraction of the current bit (Inlined readBit logic)
    uint8_t currentBit = (ByteSet[bit_counter / 8] & (1 << (bit_counter % 8)));

    // -------------------------------------------------------------------------
    // MODE: OLDER BOARDS (PU-7 to PU-20) - Standard Gate Logic
    // -------------------------------------------------------------------------
    if (!wfck_mode)
    {
      if (currentBit == 0)
      {
        // For OLD boards, bit 0 is a forced LOW signal
        funPinMode(PD6, GPIO_Speed_30MHz | GPIO_CNF_OUT_PP); // PD6 -> Data Output
        funDigitalWrite(PD6, 0);                             // PD6 -> Data Low
        Delay_Us(bits_delay);
      }
      else
      {
        // For OLD boards, bit 1 is High-Z (Pin set as input)
        funPinMode(PD6, GPIO_CNF_IN_FLOATING); // PD6 <- Data Input
        Delay_Us(bits_delay);
      }
    }

    // -------------------------------------------------------------------------
    // MODE: NEWER BOARDS (PU-22 or newer) - WFCK Clock Synchronization
    // -------------------------------------------------------------------------
    else if (wfck_mode)
    {
      if (currentBit == 0)
      {
        // For NEW boards, bit 0 is also a forced LOW signal
        funPinMode(PD6, GPIO_Speed_30MHz | GPIO_CNF_OUT_PP); // PD6 -> Data Output
        funDigitalWrite(PD6, 0);                             // PD6 Data Low
        Delay_Us(bits_delay);                                // Wait for specified delay between bits
      }
      else
      {
        // For NEW boards, bit 1 must be modulated with the WFCK clock signal
        funPinMode(PD6, GPIO_Speed_30MHz | GPIO_CNF_OUT_PP); // PD6 -> Data Output
        //-----------------------------------------------
        funPinMode(PD5, GPIO_CNF_IN_FLOATING); // PD5 <- Wfck Input AGGIUNTA DA ME

        for (uint8_t count = 30; count > 0; count--)
        {

          while ((funDigitalRead(PD5)) == 1)
            ;                      // Wait for Falling Edge  // While WfckRead == 1
          funDigitalWrite(PD6, 0); // PD6 -> Data Low

          while ((funDigitalRead(PD5)) == 0)
            ;                      // Wait for Rising Edge // While WfckRead == 0
          funDigitalWrite(PD6, 1); // PD6 -> Data High
        }
      }
    }
  }
  // After injecting SCEX data, set DATA pin as output and clear (low)
  funPinMode(PD6, GPIO_Speed_30MHz | GPIO_CNF_OUT_PP); // PD6 -> Data Output
  funDigitalWrite(PD6, 0);                             // PD6 -> Data Low
  Delay_Ms(injections_delay);
}

/*
// *****************************************************************************************
// Function: Bios_Inject
// Description:
// Injects Bios Fix patching based on the bios_mode var
// Also for each bios fix we force the wfck mode, string injection region and
// the Ws2812 rgb led color
// Parameters:
// bios_mode var | bios variant | region variant | Wfck_mode |RGB color set
  0 == Don't apply any bios fix
  1 == Scph-102 | 200,10,200  ws2812b -> Violet
  2 == All JAP bios | 10, 200, 200 ws2812b -> Light Blue
  3 == NOT AVAIABLE
  4 == NOT AVAIABLE
  5 == NOT AVAIABLE
  6 == NOT AVAIABLE
  7 == NOT AVAIABLE
*/
void Bios_Set()
{
  switch (bios_mode)
  {
  case 0:
    // do nothing
    break;
  case 1: // Apply SCPH-102 PAL fix [Onechip mode]
    Scph_102();
    break;
  case 2: // Apply Jap Bios Fix [Jap_Bios_Unlocker mode]
    Jfix = 1;
    Trigger = 0;
    Jready = 0;
    break;
  case 3: // FREE Slot
    bios_fail();
    break;
  case 4: // FREE Slot
    bios_fail();
    break;
  case 5: // FREE Slot
    bios_fail();
    break;
  case 6: // FREE Slot
    bios_fail();
    break;
  case 7: // FREE Slot
    bios_fail();
    break;
  }
}

int main()
{
  SystemInit();
  // Enable GPIOs
  RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO;

  funPinMode(PA1, GPIO_CNF_IN_FLOATING); // PA1 <- Bios_CE Input
  funPinMode(PA1, GPIO_CNF_IN_FLOATING); // PA2 <- Speed Input
  funPinMode(PC0, GPIO_Speed_30MHz | GPIO_CNF_OUT_PP);
  funDigitalWrite(PC0, 1); // PC0 <- VCD PullUP Input
  funPinMode(PC1, GPIO_Speed_30MHz | GPIO_CNF_OUT_PP);
  funDigitalWrite(PC1, 1); // PC1 <- Bios_Bit0 PullUP Input
  funPinMode(PC2, GPIO_Speed_30MHz | GPIO_CNF_OUT_PP);
  funDigitalWrite(PC2, 1); // PC2 <- Bios_Bit1 PullUP Input
  funPinMode(PC3, GPIO_Speed_30MHz | GPIO_CNF_OUT_PP);
  funDigitalWrite(PC3, 1); // PC3 <- Bios_Bit2 PullUP Input
  funPinMode(PC4, GPIO_Speed_30MHz | GPIO_CNF_OUT_PP);
  funDigitalWrite(PC4, 1); // PC4 <- Switch PullUp Input
  funPinMode(PC5, GPIO_Speed_30MHz | GPIO_CNF_OUT_PP);
  funDigitalWrite(PC5, 1); // PC5 <- Region_Bit0 PullUP Input
  funPinMode(PC6, GPIO_Speed_30MHz | GPIO_CNF_OUT_PP);
  funDigitalWrite(PC6, 1); // PC6 <- Region_Bit1 PullUP Input
  funPinMode(PC7, GPIO_CNF_IN_FLOATING);               // PC7 <- SQCK Input
  funPinMode(PD2, GPIO_CNF_IN_FLOATING);               // PD2 <- SUBQ Input
  funPinMode(PD3, GPIO_CNF_IN_FLOATING);               // PD3 <- Bios_Dx Input
  funPinMode(PD4, GPIO_CNF_IN_FLOATING);               // PD4 <- Bios_Ax Input
  funPinMode(PD5, GPIO_CNF_IN_FLOATING);               // PD5 <- WFCK\GATE Input
  funPinMode(PD6, GPIO_CNF_IN_FLOATING);               // PD6 <- DATA Input
  funPinMode(PD0, GPIO_Speed_30MHz | GPIO_CNF_OUT_PP); // PD0 -> WS2812b Output

  // SPEED_INTERRUPT_RISING on PA2;
  // Configure the IO as an interrupt.
  AFIO->EXTICR = AFIO_EXTICR_EXTI2_PA;
  EXTI->INTENR = EXTI_INTENR_MR2; // Enable EXT2
  EXTI->RTENR = EXTI_RTENR_TR2;   // Rising edge trigger

  Modules_set(); // Read the settings based on jumpers

  if ((bios_mode != 0) && (Switch)) // If bios fix is SET and Switch == High, enable the bios patching function!
  {
    Bios_Set();
  }

  LED_SendColour(200, 200, 200); // Send WHITE //Setup begin!:

  while (!(funDigitalRead(PC7)))
    ; // Sqck read==0
  while (!(funDigitalRead(PD5)))
    ; // Wfck read==0

  board_detection();

  LED_SendColour(0, 0, 0); // Send BLACK, Setup done!

  while (1)
  {
    Delay_Ms(1); // Start with a small delay, which can be necessary in cases where the MCU loops too quickly and picks up the laster SUBQ trailing end

    //-------- Japan import bios Fix----------
    if (Jfix && Jready)
    {
      JAP_fix();
    }
    //--------------------------------------
    //__disable_irq(); // start critical section

    do
    {
      for (bitpos = 0; bitpos < 8; bitpos++)
      {
        while ((funDigitalRead(PC7) != 0)) // Sqck read == 1  // wait for clock to go low
        {
          timeout_clock_counter++;
          // a timeout resets the 12 byte stream in case the PSX sends malformatted clock pulses, as happens on bootup
          if (timeout_clock_counter > 1000)
          {
            scpos = 0;
            timeout_clock_counter = 0;
            bitbuf = 0;
            bitpos = 0;
            continue;
          }
        }

        // Wait for clock to go high
        while ((funDigitalRead(PC7)) == 0)
          ; // Sqck read == 0

        if ((funDigitalRead(PD2)) == 1) // Subq == 1 // If clock(?) pin high
        {
          bitbuf |= 1 << bitpos; // Set the bit at position bitpos in the bitbuf to 1. Using OR combined with a bit shift
        }

        timeout_clock_counter = 0; // no problem with this bit
      }

      scbuf[scpos] = bitbuf; // One byte done
      scpos++;
      bitbuf = 0;
    }

    while (scpos < 12); // Repeat for all 12 bytes

    //__enable_irq(); // End critical section

    //************************************************************************
    // Check if read head is in wobble area
    // We only want to unlock game discs (0x41) and only if the read head is in the outer TOC area.
    // We want to see a TOC sector repeatedly before injecting (helps with timing and marginal lasers).
    // All this logic is because we don't know if the HC-05 is actually processing a getSCEX() command.
    // Hysteresis is used because older drives exhibit more variation in read head positioning.
    // While the laser lens moves to correct for the error, they can pick up a few TOC sectors.
    //************************************************************************

    // This variable initialization macro is to replace (0x41) with a filter that will check that only the three most significant bits are correct. 0x001xxxxx
    uint8_t isDataSector = (((scbuf[0] & 0x40) == 0x40) && (((scbuf[0] & 0x10) == 0) && ((scbuf[0] & 0x80) == 0)));

    currentLogic(isDataSector);

    // hysteresis value "optimized" using very worn but working drive on ATmega328 @ 16Mhz
    // should be fine on other MCUs and speeds, as the PSX dictates SUBQ rate
    if (hysteresis >= HYSTERESIS_MAX)
    {
      // If the read head is still here after injection, resending should be quick.
      // Hysteresis naturally goes to 0 otherwise (the read head moved).
      hysteresis = 11;

      //************************************************************************
      // Executes the region code patch injection sequence.
      //************************************************************************
      LED_SendColour(led_r, led_g, led_b); // WS2812b show color based on reg_set//Start Injection

      funPinMode(PD6, GPIO_Speed_30MHz | GPIO_CNF_OUT_PP); // PD6 -> Data Output
      funDigitalWrite(PD6, 0);                             // PD6 Data Low

      if (!wfck_mode) // If wfck_mode is fals (oldmode)
      {
        funPinMode(PD5, GPIO_Speed_30MHz | GPIO_CNF_OUT_PP); // PD5 -> Wfck Output
        funDigitalWrite(PD5, 0);                             // PD5 Wfck Low
      }

      Delay_Ms(injections_delay); // HC-05 waits for a bit of silence (pin low) before it begins decoding.

      // Inject symbols now. 2 x 3 runs seems optimal to cover all boards
      for (uint8_t scex = 0; scex < 2; scex++)
      {
        inject_SCEX(Xbit); // Triple injection based on Xbit val
        inject_SCEX(Xbit); // e = SCEE, a = SCEA, i = SCEI
        inject_SCEX(Xbit); // 1 = SCEE, 2 = SCEA, 3 = SCEI
      }

      if (!wfck_mode)
      {
        funPinMode(PD5, GPIO_CNF_IN_FLOATING); // Pd5 <- Wfck Input
      }

      funPinMode(PD6, GPIO_CNF_IN_FLOATING); // PD6 <- Data Input

      Jready = 1; // For JAP bios patching, now we are ready to inject.

      LED_SendColour(0, 0, 0); // WS2812b LOW //Injection done!
    }
  }
}
