#ifndef __GAME_SHARED_H__
#define __GAME_SHARED_H__

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    STATE_WELCOME = 0,
    STATE_LOAD,
    STATE_EDIT,
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

/* Minh: UART, System Clock, Display, LEDs */
void System_Clock_Init_Reg(void);
void UART0_Init_Reg(void);
void UART0_SendChar(char c);
void UART0_SendString(const char *str);
void Display_RenderScreen(void);
void Display_ShowEndGame(bool won);
void Game_CheckWinLose(void);
void LED_Init_Reg(void);
void LED_FlashHit_3x(void);

/* Tin: Inputs, ISRs, Hardware Timers, Rules, LCD set up, Editor, Game Logic */
void GPIO_Input_Init_Reg(void);
void Timer_Init_Reg(void);
void Game_FireAtCursor(void);
void Display_LCD_RenderScreen(void);
void Display_LCD_ShowEndGame(bool won);
void Display_LCD_ShowWelcome(void);
void Display_LCD_ShowWaitingForMap(void);
bool Map_ValidateAndLoad(const char *buffer);
bool MapRules_CheckPairsAndSpacing(uint8_t grid[8][8]);
/* On-board map editor (touch + joystick) */
void Editor_Init(void);
void Editor_PollTouch(void);
void Editor_Redraw(void);
void Editor_SelectCursor(void);
#endif /* __GAME_SHARED_H__ */