# Viss Showcase Games (v0.2.1 "Hambo")

A collection of complete, fully functional, standalone games written entirely in **Viss** to demonstrate the language's capabilities, syntax expressiveness, and runtime engine.

---

## The Games

### 1. [Retro Tetris](tetris/)
A full arcade-accurate Tetris clone powered by the built-in `retrotech` (`rt`) engine.
- **Path**: [`preview_projects/tetris/tetris.viss`](tetris/tetris.viss)
- **Features**: All 7 standard tetrominoes, 4-way rotation matrices, 10x20 well, line clear scoring, sound effects, level progression, and next piece preview.
- **Run**:
  ```bash
  viss run preview_projects/tetris/tetris.viss
  ```

---

### 2. [Terminal Wordle](wordle/)
A sleek, color-coded word guessing game played entirely in the terminal.
- **Path**: [`preview_projects/wordle/wordle.viss`](wordle/wordle.viss)
- **Features**: 50 curated English 5-letter words, exact Wordle color rules (Green / Yellow / Gray tiles), input validation, session statistics (played, won, streak, best streak), and multi-round replayability.
- **Run**:
  ```bash
  viss run preview_projects/wordle/wordle.viss
  ```

---

### 3. [Retro Snake](snake/)
The classic arcade Snake game with retro 8-bit aesthetic and audio.
- **Path**: [`preview_projects/snake/snake.viss`](snake/snake.viss)
- **Features**: 20x20 walled arena, smooth grid movement, food spawning, tail growth, speed scaling per apples eaten, sound effects, HUD sidebar, and game over restart screen.
- **Run**:
  ```bash
  viss run preview_projects/snake/snake.viss
  ```

---

## Building Executables
Each project can be compiled into a standalone native binary without needing Python or external runtimes:

```bash
viss build preview_projects/tetris/tetris.viss -o tetris.exe
viss build preview_projects/wordle/wordle.viss -o wordle.exe
viss build preview_projects/snake/snake.viss   -o snake.exe
```
