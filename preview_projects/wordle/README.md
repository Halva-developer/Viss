# Terminal Wordle (Viss v0.2.1 "Hambo")

A sleek, color-coded terminal word guessing game implemented purely in **Viss**.

![Terminal Wordle](https://img.shields.io/badge/Language-Viss%200.2.1-blue)
![Interface](https://img.shields.io/badge/UI-ANSI%20Terminal-brightgreen)

---

## Features
- **50 Curated 5-Letter English Words**: Randomly chosen every round using `sys.random()`.
- **Accurate Wordle Feedback Engine**:
  - `Green`: Correct letter in the exact correct position.
  - `Yellow`: Correct letter, but wrong position (handles duplicate letters correctly).
  - `Gray`: Letter is not in the secret word.
- **Visual Board**: 6x5 grid of styled ANSI colored tiles.
- **Input Validation**: Rejects words that are not 5 letters long or contain non-alphabetic characters without penalizing attempts.
- **Session Stats**: Tracks Games Played, Wins, Current Win Streak, and Best Streak.
- **Replayability**: Prompt to start a new round after each game.

---

## How to Play
1. Guess the 5-letter word in **6 attempts**.
2. Type your 5-letter guess and press <kbd>Enter</kbd>.
3. Use the colored tile feedback to deduce the hidden word.
4. Type `Q` or `quit` anytime to reveal the word and surrender.

---

## How to Run

### Run directly with the Viss runner:
```bash
viss run preview_projects/wordle/wordle.viss
```

### Or compile into a standalone binary:
```bash
viss build preview_projects/wordle/wordle.viss -o wordle.exe
./wordle.exe
```
