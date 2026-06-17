#ifndef _FUNCONFIG_H
#define _FUNCONFIG_H
#define CH32V003        1

// Place configuration items here, you can see a full list in ch32fun/ch32fun.h
// To reconfigure to a different processor, update TARGET_MCU in the  Makefile
#define FUNCONF_USE_HSI 1               // Use HSI Internal Oscillator
#define FUNCONF_SYSTEM_CORE_CLOCK 48000000  // Computed Clock in Hz (Default only for 003, other chips have other defaults)
#define FUNCONF_SYSTICK_USE_HCLK 1      // Should systick be at 48 MHz (1) or 6MHz (0) on an '003.  Typically set to 0 to divide HCLK by 8.
//#define FUNCONF_ISR_IN_RAM 0            // Put the interrupt vector in RAM.

#endif

