#include "NuMicro.h"
#include "game_shared.h"
#include "EBI_LCD_Module.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    System_Clock_Init_Reg();
    UART0_Init_Reg();
    GPIO_Input_Init_Reg();
    Timer_Init_Reg();
    LED_Init_Reg();

    ILI9341_Initial();  /* Initialize LCD display */
    Timer3_Init();      /* Initialize Timer3 for LCD refresh */

    memset((void*)&g_game, 0, sizeof(GameContext_t));
    g_game.state = STATE_WELCOME;
    UART0_SendString("\033[2J\033[HBATTLESHIP M487 INITIALISED\r\nPress SW2 to start loading map...\r\n");

    while (1) {
        if (g_game.state == STATE_LOAD) {
            /* Handled in main loop when map byte buffer fills */
            extern volatile bool s_map_ready;
            extern char s_uart_rx_buf[128]; // UART receive buffer
            extern volatile uint8_t s_rx_index; // Index for the UART receive buffer

            if (s_map_ready) { // Map is ready for processing. It will go to the play state
                s_map_ready = false; // Reset map ready flag
                if (Map_ValidateAndLoad(s_uart_rx_buf)) {
                    UART0_SendString("MAP LOADED\r\n");
                    g_game.shots_left = 24;
                    g_game.hits = 0;
                    g_game.ships_sunk = 0;
                    g_game.cursor_row = 0;
                    g_game.cursor_col = 0;
                    g_game.shot_timer = 10;
                    g_game.elapsed_seconds = 0;
                    g_game.state = STATE_PLAY;
                    Display_RenderScreen();
                } else {
                    UART0_SendString("MAP INVALID\r\n"); // Map failed validation. It will stay at load state
                    s_rx_index = 0; // Reset buffer index and keep waiting
                }
            }
        } else if (g_game.state == STATE_PLAY) {
            /* Game is active, managed by interrupts (joystick, timer) */
            /* Main loop yields to allow ISRs to run */
            __WFE(); /* Wait for events (ISRs) */
        } else if (g_game.state == STATE_WIN || g_game.state == STATE_LOSE) {
            /* Game over, waiting for restart via SW2 (handled in GPG_IRQHandler) */
            __WFE();
        }
    }
}