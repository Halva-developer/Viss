<div align="center">

<img src="viss_logo.jpg" alt="Viss Logo" width="220" />

# Viss Programming Language
### The High-Performance, Visually Structured Language for Everyday Development & Systems

[![Version](https://img.shields.io/badge/version-v0.4.1%20%22Flame%20Princess%22-blue.svg)](https://github.com/Halva-developer/Viss)
[![Release](https://img.shields.io/badge/%F0%9F%94%A5%20flame%20princess-v0.4.1-orange.svg)](https://github.com/Halva-developer/Viss)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-orange.svg)](https://github.com/Halva-developer/Viss)

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
| `v0.2.1` | **"Hambo"** | Hambo | Dynamic auto-lists, Map/Dict literals (`{}`), range slicing (`..`), `in`/`!in` membership operators, tuple unpacking, native `!async`/`!await`, string kinds (`r""`, `b""`, `""""""`), `fs.glob`, `json.loads`/`dumps`, pure native C++ runtime. |
| `v0.3.0` | **"Peppermint Butler"** | Peppermint Butler | VSC - Viss Script Core & CLI runner (`vsc` / `ssi`), Official Package Manager (`viss pkg`), Cross-Platform GUI (`libs/std/gui.hpp`), Native Language Server Protocol (`viss lsp`), Embedded JSON Document Database (`libs/std/db.hpp`), HTTP Networking (`libs/std/net.hpp`), Go-Style Concurrency Channels (`libs/std/chan.hpp`), Independent Native Execution Engine (`viss vm`), Native Embedded Windows Icons, and VS Code Extension. |
| `v0.3.1` | **"Peppermint Butler"** | Peppermint Butler | CLI argument handling (`sys.args()`, `sys.arg()`, `sys.argc()`), First-Class Map Literals (`{"key": val}`), Full JSON module (`json.parse()`, `json.stringify()`), Object String & List methods (`.upper()`, `.lower()`, `.trim()`, `.split()`, `.replace()`, `.join()`), Extended filesystem (`fs.list()`, `fs.mkdir()`, `fs.remove()`), Shell execution (`sys.cwd()`, `sys.exec()`). |
| `v0.3.2` | **"Hot Dog Princess"** | Hot Dog Princess | Complete audit cleanup; strict security hardening (SQL injection prevention, path traversal protection, removal of proprietary license code, workspace trust); 64-bit integer literal precision; class inheritance (`!class Player :: Entity`); bounds checking; UTF-8 codepoint string handling (`.len`, `.substr`, `.upper()`, `.lower()`); control loop fixes (`!for`, `!while`, `?if` without paren collisions); non-zero error exits. |
| `v0.3.3h` | **"Flame Princess"** | Flame Princess | VM parser and tokenization fixes (eliminated brace counter corruption on comparison operators `<=`, `>=`, `==` in `?if`/`!while`); regular expressions standard module `re` (`re.match`, `re.search`, `re.findall`, `re.sub`, `re.split`); zero-dependency zip archive module `zip` (`zip.list`, `zip.read_text`, `zip.extract` for reading `.docx` and archives); hex (`0xFF`), binary (`0b1010`), and scientific notation number literals; VSC arrow return statements normalization. |
| `v0.4.0hl` | **"Flame Princess"** | Flame Princess | Hot speed & loud: VM branch precedence and `?else if` execution order fixes; string interpolation `i"..."` in VM; strings containing `|` in VM; automatic declaration of `@me.field` class members in C++ struct codegen; fixed parameterized SQL placeholder substitution in `db.exec_params`; comment preservation in formatter (`viss fmt`); compiler auto-discovery in PATH for `viss run`/`build`. |
| `v0.4.1` | **"Flame Princess"** | Flame Princess | **Current Release:** Complete Linux GCC 13/14 compilation audit (zero compiler warnings with `-Wall -Wextra`); variable lookahead type promotion for untyped floats; type-conversion pipelines without error leaks; VM error handling (eliminated trailing null after NameError); SQL placeholder parser hardened against string literals; CSPRNG hardware entropy for `random_bytes` and `uuid`; HTTP server active connection throttling (`max_clients = 256`); project-local `libs/` resolution for `$import lib`; AudioCoverWatcher v0.4.1 native modernization. |

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

The Viss toolchain provides subcommands for unified multiplatform compilation, instant execution, package management, language server, and syntax diagnostics:

```bash
# Multiplatform Binary Builder (v0.3.0):
viss create exe app.viss [-o app.exe] [--gui] [--icon icon.ico]  # Windows standalone executable
viss create bin app.viss [-o app]                                # Linux ELF binary / distribution pkg
viss create apple app.viss [-o app.app]                          # macOS Application Bundle (.app)

# Fast Execution, VSC & REPL:
viss repl                                  # Real-time in-memory Pythonic REPL (or just run 'viss')
viss vm script.viss                        # Native VissVM execution (0ms compile overhead)
viss exec script.viss                      # Alias for 'viss vm'
viss run script.vsc                        # Run VSC script directly via Viss compiler
vsc script.vsc                             # VSC - Viss Script Core standalone runner (0ms overhead)
vsc -c "RUN- println('Hi'); END;"          # Run inline VSC code block
vsc -e "a(i)-return(i+10);END;println(a(1));" # Evaluate expression or snippet directly
viss -e "math.sqrt(16.0)"                  # Evaluate inline Viss expression directly
viss run app.viss [--fast]                 # Transpile, compile, and run (with caching)

# Package Manager:
viss pkg install <package|git-url> [-g]    # Install package
viss pkg purge <package>                   # Remove installed package
viss pkg update                            # Fetch latest repository index
viss pkg upgrade                           # Upgrade all installed packages
viss pkg available                         # Browse available packages from registry
viss pkg search <query>                    # Search packages by name or description
viss pkg list                              # List installed packages
viss pkg server <url|none>                 # Configure package registry endpoint
viss pkg init [name]                       # Initialize new package manifest

# Self-Updater:
viss update                                 # Automatically update Viss compiler & stdlib from GitHub
viss update --check                         # Check if newer version is available without installing
viss update --force                         # Force reinstall / upgrade
viss update --source                        # Build and install directly from Git source
viss update dev                             # Update directly from development branch

# Toolchain, Diagnostics & Bundling:
viss check app.viss                        # Fast syntax validator and linter
viss fmt [--check] app.viss                # Format source code to canonical style
viss build app.viss                        # Transpile and compile to standard executable
viss bundle app.viss -o app.exe            # 100% standalone binary (Zero DLL dependencies)
viss sfx installer.viss -p dir/ -o out.exe # Self-extracting single-file installer
viss lsp                                   # Start Language Server Protocol daemon
viss test                                  # Run automated test suites in tests/
viss clean                                 # Clean cache (.viss_cache/)
viss version                               # Print toolchain version & codename
```

---

## Standard Library Modules (`libs/std`)

* **`iostream` (`io`)**: Formatted variadic console I/O, truecolor ANSI, cursor control, interactive single-key input (`io.read_char()`), file I/O.
* **`db`**: Lightweight embedded JSON document store and key-value database (`db.open`, `db.exec`, `db.query_json`, `db.count`, `db.close`).
* **`net`**: HTTP networking client and development server (`net.create_server`, `@srv.get`, `@srv.post`, `@srv.listen`, `net.http_get`, `net.http_post`, `net.download_file`).
* **`chan`**: Go-style thread-safe buffered and unbuffered concurrency channels (`chan.create<T>`, `chan.send`, `chan.recv`, `chan.try_recv`, `chan.close`).
* **`gui`**: Cross-platform hardware-accelerated GUI and software rasterizer with identical API across Windows (GDI+), Linux, and macOS.
* **`audio` / `media`**: SFX sound effects, synth waveforms, background music playback, and ID3/Vorbis/FLAC metadata tagging.
* **`retrotech` (`rt`)**: Virtual retro graphics display engine (`DrawRawPixels`, `ColorScreen`, `UpdateScreen`), palette modulation, system sound (`Beep`).
* **`system` (`sys`)**: Process spawning (`sys.command`, `sys.exec`), environment variables (`sys.env`), CPU core detection (`sys.cpu_count`), OS detection (`sys.os`), distro detection (`sys.distro`).
* **`fs`**: File system manipulation (`read`, `write`, `exists`, `mkdir`, `list`, `size`, `remove`).
* **`math`**: Complete mathematical functions (`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sqrt`, `cbrt`, `pow`, `abs`, `round`, `floor`, `ceil`, `clamp`, `random_int`, `random_dec`, `PI`, `E`).
* **`time`**: Timestamps (`time.now_ms()`, `time.now_secs()`), high-resolution sleep, timestamp formatting.
* **`str`**: Complete UTF-8 aware string manipulation (`split`, `join`, `trim`, `lower`, `upper`, `contains`, `replace`, `starts_with`, `ends_with`, `substr`, `len`).
* **`re`**: Full regular expressions engine (`match`, `search`, `findall`, `sub`, `split`).
* **`zip`**: Zero-dependency ZIP archive reader and extractor (`list`, `read_text`, `extract`).
* **`asyncIO` (`async`)**: High-performance thread pool, timers, async task dispatching.

---

## VS Code Extension (`viss-vscode`)

Viss features an official Visual Studio Code extension located in `extensions/viss-vscode`:
* **Language Server Integration**: Real-time LSP completion, diagnostics, and hover symbols powered by `viss lsp`.
* **Syntax Highlighting**: Full TextMate grammar for all Viss constructs, raw memory sigils `&`, interpolation `i"..."`, single quotes `'...'`, and pipeline typings.
* **Diagnostics & Linter**: Instant on-save syntax error detection and human-friendly diagnostics (`viss check`).
* **Hover Documentation**: Detailed documentation popups for all primitives, keywords, and standard library modules.
* **IntelliSense Completions**: Contextual autocompletion for `db`, `net`, `chan`, `gui`, `audio`, `math`, `io`, and more.
* **Snippets**: Pre-built templates for functions, classes, apps, HTTP servers (`netserver`), channels (`chanapp`), and GUI apps (`guiapp`).
* **Keybindings**: `F5` Run, `Ctrl+F5` Check Syntax, `Shift+F5` Build, `Alt+F5` Bundle Standalone Executable.

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
- [x] Independent Native Execution Engine (`viss vm` / `VissVM`)
- [x] Official Visual Studio Code Extension (`viss-vscode`)
- [x] Official Package Manager (`viss pkg`)
- [x] Language Server Protocol Daemon (`viss lsp`)
- [x] Embedded JSON Document Database Engine (`libs/std/db.hpp`)
- [x] HTTP Networking & Development Server (`libs/std/net.hpp`)
- [x] Go-style Concurrency Channels (`libs/std/chan.hpp`)
- [x] Cross-Platform Windowing & Software Rasterizer GUI (`libs/std/gui.hpp`)
- [x] Viss 0.3.2 Security Hardening, Audit Fixes & UTF-8 Strings
- [x] Viss 0.3.3h Flame Princess (Hot Speed) — VM Comparison Parser Fixes, Regex (`re`), Zero-Dependency ZIP (`zip`), Hex/Bin Numerics
- [x] Viss 0.4.0hl Flame Princess (Hot Speed & Loud) — VM if/elif/else branch precedence, string interpolation `i"..."`, `@me.field` auto-declaration, `db.exec_params` placeholder fix, formatter comments preservation

---

## License

Viss is released under the terms of the **MIT License**. You are free to use, modify, distribute, and build commercial software with Viss without restrictions. See [LICENSE](LICENSE) for details.

Developed by **Halva-developer**.
