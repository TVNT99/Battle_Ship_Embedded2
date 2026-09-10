# Battleship — NuMaker-PFM-M487

An embedded C implementation of Battleship for the Nuvoton NuMaker-PFM-M487
(M487JIDAE, Cortex-M4) board. The game runs entirely on the MCU: a 240x320
ILI9341 LCD (driven over EBI) shows the live grid, a joystick + button fires
shots and moves the cursor, and a UART link (or the on-board touch/joystick
editor) is used to load the ship layout.


## Hardware

| Function            | Peripheral / Pins |
|----------------------|--------------------|
| LCD (ILI9341, 240x320) | EBI bank0, 16-bit bus (`EBI_LCD_Module.c`) |
| Resistive touch panel  | EADC0 (X/Y channels), used by the on-board map editor |
| Joystick UP / RIGHT / FIRE | PG.2 / PG.4 / PG.3 (GPIO IRQ, debounced) |
| Joystick LEFT / DOWN       | PC.9 / PC.10 (GPIO IRQ, debounced) |
| SW2 (start / restart)      | PG.15 (GPIO IRQ, debounced) |
| Hit-feedback LEDs (R/Y/G)  | PH.0 / PH.1 / PH.2, active-low |
| UART0 (map upload, game log) | PB.12 (RX) / PB.13 (TX), 57600-8-N-1 |
| Timer0                 | 1 Hz tick — per-shot countdown & elapsed match time |
| Timer3                 | 100 ms tick — LCD/touch-panel polling |

## Building & flashing

Open `Task1_BattleShip.uvproj` in Keil uVision (ARMCLANG v6.22 toolchain,
target `M487JIDAE`) and build/download as usual via the Nu-Link debugger.
Only one uVision session can hold the Nu-Link USB debugger at a time, so
close any other open debug session first.

## Game flow

```
STATE_WELCOME --(SW2)--> STATE_LOAD --(UART map / joystick tap)--> STATE_EDIT
STATE_LOAD --(valid map over UART)-----------------------------> STATE_PLAY
STATE_EDIT --(5 ships placed)------------------------------------> STATE_PLAY
STATE_PLAY --(all 5 ships sunk)-----------------------------------> STATE_WIN
STATE_PLAY --(shots exhausted or 240s elapsed)--------------------> STATE_LOSE
STATE_WIN / STATE_LOSE --(SW2)-----------------------------------> STATE_LOAD
```

- **WELCOME** — idle screen, press SW2 to begin.
- **LOAD** — waiting for a map. Either send one over UART, or press any
  joystick direction to switch to the on-board **EDIT** editor.
- **EDIT** — place 5 ships by tapping the touchscreen (or moving the
  joystick cursor and pressing FIRE to select) two adjacent cells at a
  time. Invalid placements flash red and are rejected.
- **PLAY** — move the cursor with the joystick, FIRE to shoot. Each shot
  is scored as a hit, miss, or (if repeated) a wasted shot.
- **WIN / LOSE** — end-game summary (shots used, time, score) shown on
  both the LCD and UART terminal. Press SW2 to play again.

## Controls (STATE_PLAY / STATE_EDIT)

| Input | Action |
|---|---|
| Joystick UP / DOWN / LEFT / RIGHT | Move cursor |
| Joystick FIRE (center) | Fire at cursor (PLAY) / select cell (EDIT) |
| SW2 | Start / restart |

## Map format (UART upload)

Send exactly 64 symbols (whitespace is ignored) representing the 8x8
grid, row-major:

- `-` or `0` — empty cell
- `X` or `1` — ship cell

The map must contain exactly 10 ship cells (five 2-cell ships), and every
ship cell must have exactly one orthogonal neighbor (its partner) with no
ship touching another, including diagonally. Invalid maps are rejected
with `MAP INVALID` over UART and the buffer is reset so a corrected map
can be resent. `map.txt` contains a sample map for testing.

## Rules & scoring

- 24 total shots, a 10-second countdown per shot (an expired countdown
  costs a shot automatically), and a 240-second match time limit.
- Hitting both cells of a ship marks it as sunk (`#`) on the display grid.
- Win: all 5 ships sunk. Lose: out of shots or match time expires.
- Score = `500 − shots_used*10 − elapsed_seconds`, plus a 500-point win
  bonus.

## Project structure

| File | Responsibility |
|---|---|
| `main.c` | Init + top-level state machine loop |
| `game_shared.h` | Shared game state (`GameContext_t`), state enum, function decls |
| `uart_comm.c` | Clocks, UART0 driver, map-upload RX interrupt |
| `controls_logic.c` | GPIO/joystick input, hardware timers, LEDs, firing & win/lose rules |
| `map_display.c` | Map validation, UART terminal rendering, LCD rendering, on-board touch/joystick map editor |
| `EBI_LCD_Module.c/h` | Low-level ILI9341/EBI LCD driver + touch-panel ADC reads |
| `image_16x32.c` | Font/bitmap data used by the LCD driver |
| `Library/` | Nuvoton M480 StdDriver/CMSIS/device library |
| `Task1_BattleShip.uvproj` | Keil uVision project for this game |

