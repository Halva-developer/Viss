<div align="center">

<img src="viss_logo.jpg" alt="Viss Logo" width="220" />

# Viss Programming Language
### The High-Performance, Visually Structured Language for Everyday Development & Systems

[![Version](https://img.shields.io/badge/version-v0.2.0%20%22Marceline%22-blue.svg)](https://github.com/Halva-developer/Viss)
[![License](https://img.shields.io/badge/license-UniLicense-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-orange.svg)](https://github.com/Halva-developer/Viss)
[![Standard](https://img.shields.io/badge/standard-Viss%202.0-purple.svg)](https://github.com/Halva-developer/Viss)

</div>

---

## What is Viss?

**Viss** is a modern general-purpose programming language designed to unite the **extreme speed and standalone deployment of C++** with the **ergonomics, joy, and rapid development of Python**.

Unlike traditional C-family languages, Viss introduces an iconic **Prefix Design System** (`!`, `?`, `@`, `$`) combined with strict architectural structure and clean pipeline typings (`| type`). In Viss, code is readable at a glance: actions, logical conditions, identifiers, and compiler directives are immediately distinguishable without mental fatigue.

### Core Philosophy
* **Pure Native Speed:** Compiles directly into zero-overhead native machine code (C++20 backend). No heavy interpreter, no virtual machine warmup, no GIL.
* **Single Binary Deployment:** Produces lightweight standalone executables (1-2 MB). No runtime installations or environment configurations required for end users.
* **Zero Boilerplate, Strict Structure:** Eliminates unstructured script chaos by enforcing clear entry points (`!func main()`) and structured entity definitions.
* **Batteries Included via C/C++ Ecosystem:** Out-of-the-box support for modern graphics, 2D/3D audio, networking, system shell scripting, and instant interop with millions of existing C/C++ libraries.

---

## Release Codenames (The Adventure Time Scheme)

Following the beloved Debian tradition with *Toy Story*, Viss codenames each official milestone after characters from the Land of Ooo (*Adventure Time*):

| Version | Codename | Character | Milestone Highlights |
| :--- | :--- | :--- | :--- |
| `v0.0.1` | **"Finn"** | Finn the Human | The initial prototype, prefix syntax discovery, and native proof-of-concept. |
| `v0.1.0` | **"Jake"** | Jake the Dog | Modular runtime refactor, standard libraries (`io`, `math`, `fs`, `sys`). |
| `v0.1.2` | **"BMO"** | BMO | Retro-tech audio synthesizer, pixel screen buffer, and sound chip. |
| `v0.2.0` | **"Marceline"** | Marceline the Vampire Queen | Struct member methods, enums, `Vec2`/`Vec3` vector math, pipelines (`\|>`), Elvis (`?:`), `!defer`, and complete handbook specs. |
| `v0.2.1` | **"Hambo"** | Hambo | **Current Release:** Dynamic auto-lists, Map/Dict literals (`{}`), range slicing (`..`), `in`/`!in` membership operators, tuple unpacking, native `!async`/`!await`, string kinds (`r""`, `b""`, `""""""`), `fs.glob`, `json.loads`/`dumps`, pure native C++ runtime. |

> [NOTE] **Unstable Development Branch:** Named **"Lemongrab"** (*"UNACCEPTABLE!"*) for cutting-edge nightly builds.

---

## The Golden Prefix Architecture

Every token in Viss clearly defines its semantic intent via prefixes:

| Prefix | Semantic Role | Examples |
| :---: | :--- | :--- |
| **`!`** | **Flow & Action** | `!func`, `!async.func`, `!class`, `!struct`, `!for`, `!break;`, `!continue;` |
| **`?`** | **Logic & Error Boundaries** | `?if`, `?else`, `?match`, `?try`, `?expect`, `?error`, `??` |
| **`@`** | **State & Data Identifiers** | `@player`, `@score`, `@inventory`, `@me` *(instance context)* |
| **`&`** | **Raw Buffers & Hardware Memory** | `&map create \| bytes, 1024;`, `&flags create \| bits;`, `&buf[0] = 213;` |
| **`$`** | **Compiler & Environment Directives** | `$import lib "..." as ...`, `$import cpp <...> as ...`, `$kernel` |

---

## Syntax at a Glance

### 1. Variables & Pipeline Typing
Variables are declared and assigned dynamically with explicit type hinting via the pipe (`|`) operator:
```viss
@name = "Halva" | str;
@score = 100 | int;
@speed = 12.5 | dec;
@active = true | bool;

// Dynamic heterogeneous list (stores any types without crashes!)
@inventory = ["Sword", 42, true] | list;

// Constants with comma modifier (cannot be reassigned)
@MAX_HEALTH = 100 | int, const;
@MAX_HEALTH del; // Explicit memory cleanup when needed

// Type conversion pipeline
@user_input = "25" | str;
@age = @user_input | int; // Converted to integer instantly!
```

### 2. String Interpolation (`i"..."`)
Strings prefixed with `i` support direct in-place variable embedding without cumbersome concatenation or forced brackets:
```viss
io.println(i"Welcome @player.name! Current score: @player.score");

// Complex expressions use curly brackets:
io.println(i"Next level in: {@needed_score - @player.score} points");
```

### 3. Object-Oriented Programming & Classes
Classes are defined with `!class`. The constructor is declared as `!func main(...)`, and the instance is referenced via `@me`:
```viss
!class Player :: Entity { // Inheritance via '::'
    !func main(name as str, score as int) {
        @me.name = name | str;
        @me.score = score | int;
        @me.items = [] | list;
    }

    !func add_item(item as str) {
        @me.items.append(item);
    }
}

// Instantiating with explicit entity classifier:
@hero = Player("Arthur", 100) | class;
@hero.add_item("Excalibur");
```

### 4. Dynamic Variable Bags (`| inf`)
For dynamic systems (game state, AI agents, server entity-component systems), Viss introduces **Infinite Dynamic Tables (`inf`)**:
```viss
!class StateStore {
    !func main() {
        @me.vars = {"health": 100} | inf;
    }

    !func set(key as str, val as any) {
        @me.vars[key] = val; // Add or mutate properties on the fly!
    }
}

@store = StateStore() | class;
@store.set("buff_strength", 25);
io.println(i"Buff: {@store.vars["buff_strength"]}");
```

### 5. Control Flow, Loops & Ranges
Viss provides range iterators and collection traversals:
```viss
// Number range iteration (0 to 10)
!for @i in 0..10 {
    io.println(i"Iteration: @i");
}

// Collection iteration
!for item in @hero.items {
    io.println(i"Carrying: @item");
}
```

### 6. Pattern Matching (`?match`)
Pattern matching provides clean, expressive multi-way branching with automatic fallthrough prevention:
```viss
!func get_rank(score as int) to str {
    ?match score {
        case 100 {
            return "Champion";
        }
        case 50 {
            return "Warrior";
        }
        else {
            return "Recruit";
        }
    }
}
```

### 7. Asynchronous Flow & Concurrency
Asynchronous functions run on an optimized thread pool and are awaited with clean `await`:
```viss
!async.func fetch_bonus(player_name as str) to int {
    await async.sleep(100); // Non-blocking background sleep
    return 25;
}

// Inside an async function:
@bonus = await fetch_bonus(@player.name);
@player.score += @bonus;
```

### 8. Error Handling & Predicates
Predictive functions returning booleans end with `?()` (e.g. `is_active?()`). Errors are handled via `?try` and `?expect`, or triggered with `?error`:
```viss
!func verify_health(hp as int) {
    ?if (hp <= 0) {
        ?error "Player is defeated!";
    }
}

?try {
    verify_health(@player.hp);
}
?expect Exception as @e {
    io.println(i"Error intercepted: @e");
}
```

### 9. Resource Safety with `defer`
Ensure files, sockets, and memory handles close reliably upon scope exit:
```viss
@file = sys.file.open("config.json", "w");
defer @file.close(); // Guaranteed execution on function exit

@file.write("{\"status\": \"ok\"}");
```

### 10. Low-Level Memory Buffers & Retro Gamedev (`&buffer create | bytes`)
For microcontrollers, retro games, and zero-latency simulations where memory is strictly budgeted (e.g. 1 KB levels), Viss provides raw hardware-level memory buffers (`bytes` and `bits`) using the **`&`** sigil. If no size is specified, it automatically allocates the maximum default capacity (1024 bytes / 8192 bits):

```viss
$import lib "iostream" as io
$import lib "retrotech" as rt

!func main() {
    &colormask create | bytes, 3; // RGB tint mask (3 bytes)
    &colormask[0] = 213; // Red
    &colormask[1] = 6;   // Green
    &colormask[2] = 31;  // Blue

    &map create | bytes; // 1 KB level grid (defaults to max 1024 bytes)
    &map = map.fill_map!(map, 100);

    rt.DrawRawPixels(&map, 32, 32);  // Blit raw 32x32 pixel grid
    rt.ColorScreen(&colormask, &map); // Color screen with mask
    rt.UpdateScreen();               // Truecolor ANSI retro display!
}

!class map {
    !func fill_map!(map, pattern) {
        pattern.clamp(100, 350);
        for @i in 0..map.size() {
            @block = int.random(pattern - double.sqrt(pattern), pattern + double.sqrt(pattern + 3)) | int;
            @block.clamp(0, 255);
            &map[@i] = @block;
        }
        return(&map);
    }
}
```

---

## Canonical Code Reference (`preview.viss`)

The full feature set of Viss v0.0.1.2 is demonstrated in `preview.viss`:

```viss
$import lib "asyncIO" as async // Import asynchronous runtime
$import lib "iostream" as io  // Import console I/O

!async.func main() { // Main entry point
    ?try {
        // Instantiate class with explicit | class classifier
        @player = Player("Halva", 100) | class;
        @rank = get_rank(@player.score);
        io.println(i"Player: @player.name, Rank: @rank");

        // Async task execution
        @bonus = await load_bonus_points(@player.name);
        @player.score += @bonus;
        io.println(i"New score: @player.score");

        // Collections & iteration
        @inventory = ["Sword", "Shield", "Elixir"] | list;
        !for item in @inventory {
            @player.add_item(item);
        }
        io.println(i"Inventory count: {@player.items.len}, Last: {@player.items.last}");
    }
    ?expect Exception as @e {
        io.println(i"Critical error: @e");
    }
}

!class Player {
    !func main(name as str, score as int) {
        @me.name = name | str;
        @me.score = score | int;
        @me.items = [] | list;
    }

    !func add_item(item as str) {
        @me.items.append(item);
    }
}

!func get_rank(score as int) to str {
    ?match score {
        case 100 {
            return "Champion";
        }
        case 50 {
            return "Warrior";
        }
        else {
            return "Recruit";
        }
    }
}

!async.func load_bonus_points(name as str) to int {
    await async.sleep(100);
    return 25;
}
```

---

## CLI Usage & Toolchain

The Viss toolchain provides subcommands for compilation, instant execution, and syntax diagnostics:

```bash
# Check syntax and diagnostics:
viss check oldgame.viss

# Transpile and compile to native executable:
viss compile oldgame.viss

# Compile and immediately run:
viss run oldgame.viss

# Initialize a new Viss project:
viss init my_game

# Display toolchain version & architecture:
viss version
```

---

## Standard Library Modules (`libs/std`)

* **`iostream` (`io`)**: Formatted variadic console I/O, truecolor ANSI, cursor control, interactive single-key input (`io.read_char()`), file I/O.
* **`retrotech` (`rt`)**: Virtual retro graphics display engine (`DrawRawPixels`, `ColorScreen`, `UpdateScreen`), palette modulation, system sound (`Beep`).
* **`system` (`sys`)**: Process spawning (`sys.command`, `sys.exec`), environment variables (`sys.env`), CPU core detection (`sys.cpu_count`), OS detection (`sys.os`).
* **`fs`**: File system manipulation (`read_bytes`, `write_bytes`, `exists`, `mkdir`, `list_dir`, `size`, `remove`).
* **`math`**: Complete mathematical functions (`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sqrt`, `cbrt`, `pow`, `abs`, `round`, `floor`, `ceil`, `clamp`, `random_int`, `random_dec`, `PI`, `E`).
* **`time`**: Timestamps (`time.now_ms()`, `time.now_secs()`), high-resolution sleep, timestamp formatting.
* **`str`**: Complete string manipulation (`split`, `join`, `trim`, `lower`, `upper`, `contains`, `replace`, `starts_with`, `ends_with`, `pad_left`, `pad_right`, `repeat`).
* **`asyncIO` (`async`)**: High-performance thread pool, timers, async task dispatching.

---

## VS Code Extension (`viss-vscode`)

Viss features a complete Visual Studio Code extension located in `.vscode/extensions/viss-vscode`:
* **Syntax Highlighting**: Full TextMate grammar for all Viss 2.0 constructs, raw memory sigils `&`, interpolation `i"..."`, and pipeline typings.
* **Diagnostics & Linter**: Instant on-save syntax error detection and bracket balancing with line/column markers.
* **Hover Documentation**: Detailed documentation popups for all primitives (`bytes`, `bits`, `inf`), keywords, and standard library modules.
* **IntelliSense Completions**: Contextual autocompletion for keywords, collections, and standard methods (`.len`, `.append`, `.set_bit`, `.to_hex`).
* **Snippets**: Pre-built templates for functions, classes, raw byte buffers, retro screens, and loops.
* **One-Click Run**: Press the Run button in the editor header or trigger `Viss: Run Program`.

---

## Roadmap & Milestones

- [x] Viss 1.0 Initial Prototype & Prefix Specification
- [x] Modular Standard Library Split (`libs/std/`)
- [x] Dynamic String Interpolation (`i"..."`)
- [x] Pipeline Variable Declarations (`| type`, `| const`)
- [x] Class Inheritance (`::`) and Structured Constructor (`!func main`)
- [x] Dynamic Expando Table Storage (`| inf`)
- [x] Pattern Matching (`?match`, `case`, `else`)
- [x] Cross-platform ThreadPool Concurrency (`!async.func`, `await`)
- [ ] Self-contained TCC / Clang embedded JIT compiler for zero-dependency execution
- [ ] Official Visual Studio Code & JetBrains syntax highlighting extension bundle

---

## License

Viss is released under the terms of the **UniLicense**. You are free to use, modify, distribute, and build commercial software with Viss without restrictions. See [LICENSE](LICENSE) for details.

Developed by **Halva-developer**.
