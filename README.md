# Viss: A Modern & Young Programming Language with a Cool Future 🌟🚀

```
____   ____.___ 
\   \ /   /|   |
 \   Y   / |   |
  \     /  |   |
   \___/   |___|
```

> [!IMPORTANT]
> **📬 Contact Info / Get in Touch:**
> * **Email:** [halva@horni.cc](mailto:halva@horni.cc)
> * **Discord:** `@halvu`
> 
> Reach out to collaborate, suggest improvements, or ask questions!

Viss is a modern, statically typed, prefix-oriented programming language designed for high-performance native development. By transpiling directly to clean, debuggable C++17, Viss combines the safety and readability of expressive syntax with the raw speed and portability of native binaries.

---

## 📢 Call to Action: Help Us Shape the Future of Viss!

**Viss is young, fast, and has a cool future ahead. We need YOUR help to make it even better!**

We have built a strong foundation: a native C++ compiler (`src/vissc.cpp`), a cross-platform standard runtime (`libs/vissrt.hpp`), and a package manager (`vpm.py`). Now, it's time for the global open-source community to take Viss to the next level.

### 🌟 How You Can Support & Contribute:
* **Star the Repo:** Help more developers discover Viss!
* **Implement VSGC:** Build the background cycle-detecting garbage collector inside `vissrt.hpp`.
* **Build an LSP:** Create a Language Server Protocol (LSP) for autocompletion and diagnostics in VS Code.
* **Expand the Package Registry:** Write new libraries and publish them using `vpm`.
* **Port standard libraries:** Implement OpenGL/Vulkan rendering loops on Linux and macOS.

If you love compiler design, system programming, or building tools, **Viss is the perfect playground for you. Let's build a cool future together!**

---

## 📂 Project Directory Structure

To keep the repository clean and manageable, Viss organizes its workspace as follows:

* **`libs/`**
  * [vissrt.hpp](file:///C:/Users/halva/Desktop/Viss/libs/vissrt.hpp) - Header-only standard library runtime. Exposes I/O, file system, thread pool, math, string utilities, WinSock networking, and WebViss.
* **`src/`**
  * [vissc.cpp](file:///C:/Users/halva/Desktop/Viss/src/vissc.cpp) - The native standalone C++ compiler.
  * [vissc.py](file:///C:/Users/halva/Desktop/Viss/src/vissc.py) - The Python bootstrap transpiler.
* **Root Directory**
  * [vpm.py](file:///C:/Users/halva/Desktop/Viss/vpm.py) - Viss Package Manager with recursive dependency resolution and SHA-256 validation.
  * `LICENSE` - Viss UniLicense terms.
  * `README.md` - Complete language documentation.

---

## Table of Contents
1. [Syntax & Grammar Specification](#1-syntax--grammar-specification)
2. [Compiler Architecture](#2-compiler-architecture)
3. [Standard Runtime (vissrt)](#3-standard-runtime-vissrt)
4. [Custom Modules (VCM) & Package Management (VPM)](#4-custom-modules-vcm--package-management-vpm)
5. [Developer & Contribution Guide](#5-developer--contribution-guide)

---

## 1. Syntax & Grammar Specification

Viss utilizes a strict prefix-oriented symbolic grammar designed to categorize tokens, variables, and logic at a glance.

### A. The Prefix Design System

* **`!` Block flow prefix:** Used for defining structurally nested units (functions, loops, classes, namespaces).
  * `!func main()` - Declares the entry point.
  * `!loop (@i < 10)` - Declares a while loop.
  * `!class Player` - Declares a class/structure definition.
  * `!space utils` - Declares a namespace.
* **`?` Logical branching prefix:** Used for conditional code execution, pattern matching, and error boundary blocks.
  * `?if (@condition)` - Logical conditional.
  * `?else` - Conditional fallback.
  * `?match (@score)` - Starts a pattern matching case selector block.
  * `?try` - Declares a try block for exception boundaries.
  * `?catch (Error @err)` - Catches system or custom exceptions.
* **`@` Identifier prefix:** Denotes variable names, user parameters, and import operations.
  * `Int @score = 0` - Variable declaration.
  * `@use math for *` - Namespace imports.
* **`$` Asynchronous flow prefix:** Reserved for concurrency, coroutines, and async actions.
  * `!$func fetchData()` - Declares an asynchronous function.
  * `$await socket.read()` - Suspends execution waiting for I/O.
* **`+` Direct native C++ prefix:** Allows direct embedding of standard C++ syntax for low-level optimizations.
  * `+cpp { std::vector<int> v; }` - Inline C++ execution.

### B. Standard Core Types
Viss wraps standard types into unified high-level types with clean naming conventions:
* `Str` -> String type (`std::string`)
* `Int` -> 64-bit Integer type (`long long`)
* `Dec` -> Double-precision Float type (`double`)
* `Bool` -> Boolean type (`bool`)
* `List<T>` -> Dynamic array template (`viss::List<T>`, wrapped in `std::shared_ptr` to provide automatic reference-counted memory sharing and thread-safe mutex operations).
* `Map<K, V>` -> Hash map template (`viss::Map<K, V>`, wrapped in `std::shared_ptr` to provide automatic reference-counted memory sharing and thread-safe mutex operations).

### C. Automatic Semicolon Insertion (ASI)
Viss does not require semicolons `;` at the end of statements. The compiler automatically analyzes lines and appends semicolons during transpilation, keeping Viss code modern, clean, and minimal. You can still use semicolons if you prefer.

### Code Example:
```viss
@use IOstream for *
@use math for sin, PI

!class Point {
    Int @x = 0
    Int @y = 0

    !func set(Int @newX, Int @newY) {
        @x = @newX
        @y = @newY
    }
}

!func main() {
    Point @pt = {}
    @pt.set(100, 200)
    
    Dec @val = sin(PI / 2.0)
    io.println("sin(PI/2) = " + toStr(@val))

    Int @score = 100
    ?match (@score) {
        100 => {
            io.println("Perfect score!")
        }
        ?else => {
            io.println("Try again!")
        }
    }

    using main {
        return 0
    }
}
```

---

## 2. Compiler Architecture

The Viss compiler is written in C++ for maximum execution speed, zero-dependency builds, and easy cross-platform packaging. The source file is located at [src/vissc.cpp](file:///C:/Users/halva/Desktop/Viss/src/vissc.cpp).

### Compilation Pipeline:
```
  [Source.viss]
        |
        v
  [Syntax Validator]  =======> (Sanitizes code, evaluates structure, outputs clear diagnostics)
        |
        v
  [VCM Dependency Resolver] => (Recursively processes and generates .hpp modules)
        |
        v
  [Transpiler Engine] =======> (Performs regex token mapping to C++17)
        |
        v
  [Native Linker] ===========> (Invokes g++/clang++/cl to build final executable)
```

### Static Syntax Validator:
To ensure syntax errors are caught before C++ compilation, `src/vissc.cpp` parses the source file and outputs detailed English diagnostics with carets pointing to the exact column:
```
Syntax Error in main.viss:5:
    Int x = 10;
    ^^^^^^^^^^^
  Detail: Variables in Viss must start with a '@' prefix. Did you mean 'Int @x'?
```

---

## 3. Standard Library Runtime (`libs/vissrt.hpp`)

The runtime library [libs/vissrt.hpp](file:///C:/Users/halva/Desktop/Viss/libs/vissrt.hpp) is a single-header cross-platform library that wraps standard OS APIs.

### Namespace Reference:

#### `io` (Console I/O)
* `io.print(val)` / `io.println(val)` - Prints value to standard output.
* `io.eprint(val)` / `io.eprintln(val)` - Prints value to standard error output.
* `io.readln()` - Reads a line from console.
* `io.color(code)` - Sets console text color (ANSI escape sequences & Win32 compatible).
* `io.clear()` - Clears the console screen.
* `io.cursor(x, y)` - Sets cursor position on the screen.

#### `fs` (File System)
* `fs.write(path, content)` - Writes content to file.
* `fs.read(path)` - Reads entire file content as string.
* `fs.append(path, content)` - Appends content to file.
* `fs.exists(path)` - Checks if file exists.
* `fs.remove(path)` - Deletes a file.
* `fs.mkdir(path)` - Creates a directory.

#### `math` (Trigonometry & Calculus)
* `math.PI` / `math.E` - Mathematical constants.
* `math.sin(x)` / `math.cos(x)` / `math.tan(x)` - Trig functions.
* `math.sqrt(x)` / `math.pow(base, exp)` - Roots and powers.
* `math.abs(x)` / `math.round(x)` / `math.floor(x)` / `math.ceil(x)` - Formatting.

#### `time` (System Time & Sleep)
* `time.now()` - Returns Unix epoch timestamp (seconds).
* `time.ms()` - Returns current system time in milliseconds.
* `time.sleep(ms)` - Suspends execution for specified milliseconds.

#### `str` (String Utilities)
* `str.len(s)` - Returns length of string.
* `str.sub(s, start, len)` - Returns substring.
* `str.find(s, subStr)` - Finds position of substring.
* `str.lower(s)` / `str.upper(s)` - Alters casing.
* `str.split(s, delimiter)` - Splits string by delimiter, returning `List<Str>`.

#### `net` (TCP Networking)
* `net.Socket` - Cross-platform TCP client socket (BSD Sockets on Unix, Winsock on Windows).
  * `connect(host, port)` - Establishes connection.
  * `send(data)` - Sends string messages.
  * `recv()` - Receives string messages.
  * `close()` - Closes socket.

#### `thread` (Threading & Pool)
* `thread.run(func)` - Spawns a background system thread running the given function reference.
* `getGlobalThreadPool()` - Enqueues asynchronous calls (`!$func`) into a lightweight, high-performance thread pool matching system CPU concurrency.

#### `webviss` (Web & Secure VTS Protocol)
* `webviss.protocol(proto)` - Registers protocols. Passing `"vts"` enables the Viss Tunnel Security protocol.
* `webviss.fetch(url)` - Fetches response data from `http://`, `https://`, or `vts://` URLs.
  * **Viss Tunnel Security (VTS)**: Automatically negotiates ephemeral ECDH keys for perfect forward secrecy, uses dynamic ratcheting symmetric encryption, and appends randomized noise padding to deceive Deep Packet Inspection (DPI) censorship firewalls.
* `webviss.startGateway(port)` - Starts a local VTS-to-HTTP gateway proxy to browse `vts://` addresses inside standard web browsers.
* `webviss.registerProtocol()` - Registers `vts://` URI handler in Windows Registry.

---

## 4. Custom Modules (VCM) & Package Management (VPM)

### Viss Custom Modules (VCM)
When importing a custom `.viss` file:
```viss
&vcm import mymodule as @my for *
```
The compiler recursively compiles `mymodule.viss` to `mymodule.hpp`, wrapping its methods in `namespace my`, and includes it.

### Viss Package Manager (VPM)
The package manager [vpm.py](file:///C:/Users/halva/Desktop/Viss/vpm.py) manages external dependencies.

#### Creating a VPM Package Registry:
To publish packages for Viss:
1. Create a public Git repository.
2. Add a `packages.json` file at the root:
   ```json
   {
       "packages": {
           "mymodule": {
               "file": "mymodule.viss",
               "version": "1.2.0",
               "description": "Helper functions for math conversions",
               "dependencies": ["anotherpkg"],
               "sha256": "abcdef123456..."
           }
       }
   }
   ```
3. Put the source `.viss` files in the repository.

#### Using VPM:
```bash
# Connect to your GitHub registry
python vpm.py connect https://github.com/username/viss-packages.git

# Update packages list
python vpm.py update

# Install all packages from connected repositories (validates dependencies and SHA-256)
python vpm.py install -a -y

# Uninstall a package
python vpm.py uninstall mymodule -f
```

---

## 5. Developer & Contribution Guide

Viss is fully open-source. Here is how you can expand the language:

### Adding New Syntax Translations:
Line translations are managed inside `transpileLine` in [src/vissc.cpp](file:///C:/Users/halva/Desktop/Viss/src/vissc.cpp).
For example, to compile a new flow block keyword (e.g., `!unless(x)`):
1. Locate the block definitions inside `transpileLine` in `src/vissc.cpp`.
2. Add a `std::regex_replace` rule:
   ```cpp
   line = std::regex_replace(line, std::regex(R"(!unless\s*\(([^)]+)\))"), "if (!($1))");
   ```
3. Recompile the compiler:
   ```bash
   g++ -std=c++17 src/vissc.cpp -o vissc.exe
   ```
4. Add the keyword to the VS Code TextMate grammar file: `viss.tmLanguage.json`.
