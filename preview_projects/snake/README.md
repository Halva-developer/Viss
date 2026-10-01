# Retro Snake (Viss v0.2.1 "Hambo")

A classic, polished arcade Snake game implemented purely in **Viss** using the `retrotech` (`rt`) library.

![Retro Snake](https://img.shields.io/badge/Language-Viss%200.2.1-blue)
![Engine](https://img.shields.io/badge/Engine-retrotech-green)

---

## Features
- **Dynamic 2D Arena**: 20x20 walled playfield surrounded by retro borders.
- **Classic Snake Mechanics**:
  - Fluid grid movement with 180-degree turn prevention.
  - Tail following and length growth.
  - Flashing food / apple spawning on random empty cells.
- **Audio Effects**:
  - `rt.sound_coin()` on game start.
  - `rt.sound_bump()` on sharp turns.
  - `rt.sound_powerup()` when munching apples.
  - `rt.sound_game_over()` upon crash.
- **Level & Speed Scaling**: Snake speed accelerates as your apple count rises!
- **Sidebar HUD**: Real-time display of Score, High Score, Apples Eaten, Level, and Controls.
- **Game Over & Restart**: Instant restart (<kbd>R</kbd>) or quit (<kbd>Q</kbd>).

---

## Controls
| Key | Action |
|:---|:---|
| <kbd>Up</kbd> / <kbd>W</kbd> | Slither Up |
| <kbd>Down</kbd> / <kbd>S</kbd> | Slither Down |
| <kbd>Left</kbd> / <kbd>A</kbd> | Slither Left |
| <kbd>Right</kbd> / <kbd>D</kbd> | Slither Right |
| <kbd>R</kbd> | Restart Game (when Game Over) |
| <kbd>Q</kbd> / <kbd>Esc</kbd> | Quit Game |

---

## How to Run

### Run directly with the Viss runner:
```bash
viss run preview_projects/snake/snake.viss
```

### Or compile into a standalone binary:
```bash
viss build preview_projects/snake/snake.viss -o snake.exe
./snake.exe
```
