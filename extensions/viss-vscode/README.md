# Viss Language Support for Visual Studio Code

The official Visual Studio Code extension for the **Viss** programming language (v0.2.1+ / v0.3.x).

Provides first-class editing experience, full syntax highlighting, intelligent context-aware autocompletions, real-time compiler diagnostics, code snippets, and integrated one-click execution.

---

## ✨ Features

### 1. 🎨 Comprehensive Syntax Highlighting
- **Sigils & Primitives**: Full recognition of variable sigils (`@name`, `@me`), hardware buffer sigils (`&buf`), type pipes (`| type`), statement action prefixes (`!func`, `!main`, `!class`, `!for`, `!while`, `!return`), logic conditions (`?if`, `?elif`, `?else`, `?match`, `?try`, `?expect`), and preprocessor directives (`$import`, `$include`).
- **All String Literals**:
  - Interpolated strings: `i"Value is: {@var} and {1 + 2}"` with embedded expressions and variable highlights.
  - Multiline triple-quoted strings: `"""..."""`.
  - Raw strings: `r"C:\Windows\System32"`.
  - Byte strings: `b"POST /api HTTP/1.1\r\n"`.
- **Modern Operators**: Membership operators (`in`, `!in`, `not in`), arrow return types (`->`), ranges (`0..10`), bitwise, arithmetic, and logical operators.
- **Builtin Modules & Namespaces**: `rt`, `io`, `str`, `sys`, `fs`, `json`, `time`, `math`, `async`, `crypto`, `net`, `env`.

### 2. 🧠 Smart Contextual Autocompletion
Unlike standard naive extensions that flood the completion popup with hundreds of unrelated methods on every dot, Viss VS Code provides **strict context-aware scoping**:
- **Module Dispatch**: Typing `rt.` only suggests retrotech graphics/sound functions (`InitScreen`, `SetPixel`, `DrawRect`, `PlaySfx`, etc.). Typing `io.` only suggests I/O functions (`println`, `read_line`, `get_key`, etc.).
- **Hardware Buffers**: Typing `&buf.` strictly shows buffer operations (`read_u8`, `write_u8`, `crc32`, `grid_edit`, etc.).
- **Collections & Objects**: Typing `@var.` strictly shows valid collection methods (`size`, `len`, `append`, `pop`, `insert`, `remove`, `slice`, `sort`, `reverse`, `join`, `get`, `has`, `keys`, `values`).
- **Sigil Triggers**:
  - `@` triggers suggestions of all variables declared in the current file plus `@me`.
  - `&` triggers suggestions of declared hardware buffers.
  - `| ` triggers type hints (`int`, `str`, `float`, `bool`, `list<T>`, `map<K, V>`, `const`, etc.).
  - `!` triggers control keywords (`!func`, `!async func`, `!main`, `!for`, `!while`, `!class`, `!return`).
  - `?` triggers branching constructs (`?if`, `?elif`, `?else`, `?match`, `?try`, `?expect`).
  - `$` triggers compiler directives (`$import`, `$include`).

### 3. 🔍 Real-Time Compiler Diagnostics & Linting
- Automatically runs `viss check` on file save (or while typing with debounce).
- Parses exact compiler error messages (`[Viss Syntax Error]`, `[Viss TypeError]`, `[Viss NameError]`, `[Viss CompilationError]`).
- Highlights exact lines and columns in the editor and populates the **Problems** tab.

### 4. ⚡ One-Click Runner & Build Actions
- **Editor Title Bar Button**: Click the `$(play)` button in the top-right corner to run the active Viss file immediately in the integrated terminal.
- **Status Bar**: Live status indicator showing `$(play) Run Viss` and active compiler version.
- **Context Menus**: Right-click anywhere in a `.viss` file to:
  - Run Viss Program
  - Check Syntax & Diagnostics
  - Build Executable (`viss build`)

---

## ⌨️ Keybindings

| Keybinding | Action | Description |
|---|---|---|
| <kbd>F5</kbd> | `viss.run` | Run active `.viss` script in integrated terminal |
| <kbd>Ctrl</kbd> + <kbd>F5</kbd> | `viss.check` | Check syntax and report compiler diagnostics |
| <kbd>Shift</kbd> + <kbd>F5</kbd> | `viss.compile` | Build standalone native `.exe` binary |

---

## 🧩 Code Snippets

Quickly scaffold Viss constructs by typing prefixes and pressing <kbd>Tab</kbd>:

| Prefix | Construct | Generated Template |
|---|---|---|
| `func` | Function | `!func name(params) { ... }` |
| `asyncfunc` | Async Function | `!async func name(params) { ... }` |
| `main` | Main Entry | `!func main() { ... }` |
| `if` | If Condition | `?if (cond) { ... }` |
| `elif` | Elif Condition | `?elif (cond) { ... }` |
| `match` | Match Pattern | `?match @val { case p { ... } else { ... } }` |
| `try` | Try / Expect | `?try { ... } ?expect Exception as @e { ... }` |
| `for` | Range Loop | `!for @i in 0..10 { ... }` |
| `forin` | Collection Loop | `!for item in @list { ... }` |
| `while` | While Loop | `!while (cond) { ... }` |
| `gameloop` | 60 FPS Game Loop | `rt.InitScreen` + update/render loop with `time.sleep` |
| `sfx` | NES Sound Effect | `rt.PlaySfx("jump")` |
| `bytes` | Raw Byte Buffer | `&buf create \| bytes, 1024;` |
| `bits` | Raw Bitfield Buffer | `&flags create \| bits, 8192;` |
| `list` | List Literal | `@items = [1, 2, 3];` |
| `map` | Map Literal | `@dict = {"key": value};` |

---

## ⚙️ Extension Settings

| Setting | Default | Description |
|---|---|---|
| `viss.executablePath` | `"viss"` | Path to the `viss` executable (defaults to workspace root, standard install, or system `PATH`). |
| `viss.checkOnSave` | `true` | Automatically run syntax & diagnostic checks when saving a `.viss` file. |

---

## 📦 Manual Installation

1. Copy the `viss-vscode` directory to your VS Code extensions folder:
   - **Windows**: `%USERPROFILE%\.vscode\extensions\viss-vscode`
   - **Linux / macOS**: `~/.vscode/extensions/viss-vscode`
2. Restart or reload VS Code (`Ctrl+Shift+P` -> `Developer: Reload Window`).
3. Open any `.viss` file and start coding!
