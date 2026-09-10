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

/* Hardware Timer 0 ISR: Manages 10s per-shot countdown and game elapsed time */
void TMR0_IRQHandler(void) {
    if (TIMER0->INTSTS & (1 << 0)) {  /* TIF: Timer Interrupt Flag */
        TIMER0->INTSTS = (1 << 0);    /* Clear interrupt flag */
        PH->DOUT ^= (1 << 1);        // Measure the timer tick with the yellow LED
        if (g_game.state == STATE_PLAY) {
            g_game.elapsed_seconds++;
            Display_LCD_RenderScreen();  // Update the LCD display with the latest game state

            if (g_game.shot_timer > 0) {
                g_game.shot_timer--;
            }

            /* Timeout: 10 seconds expired - deduct a shot */
            if (g_game.shot_timer == 0) {
                if (g_game.shots_left > 0) {
                    g_game.shots_left--;
                }
                g_game.shot_timer = 10;
                Display_RenderScreen(); // Update the display after a shot is deducted
            }

            /* Shots exhausted or the 240s match limit reached -> lose */
            Game_CheckWinLose();
        }
    }
}

// Sunk Detection.
static bool CheckAndSinkPartner(int8_t r, int8_t c) {
    int8_t pr = -1;
    int8_t pc = -1;

    /* Find the orthogonal partner cell */
    if (r > 0 && g_game.hidden_map[r - 1][c]) {
        pr = r - 1;
        pc = c;
    } else if (r < 7 && g_game.hidden_map[r + 1][c]) {
        pr = r + 1;
        pc = c;
    } else if (c > 0 && g_game.hidden_map[r][c - 1]) {
        pr = r;
        pc = c - 1;
    } else if (c < 7 && g_game.hidden_map[r][c + 1]) {
        pr = r;
        pc = c + 1;
    }

    if (pr == -1 || pc == -1) {
        return false;
    }

    if (g_game.display_grid[pr][pc] == 'X') {
        /* Both halves hit -> mark as '#' (fully sunk) */
        g_game.display_grid[r][c] = '#';
        g_game.display_grid[pr][pc] = '#';
        g_game.ships_sunk++;
        return true;
    }

    return false;
}

/* Refresh both the serial terminal and LCD panel with the current in-play state */
static void Display_RefreshAll(void) {
    Display_RenderScreen();
    Display_LCD_RenderScreen();
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
        Display_RefreshAll();
        UART0_SendString("  > ALREADY FIRED HERE! (Shot lost)\r\n"); // Attempted to fire at a previously targeted cell
    } else if (g_game.hidden_map[r][c] == 1) {
        g_game.display_grid[r][c] = 'X'; // Mark hit on display grid
        g_game.hits++; // Increment total hits
        LED_FlashHit_3x(); // Flash LED to indicate a hit

        if (CheckAndSinkPartner(r, c)) {
            Display_RefreshAll();
            UART0_SendString("  > HIT! SHIP SUNK!\r\n"); // Successful hit and ship sunk
        } else {
            Display_RefreshAll();
            char hit_message[40];
            snprintf(hit_message, sizeof(hit_message),
                     "  > HIT! AT CURSOR (%d, %d)\r\n", r, c);
            UART0_SendString(hit_message); // Successful hit on a ship segment
        }
    } else {
        g_game.display_grid[r][c] = 'o';
        Display_RefreshAll();
        UART0_SendString("  > MISS!\r\n"); // Missed shot
    }

    /* Win / Lose resolution */
    Game_CheckWinLose();
}

/* Check win/lose conditions and update game state + end-game screens.
 * Single source of truth - also called every second from TMR0_IRQHandler
 * so the 240s match limit is enforced even without a shot being fired. */
void Game_CheckWinLose(void) {
    if (g_game.ships_sunk == 5) {
        g_game.state = STATE_WIN;
        Display_ShowEndGame(true);
        Display_LCD_ShowEndGame(true);
    } else if (g_game.shots_left == 0 || g_game.elapsed_seconds >= 240) {
        g_game.state = STATE_LOSE;
        Display_ShowEndGame(false);
        Display_LCD_ShowEndGame(false);
    }
}

void GPG_IRQHandler(void) {
    uint32_t status = PG->INTSRC;
    PG->INTSRC = status;

    /* SW2 Restart (PG.15) */
    if (status & (1 << 15)) {
        if (g_game.state == STATE_WELCOME || g_game.state == STATE_EDIT || // Check if the game is in a state that allows restarting
            g_game.state == STATE_WIN || g_game.state == STATE_LOSE) {
            g_game.state = STATE_LOAD; // Set the game state to load a new map
            extern volatile uint8_t s_rx_index;
            extern volatile bool s_map_ready;
            s_rx_index = 0;
            s_map_ready = false;
            UART0_SendString("WELCOME - WAITING FOR MAP\r\n");
            Display_LCD_ShowWaitingForMap();
        }
    }

    /* Any joystick press while waiting for a map starts the on-board editor */
    if (g_game.state == STATE_LOAD && (status & ((1 << 2) | (1 << 3) | (1 << 4)))) {
        g_game.state = STATE_EDIT;
        Editor_Init();
        return;
    }

    if (g_game.state == STATE_PLAY) {
        if (status & (1 << 2)) {  /* UP (PG.2) */
            if (g_game.cursor_row > 0) g_game.cursor_row--;
            Display_RefreshAll();
        }
        if (status & (1 << 4)) {  /* RIGHT (PG.4) */
            if (g_game.cursor_col < 7) g_game.cursor_col++;
            Display_RefreshAll();
        }
        if (status & (1 << 3)) {  /* FIRE (Center, PG.3) */
            UART0_SendString("FIRE pressed\r\n");
            Game_FireAtCursor();
        }
    } else if (g_game.state == STATE_EDIT) {
        if (status & (1 << 2)) {  /* UP (PG.2) */
            if (g_game.cursor_row > 0) g_game.cursor_row--;
            Editor_Redraw();
        }
        if (status & (1 << 4)) {  /* RIGHT (PG.4) */
            if (g_game.cursor_col < 7) g_game.cursor_col++;
            Editor_Redraw();
        }
        if (status & (1 << 3)) {  /* FIRE (Center, PG.3) -> select cell */
            Editor_SelectCursor();
        }
    }
}

void GPC_IRQHandler(void) {
    uint32_t status = PC->INTSRC;
    PC->INTSRC = status;

    /* Any joystick press while waiting for a map starts the on-board editor */
    if (g_game.state == STATE_LOAD && (status & ((1 << 9) | (1 << 10)))) {
        g_game.state = STATE_EDIT;
        Editor_Init();
        return;
    }

    if (g_game.state == STATE_PLAY) {
        if (status & (1 << 9)) {  /* LEFT (PC.9) */
            if (g_game.cursor_col > 0) g_game.cursor_col--;
            Display_RefreshAll();
        }
        if (status & (1 << 10)) {  /* DOWN (PC.10) */
            if (g_game.cursor_row < 7) g_game.cursor_row++;
            Display_RefreshAll();
        }
    } else if (g_game.state == STATE_EDIT) {
        if (status & (1 << 9)) {  /* LEFT (PC.9) */
            if (g_game.cursor_col > 0) g_game.cursor_col--;
            Editor_Redraw();
        }
        if (status & (1 << 10)) {  /* DOWN (PC.10) */
            if (g_game.cursor_row < 7) g_game.cursor_row++;
            Editor_Redraw();
        }
    }
}
