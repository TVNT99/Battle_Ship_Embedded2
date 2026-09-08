#include "NuMicro.h"
#include "game_shared.h"
#include <stdio.h>
#include <string.h>

/* Simple delay without busy loops that lock out ISRs */
static void Software_Delay(uint32_t count) {
    while (count--) __NOP();
}

/* GPIO Inputs: Joystick (PG.2, PC.10, PC.9, PG.4, PG.3, PF.11), SW2 (PG.15) */
void GPIO_Input_Init_Reg(void) {
    SYS_UnlockReg();

    /* Set Pin Modes to Input (00b) */
    /* PG.2, PG.3, PG.4, PG.15 */
    PG->MODE &= ~((0x3 << 4) | (0x3 << 6) | (0x3 << 8) | (0x3 << 30));

    /* PC.9, PC.10 */
    PC->MODE &= ~((0x3 << 18) | (0x3 << 20));

    /* PF.11 */
    PF->MODE &= ~(0x3 << 22);

    /* Enable Debounce Engine */
    GPIO->DBCTL =  (1 << 4) | (0x7 << 0);  /* Enable, LIRC(10kHz), DBCLKSEL=128 clks (~12.8ms) */

    /* Enable debounce on input pins */
    PG->DBEN |= (1 << 2) | (1 << 3) | (1 << 4) | (1 << 15);  /* PG.2, PG.3, PG.4, PG.15 */
    PC->DBEN |= (1 << 9) | (1 << 10);                        /* PC.9, PC.10 */
    PF->DBEN |= (1 << 11);                                   /* PF.11 */

    /* Falling-edge trigger configuration (buttons active-low) */
    PG->INTTYPE &= ~((1 << 2) | (1 << 3) | (1 << 4) | (1 << 15));  /* Edge trigger  */
    PG->INTEN  |= (1 << 2) | (1 << 3) | (1 << 4) | (1 << 15);      /* Enable interrupts */

    PC->INTTYPE &= ~((1 << 9) | (1 << 10));                        /* Edge trigger */
    PC->INTEN  |= (1 << 9) | (1 << 10);                            /* Enable interrupts */

    PF->INTTYPE &= ~(1 << 11);                                     /* Edge trigger */
    PF->INTEN  |= (1 << 11);                                       /* Enable interrupt */

    /* Enable NVIC interrupts */
    NVIC_EnableIRQ(GPG_IRQn);
    NVIC_EnableIRQ(GPC_IRQn);
    NVIC_EnableIRQ(GPF_IRQn);

    SYS_LockReg();
}

/* Hardware Timer 0: 1 Hz Periodic Tick for 10-second Shot Timer & Measurement */
void Timer_Init_Reg(void) {
    /* Set Timer 0 clock source to HCLK */
    CLK->CLKSEL1 &= ~(0x7 << 8);      /* Clear bits 10:8 for TMR0SEL */
    CLK->CLKSEL1 |= (0x0 << 8);       /* Set to HCLK (000b) - 12MHz */

    /* Enable Timer 0 clock */
    CLK->APBCLK0 |= (1 << 2);

    /* Configure Timer 0: Periodic mode, 1Hz tick */
    /* HCLK = 12 MHz */
    /* Prescaler: 12 (divides 12MHz to 1MHz) */
    /* Compare value: 1,000,000 (for 1 second period) */
    TIMER0->CTL = 0;                  /* Reset CTL register */
    TIMER0->CTL |= (11 << 0);         /* PSC=11 (divider=12) → 1MHz */
    TIMER0->CTL |= (0x1 << 27);       /* Set MODE=1 (Periodic mode) */
    TIMER0->CTL |= (1 << 29);         /* Enable interrupt (INTEN) */
    TIMER0->CMP = 1000000-1;            /* 1 second timeout */

    /* Enable Timer 0 interrupt in NVIC */
    NVIC_EnableIRQ(TMR0_IRQn);

    /* Start Timer 0 */
    TIMER0->CTL |= (1 << 30);  /* CNTEN */
}

/* LED Hit Feedback (PH.0: Red, PH.1: Yellow, PH.2: Green) */
void LED_Init_Reg(void) {
    /* Set PH.0, PH.1, PH.2 as output mode (01b) */
    PH->MODE &= ~((0x3 << 0) | (0x3 << 2) | (0x3 << 4));
    PH->MODE |= (0x1 << 0) | (0x1 << 2) | (0x1 << 4);

    /* Initialize LEDs as off (active-low = 1 = off) */
    PH->DOUT |= (1 << 0) | (1 << 1) | (1 << 2);
}

void LED_FlashHit_3x(void) {
		int i;
    for (i = 0; i < 3; i++) {
        PH->DOUT &= ~((1 << 0) | (1 << 1) | (1 << 2)); /* LED ON */
        Software_Delay(600000);
        PH->DOUT |= (1 << 0) | (1 << 1) | (1 << 2); /* LED OFF */
        Software_Delay(600000);
    }
}

/* Hardware Timer 0 ISR: Manages 10s countdown, game elapsed time, and pin toggle */
void TMR0_IRQHandler(void) {
    if (TIMER0->INTSTS & (1 << 0)) {  /* TIF: Timer Interrupt Flag */
        TIMER0->INTSTS = (1 << 0);    /* Clear interrupt flag */
        PH->DOUT ^= (1 << 0);         /* Toggle PH.0 for Logic Analyser measurement */

        if (g_game.state == STATE_PLAY) {
            g_game.elapsed_seconds++;

            if (g_game.shot_timer > 0) {
                g_game.shot_timer--;
            }

            /* Timeout: 10 seconds expired - deduct a shot */
            if (g_game.shot_timer == 0) {
                // UART0_SendString("[TMR0] Shot timer expired\r\n");
                if (g_game.shots_left > 0) {
                    g_game.shots_left--;
                }
                g_game.shot_timer = 10;
                // Display_RenderScreen();  /* Update on timeout event */

                if (g_game.shots_left == 0) {
                    g_game.state = STATE_LOSE;
                    Display_ShowEndGame(false);
                }
            }
        }
    }
}

/* Sunk Detection: A ship is 2 cells. Find partner cell and check if both are struck */
static bool CheckAndSinkPartner(int8_t r, int8_t c) {
    int8_t pr = -1, pc = -1; /* Partner row and column */
    /* Find orthogonal partner cell */
    if (r > 0 && g_game.hidden_map[r - 1][c]) { pr = r - 1; pc = c; } // Check cell above for partner
    else if (r < 7 && g_game.hidden_map[r + 1][c]) { pr = r + 1; pc = c; } // Check cell below for partner
    else if (c > 0 && g_game.hidden_map[r][c - 1]) { pr = r; pc = c - 1; } // Check cell to the left for partner
    else if (c < 7 && g_game.hidden_map[r][c + 1]) { pr = r; pc = c + 1; } // Check cell to the right for partner

    if (pr != -1 && pc != -1) {
        if (g_game.display_grid[pr][pc] == 'X') {
            /* Both halves hit -> Mark as '#' (fully sunk) */
            g_game.display_grid[r][c] = '#';
            g_game.display_grid[pr][pc] = '#';
            g_game.ships_sunk++;
            return true;
        }
    }
    return false;
}

/* Fire Handling at Cursor Position */
void Game_FireAtCursor(void) {
    int8_t r = g_game.cursor_row;
    int8_t c = g_game.cursor_col;

    if (g_game.shots_left == 0) return; // No shots left, cannot fire
    g_game.shots_left--;
    g_game.shot_timer = 10; /* Reset shot countdown on fire */

    /* Repeat shot check */
    if (g_game.display_grid[r][c] != '.') {
        Display_RenderScreen();
        UART0_SendString("  > ALREADY FIRED HERE! (Shot lost)\r\n"); // Attempted to fire at a previously targeted cell
    } else if (g_game.hidden_map[r][c] == 1) {
        g_game.display_grid[r][c] = 'X'; // Mark hit on display grid
        g_game.hits++; // Increment total hits
        LED_FlashHit_3x(); // Flash LED to indicate a hit

        if (CheckAndSinkPartner(r, c)) {
            Display_RenderScreen();
            UART0_SendString("  > HIT! SHIP SUNK!\r\n"); // Successful hit and ship sunk
        } else {
            Display_RenderScreen();
            UART0_SendString("  > HIT!\r\n"); // Successful hit on a ship segment
        }
    } else {
        g_game.display_grid[r][c] = 'o';
        Display_RenderScreen();
        UART0_SendString("  > MISS!\r\n"); // Missed shot
    }

    /* Win / Lose resolution */
    if (g_game.ships_sunk == 5) {
        g_game.state = STATE_WIN;
        Display_ShowEndGame(true);
    } else if (g_game.shots_left == 0) {
        g_game.state = STATE_LOSE;
        Display_ShowEndGame(false);
    }
}

/* Check win/lose conditions and update game state */
void Game_CheckWinLose(void) {
    if (g_game.ships_sunk == 5) {
        g_game.state = STATE_WIN;
        Display_ShowEndGame(true);
    } else if (g_game.shots_left == 0) {
        g_game.state = STATE_LOSE;
        Display_ShowEndGame(false);
    }
}
void GPG_IRQHandler(void) {
    uint32_t status = PG->INTSRC;
    PG->INTSRC = status;

    /* SW2 Restart (PG.15) */
    if (status & (1 << 15)) {
        if (g_game.state == STATE_WELCOME || g_game.state == STATE_WIN || g_game.state == STATE_LOSE) {
            g_game.state = STATE_LOAD;
            extern volatile uint8_t s_rx_index;
            extern volatile bool s_map_ready;
            s_rx_index = 0;
            s_map_ready = false;
            UART0_SendString("\033[2J\033[HWELCOME - WAITING FOR MAP\r\n");
        }
    }

    if (g_game.state == STATE_PLAY) {
        if (status & (1 << 2)) {  /* UP (PG.2) */
            if (g_game.cursor_row > 0) g_game.cursor_row--;
            Display_RenderScreen();
        }
        if (status & (1 << 4)) {  /* RIGHT (PG.4) */
            if (g_game.cursor_col < 7) g_game.cursor_col++;
            Display_RenderScreen();
        }
        if (status & (1 << 3)) {  /* FIRE (Center, PG.3) */
            UART0_SendString("FIRE pressed\r\n");
            Game_FireAtCursor();
        }
    }
}

void GPC_IRQHandler(void) {
    uint32_t status = PC->INTSRC;
    PC->INTSRC = status;

    if (g_game.state == STATE_PLAY) {
        if (status & (1 << 9)) {  /* LEFT (PC.9) */
            if (g_game.cursor_col > 0) g_game.cursor_col--;
            Display_RenderScreen();
        }
        if (status & (1 << 10)) {  /* DOWN (PC.10) */
            if (g_game.cursor_row < 7) g_game.cursor_row++;
            Display_RenderScreen();
        }
    }
}

void GPF_IRQHandler(void) {
    uint32_t status = PF->INTSRC;
    PF->INTSRC = status;

    if (g_game.state == STATE_PLAY) {
        if (status & (1 << 11)) {  /* FIRE (Center, PF.11) */ 
            Game_FireAtCursor(); // Additional Fire button
        }
    }
}
