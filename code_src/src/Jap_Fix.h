#pragma once

#define Timeout 10000 //20000

uint8_t Jready = 0;
volatile uint8_t Jfix = 0;
volatile uint8_t Trigger = 0;
volatile uint32_t systick_millis;

uint8_t led_r = 0;
uint8_t led_g =0;
uint8_t led_b = 0;

void systick_init(void)
{
  // Reset any pre-existing configuration
  SysTick->CTLR = 0x0000;

  // Set the compare register to trigger once per millisecond
  SysTick->CMP = DELAY_MS_TIME - 1;

  // Reset the Count Register, and the global millis counter to 0
  SysTick->CNT = 0x00000000;
  systick_millis = 0x00000000;

  // Set the SysTick Configuration
  // NOTE: By not setting SYSTICK_CTLR_STRE, we maintain compatibility with
  // busywait delay funtions used by ch32v003_fun.

  SysTick->CTLR |= SYSTICK_CTLR_STE |  // Enable Counter
                   SYSTICK_CTLR_STIE | // Enable Interrupts
                   SYSTICK_CTLR_STCLK; // Set Clock Source to HCLK/1

  // Enable the SysTick IRQ
  NVIC_EnableIRQ(SysTick_IRQn);
}

void Jfix_Injection()
{
  Delay_Us(100);

  LED_SendColour(10, 200, 200); // Send Light Blue

  while ((GPIOA->INDR >> (1 & 0xf) & 1))
    ; // While Bios_CE is High wait and nothing

  if (!(GPIOA->INDR >> (1 & 0xf) & 1)) // If Bios_CE is Low
  {
    // PD3 -> Bios_D2 Output ------------------------------------------------
    GPIOD->CFGLR &= ~(0xF << (4 * 3));
    GPIOD->CFGLR |= (3 | 0) << (4 * 3);
    //------------------------------------------------------------------
    GPIOD->BSHR = (1 << 16 + 3); // PC3 -> Bios_D2 = output. Drags line low now
  }

  /*
  while ((GPIOD->INDR >> (4 & 0xf) & 1));
  if (GPIOD->INDR >> (4 & 0xf) & 1) // Bios_A18 == 1
{
  Delay_Us(1);
    // PD3 <- Bios_D2 Input ------------------------------------------------
  GPIOD->CFGLR = (GPIOD->CFGLR & (~(0xf << (4 * (3 & 0xf))))) | (4 << (4 * (3 & 0xf)));
  //-----------------------------------------------------------------------
}
 */

  Delay_Us(900);

  // PD3 <- Bios_D2 Input ------------------------------------------------
  GPIOD->CFGLR = (GPIOD->CFGLR & (~(0xf << (4 * (3 & 0xf))))) | (4 << (4 * (3 & 0xf)));
  //-----------------------------------------------------------------------
  
  Jfix = 0; // Patch done

  LED_SendColour(0, 0, 0); // Send BLACK
}

void JAP_fix()
{
  switch (Trigger)
  {
  case 0:
    // SPEED INTERRUPT ENABLE
    NVIC_EnableIRQ(EXTI7_0_IRQn);
    // Start Timer with timeout of 20sec.
    //systick_millis = 0; // Not needed anymore
    systick_init();
    break;
  case 1:
    NVIC_DisableIRQ(EXTI7_0_IRQn);
    Jfix_Injection();
    break;
  }
}

// ISR (Interrupt Service Routine)
void EXTI7_0_IRQHandler(void) __attribute__((interrupt));
void EXTI7_0_IRQHandler(void)
{
  Trigger = 1;
  EXTI->INTFR = EXTI_Line2; // Ack the interrupt.
  JAP_fix();
  // NVIC_DisableIRQ(EXTI7_0_IRQn);
}

/*
 * SysTick ISR - must be lightweight to prevent the CPU from bogging down.
 * Increments Compare Register and systick_millis when triggered (every 1ms)
 * NOTE: the `__attribute__((interrupt))` attribute is very important
 */
void SysTick_Handler(void) __attribute__((interrupt));
void SysTick_Handler(void)
{
  // Increment the Compare Register for the next trigger
  // If more than this number of ticks elapse before the trigger is reset,
  // you may miss your next interrupt trigger
  // (Make sure the IQR is lightweight and CMP value is reasonable)
  SysTick->CMP += DELAY_MS_TIME;

  // Clear the trigger state for the next IRQ
  SysTick->SR = 0x00000000;

  // Increment the milliseconds count
  systick_millis++;

  if (systick_millis > Timeout)
  {
    Jfix = 0; // Too much time, Jap fix disabled
    NVIC_DisableIRQ(SysTick_IRQn);
  }
}