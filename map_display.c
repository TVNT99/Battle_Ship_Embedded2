#include "NuMicro.h"
#include "game_shared.h"
#include <stdio.h>
#include <string.h>

/* Convert map buffer (symbols) to grid (0/1), and validate structure */
bool Map_ValidateAndLoad(const char *buffer) {
    uint8_t temp_grid[8][8];
    int ship_count = 0;

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

    /* Validate each ship cell has exactly one orthogonal neighbor */
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (temp_grid[r][c]) {
                int neighbor_count = 0;
                /* Check up, down, left, right */
                if (r > 0 && temp_grid[r-1][c]) neighbor_count++; // Check cell above
                if (r < 7 && temp_grid[r+1][c]) neighbor_count++; // Check cell below
                if (c > 0 && temp_grid[r][c-1]) neighbor_count++; // Check cell to the left
                if (c < 7 && temp_grid[r][c+1]) neighbor_count++; // Check cell to the right

                /* Each ship cell must have exactly one neighbor (its partner) */
                if (neighbor_count != 1) {
                    return false;
                }
            }
        }
    }

    /* Validate no diagonal touching between ships */
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (temp_grid[r][c]) {
                /* Check all 8 diagonal neighbors */
                if (r > 0 && c > 0 && temp_grid[r-1][c-1]) return false;   // NW diagonal neighbor
                if (r > 0 && c < 7 && temp_grid[r-1][c+1]) return false;   // NE diagonal neighbor
                if (r < 7 && c > 0 && temp_grid[r+1][c-1]) return false;   // SW diagonal neighbor
                if (r < 7 && c < 7 && temp_grid[r+1][c+1]) return false;   // SE diagonal neighbor
            }
        }
    }

    /* Map is valid - store in global game context */
    memcpy((void *)g_game.hidden_map, (void *)temp_grid, sizeof(temp_grid));

    /* Initialize display grid with all unfired cells */
    for (int r = 0; r < 8; r++) {
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

    /* Clear screen and move cursor to home */
    UART0_SendString("\033[2J\033[H");

    /* Display header */
    UART0_SendString("  BATTLESHIP  -  M487\r\n");
    UART0_SendString("  Shots left: ");
    sprintf(time_buf, "%2d", g_game.shots_left);
    UART0_SendString(time_buf);

    UART0_SendString("    Hits: ");
    sprintf(time_buf, "%2d", g_game.hits);
    UART0_SendString(time_buf);

    UART0_SendString("    Ships sunk: ");
    sprintf(time_buf, "%d", g_game.ships_sunk);
    UART0_SendString(time_buf);
    UART0_SendString(" / 5\r\n");

    /* Display time and shot timer */
    FormatTime(g_game.elapsed_seconds, time_buf);
    UART0_SendString("  Time: ");
    UART0_SendString(time_buf);

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
void Display_ShowEndGame(bool won) {
    char time_buf[16];
    uint16_t shots_used;
    uint16_t score;

    /* Clear screen */
    UART0_SendString("\033[2J\033[H");

    if (won) {
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
        if (score < 0) score = 0;  /* Minimum score is 0 */

        UART0_SendString("\r\n  Score: ");
        sprintf(time_buf, "%d", score);
        UART0_SendString(time_buf);
        UART0_SendString("\r\n");
    } else {
        UART0_SendString("  ========================================\r\n");
        UART0_SendString("  GAME OVER - OUT OF SHOTS\r\n");
        UART0_SendString("  ========================================\r\n\r\n");

        /* Reveal the hidden map on loss */
        UART0_SendString("  HIDDEN MAP REVEALED:\r\n");
        UART0_SendString("      0 1 2 3 4 5 6 7\r\n");

        for (int r = 0; r < 8; r++) {
            sprintf(time_buf, "   %d  ", r);
            UART0_SendString(time_buf);

            for (int c = 0; c < 8; c++) {
                char symbol = g_game.hidden_map[r][c] ? 'X' : '-';
                UART0_SendChar(symbol);
                UART0_SendChar(' ');
            }
            UART0_SendString("\r\n");
        }

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
