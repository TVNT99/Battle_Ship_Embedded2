#include "NuMicro.h"
#include "game_shared.h"
#include <stdio.h>
#include <string.h>

/* UART receive buffer for map loading */
volatile char s_uart_rx_buf[128];
volatile uint8_t s_rx_index = 0;
volatile bool s_map_ready = false;

/* Define global game context - allocates memory for g_game */
volatile GameContext_t g_game;

#define HXTSTB (1 << 0)
#define HXTEN  (1 << 0)

/* System clock initialization - enables HXT 12MHz and sets CPU clock */
void System_Clock_Init_Reg(void) {
    SYS_UnlockReg();

    /* Set XT1_OUT (PF.2) and XT1_IN (PF.3) to input mode */
    PF->MODE &= ~((0x3 << 4) | (0x3 << 6));

    /* Enable External High-Speed Crystal (HXT) */
    CLK->PWRCTL |= HXTEN;
    while (!(CLK->STATUS & HXTSTB));

    /* Set HCLK clock source to HXT (12MHz) */
    CLK->CLKSEL0 &= ~(0x7 << 0);  /* Clear HCLKSEL */
    CLK->CLKSEL0 |= 0x0;            /* Select HXT as HCLK source */

    /* Set HCLK divider to 1 (no division) */
    CLK->CLKDIV0 &= ~(0xF << 0);   /* Clear HCLKDIV */
    CLK->CLKDIV0 |= 0x0;            /* Divider = 1 */

    SYS_LockReg();
}

/* UART0 initialization: 57600 bps, 8 data bits, no parity, 1 stop bit */
void UART0_Init_Reg(void) {
    SYS_UnlockReg();
    /* Set PB.12 (RX) and PB.13 (TX) pins */
    PB->MODE &= ~((0x3 << 24) | (0x3 << 26));  /* Clear mode for PB.12, PB.13 */
    PB->MODE |= (0x0 << 24) | (0x1 << 26);     /* PB.12 input, PB.13 output */

    /* Set PB.12/PB.13 multi-function to UART0 RXD/TXD */
    SYS->GPB_MFPH &= ~((0xF << 16) | (0xF << 20));  /* Clear PB.12, PB.13 settings */
    SYS->GPB_MFPH |= (6 << 16) | (6 << 20);     /* Set to UART0 (MFP6) */

    /* Set UART0 clock source to HIRC (12 MHz) */
    CLK->CLKSEL1 &= ~(0x3 << 24);              /* Clear bits 25:24 (UART0SEL) */
    CLK->CLKSEL1 |= (0x3 << 24);               /* Set to HIRC (11b) */

    /* Set UART0 clock divider to 1 */
    CLK->CLKDIV0 &= ~(0xF << 8);               /* Clear bits 11:8 (UART0DIV) */
    CLK->CLKDIV0 |= (0x0 << 8);                /* Divider = 1 */

    /* Enable UART0 clock */
    CLK->APBCLK0 |= (1 << 16);

    /* Configure UART0 operation */
    UART0->LINE &= ~(0x3 << 0);
    UART0->LINE |= (0x3 << 0);      /* 8 data bits */
    UART0->LINE &= ~(1 << 2);       /* 1 stop bit */
    UART0->LINE &= ~(1 << 3);       /* No parity */

    /* Clear FIFO */
    UART0->FIFO |= (1 << 1);        /* Clear RX FIFO */
    UART0->FIFO |= (1 << 2);        /* Clear TX FIFO */
    UART0->FIFO &= ~(0xF << 16);   /* RX trigger level = 1 byte (interrupt fires immediately) */

    /* Configure baud rate: 57600 bps at 12MHz */
    /* Mode 0: Baud rate = UART clock / (16 * (BAUD+2)) */
    /* 12,000,000 / (16 * 57600) = 13 */
    UART0->BAUD &= ~(0x3 << 28);    /* Mode 0 */
    UART0->BAUD &= ~(0xFFFF << 0);
    UART0->BAUD |= 11;

    /* Enable RX interrupt */
    UART0->INTEN |= (1 << 0);       /* Enable RDAINT */
    NVIC_EnableIRQ(UART0_IRQn);

    SYS_LockReg();
}

void UART0_SendChar(char c) {
    while (UART0->FIFOSTS & (1 << 23));  /* Wait until TX FIFO not full (bit 23) */
    UART0->DAT = c;
}

/* UART0 transmit string */
void UART0_SendString(const char *str) {
    while (*str) {
        UART0_SendChar(*str++);
    }
}

/* UART0 receive interrupt handler - stores map bytes into buffer */
void UART0_IRQHandler(void) {
    if (UART0->INTSTS & (1 << 0)) {  /* RDAINT: RDA interrupt */
        char c = UART0->DAT;

        /* Skip whitespace and line breaks */
        if (c == ' ' || c == '\r' || c == '\n' || c == '\t') {
            return;
        }

        /* Only accept valid map symbols: -, X, 0, 1 */
        if ((c != '-') && (c != 'X') && (c != '0') && (c != '1')) {
            return;
        }

        /* Store valid symbol if buffer not full */
        if (s_rx_index < 64) {
            s_uart_rx_buf[s_rx_index++] = c;

            /* Signal when 64 valid symbols received */
            if (s_rx_index >= 64) {
                s_map_ready = true;
            }
        }
    }
}

