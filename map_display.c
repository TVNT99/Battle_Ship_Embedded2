#include "NuMicro.h"
#include "game_shared.h"
#include <stdio.h>
#include <string.h>
/* LCD Display Functions */
#include "EBI_LCD_Module.h"

/* Shared ship-placement rules: every ship cell has exactly one orthogonal
 * partner cell, and no ship touches another (including diagonally).
 * Used both for the UART map loader and the on-board touch editor, so the
 * two entry points always enforce identical rules. */
bool MapRules_CheckPairsAndSpacing(uint8_t grid[8][8]) {
    /* Each ship cell must have exactly one orthogonal neighbor (its partner) */
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (grid[r][c]) {
                int neighbor_count = 0;
                if (r > 0 && grid[r-1][c]) neighbor_count++; // Check cell above
                if (r < 7 && grid[r+1][c]) neighbor_count++; // Check cell below
                if (c > 0 && grid[r][c-1]) neighbor_count++; // Check cell to the left
                if (c < 7 && grid[r][c+1]) neighbor_count++; // Check cell to the right

                if (neighbor_count != 1) {
                    return false;
                }
            }
        }
    }

    /* No diagonal touching between ships */
    for (r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (grid[r][c]) {
                if (r > 0 && c > 0 && grid[r-1][c-1]) return false;   // NW diagonal neighbor
                if (r > 0 && c < 7 && grid[r-1][c+1]) return false;   // NE diagonal neighbor
                if (r < 7 && c > 0 && grid[r+1][c-1]) return false;   // SW diagonal neighbor
                if (r < 7 && c < 7 && grid[r+1][c+1]) return false;   // SE diagonal neighbor
            }
        }
    }

    return true;
}

/* Convert map buffer (symbols) to grid (0/1), and validate structure */
bool Map_ValidateAndLoad(const char *buffer) {
    uint8_t temp_grid[8][8];
    /* Convert buffer symbols to grid (0 = empty, 1 = ship) */
    for (int i = 0; i < 64; i++) {
        char c = buffer[i];
        temp_grid[i / 8][i % 8] = (c == 'X' || c == '1') ? 1 : 0; // Convert 'X' or '1' to ship, else empty
    }

    /* Count total ship cells - must be exactly 10 (five 2-cell ships) */
    int ship_cells = 0; // Total number of ship cells counted
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (temp_grid[r][c]) ship_cells++; // Count ship cells
        }
    }
    if (ship_cells != 10) {
        return false; // Invalid number of ship cells
    }

    if (!MapRules_CheckPairsAndSpacing(temp_grid)) {
        return false;
    }

    /* Map is valid - store in global game context */
    memcpy((void *)g_game.hidden_map, (void *)temp_grid, sizeof(temp_grid)); // Copy the validated temporary grid to the hidden map

    /* Initialize display grid with all unfired cells */
    for (r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            g_game.display_grid[r][c] = '.';
        }
    }

    return true;
}

/* Format time as MM:SS for display */
static void FormatTime(uint16_t seconds, char *buf) {
    uint16_t mins = seconds / 60;
    uint16_t secs = seconds % 60;
    sprintf(buf, "%02d:%02d", mins, secs);
}

/* Clear screen and render game display */
void Display_RenderScreen(void) {
    char time_buf[8];


    /* Display header */
    UART0_SendString("  BATTLESHIP  -  M487\r\n");
    UART0_SendString("  Shots left: ");
    sprintf(time_buf, "%2d", g_game.shots_left);
    UART0_SendString(time_buf);
    /* Display hits */
    UART0_SendString("    Hits: ");
    sprintf(time_buf, "%2d", g_game.hits);
    UART0_SendString(time_buf);
    /* Display ships sunk */
    UART0_SendString("    Ships sunk: ");
    sprintf(time_buf, "%d", g_game.ships_sunk);
    UART0_SendString(time_buf);
    UART0_SendString(" / 5\r\n");

    /* Display time and shot timer */
    FormatTime(g_game.elapsed_seconds, time_buf);
    UART0_SendString("  Time: ");
    UART0_SendString(time_buf);
    /* Display shot timer */
    UART0_SendString("       Shot timer: ");
    sprintf(time_buf, "%2d", g_game.shot_timer);
    UART0_SendString(time_buf);
    UART0_SendString(" s\r\n\r\n");

    /* Display grid header (column numbers) */
    UART0_SendString("      0 1 2 3 4 5 6 7\r\n");
    /* Display grid with row numbers */
    for (int r = 0; r < 8; r++) {
        sprintf(time_buf, "   %d  ", r);
        UART0_SendString(time_buf);

        for (int c = 0; c < 8; c++) {
            /* Show cursor as brackets around current cell */
            if (r == g_game.cursor_row && c == g_game.cursor_col) {
                UART0_SendChar('[');
                UART0_SendChar(g_game.display_grid[r][c]);
                UART0_SendChar(']');
            } else {
                UART0_SendChar(g_game.display_grid[r][c]);
                UART0_SendChar(' ');
            }
        }
        UART0_SendString("\r\n");
    }
    UART0_SendString("\r\n");
}

/* Display end game screen (win or lose) */
void Display_ShowEndGame(bool won) { // true if the player won, false if lost
    char time_buf[16];
    uint16_t shots_used;
    uint16_t score;

    if (won) { // Player won
        UART0_SendString("  ========================================\r\n");
        UART0_SendString("  CONGRATULATIONS - YOU WIN!\r\n");
        UART0_SendString("  ========================================\r\n\r\n");

        shots_used = 24 - g_game.shots_left;

        UART0_SendString("  Shots used: ");
        sprintf(time_buf, "%d", shots_used);
        UART0_SendString(time_buf);

        UART0_SendString("\r\n  Time: ");
        FormatTime(g_game.elapsed_seconds, time_buf);
        UART0_SendString(time_buf);

        /* Calculate score: (500 - shots_used*10 - elapsed_seconds) + 500 bonus for winning */
        score = (500 - (shots_used * 10) - g_game.elapsed_seconds) + 500;

        UART0_SendString("\r\n  Score: ");
        sprintf(time_buf, "%d", score);
        UART0_SendString(time_buf);
        UART0_SendString("\r\n");
    } else { // Player lost
        UART0_SendString("  ========================================\r\n");
        UART0_SendString("  GAME OVER - OUT OF SHOTS\r\n");
        UART0_SendString("  ========================================\r\n\r\n");

        /* Reveal the hidden map on loss */
        UART0_SendString("  HIDDEN MAP REVEALED:\r\n");
        UART0_SendString("      0 1 2 3 4 5 6 7\r\n");
        /* Display the hidden map */
        int r,c;
        for (r = 0; r < 8; r++) {
            sprintf(time_buf, "   %d  ", r);
            UART0_SendString(time_buf);

            for (c = 0; c < 8; c++) {
                char symbol = g_game.hidden_map[r][c] ? 'X' : '-';
                UART0_SendChar(symbol);
                UART0_SendChar(' ');
            }
            UART0_SendString("\r\n");
        }
        /* Display ships sunk and score */
        UART0_SendString("\r\n  Ships sunk: ");
        sprintf(time_buf, "%d", g_game.ships_sunk);
        UART0_SendString(time_buf); // Display the number of ships sunk
        UART0_SendString(" / 5\r\n"); // Display ships sunk out of total 5

        UART0_SendString("  Score: "); // Display the score label
        shots_used = 24 - g_game.shots_left;  /* Calculate shots used for score */
        score = 500 - (shots_used * 10) - g_game.elapsed_seconds;  /* Ensure score is 0 on loss */
        sprintf(time_buf, "%d", score); // Format the score into the buffer
        UART0_SendString(time_buf); // Display the calculated score
        UART0_SendString("\r\n");
    }

    UART0_SendString("\r\n  Press SW2 to restart...\r\n");
}
void Display_LCD_RenderScreen(void) {
    char buf[32];
    char time_buf[8];

    /* Stats line 1: Shots & Hits */
    sprintf(buf, "Shots:%2d  Hits:%2d", g_game.shots_left, g_game.hits);
    LCD_PutString(10, 5, (uint8_t*)buf, C_BLACK, C_WHITE);

    /* Stats line 2: Ships sunk & per-shot countdown */
    sprintf(buf, "Sunk:%d/5  Shot Timer:%2ds", g_game.ships_sunk, g_game.shot_timer);
    LCD_PutString(10, 20, (uint8_t*)buf, C_BLACK, C_WHITE);

    /* Stats line 3: Elapsed match time as MM:SS */
    FormatTime(g_game.elapsed_seconds, time_buf);
    sprintf(buf, "Time: %s", time_buf);
    LCD_PutString(10, 35, (uint8_t*)buf, C_BLACK, C_WHITE);
    /* The LCD size is 240x320 pixels */
    /* Draw 8x8 grid starting at (16, 60) with 26px cells (8*26=208px,
     * centered on the 240px-wide screen) */
    int grid_x = 16;
    int grid_y = 60;
    int cell_size = 26;

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            int x = grid_x + c * cell_size;
            int y = grid_y + r * cell_size;
            char cell = g_game.display_grid[r][c];
            uint16_t color;

            /* Color based on cell state */
            if (cell == 'X') color = C_RED;        /* Hit */
            else if (cell == 'o') color = C_BLUE;  /* Miss */
            else if (cell == '#') color = C_GREEN; /* Sunk */
            else color = C_WHITE;                  /* Empty/not fired */

            /* Draw a gray grid-line border first so empty cells stay visible
             * against the white background, then fill the inset with the
             * cell's state color */
            LCD_BlankArea(x, y, cell_size - 2, cell_size - 2, C_GRAY);
            LCD_BlankArea(x + 1, y + 1, cell_size - 4, cell_size - 4, color);

            /* Highlight cursor with a black border (visible on white background) */
            if (r == g_game.cursor_row && c == g_game.cursor_col) {
                LCD_BlankArea(x, y, cell_size - 2, 2, C_BLACK);
                LCD_BlankArea(x, y, 2, cell_size - 2, C_BLACK);
            }
        }
    }
}

/* Simple LCD screens shown before a game is in progress, so the panel is
 * never left blank/white with no feedback */
void Display_LCD_ShowWelcome(void) {
    LCD_BlankArea(0, 0, 240, 320, C_WHITE);
    LCD_PutString(40, 100, (uint8_t*)"BATTLESHIP", C_BLACK, C_WHITE);
    LCD_PutString(20, 140, (uint8_t*)"Press SW2 to start", C_BLACK, C_WHITE);
}
/* Display the waiting for map screen */
void Display_LCD_ShowWaitingForMap(void) {
    LCD_BlankArea(0, 0, 240, 320, C_WHITE);
    LCD_PutString(10, 100, (uint8_t*)"WAITING FOR MAP", C_BLACK, C_WHITE);
    LCD_PutString(10, 130, (uint8_t*)"Touch screen to", C_BLACK, C_WHITE);
    LCD_PutString(10, 150, (uint8_t*)"place ships, or", C_BLACK, C_WHITE);
    LCD_PutString(10, 170, (uint8_t*)"send map via UART", C_BLACK, C_WHITE);
}
/* Display the end game screen */
void Display_LCD_ShowEndGame(bool won) {
    char buf[32];
    uint16_t shots_used;
    uint16_t score;

    /* Clear screen */
    LCD_BlankArea(0, 0, 240, 320, C_WHITE);

    if (won) { // Player won
        LCD_PutString(40, 50, (uint8_t*)"YOU WIN!", C_GREEN, C_WHITE);
        shots_used = 24 - g_game.shots_left;

        sprintf(buf, "Shots: %d", shots_used);
        LCD_PutString(20, 100, (uint8_t*)buf, C_BLACK, C_WHITE);

        sprintf(buf, "Time: %d sec", g_game.elapsed_seconds);
        LCD_PutString(20, 120, (uint8_t*)buf, C_BLACK, C_WHITE);

        score = (500 - (shots_used * 10) - g_game.elapsed_seconds) + 500;
        if (score < 0) score = 0;

        sprintf(buf, "SCORE: %d", score);
        LCD_PutString(30, 150, (uint8_t*)buf, C_BLUE, C_WHITE);
    } else { // Player lost
        LCD_PutString(30, 50, (uint8_t*)"GAME OVER", C_RED, C_WHITE);

        sprintf(buf, "Ships sunk: %d/5", g_game.ships_sunk);
        LCD_PutString(20, 100, (uint8_t*)buf, C_BLACK, C_WHITE);

        shots_used = 24 - g_game.shots_left;
        score = 500 - (shots_used * 10) - g_game.elapsed_seconds;

        sprintf(buf, "SCORE: %d", score);
        LCD_PutString(30, 150, (uint8_t*)buf, C_BLUE, C_WHITE);
    }

    LCD_PutString(30, 250, (uint8_t*)"Press SW2 to restart", C_BLACK, C_WHITE);
}

/* ---------------- On-board touch map editor ---------------- */
/* Same grid layout as Display_LCD_RenderScreen so touch coordinates map 1:1 */
#define EDIT_GRID_X   16
#define EDIT_GRID_Y   45
#define EDIT_CELL_SZ  26
/* On-board touch map editor grid layout and state */
static uint8_t s_edit_grid[8][8]; 
static int8_t  s_pending_row = -1; // Pending row for the touch editor
static int8_t  s_pending_col = -1; // Pending column for the touch editor
static uint8_t s_ships_placed = 0; // Number of ships placed in the editor
static bool    s_touch_was_down = false; // Whether the touch screen was previously pressed

static void Editor_Delay(uint32_t count) {
    while (count--) __NOP();
}

static void Editor_Draw(void) {
    char buf[32];

    LCD_BlankArea(0, 0, 240, 320, C_WHITE);

    sprintf(buf, "Place ships  %d/5", s_ships_placed);
    LCD_PutString(10, 5, (uint8_t*)buf, C_BLACK, C_WHITE);
    LCD_PutString(10, 20, (uint8_t*)"Move+Joystick or tap 2 cells", C_BLACK, C_WHITE);

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            int x = EDIT_GRID_X + c * EDIT_CELL_SZ;
            int y = EDIT_GRID_Y + r * EDIT_CELL_SZ;
            uint16_t color = s_edit_grid[r][c] ? C_BLACK : C_WHITE;

            /* Gray grid-line border so empty cells stay visible on white */
            LCD_BlankArea(x, y, EDIT_CELL_SZ - 2, EDIT_CELL_SZ - 2, C_GRAY);
            LCD_BlankArea(x + 1, y + 1, EDIT_CELL_SZ - 4, EDIT_CELL_SZ - 4, color); 

            /* Joystick cursor (blue) - drawn first so the pending-cell
             * highlight (yellow) below still wins if they overlap */
            if (r == g_game.cursor_row && c == g_game.cursor_col) {
                LCD_BlankArea(x, y, EDIT_CELL_SZ - 2, 2, C_BLUE);
                LCD_BlankArea(x, y, 2, EDIT_CELL_SZ - 2, C_BLUE);
            }
            /* Pending-cell highlight (yellow) */
            if (r == s_pending_row && c == s_pending_col) {
                LCD_BlankArea(x, y, EDIT_CELL_SZ - 2, 2, C_YELLOW);
                LCD_BlankArea(x, y, 2, EDIT_CELL_SZ - 2, C_YELLOW);
            }
        }
    }
}

/* Reset the editor and start placing ships from scratch */
void Editor_Init(void) {
    memset(s_edit_grid, 0, sizeof(s_edit_grid));
    s_pending_row = -1;
    s_pending_col = -1;
    s_ships_placed = 0;
    s_touch_was_down = false;
    g_game.cursor_row = 0;
    g_game.cursor_col = 0;
    Editor_Draw();
}

/* Commit the finished edit grid into the live game state and start playing */
static void Editor_Commit(void) {
    memcpy((void *)g_game.hidden_map, s_edit_grid, sizeof(s_edit_grid));

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            g_game.display_grid[r][c] = '.';
        }
    }
    /* Initialize the game state for a new play session */
    g_game.shots_left = 24;
    g_game.hits = 0;
    g_game.ships_sunk = 0;
    g_game.cursor_row = 0;
    g_game.cursor_col = 0;
    g_game.shot_timer = 10;
    g_game.elapsed_seconds = 0;
    g_game.state = STATE_PLAY;
    /* Clear the LCD to prepare for the play screen */
    LCD_BlankArea(0, 0, 240, 320, C_WHITE);
    Display_RenderScreen();
    Display_LCD_RenderScreen();
}

static void Editor_HandleTap(int row, int col) {
    if (s_edit_grid[row][col]) return; /* already part of a placed ship */

    if (s_pending_row < 0) {
        /* First tap of a new ship: mark it pending */
        s_pending_row = (int8_t)row; // Set the pending row
        s_pending_col = (int8_t)col; // Set the pending column
        Editor_Draw();
        return;
    }

    if (row == s_pending_row && col == s_pending_col) {
        /* Re-tap the pending cell cancels the selection */
        s_pending_row = -1; // Clear the pending row
        s_pending_col = -1; // Clear the pending column
        Editor_Draw();
        return;
    }

    int row_diff = row - s_pending_row; // Difference in rows between the tapped cell and the pending cell
    int col_diff = col - s_pending_col; // Difference in columns between the tapped cell and the pending cell
    if (row_diff < 0) row_diff = -row_diff; // Absolute value of row difference
    if (col_diff < 0) col_diff = -col_diff; // Absolute value of column difference
    /* Check if the tapped cell is adjacent to the pending cell */
    bool adjacent = (row_diff + col_diff == 1);

    if (!adjacent) {
        /* Not adjacent to the pending cell: start a new pending selection here instead */
        s_pending_row = (int8_t)row;
        s_pending_col = (int8_t)col;
        Editor_Draw();
        return;
    }

    /* Candidate placement: try completing the 2-cell ship */
    uint8_t candidate[8][8];
    int pend_row = s_pending_row;
    int pend_col = s_pending_col;
    memcpy(candidate, s_edit_grid, sizeof(candidate)); 
    candidate[pend_row][pend_col] = 1; 
    candidate[row][col] = 1;
    /* Clear the pending selection after attempting placement */
    s_pending_row = -1;
    s_pending_col = -1;
    /* Attempt to place the ship if it passes the rules check */
    if (MapRules_CheckPairsAndSpacing(candidate)) {
        memcpy(s_edit_grid, candidate, sizeof(candidate));
        s_ships_placed++;
        Editor_Draw();
        if (s_ships_placed == 5) {
            Editor_Commit();
        }
    } else {
        /* Invalid placement (breaks pairing/no-touch rules): flash the rejected cell, then redraw */
        int x = EDIT_GRID_X + col * EDIT_CELL_SZ;
        int y = EDIT_GRID_Y + row * EDIT_CELL_SZ;
        LCD_BlankArea(x, y, EDIT_CELL_SZ - 2, EDIT_CELL_SZ - 2, C_RED);
        Editor_Delay(300000);
        Editor_Draw();
    }
}

/* Poll the touch panel once (call each Timer3 tick while waiting for a map
 * or while STATE_EDIT is active) and drive the on-board editor. */
void Editor_PollTouch(void) {
    uint16_t x = Get_TP_X();
    uint16_t y_raw = Get_TP_Y();
    bool touch_down = (x != (LCD_W - 1)) && (y_raw != (LCD_H - 1));

    if (touch_down && !s_touch_was_down) {
        /* Get_TP_Y() returns an axis inverted relative to the LCD's own
         * addressing (see Tutorial_8_M480_LCD_with_Touch/main.c, which
         * displays touch position as "LCD_H - y") - flip it back here
         * before using it as a screen/grid Y coordinate. */
        uint16_t y = LCD_H - y_raw;

        /* Rising edge: a new tap just occurred */
        if (g_game.state == STATE_LOAD) {
            g_game.state = STATE_EDIT;
            Editor_Init();
        } else if (g_game.state == STATE_EDIT) {
            if (x >= EDIT_GRID_X && y >= EDIT_GRID_Y) {
                int col = (x - EDIT_GRID_X) / EDIT_CELL_SZ;
                int row = (y - EDIT_GRID_Y) / EDIT_CELL_SZ;
                if (row >= 0 && row < 8 && col >= 0 && col < 8) {
                    Editor_HandleTap(row, col);
                }
            }
        }
    }

    s_touch_was_down = touch_down; // Update the previous touch state
}

/* Redraw the editor screen */
void Editor_Redraw(void) {
    Editor_Draw();
}

/* Select the cell currently under the joystick cursor - same effect as a tap */
void Editor_SelectCursor(void) {
    Editor_HandleTap(g_game.cursor_row, g_game.cursor_col);
}
