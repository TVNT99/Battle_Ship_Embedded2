/*************************************************************************//**
 * @file     main.c
 * @version  V1.00 
 * @brief    EEET2480 - TUTORIAL 8 - ADC
 * @board: NuMaker-PFM-M487
 * Description: Working with ADC
 * EADC0 Channel 1 is used. Input source is from external pin PB.1 (PIN A5 on the board)
 * ADC clock frequency is 1 MHz
 * EADC 0 channel 1 is used. Input source is from external pin PB.1
 * Value of the analog input voltage is dependent on the rotational position of the potentiometer.
 * Send the value through UART0
*****************************************************************************/
#include <stdio.h>
#include "NuMicro.h"



#define HXTSTB 1<<0				// HXT Clock Sourse Stable Flag
#define HXTEN  1<<0				// HXT Enable Bit, write 1 to enable

void EADC0_Init();		

int main(void)
{
		uint32_t result;
		//System initialization
		SYS_UnlockReg();    // Unlock protected registers
		
		// ---------------------------------------------------
		// CPU Clock setting
		// Clock source: 12MHz external crystal
		// HCLK = 12 MHz
		// ---------------------------------------------------
	
		//Set XT1_OUT (PF.2) and XT1_IN (PF.3) to input mode
    PF->MODE &= ~((0x3 << 4) | (0x3 << 6));
	
		//Enable External High-Speed Crystal (HXT)
    CLK->PWRCTL |= HXTEN; // Enable HXT
    while (!(CLK->STATUS & HXTSTB)); // Wait for HXT to stabilize
		
		// Set HCLK to HXT
		CLK->CLKSEL0 &= (~(0x07 << 0)); // Clear current settings for 
    CLK->CLKSEL0 |= 0x00; 					// Set a new value
	
		// Set HCLK Divider to 0
		CLK->CLKDIV0 &= (~0x0F); // Clear current settings for HCLKDIV
		CLK->CLKDIV0 |= 0x00;			// Set new value

				
		// ---------------------------------------------------
		// Configure EADC0 Channel 1 (PB.1 or A2 on the board)
		// 
		// ---------------------------------------------------	
		// EADC Clock selection and configuration
		// EADC clock source is PCLK1 12 MHz
		CLK->CLKDIV0 &= ~(0x0FF << 16);		// Clear current settings
		CLK->CLKDIV0 |= (11 << 16); 			// EADC clock divider is (11+1) --> ADC clock is 12/12 = 1 MHz
		CLK->APBCLK0 |= (1 << 28); 				// enable EADC0 clock
		
		// PB.1 as input 
		PB->MODE &= (0b11 << 2);			// clear current settings
		PB->DINOFF |= (1 << (16 + 1));	// Disable PB.1 digital input path
		// Select EADC0 channel 1 analog input for PB.1
		SYS->GPB_MFPL &= ~(0xF << 4);			// Clear current settings PB.1 -> [7:4];
		SYS->GPB_MFPL |= (1 << 4);					// EADC0 - MFP1
		
		// Configure Operation Mode
		// Single-ended input mode
		EADC->CTL &= ~ (1 << 8); 								// 0: sigle-end analog input mode; 1: differential analog input mode
		EADC->CTL |= (1 << 0);									// Enable EADC
		while (!(EADC0->PWRM & (1 << 0)));			// Wait for EADC is ready for conversion
		
		// Configure sample module 0 for EADC0_CH1; software trigger
		EADC->SCTL[0] &= ~(0x1F << 16);					// TRGSEL = 0 -> Disable trigger sources
		EADC->SCTL[0] &= ~(0xF << 0);						// Clear settings for channel selection
		EADC->SCTL[0] |= (1 << 0);							// Select EADC0_CH1
		
		EADC->STATUS2 |= (1 << 0);								// Clear any previous interrupt 0 flags for sure
	
		// ---------------------------------------------------
		// Configure and use UART channel 0 - GPIO port B pin 13 (i.e., the TX pin of UART0). 
		// UART clock source is 32.768 kHz
		// UART0 channel is used to transmit data 
		// Data packet: 1 start bit + 8 data bit + no parity bit + 1 stop bit
		// Baud rate: 57600 bps
		// ---------------------------------------------------	
		// UART 0 clock setting
		CLK->CLKSEL1 |= (0b11 << 24); 	// UART 0 clock source is 12 MHz internal high speed RC Oscillator (HIRC)
		CLK->CLKDIV0 &= ~(0xF << 8);		// Clock divider is 1
		CLK->APBCLK0 |= (1 << 16); 			// Enable UART0 clock
		
		// UART 0 - Pin configuration
		// PB.12 (RX) --> Input ; PB.13 (TX) --> output
		PB->MODE &= ~(0b11 << 24); // PB.12 as input
		PB->MODE &= ~(0b11 << 26); // clear setting PB.13
		PB->MODE |= (0b01 << 26); // PB.13 as output push-pull mode
		
		// Set GPB Multi-function pins for UART0 RXD and TXD
		SYS->GPB_MFPH &= ~ ((0xF << 16) | (0xF << 20)); 	// Clear current settings PB.12 -> [19:16]; PB.13 [23:20]
		SYS->GPB_MFPH |= ( (6 << 16) | (6 << 20) );				// UART0 - MFP6
	
		// UART 0 operation configuration
		UART0->LINE |= (0b11 << 0); 			// 8 data bit
		UART0->LINE &= ~(1 << 2); 				// One stop bit
		UART0->LINE &= ~(1 << 3);					// No parity bit
		UART0->FIFO |= (1 << 1); 					// Clear RX FIFO
		UART0->FIFO |= (1 << 2); 					// Clear TX FIFO
		UART0->FIFO &= ~(0xF << 16); 			// FIFO Trigger level is 1 byte
	
		// Baud rate config: Mode 0 - 57600 bps
		UART0->BAUD &= ~(0b11 << 28); // mode 0
		UART0->BAUD &= ~(0xFFFF << 0);
		UART0->BAUD |= 11;	
		
		// LED settings - LED0 on PH.0 as output
		PH->MODE &= ~(0x03 << 0); // Clear the current modes
		PH->MODE |= (0x01 << 0); // Set output mode (0x01). For input mode, set (0x00)
		
		SYS_LockReg();  // Lock protected registers
		

    
		while(1)
		{
			// Start EADC0 conversion on Channel 1		
			EADC->SWTRG |= (1 << 0);									// Start conversion for sample module 0
			while (!( EADC->STATUS0 & (1 << 0)));			// Wait for conversion to complete
			result = (EADC->DAT[0] & 0xFFFF);					// Get conversion result
			printf("EADC Conversion Result: %d\n", result);	// Print conversion result
			
			CLK_SysTickDelay(1000000); 
		}
    
}



