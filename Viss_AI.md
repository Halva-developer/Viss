# Viss AI System Specification & Language Reference (LLM Edition)

> **Document Purpose:** Definitive, formal, zero-hallucination specification of the Viss programming language for AI Models, Coding Agents, LLMs, and Compiler Engineers.  
> **Compiler Target:** Native C++20 via `src/vissc.cpp` -> MinGW GCC / Clang standalone binary.  
> **Transpilation Mode:** Single-pass AST-free deterministic transpiler with regex pattern matching and scope-tracking code emission.  
> **Current Release:** Viss v0.2.1 "Hambo" (Native)  
> **Creator & Author:** Halva (Halva-developer)

---

## 1. System Prompt & LLM Generation Directives

When writing code in Viss, the AI model MUST strictly adhere to the following invariant rules:

1. **Variable Prefix Invariant:**
   - Every variable definition, assignment, and usage MUST be prefixed with `@` (e.g. `@x = 10`, `@speed = 5.5`, `@player.hp = 100`).
   - Exception: Raw binary byte/mask buffers MUST be prefixed with `&` (e.g. `&buf create | bytes, 128`).
2. **Deterministic & Safe Prefixes:**
   - Function declarations: `!func name(@param: type) as ret_type { ... }` or `func name(...) { ... }`.
   - Entry point: `!main { ... }`.
   - Conditions: `?if cond { ... } ?else if cond { ... } else { ... }`.
   - Loops: `!while ui.is_open() { ui.poll() ... }`, `for @i in 0..100 { ... }`.
   - Match/Switch: `?match @expr { case Value { ... } default { ... } }`.
   - Defer: `!defer cleanup()` or `defer cleanup()`.
3. **Casting Rules (Zero Hallucination):**
   - NEVER emit C-style casts like `(dec)@x` or `(int)@y` unless strictly necessary.
   - ALWAYS prefer functional casts: `dec(@x)`, `int(@y)`, `str(@z)`, `bool(@w)`, or the `as` operator: `@x as dec`, `@y as int`.
4. **Typed Collections Invariant:**
   - When declaring a list of structs or custom classes, ALWAYS use the pipe syntax:
     ```viss
     @enemies = [] | Enemy
     @bullets = [] | Bullet
     @stars = [] | Star
     ```
   - When filtering lists in-place, NEVER use reverse deletion loops when `retain` is cleaner:
     ```viss
     @bullets.retain(|b| b.active && b.life > 0.0)
     ```
5. **String Interpolation Invariant:**
   - String interpolation uses `i"..."` with `{expression}` inside.
   - NEVER emit Python's `f"..."`, C#'s `$"..."`, or JS's `` `...` ``.

---

## 2. Formal Grammar (EBNF Subset)

```ebnf
Program         ::= { Import | StructDecl | EnumDecl | ClassDecl | FuncDecl | Statement }
Import          ::= "$import" ("lib" | "file") StringLiteral "as" Identifier [";"]

StructDecl      ::= ("!struct" | "struct") Identifier "{" { StructMember } "}"
StructMember    ::= FieldDecl | FuncDecl
FieldDecl       ::= ["@"] Identifier ":" Type ["=" Expression] [";"]
                  | ["@"] Identifier "=" Expression [";"]

EnumDecl        ::= ("!enum" | "enum") Identifier "{" { EnumItem } "}"
EnumItem        ::= Identifier ["=" IntLiteral] ["," | ";"]

FuncDecl        ::= ["!"] ["async."] "func" Identifier "(" [ParamList] ")" [("as" | "to" | "->") Type] "{" { Statement } "}"
ParamList       ::= Param { "," Param }
Param           ::= ["@"] Identifier ((":" | "as") Type) ["=" Expression]
                  | ["@"] Identifier ["=" Expression]

MainDecl        ::= ("!main" | "main" | "!func main()" | "func main()") "{" { Statement } "}"

Statement       ::= VarAssign
                  | IfStmt
                  | MatchStmt
                  | ForStmt
                  | WhileStmt
                  | DeferStmt
                  | ReturnStmt
                  | Expression [";"]

VarAssign       ::= "@" Identifier ["." Identifier] ("=" | "+=" | "-=" | "*=" | "/=") Expression [";"]
                  | "@" Identifier "=" "[]" "|" Identifier [";"]

IfStmt          ::= ("?if" | "if") Expression "{" { Statement } "}"
                    { ("?else if" | "else if" | "} else if") Expression "{" { Statement } "}" }
                    [ ("?else" | "else" | "} else") "{" { Statement } "}" ]

MatchStmt       ::= ("?match" | "match" | "switch") Expression "{" { CaseClause } "}"
CaseClause      ::= "case" Expression "{" { Statement } "}"
                  | ("default" | "case _" | "else") "{" { Statement } "}"

ForStmt         ::= [("!" | "?")] "for" "@" Identifier "in" Expression ".." Expression "{" { Statement } "}"
                  | [("!" | "?")] "for" "@" Identifier "in" Expression "{" { Statement } "}"

WhileStmt       ::= [("!" | "?")] "while" Expression "{" { Statement } "}"

DeferStmt       ::= ("!defer" | "defer") Expression [";"]

Type            ::= "int" | "dec" | "double" | "float" | "str" | "string" | "bool" | "bytes" | "bits"
                  | "Vec2" | "Vec3" | "List" ["<" Type ">"] | "Map" ["<" Type "," Type ">"] | Identifier
```

---

## 3. Lexical Structure & Transpiler Rules

### Variable Scope Resolution
The transpiler tracks declared variables using a lexical `ScopeTracker`.
- When an assignment `@x = expr` is transpiled:
  - If `x` has already been declared in the current or outer scope, it transpiles to:
    ```cpp
    x = expr;
    ```
  - If `x` has NOT been declared, it transpiles to:
    ```cpp
    auto x = expr;
    ```
- **Inside Struct Methods:**
  - All struct fields are automatically pre-registered in the method's scope.
  - References like `@hp = @hp - @dmg` transpile directly to:
    ```cpp
    hp = hp - dmg;
    ```
    which modifies the struct member `this->hp`.

### String Interpolation Translation
- Source: `i"Current: {@x}, Next: {@x + 1}"`
- Transpiled C++:
  ```cpp
  (viss::Str("Current: ") + viss::toStr(x) + viss::Str(", Next: ") + viss::toStr(x + 1))
  ```

### Enum Translation
- Source:
  ```viss
  enum State {
      Idle = 0,
      Running = 1,
      Dead = 2
  }
  ```
- Transpiled C++:
  ```cpp
  struct _enum_State {
      int _v = 0;
      _enum_State() : _v(0) {}
      _enum_State(int v) : _v(v) {}
      operator int() const { return _v; }
      static const int Idle = 0;
      static const int Running = 1;
      static const int Dead = 2;
  };
  using State = _enum_State;
  ```
- Both `State.Idle` and `State::Idle` evaluate to the integer constant `0` at compile time and work in `switch`/`case`.

### Struct Methods Translation
- Source:
  ```viss
  struct Ship {
      hp: dec = 100.0
      !func hit(@amt: dec) {
          @hp -= @amt
      }
  }
  ```
- Transpiled C++:
  ```cpp
  struct Ship {
      viss::Dec hp = 100.0;
      inline auto hit(viss::Dec amt) {
          hp -= amt;
      }
      Ship() = default;
      Ship(viss::Dec _p0_hp) : hp(_p0_hp) {}
  };
  ```

### Pipeline Operator (`|>`)
- Syntax: `expr |> fn` or `expr |> fn(arg2, ...)`
- Sequential forwarding of LHS as first argument to RHS:
  - `@str |> str.trim |> str.upper` -> `viss::str::upper(viss::str::trim(str))`
  - `@val |> math.clamp(0.0, 100.0)` -> `viss::math::clamp(val, 0.0, 100.0)`

### Elvis Operator (`?:`)
- Syntax: `lhs ?: rhs`
- Transpiles to `viss::elvis(lhs, rhs)` with fallback semantics:
  - For strings: evaluates to `lhs.empty() ? Str(rhs) : lhs`.
  - For bool / numeric types: evaluates to `bool(lhs) ? lhs : rhs`.
- Chaining supported: `@a ?: @b ?: @c` -> `viss::elvis(viss::elvis(a, b), c)`.

---

## 4. Complete Standard Library Reference

### 1. `io` (`libs/std/io.hpp`)
```cpp
void println(const T& val);
void print(const T& val);
std::string read_line();
void error(const T& val);
```

### 2. `math` (`libs/std/math.hpp`)
```cpp
Dec sin(Dec rad);
Dec cos(Dec rad);
Dec tan(Dec rad);
Dec asin(Dec val);
Dec acos(Dec val);
Dec atan2(Dec y, Dec x);
Dec sqrt(Dec val);
Dec pow(Dec base, Dec exp);
Dec abs(Dec val);
Dec min(Dec a, Dec b);
Dec max(Dec a, Dec b);
Dec clamp(Dec val, Dec min, Dec max);
Dec round(Dec val);
Dec floor(Dec val);
Dec ceil(Dec val);
Dec random_dec();               // Returns double in [0.0, 1.0)
Int random_int(Int min, Int max);// Returns random integer in [min, max]
Dec deg_to_rad(Dec deg);
Dec rad_to_deg(Dec rad);
const Dec pi = 3.14159265358979323846;
```

### 3. `str` (`libs/std/str.hpp`)
```cpp
Int len(const Str& s);
Str trim(const Str& s);
Str lower(const Str& s);
Str upper(const Str& s);
Bool contains(const Str& s, const Str& sub);
List<Str> split(const Str& s, const Str& sep);
Str join(const List<Str>& list, const Str& sep);
Str replace(const Str& s, const Str& from, const Str& to);
Bool starts_with(const Str& s, const Str& prefix);
Bool ends_with(const Str& s, const Str& suffix);
Str sub(const Str& s, Int start, Int len);
Int to_int(const Str& s, Int def = 0);
Dec to_dec(const Str& s, Dec def = 0.0);
```

### 4. `fs` (`libs/std/fs.hpp`)
```cpp
Bool exists(const Str& path);
Str read(const Str& path);
Bool write(const Str& path, const Str& content);
Bool append(const Str& path, const Str& content);
Bool remove(const Str& path);
Int size(const Str& path);
List<Str> list_dir(const Str& path);
Bool create_dir(const Str& path);
```

### 5. `time` (`libs/std/time.hpp`)
```cpp
Int now_ms();               // Current Unix timestamp in ms
Int now();                  // Current Unix timestamp in seconds
void sleep_ms(Int ms);      // Sleep current thread for ms
void sleep(Dec sec);        // Sleep current thread for seconds
Str format(const Str& fmt); // Strftime formatted date
```

### 6. `audio` (`libs/std/audio.hpp`)
```cpp
// Background Music (Streaming MP3/OGG)
Bool play_bgm(const Str& path, Dec volume = 0.2);
void stop_bgm();
void pause_bgm();
void resume_bgm();
void set_bgm_volume(Dec volume); // volume: 0.0 to 1.0

// Synthesized Warm Analog Sound Effects (Low-Pass Filtered)
void laser(Dec freq = 880.0, Dec duration_ms = 80.0);
void hit();
void gem();
void powerup();
void explosion();
void dash();
```

### 7. `ui` / `gui` (`libs/std/gui.hpp`)
```cpp
// Lifecycle
Bool create(const Str& title, Int width = 800, Int height = 600);
Bool is_open();
Bool poll();
void clear(Color c = Colors::DarkBg);
void update();              // Present backbuffer to screen
void close();

// Input
Bool key_down(Int vk_code);     // True while key is held down
Bool key_pressed(Int vk_code);  // True only on the frame key was pressed
Int mouse_x();
Int mouse_y();
Bool mouse_down();
Bool mouse_clicked();

// Rendering
void draw_rect(Int x, Int y, Int w, Int h, Color c, Bool fill = true);
void draw_round_rect(Int x, Int y, Int w, Int h, Int r, Color c, Bool fill = true, Color border_c = Color(0,0,0,0), Int border_w = 1);
void draw_circle(Int x, Int y, Int radius, Color c, Bool fill = true);
void draw_triangle(Int x1, Int y1, Int x2, Int y2, Int x3, Int y3, Color c, Bool fill = true, Color border_c = Color(0,0,0,0), Int border_w = 1);
void draw_quad(Int x1, Int y1, Int x2, Int y2, Int x3, Int y3, Int x4, Int y4, Color c, Bool fill = true, Color border_c = Color(0,0,0,0), Int border_w = 1);
void draw_line(Int x1, Int y1, Int x2, Int y2, Color c, Int width = 1);
void draw_text(Int x, Int y, const Str& text, Color c, Int size = 14, const Str& font = "Segoe UI", Bool bold = false);

// Colors
Color Color::rgb(Int r, Int g, Int b);
Color Color::rgba(Int r, Int g, Int b, Int a);
Color Color::hex(Int hex_val);
```

### 8. `Vec2` & `Vec3` Vector Math (`libs/vissrt.hpp`)
```cpp
// Vec2
Vec2 vec2(Dec x = 0.0, Dec y = 0.0);
Vec2 Vec2::operator+(const Vec2& o);
Vec2 Vec2::operator-(const Vec2& o);
Vec2 Vec2::operator*(Dec scalar);
Vec2 Vec2::operator/(Dec scalar);
Dec Vec2::len();            // or .size()
Dec Vec2::len_sq();
Dec Vec2::dist(const Vec2& other);
Dec Vec2::dot(const Vec2& other);
Vec2 Vec2::normalize();

// Vec3
Vec3 vec3(Dec x = 0.0, Dec y = 0.0, Dec z = 0.0);
Vec3 Vec3::operator+(const Vec3& o);
Vec3 Vec3::operator-(const Vec3& o);
Vec3 Vec3::operator*(Dec scalar);
Vec3 Vec3::operator/(Dec scalar);
Dec Vec3::len();            // or .size()
Dec Vec3::dist(const Vec3& other);
Dec Vec3::dot(const Vec3& other);
Vec3 Vec3::cross(const Vec3& other);
Vec3 Vec3::normalize();
```

---

## 5. Verified Canonical Implementation Templates

### Template 1: High-Performance Data Processing with Structs, Methods & Enums
```viss
$import lib "iostream" as io

enum ItemRarity {
    Common = 0,
    Rare = 1,
    Legendary = 2
}

struct Item {
    name: str = ""
    rarity: ItemRarity = ItemRarity.Common
    value: int = 10

    !func get_sell_price() as int {
        ?match @rarity {
            case ItemRarity.Legendary {
                return @value * 5
            }
            case ItemRarity.Rare {
                return @value * 2
            }
            default {
                return @value
            }
        }
    }
}

!main {
    @inventory = [] | Item
    @inventory.add(Item("Rusty Dagger", ItemRarity.Common, 15))
    @inventory.add(Item("Shadow Bow", ItemRarity.Rare, 80))
    @inventory.add(Item("Excalibur", ItemRarity.Legendary, 500))

    // Filter in-place: keep items worth >= 50
    @inventory.retain(|item| item.get_sell_price() >= 50)

    for @item in @inventory {
        io.println(i"Item: {@item.name} | Price: {@item.get_sell_price()} Gold")
    }
}
```

### Template 2: Standalone 2D/3D Interactive Game Loop (60 FPS Native Window)
```viss
$import lib "iostream" as io
$import lib "gui" as ui
$import lib "time" as time
$import lib "math" as math
$import lib "audio" as audio

struct Particle {
    pos: Vec2 = vec2(0.0, 0.0)
    vel: Vec2 = vec2(0.0, 0.0)
    life: dec = 60.0
    color: int = 16776960
}

!main {
    @win_w = 900
    @win_h = 600
    ui.create("Viss Vector Engine", @win_w, @win_h)
    !defer ui.close()

    @player_pos = vec2(450.0, 300.0)
    @particles = [] | Particle

    !while ui.is_open() {
        ui.poll()

        // 1. Input & Physics
        @move = vec2(0.0, 0.0)
        ?if ui.key_down(65) { @move.x -= 1.0 } // A
        ?if ui.key_down(68) { @move.x += 1.0 } // D
        ?if ui.key_down(87) { @move.y -= 1.0 } // W
        ?if ui.key_down(83) { @move.y += 1.0 } // S

        ?if @move.len() > 0.0 {
            @player_pos += @move.normalize() * 5.0
            @particles.add(Particle(@player_pos, vec2(math.random_dec() * 2.0 - 1.0, math.random_dec() * 2.0 - 1.0), 30.0, 65535))
        }

        // 2. Update Particles & in-place cleanup
        for @p in @particles {
            @p.pos += @p.vel
            @p.life -= 1.0
        }
        @particles.retain(|p| p.life > 0.0)

        // 3. Render
        ui.clear(ui.Color.rgb(15, 18, 30))

        for @p in @particles {
            ui.draw_circle(int(@p.pos.x), int(@p.pos.y), 2, ui.Color.hex(@p.color), true)
        }

        ui.draw_circle(int(@player_pos.x), int(@player_pos.y), 12, ui.Color.rgb(0, 255, 180), true)
        ui.draw_text(20, 20, i"Particles: {@particles.len()}", ui.Color.rgb(255, 255, 255), 14, "Segoe UI", true)

        ui.update()
        time.sleep_ms(16)
    }
}
```
