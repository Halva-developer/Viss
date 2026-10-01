# Retro Tetris (Viss v0.2.1 "Hambo")

A complete, arcade-accurate Tetris clone implemented purely in **Viss** using the `retrotech` (`rt`) library.

![Retro Tetris](https://img.shields.io/badge/Language-Viss%200.2.1-blue)
![Engine](https://img.shields.io/badge/Engine-retrotech-green)

---

## Features
- **All 7 Standard Tetrominoes**: I, O, T, S, Z, J, L with accurate color palettes and 4-way rotational matrices.
- **Classic Playfield**: Standard 10x20 well with collision detection and wall-kicks.
- **Audio Feedback**: Built-in 8-bit sound effects using `rt.sound_*()` for piece movement, rotations, hard drops, line clears, and game over.
- **Scoring & Level Progression**: Standard scoring system (100, 300, 500, 800 per lines cleared) with speed increases every 10 lines.
- **Side Panel**: Displays real-time Score, High Score, Level, Lines cleared, and Next Piece preview.

---

## Controls
| Key | Action |
|:---|:---|
| <kbd>Left</kbd> / <kbd>A</kbd> | Move Left |
| <kbd>Right</kbd> / <kbd>D</kbd> | Move Right |
| <kbd>Up</kbd> / <kbd>W</kbd> | Rotate Clockwise |
| <kbd>Down</kbd> / <kbd>S</kbd> | Soft Drop (+1 pt) |
| <kbd>Space</kbd> | Hard Drop (+2 pts/cell) |
| <kbd>R</kbd> | Restart Game (when Game Over) |
| <kbd>Q</kbd> / <kbd>Esc</kbd> | Quit Game |

---

## How to Run

### Run directly with the Viss interpreter / JIT runner:
```bash
viss run preview_projects/tetris/tetris.viss
```

### Or compile into a standalone binary:
```bash
viss build preview_projects/tetris/tetris.viss -o tetris.exe
./tetris.exe
```
