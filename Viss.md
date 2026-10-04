# The Viss Programming Language: Complete Handbook from A to Z

> **Language Version:** Viss v0.2.1 "Hambo" (Native)  
> **Target Architecture:** Native Machine Code via C++20 (Zero External Dependencies)  
> **Creator & Language Author:** Halva (Halva-developer)  

---

## 1. Introduction & Philosophy of Viss

**Viss** is an ultra-high-performance general-purpose and systems programming language. It combines the conciseness and ergonomics of modern scripting languages (such as Python and TypeScript) with the raw execution speed and low-level control of native C++20 machine code.

### Core Highlights:
- **Zero Dependencies:** Compiling via `viss bundle` produces a standalone, compact `.exe` binary (often under 1 MB) requiring no runtimes, .NET framework, Python interpreters, or external dynamic libraries (DLLs).
- **Symbolic Expressiveness:** Explicit semantic sigils and prefixes make code intent immediately recognizable at a glance:
  - `@`  -  Variable declaration, assignment, or struct field access.
  - `&`  -  Low-level binary memory buffer or bitmask.
  - `!`  -  Imperative / deterministic operation (entry point `!main`, functions `!func`, `!while`, `!defer`).
  - `?`  -  Conditional / safety-checked expression (`?if`, pattern matching `?match`).
  - `|`  -  Data pipeline operator (`|>`), collection type tagging, or closure parameters (`|item|`).
- **Batteries-Included Multimedia & Game Engine:** High-performance 2D/3D hardware-accelerated GUI (GDI+ with flicker-free double buffering), built-in analog sound synthesizer with low-pass filters, lossless and lossy audio playback (MP3, OGG, FLAC), and ID3/Vorbis tag metadata manipulation.

---

## 2. Symbolic Alphabet of Viss

| Sigil | Meaning | Code Example | Semantic Description |
| :---: | :--- | :--- | :--- |
| `@` | Variable / Field | `@speed = 120.0` | Declares or references mutable variables and struct fields. |
| `&` | Memory Buffer | `&buf create \| bytes, 1024` | Allocates and manipulates raw binary byte buffers and bit arrays. |
| `!` | Imperative Action | `!func shoot() { }` | Defines functions, entry point `!main`, loops `!while`, or `!defer`. |
| `?` | Conditional / Check | `?if @hp <= 0.0 { }` | Branching `?if`, pattern matching `?match`, and error handling. |
| `\|` | Type Tag / Closure | `@list = [] \| Bullet` | Type annotations for lists, closure parameters `\|n\| n > 0`. |
| `\|>` | Pipeline Operator | `@text \|> str.trim \|> str.upper` | Pipes output of left-hand expression into right-hand function. |
| `?:` | Elvis Operator | `@name = @input ?: "Guest"` | Fallback value if left-hand expression is empty, false, or null. |

---

## 3. Basic Data Types and Literals

### Primitive Types
- `int`  -  64-bit signed integer (`int64_t`).  
  *Examples:* `42`, `-100`, `0xFF`, `0b1010`.
- `dec` (aliases: `double`, `float`)  -  64-bit double-precision IEEE-754 floating-point number.  
  *Examples:* `3.14159`, `-0.5`, `100.0`.
- `str` (alias: `string`)  -  Dynamic UTF-8 string with SSO and built-in manipulation methods.  
  *Examples:* `"Hello, World!"`, `"Line 1\nLine 2"`.
- `bool`  -  Boolean truth value (`true`, `false`).

### String Interpolation: `i"..."`
Strings prefixed with `i` support arbitrary embedded expressions inside curly braces `{...}`:
```viss
@name = "Halva"
@lvl = 42
io.println(i"Player: {@name}, Level: {@lvl}, Next Level: {@lvl + 1}")
```

### 2D and 3D Vectors (`vec2`, `vec3`)
Viss features first-class vector math types with operator overloading (`+`, `-`, `*`, `/`, `+=`, `-=`, `*=`):
```viss
@pos = vec2(100.0, 200.0)
@vel = vec2(5.0, -2.5)
@pos += @vel * 2.0

@distance = @pos.dist(vec2(0.0, 0.0))
@direction = @vel.normalize()
@length = @vel.len() // or @vel.size()

@pos3d = vec3(0.0, 10.0, -50.0)
@dir3d = @pos3d.normalize()
```

---

## 4. Type Casting

Viss provides multiple explicit casting styles:

1. **Functional-Style Casting:**
   ```viss
   @count = 10
   @ratio = dec(@count) / 4.0        // dec(x) -> 2.5
   @rounded = int(@ratio)             // int(x) -> 2
   @text = str(@count)                // str(x) -> "10"
   @flag = bool(1)                    // bool(x) -> true
   ```
2. **`as` Operator:**
   ```viss
   @val = @xp as dec
   @num = @text as int
   ```
3. **C-Style Casting:**
   ```viss
   @ratio = (dec)@count / 4.0
   ```

---

## 5. Collections

### Lists (`List`)
Lists in Viss are dynamically resizable and memory-safe:
```viss
// Untyped / Generic List
@items = [10, 20, 30, 40]

// Explicitly typed List of structs
@bullets = [] | Bullet

// Insertion and Removal
@items.add(50)
@last = @items.pop()

// Indexing and Length
@first = @items[0]
@count = @items.len()      // or @items.size()
```

#### Functional List Methods:
- `.retain(|item| condition)`  -  In-place filtering: keeps **only** elements satisfying the predicate (ideal for removing inactive entities, bullets, or expired particles):
  ```viss
  @bullets.retain(|b| b.active && b.life > 0)
  ```
- `.remove_if(|item| condition)`  -  In-place removal: removes elements matching the condition:
  ```viss
  @enemies.remove_if(|e| e.hp <= 0.0)
  ```
- `.filter(|item| condition)`  -  Returns a **new** list containing only matching elements.
- `.map(|item| transform)`  -  Returns a **new** list with elements transformed by the closure.
- `.find_first(|item| condition, default_val)`  -  Finds the first element meeting the condition or returns `default_val`.

---

## 6. Control Flow

### Branching: `?if` / `else if` / `else`
```viss
?if @hp > 75.0 {
    io.println("Status: Optimal")
} ?else if @hp > 25.0 {
    io.println("Status: Warning - Damaged")
} else {
    io.println("Status: Critical Hull Failure!")
}
```

### Loops: `for` and `while`
```viss
// Numeric range from 0 to 9 inclusive:
for @i in 0..10 {
    io.println(i"Iteration: {@i}")
}

// Collection iteration:
for @item in @items {
    io.println(i"Item: {@item}")
}

// While loop:
!while ui.is_open() {
    ui.poll()
    // Game frame logic
}
```

### Pattern Matching: `?match` (or `match`, `switch`)
```viss
?match @weapon_type {
    case WeaponType.Blaster {
        io.println("Firing standard plasma blaster!")
    }
    case WeaponType.Laser {
        io.println("Continuous beam engaged!")
    }
    default {
        io.println("No active weapon selected.")
    }
}
```

### Deterministic Scope Cleanup: `!defer` (or `defer`)
The `!defer` statement guarantees that the given expression executes upon exiting the current function or block (ideal for releasing resources, closing windows, or stopping audio tracks):
```viss
!func run_session() {
    audio.play_bgm("cyber.mp3", 0.2)
    !defer audio.stop_bgm() // Automatically called on function return!
    
    // ... game session execution ...
}
```

---

## 7. Advanced Operators: Pipeline (`|>`) and Elvis (`?:`)

### Pipeline Operator (`|>`)
Forwards the evaluated result on the left as the first argument to the function call on the right. Perfect for readable data transformation flows:
```viss
// Equivalent to: str.upper(str.trim(@input))
@clean = @input |> str.trim |> str.upper

// With additional parameters:
@clamped = @val |> math.clamp(0.0, 100.0) // Becomes: math.clamp(@val, 0.0, 100.0)
```

### Elvis Operator (`?:`)
Evaluates to the right-hand fallback expression if the left-hand value is empty (for strings) or false/null/zero:
```viss
@display_name = @user_input ?: "Guest Player"
@timeout = @cfg_timeout ?: 30
```

---

## 8. Structs (`struct`) and Member Methods

In Viss, structs are first-class data models with default field values, auto-generated constructors, and member methods:

```viss
struct Player {
    // Typed fields with default initialization
    name: str = "Unnamed"
    hp: dec = 100.0
    max_hp: dec = 100.0
    pos: Vec2 = vec2(0.0, 0.0)

    // Member methods
    !func take_damage(@amt: dec) {
        @hp = @hp - @amt
        ?if @hp < 0.0 {
            @hp = 0.0
        }
    }

    !func heal(@amt: dec) {
        @hp = @hp + @amt
        ?if @hp > @max_hp {
            @hp = @max_hp
        }
    }

    !func is_alive() as bool {
        return @hp > 0.0
    }
}
```

### Using Structs:
```viss
// Instantiation via constructor:
@hero = Player("Cyber Ranger", 100.0, 100.0, vec2(10.0, 50.0))

// Calling methods:
@hero.take_damage(35.0)
io.println(i"Hero HP: {@hero.hp}, Alive: {@hero.is_alive()}")
```

---

## 9. Enumerations (`enum`)

`enum` declarations provide strongly typed, named integer constants fully compatible with `switch` / `?match`:
```viss
enum GameState {
    Menu = 0,
    Playing = 1,
    Paused = 2,
    GameOver = 3
}

@state = GameState.Playing
?if @state == GameState.Playing {
    io.println("Game is running!")
}
```

---

## 10. Complete Standard Library Reference

### `io`  -  Console Input & Output
- `io.println(val)`  -  Prints value followed by a newline.
- `io.print(val)`  -  Prints value without a newline.
- `io.read_line()`  -  Reads a string from standard input.
- `io.error(val)`  -  Writes message to standard error stream (`stderr`).

### `math`  -  Mathematics & Geometry
- `math.sin(x)`, `math.cos(x)`, `math.tan(x)`  -  Trigonometric functions.
- `math.sqrt(x)`, `math.pow(x, y)`, `math.abs(x)`  -  Power and root functions.
- `math.min(a, b)`, `math.max(a, b)`, `math.clamp(val, min, max)`  -  Clamping and bounds.
- `math.random_dec()`  -  Generates a pseudo-random decimal in range `[0.0, 1.0]`.
- `math.random_int(min, max)`  -  Generates a random integer in range `[min, max]`.
- `math.deg_to_rad(deg)`, `math.rad_to_deg(rad)`, `math.pi`  -  Angle conversion and constants.

### `str`  -  String Manipulation
- `str.len(s)`  -  Returns the character length of the string.
- `str.trim(s)`  -  Trims leading and trailing whitespace.
- `str.lower(s)`, `str.upper(s)`  -  Converts case.
- `str.contains(s, sub)`  -  Checks whether `sub` exists within `s`.
- `str.split(s, sep)`  -  Splits string into `List<str>`.
- `str.join(list, sep)`  -  Joins a list of strings by delimiter.
- `str.replace(s, from, to)`  -  Replaces all occurrences of `from` with `to`.

### `fs`  -  File System
- `fs.exists(path)`  -  Checks whether a file or directory exists.
- `fs.read(path)`  -  Reads the entire file content into a string.
- `fs.write(path, content)`  -  Writes string content to a file (overwrites).
- `fs.append(path, content)`  -  Appends string content to the end of a file.
- `fs.remove(path)`  -  Deletes a file.
- `fs.list_dir(path)`  -  Returns a list of filenames in the specified directory.

### `time`  -  High-Resolution Clock & Timers
- `time.now_ms()`  -  Current timestamp in milliseconds (`int64_t`).
- `time.sleep_ms(ms)`  -  Suspends execution for the specified milliseconds.
- `time.sleep(sec)`  -  Suspends execution for the specified seconds.
- `time.format("%Y-%m-%d %H:%M:%S")`  -  Formats current time using strftime patterns.

### `audio`  -  Sound Effects & Music Player
- `audio.play_bgm(path, volume)`  -  Loops background music (MP3, OGG, FLAC) with volume from `0.0` to `1.0`.
- `audio.stop_bgm()`  -  Immediately stops background music playback.
- `audio.set_bgm_volume(volume)`  -  Dynamically adjusts volume in real time.
- `audio.laser(freq, dur)`  -  Generates an analog synthesized laser sound.
- `audio.hit()`  -  Synthesizes an impact sound effect.
- `audio.gem()`  -  Plays a crystal / loot pickup tone.
- `audio.powerup()`  -  Synthesizes a rising level-up / powerup arpeggio.
- `audio.explosion()`  -  Synthesizes a deep, resonant rumble explosion.

### `ui` (or `gui`)  -  2D and 3D Graphical Engine
- `ui.create(title, width, height)`  -  Spawns a native double-buffered desktop window.
- `ui.is_open()` / `ui.poll()`  -  Checks window state and dispatches OS window events.
- `ui.clear(color)`  -  Clears the backbuffer with specified color.
- `ui.update()`  -  Flips the backbuffer onto the screen (zero tearing, 60+ FPS).
- `ui.close()`  -  Closes the window and frees GDI+ resources.
- **Input Handling:**
  - `ui.key_down(vk_code)`  -  Returns true if key is held down (WASD: 65, 87, 83, 68; Arrows: 37, 38, 39, 40; Space: 32).
  - `ui.key_pressed(vk_code)`  -  Returns true if key was pressed in the current frame.
  - `ui.mouse_x()`, `ui.mouse_y()`  -  Current cursor coordinates.
  - `ui.mouse_down()`, `ui.mouse_clicked()`  -  Mouse button status.
- **Drawing Primitives:**
  - `ui.draw_rect(x, y, w, h, color, fill)`
  - `ui.draw_round_rect(x, y, w, h, radius, color, fill, border_color, border_w)`
  - `ui.draw_circle(x, y, radius, color, fill)`
  - `ui.draw_triangle(x1, y1, x2, y2, x3, y3, color, fill, border_color, border_w)`  -  **3D Polygon**
  - `ui.draw_quad(x1, y1, x2, y2, x3, y3, x4, y4, color, fill, border_color, border_w)`  -  **3D Quad**
  - `ui.draw_line(x1, y1, x2, y2, color, width)`
  - `ui.draw_text(x, y, text, color, size, font_name, bold)`
- **Color Construction (`ui.Color`):**
  - `ui.Color.rgb(r, g, b)`
  - `ui.Color.rgba(r, g, b, a)`
  - `ui.Color.hex(0xRRGGBB)`

---

## 11. The `viss` CLI: Compilation and Bundling

The native `viss.exe` compiler binary provides three straightforward commands:

```bash
# 1. Instant transpile, compile, and run:
viss run game.viss

# 2. Bundle into a standalone, portable .exe binary:
viss bundle game.viss -o MyGame.exe

# 3. Interactive Read-Eval-Print Loop (REPL):
viss repl
```

---

## 12. Complete Real-World Example: 3D Polygonal Starfield

```viss
$import lib "iostream" as io
$import lib "gui" as ui
$import lib "time" as time
$import lib "math" as math

struct Star3D {
    pos: Vec3 = vec3(0.0, 0.0, 0.0)
    color: int = 16777215
}

!main {
    @win_w = 800
    @win_h = 600
    ui.create("Viss 3D Starfield", @win_w, @win_h)
    !defer ui.close()

    @stars = [] | Star3D
    for @i in 0..100 {
        @sx = (math.random_dec() * 800.0) - 400.0
        @sy = (math.random_dec() * 600.0) - 300.0
        @sz = 100.0 + (math.random_dec() * 500.0)
        @stars.add(Star3D(vec3(@sx, @sy, @sz)))
    }

    !while ui.is_open() {
        ui.poll()
        ui.clear(ui.Color.rgb(10, 10, 20))

        for @s in @stars {
            // Forward movement along Z axis
            @s.pos.z -= 4.0
            ?if @s.pos.z <= 10.0 {
                @s.pos.z = 500.0
            }

            // 3D Perspective Projection
            @scale = 300.0 / @s.pos.z
            @proj_x = (@win_w / 2) + (@s.pos.x * @scale)
            @proj_y = (@win_h / 2) + (@s.pos.y * @scale)
            @rad = 2.0 * @scale

            ui.draw_circle(@proj_x, @proj_y, @rad, ui.Color.rgb(0, 240, 255), true)
        }

        ui.draw_text(20, 20, "Viss Native 3D Engine", ui.Color.rgb(255, 255, 255), 16, "Segoe UI", true)
        ui.update()
        time.sleep_ms(16) // ~60 FPS
    }
}
```
