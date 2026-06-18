#pragma once

void Scph_102() // Onechip SCPH-102 Bios patching method 
{
  LED_SendColour(200, 10, 200); // Send VIOLET
    
  Delay_Ms(2600); // Optimized delay for CH32V003 MCU @48MHz

  __disable_irq(); // start critical section

  while (!(GPIOD->INDR >> (4 & 0xf) & 1))
  {
    ; // wait for priming Bios_A18 pulse
  }

  Delay_Us(12);

  // PD3 -> Bios_D2 Output ------------------------------------------------
  GPIOD->CFGLR &= ~(0xF << (4 * 3));
  GPIOD->CFGLR |= (3 | 0) << (4 * 3);
  //------------------------------------------------------------------
  GPIOD->BSHR = (1 << 16 + 3); // Bios_D2 Low
  // D2 = output. Drags line low now

  Delay_Us(5); 

  // PD3 <- Bios_D2 Input ------------------------------------------------
  GPIOD->CFGLR = (GPIOD->CFGLR & (~(0xf << (4 * (3 & 0xf))))) | (4 << (4 * (3 & 0xf)));
  //-----------------------------------------------------------------------
  __enable_irq(); // end critical section
  
  // not necessary but I want to make sure these pins are now high-z again
  funPinMode(PD3, GPIO_CNF_IN_FLOATING); // PD3 <- Bios_D2 Input
  funPinMode(PD4, GPIO_CNF_IN_FLOATING); // PD4 <- Bios_A18 Input

  LED_SendColour(0, 0, 0); // Send BLACK
}

void bios_fail() // BIOS fail condition
{
  while (1)
  {
    // Infinite loop for unimplemented bios_mode
    LED_SendColour(200, 0, 0);
    Delay_Ms(100); // Without, MCU loops to quickly and glitches LED_SendColour function
  }
}