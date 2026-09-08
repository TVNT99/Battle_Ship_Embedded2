#ifndef __GAME_SHARED_H__
#define __GAME_SHARED_H__

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    STATE_WELCOME = 0,
    STATE_LOAD,
    STATE_PLAY,
    STATE_WIN,
    STATE_LOSE
} GameState_t;

typedef struct {
    uint8_t hidden_map[8][8];   /* 0: empty ('-'), 1: ship ('X') */
    char    display_grid[8][8]; /* '.', 'o', 'X', '#' */
    int8_t  cursor_row;         /* 0 to 7 */
    int8_t  cursor_col;         /* 0 to 7 */
    uint8_t shots_left;         /* Starts at 24 */
    uint8_t hits;               /* Total hits */
    uint8_t ships_sunk;         /* Total ships sunk (out of 5) */
    uint8_t shot_timer;         /* 10s countdown */
    uint16_t elapsed_seconds;   /* Total match runtime */
    GameState_t state;
} GameContext_t;

extern volatile GameContext_t g_game;

/* Minh: Comms, Map Engine, Terminal Display */
void System_Clock_Init_Reg(void);
void UART0_Init_Reg(void);
void UART0_SendChar(char c);
void UART0_SendString(const char *str);
bool Map_ValidateAndLoad(const char *buffer);
void Display_RenderScreen(void);
void Display_ShowEndGame(bool won);

/* Tin: Inputs, ISRs, Hardware Timers, LEDs, Rules */
void GPIO_Input_Init_Reg(void);
void Timer_Init_Reg(void);
void LED_Init_Reg(void);
void LED_FlashHit_3x(void);
void Game_FireAtCursor(void);
void Game_CheckWinLose(void);

#endif /* __GAME_SHARED_H__ */