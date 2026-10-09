// =============================================================================
// Viss Language Support & Toolchain Extension (v0.3.1)
// Official VS Code Extension for the Viss Programming Language
// =============================================================================
const vscode = require('vscode');
const path = require('path');
const fs = require('fs');
const { exec } = require('child_process');

function activate(context) {
    console.log('[Viss] Pro Language Extension v0.3.1 activated cleanly! ^_^');

    const diagnosticCollection = vscode.languages.createDiagnosticCollection('viss');
    context.subscriptions.push(diagnosticCollection);

    // ==========================================
    // 0. Compiler Executable Resolver
    // ==========================================
    function resolveVissExecutable(doc) {
        const configPath = vscode.workspace.getConfiguration('viss').get('executablePath');
        if (configPath && configPath !== 'viss') {
            return configPath;
        }

        // 1. Check workspace folder root
        if (doc && vscode.workspace.getWorkspaceFolder(doc.uri)) {
            const wsDir = vscode.workspace.getWorkspaceFolder(doc.uri).uri.fsPath;
            const candidates = [
                path.join(wsDir, 'viss.exe'),
                path.join(wsDir, 'bin', 'viss.exe'),
                path.join(wsDir, 'viss')
            ];
            for (const c of candidates) {
                if (fs.existsSync(c)) return c;
            }
        }

        // 2. Helper toolchain path
        const helperPath = 'C:\\AGY\\Viss_Helper\\bin\\viss.exe';
        if (fs.existsSync(helperPath)) return helperPath;

        // 3. User local programs install
        if (process.env.LOCALAPPDATA) {
            const localProg = path.join(process.env.LOCALAPPDATA, 'Programs', 'Viss', 'viss.exe');
            if (fs.existsSync(localProg)) return localProg;
        }

        // 4. Default repository path
        const repoPath = 'C:\\Users\\halva\\Desktop\\Viss\\viss.exe';
        if (fs.existsSync(repoPath)) return repoPath;

        // 5. Standard Linux/Unix paths
        for (const lp of ['/usr/local/bin/viss', '/usr/bin/viss']) {
            if (fs.existsSync(lp)) return lp;
        }

        // 6. Fallback to system PATH
        return 'viss';
    }

    // ==========================================
    // 1. Real Diagnostics / Linter (viss check)
    // ==========================================
    let diagTimeout = null;

    function runDiagnostics(document) {
        if (!document || (document.languageId !== 'viss' && !document.fileName.endsWith('.viss'))) {
            return;
        }

        if (document.isUntitled) return;

        const filePath = document.fileName;
        const vissExe = resolveVissExecutable(document);
        const cmd = `"${vissExe}" check "${filePath}"`;

        exec(cmd, (error, stdout, stderr) => {
            const combinedOutput = (stdout || '') + '\n' + (stderr || '');

            // Clean syntax check verification
            if (combinedOutput.includes('syntax verified cleanly') && !combinedOutput.includes('[Viss')) {
                diagnosticCollection.set(document.uri, []);
                return;
            }

            const vsDiags = [];
            // Parse Viss errors: [Viss Kind] file:line:col or [Viss Kind] file:line
            const errRegex = /\[Viss\s+([A-Za-z]+(?:Error|Warning)?)\]\s+([^:\r\n]+):(\d+)(?::(\d+))?[\r\n]+(?:[ \t]*(.*)[\r\n]+)?(?:[ \t]*(.*))?/g;

            let m;
            while ((m = errRegex.exec(combinedOutput)) !== null) {
                const kind = m[1];
                const lineNum = Math.max(0, parseInt(m[3], 10) - 1);
                const colNum = m[4] ? Math.max(0, parseInt(m[4], 10) - 1) : 0;
                const snippet = m[5] ? m[5].trim() : '';
                const detail = m[6] ? m[6].trim() : snippet;
                const message = `[Viss ${kind}] ${detail || snippet || 'Syntax error'}`;

                const lineText = document.lineCount > lineNum ? document.lineAt(lineNum).text : '';
                const startChar = Math.min(colNum, lineText.length);
                const endChar = lineText.length > startChar ? lineText.length : startChar + 4;

                const range = new vscode.Range(lineNum, startChar, lineNum, endChar);
                const diag = new vscode.Diagnostic(range, message, vscode.DiagnosticSeverity.Error);
                diag.source = 'viss';
                vsDiags.push(diag);
            }

            diagnosticCollection.set(document.uri, vsDiags);
        });
    }

    function scheduleDiagnostics(document) {
        if (diagTimeout) clearTimeout(diagTimeout);
        diagTimeout = setTimeout(() => runDiagnostics(document), 400);
    }

    if (vscode.window.activeTextEditor) {
        runDiagnostics(vscode.window.activeTextEditor.document);
    }

    context.subscriptions.push(
        vscode.workspace.onDidSaveTextDocument(doc => {
            if (vscode.workspace.getConfiguration('viss').get('checkOnSave')) {
                runDiagnostics(doc);
            }
        }),
        vscode.workspace.onDidChangeTextDocument(e => scheduleDiagnostics(e.document)),
        vscode.workspace.onDidOpenTextDocument(doc => runDiagnostics(doc)),
        vscode.workspace.onDidCloseTextDocument(doc => diagnosticCollection.delete(doc.uri))
    );

    // ==========================================
    // 2. Commands: Run (F5), Build (Shift+F5), Check (Ctrl+F5)
    // ==========================================
    function getVissTerminal(doc) {
        let terminal = vscode.window.terminals.find(t => t.name === 'Viss Console');
        if (!terminal) {
            let cwd = undefined;
            if (doc && vscode.workspace.getWorkspaceFolder(doc.uri)) {
                cwd = vscode.workspace.getWorkspaceFolder(doc.uri).uri.fsPath;
            } else if (doc) {
                cwd = path.dirname(doc.fileName);
            }
            terminal = vscode.window.createTerminal({ name: 'Viss Console', cwd });
        }
        return terminal;
    }

    let runCommand = vscode.commands.registerCommand('viss.run', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) return;
        await editor.document.save();

        const filePath = editor.document.fileName;
        const vissExe = resolveVissExecutable(editor.document);
        const terminal = getVissTerminal(editor.document);
        terminal.show();

        // Run directly with Viss runtime
        terminal.sendText(`& "${vissExe}" run "${filePath}"`);
    });

    let compileCommand = vscode.commands.registerCommand('viss.compile', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) return;
        await editor.document.save();

        const filePath = editor.document.fileName;
        const vissExe = resolveVissExecutable(editor.document);
        const terminal = getVissTerminal(editor.document);
        terminal.show();

        const baseName = path.basename(filePath, path.extname(filePath));
        const outExe = path.join(path.dirname(filePath), `${baseName}.exe`);
        terminal.sendText(`& "${vissExe}" build "${filePath}" -o "${outExe}"`);
    });

    let bundleCommand = vscode.commands.registerCommand('viss.bundle', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) return;
        await editor.document.save();

        const filePath = editor.document.fileName;
        const vissExe = resolveVissExecutable(editor.document);
        const terminal = getVissTerminal(editor.document);
        terminal.show();

        const baseName = path.basename(filePath, path.extname(filePath));
        const outExe = path.join(path.dirname(filePath), `${baseName}.exe`);
        terminal.sendText(`& "${vissExe}" bundle "${filePath}" -o "${outExe}"`);
    });

    let newFileCommand = vscode.commands.registerCommand('viss.newFile', async () => {
        const doc = await vscode.workspace.openTextDocument({
            language: 'viss',
            content: `$import lib "io" as io\n\n!main {\n    io.println("Hello from Viss!");\n}\n`
        });
        await vscode.window.showTextDocument(doc);
    });

    context.subscriptions.push(runCommand, compileCommand, bundleCommand, checkCommand, newFileCommand);

    // ==========================================
    // 3. Status Bar Action Buttons
    // ==========================================
    const runStatusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
    runStatusBarItem.command = 'viss.run';
    runStatusBarItem.text = '$(play) Run Viss';
    runStatusBarItem.tooltip = 'Run current Viss file (F5)';

    const versionStatusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 99);
    versionStatusBarItem.text = 'Viss v0.2.2';
    versionStatusBarItem.tooltip = 'Viss Language Engine (Prismo & Cosmic Owl)';

    function updateStatusBar() {
        const editor = vscode.window.activeTextEditor;
        if (editor && (editor.document.languageId === 'viss' || editor.document.fileName.endsWith('.viss'))) {
            runStatusBarItem.show();
            versionStatusBarItem.show();
        } else {
            runStatusBarItem.hide();
            versionStatusBarItem.hide();
        }
    }

    vscode.window.onDidChangeActiveTextEditor(updateStatusBar, null, context.subscriptions);
    updateStatusBar();
    context.subscriptions.push(runStatusBarItem, versionStatusBarItem);

    // ==========================================
    // 4. Comprehensive Hover Documentation
    // ==========================================
    const hoverDocs = {
        // Sigils & Syntax
        '@': '**@ Sigil (State & Data Identifiers)**\n\nPrefix for variables, function arguments, and instance self-references (`@me`).\n\n```viss\n@player_name = "Mario";\n@score = 100 | int;\n(@x, @y) = (10, 20);\n```',
        '@me': '**@me: Instance Self-Reference**\n\nPoints to current class/struct fields and methods (corresponds to `this` in C++/Java).\n\n```viss\n!func set_speed(@s) {\n    @me.speed = @s;\n}\n```',
        '&': '**& Sigil (Low-Level Hardware Buffers)**\n\nHigh-performance contiguous binary buffers for bytes and bitfields.\n\n```viss\n&grid create | bytes, 1024;\n&flags create | bits, 8192;\n&grid[0] = 255;\n```',
        '!': '**! Sigil (Action & Structure Directives)**\n\nDeclares definitions and control structures:\n- `!func name(params) { ... }`\n- `!async func name(params)`  -  Asynchronous task\n- `!main`  -  Main application entry point\n- `!class Name { ... }`, `!struct Name { ... }`\n- `!for @i in 0..10 { ... }`\n- `!while !@game_over { ... }`\n- `!return val;`, `!break;`, `!continue;`, `!defer`',
        '?': '**? Sigil (Logic & Control Flow)**\n\nBranching and exception constructs:\n- `?if (cond) { ... }`\n- `?elif (cond) { ... }`\n- `?else { ... }`\n- `?match @val { case X { ... } else { ... } }`\n- `?try { ... } ?expect Exception as @e { ... }`\n- `?error "message";`',
        '|': '**| Pipe Operator (Type Annotation & Modifiers)**\n\nExplicitly binds types and attributes to declarations:\n```viss\n@score = 0 | int;\n@items = [1, 2, 3] | list<int>;\n@name = "Mario" | str, const;\n```',
        '..': '**.. Range Operator**\n\nNumeric interval `[start..end)` for loops and collection slicing:\n```viss\n!for @i in 0..10 { ... }\n@sub = @str[1..4];\n@tail = @list[2..];\n```',
        '?:': '**?: Elvis Operator**\n\nFallback null-coalescing operator:\n```viss\n@val = @input ?: "default";\n```',

        // Standard Modules
        'rt': '**retrotech (rt): 2D Arcade Engine & 8-Bit Audio**\n\n- `rt.init_screen(w, h)`  -  Initialize pixel buffer\n- `rt.update_screen()`  -  Flush frame to screen\n- `rt.clear_screen(color)`  -  Clear video buffer\n- `rt.draw_pixel(x, y, color)`  -  Draw 1 pixel\n- `rt.draw_rect(x, y, w, h, color)`  -  Draw rectangle\n- `rt.draw_text(x, y, text, fg, bg)`  -  Render text HUD\n- `rt.has_key()`, `rt.get_key()`  -  Keyboard input\n- `rt.sound_coin()`, `rt.sound_bump()`, `rt.sound_game_over()`, `rt.sound_powerup()`',
        'retrotech': '**retrotech (rt): 2D Arcade Engine & 8-Bit Audio**\n\nSee `rt`.',
        'io': '**io: Formatted Console I/O**\n\n- `io.println(...)`  -  Variadic print with newline\n- `io.print(...)`  -  Print without newline\n- `io.readln()`  -  Read line from stdin\n- `io.read_char()`  -  Read single unbuffered keypress\n- `io.has_key()`  -  Non-blocking check for pending key\n- `io.get_key()`  -  Get pending key\n- `io.color(code)`  -  Terminal text color\n- `io.clear()`  -  Clear terminal screen',
        'str': '**str: String Manipulation**\n\n- `str.len(s)` / `str.size(s)`  -  Length of string\n- `str.upper(s)` / `str.lower(s)`  -  Case conversions\n- `str.trim(s)`  -  Strip surrounding whitespace\n- `str.split(s, sep)` -> `list<str>`\n- `str.join(list, sep)` -> `str`\n- `str.replace(s, old, new)`\n- `str.contains(s, sub)`\n- `str.starts_with(s, prefix)` / `str.ends_with(s, suffix)`\n- `str.from_int(v)` / `str.to_int(s)`',
        'sys': '**sys: System & Runtime Utilities**\n\n- `sys.random(min, max)`  -  Thread-safe random integer in `[min..max)`\n- `sys.clock()`  -  Current CPU clock\n- `sys.ticks()`  -  High-precision timer ticks\n- `sys.exit(code)`  -  Terminate program\n- `sys.cpu_count()`  -  Logical CPU cores\n- `sys.os_name()`  -  Operating system identifier',
        'fs': '**fs: File System Manipulation**\n\n- `fs.read(path)` / `fs.write(path, text)`\n- `fs.exists(path)` / `fs.is_file(path)` / `fs.is_dir(path)`\n- `fs.mkdir(path)` / `fs.mkdir_p(path)`\n- `fs.remove(path)` / `fs.copy(src, dst)` / `fs.move(src, dst)`\n- `fs.list(dir)` / `fs.list_recursive(dir)`\n- `fs.glob(pattern)` -> `list<str>`  -  Filesystem pattern match\n- `fs.size(path)` / `fs.filename(path)` / `fs.extension(path)`',
        'json': '**json: Fast JSON Parser & Serializer**\n\n- `json.loads(str)` -> `var` / `dict` / `list`\n- `json.dumps(val)` -> `str` (JSON string)\n- `json.parse(str)`, `json.stringify(val)`, `json.pretty(val)`',
        'time': '**time: High-Precision Timers**\n\n- `time.sleep(ms)`  -  Sleep milliseconds\n- `time.now_ms()`  -  Epoch in milliseconds\n- `time.now_us()`  -  Epoch in microseconds\n- `time.format_now(fmt)`  -  Formatted date-time\n- `time.Stopwatch(autostart)`  -  Performance stopwatch',
        'math': '**math: Mathematical Functions & Game Physics**\n\n- `math.PI`, `math.E`\n- `math.sin`, `math.cos`, `math.tan`, `math.sqrt`, `math.abs`, `math.pow`\n- `math.clamp(val, min, max)`, `math.lerp(a, b, t)`\n- `math.distance(x1, y1, x2, y2)`\n- `math.random_int(min, max)`, `math.random_dec(min, max)`',
        'async': '**async: Concurrency & Multithreading**\n\n- `!async func name(params)`  -  Asynchronous function definition\n- `!await @task`  -  Await task completion\n- `async.spawn(func)` -> `Task<T>`  -  Background execution\n- `async.sleep(ms)`  -  Non-blocking async sleep\n- `async.parallel_for(start, end, func)`  -  Multi-core parallel loop',
        'gui': '**gui: Native Windows Hardware-Accelerated GUI**\n\n- `gui.create_window(title, w, h)`  -  Spawn GPU/GDI+ window\n- `gui.button(x, y, w, h, label)`  -  Clickable button\n- `gui.label(x, y, text, size)`  -  Text label\n- `gui.checkbox(x, y, text, &checked)`  -  Toggle checkbox\n- `gui.slider(x, y, w, h, &val, min, max)`  -  Interactive slider\n- `gui.text_input(x, y, w, h, id, &text)`  -  Text input field\n- `gui.textarea(x, y, w, h, id, &text)`  -  Multi-line editor\n- `gui.progress_bar(x, y, w, h, pct)`  -  Visual progress bar\n- `gui.card(x, y, w, h)`, `gui.card_group(x, y, w, h, title)`\n- `gui.open_file()`, `gui.browse_folder()`  -  System file dialogs\n- `gui.draw_rect()`, `gui.draw_circle()`, `gui.draw_line()`, `gui.draw_image()`\n- `gui.poll()`, `gui.run()`, `gui.is_open()`',
        'audio': '**audio: High-Performance Sound & Music Engine**\n\n- `audio.play_tone(freq, ms, wave, vol)`  -  Synthesize tone\n- `audio.play_note(note, ms, wave, vol)`  -  Play musical note (e.g. "C4", "A#5")\n- `audio.synth(freq, ms, wave, a, d, s, r, vol)`  -  Full ADSR synthesizer\n- `audio.sfx(name)`  -  Play SFX ("laser", "hit", "explosion", "jump", "powerup", "gem", "dash")\n- `audio.laser()`, `audio.hit()`, `audio.jump()`, `audio.explosion()`\n- `audio.play_bgm(file, volume)` / `audio.stop_bgm()`  -  Background music streaming\n- `audio.play_file(path)` / `audio.stop_file()`  -  Play sound sample\n- `audio.set_volume(vol)` / `audio.get_volume()` / `audio.stop_all()`',
        'media': '**media: Audio Metadata, Cover Art & Video Rendering Engine**\n\n- `media.read(audio_path)` -> `AudioTag`  -  Parse Vorbis/ID3/MP4 metadata & cover\n- `media.write_tags(audio_path, tags_map)`  -  Write audio tags atomically\n- `media.embed_cover(audio_path, img_path, tags, out_path)`  -  Embed album cover\n- `media.extract_cover(audio_path, out_img)`  -  Extract attached cover\n- `media.render_video(audio, cover, out_mp4)`  -  FFmpeg video generator with blur & audio\n- `media.start_render_video(...)` / `media.is_rendering_video()`  -  Async render\n- `media.get_render_video_progress()` / `media.cancel_render_video()`',
        'crypto': '**crypto: Cryptographic Hashes & Encodings**\n\n- `crypto.sha256(text)` -> `str` (64 lowercase hex)\n- `crypto.md5(text)` -> `str` (32 lowercase hex)\n- `crypto.crc32(text)` -> `str`\n- `crypto.uuid()` -> `str` (Random UUID v4)\n- `crypto.base64_encode(text)` -> `str`\n- `crypto.base64_decode(text)` -> `str`',
        'net': '**net: Networking & HTTP**\n\n- `net.http_get(url)` -> `str`  -  HTTP/HTTPS GET request\n- `net.download_file(url, path)`  -  Stream download\n- `net.url_encode(text)` / `net.url_decode(text)`\n- `net.TcpClient()`  -  Raw TCP socket stream',
        'env': '**env: Environment & Process Execution**\n\n- `env.args()` -> `list<str>`  -  CLI arguments\n- `env.cwd()` / `env.set_cwd(path)`  -  Current directory\n- `env.os()` / `env.arch()` / `env.cpu_count()`  -  Platform info\n- `env.exec(cmd)` -> `int`  -  Shell command execution\n- `env.get(var)` / `env.set(var, val)` / `env.exit(code)`',
        'collections': '**collections: Extended Data Structures & Algorithms**\n\n- `collections.zip(a, b)` -> `list<list>`\n- `collections.enumerate(list)` -> `list<list>`\n- `collections.choice(list)` -> element\n- `collections.shuffle(list)` -> `list`\n- `collections.Stack()`, `collections.Queue()`, `collections.Set()`',

        // Primitive Types
        'int': '**int: 64-bit Signed Integer** (`viss::Int`)',
        'str': '**str: UTF-8 String** (`viss::Str`)',
        'dec': '**dec: 64-bit Floating-Point Number** (`viss::Dec`)',
        'bool': '**bool: Boolean flag** (`true` / `false`)',
        'list': '**list<T>: Dynamic Thread-Safe Array**\n\n- `@list = [1, 2, 3]`\n- `.size()`, `.append(item)`, `.pop()`, `.insert(i, item)`, `.remove(item)`, `.clear()`, `.slice(start, len)`',
        'map': '**map<K, V>: Dynamic Hash Map / Dictionary**\n\n- `@map = {"key": "val"}`\n- `.get(key)`, `.has(key)`, `.keys()`, `.values()`, `.remove(key)`',
        'bytes': '**bytes: High-Performance Binary Memory Buffer**\n\n- `&buf create | bytes, 1024;`\n- Methods: `.size()`, `&buf[i]`, `.set_bit()`, `.get_bit()`, `.fill()`, `.to_hex()`',
        'bits': '**bits: High-Density Bitfield Array**\n\n- `&flags create | bits, 8192;`\n- Methods: `.get(i)`, `.set(i, val)`, `.count_ones()`, `.invert()`',
        'mask': '**mask (bytemask, colormask): Hardware Byte-Mask & Palette Engine**\n\n- `mask.create(size)` / `mask.create_rgb(colors)` / `mask.create_2d(w, h)`\n- `mask.preset(m, name)`  -  Built-in color palettes ("tetris", "gameboy", "pico8", "nes", "fire", "neon", "matrix", etc.)\n- `mask.set_rgb(m, id, r, g, b)`, `mask.set_hsv(m, id, h, s, v)`\n- `mask.gradient(m, s, e, r1, g1, b1, r2, g2, b2)`\n- `mask.apply_stencil(dest, src, stencil)`\n- `mask.blend(dest, a, b, alpha)`\n- `mask.collides_2d(a, ax, ay, aw, ah, b, bx, by, bw, bh)`\n- `mask.threshold(src, thresh)`',
        'bytemask': '**bytemask: Byte-Mask Buffer**\n\nContiguous 1D/2D byte-oriented mask buffer for palettes, stencils, and collision masks.\n\n```viss\n&mask create | bytemask, 16;\n&mask.preset("neon");\n&mask.apply();\n```',
        'colormask': '**colormask: RGB Palette Buffer**\n\nHardware color palette buffer (3 bytes per color: R, G, B).\n\n```viss\n&pal create | colormask, 16;\n&pal.preset("tetris");\n&pal.apply();\n```',
        'preset': '**preset(name): Built-in Color Mask Presets**\n\nAvailable presets: `"tetris"`, `"gameboy"`, `"pico8"`, `"nes"`, `"fire"`, `"cyberpunk"`, `"monochrome"`, `"neon"`, `"c64"`, `"matrix"`, `"lava"`, `"pastel"`.'
    };

    const hoverProvider = vscode.languages.registerHoverProvider('viss', {
        provideHover(document, position) {
            const range = document.getWordRangeAtPosition(position, /[@&!?$]?[a-zA-Z0-9_]+|\.\.|\||\?:/);
            if (!range) return null;
            const text = document.getText(range);

            if (hoverDocs[text]) {
                return new vscode.Hover(new vscode.MarkdownString(hoverDocs[text]));
            }
            if (text.startsWith('&') && hoverDocs['&']) {
                return new vscode.Hover(new vscode.MarkdownString(hoverDocs['&']));
            }
            if (text.startsWith('@') && hoverDocs['@']) {
                return new vscode.Hover(new vscode.MarkdownString(hoverDocs['@']));
            }
            return null;
        }
    });

    // ==========================================
    // 5. Intelligent Context-Aware Completion Provider
    // ==========================================
    const moduleMethods = {
        'rt': [
            { label: 'init_screen', detail: 'rt.init_screen(w, h): Initialize retro screen buffer', snippet: 'init_screen(${1:32}, ${2:24});' },
            { label: 'update_screen', detail: 'rt.update_screen(): Render frame to terminal/window', snippet: 'update_screen();' },
            { label: 'clear_screen', detail: 'rt.clear_screen(color): Clear screen buffer with color', snippet: 'clear_screen(${1:0});' },
            { label: 'draw_pixel', detail: 'rt.draw_pixel(x, y, color): Draw single colored pixel', snippet: 'draw_pixel(${1:x}, ${2:y}, ${3:color});' },
            { label: 'draw_rect', detail: 'rt.draw_rect(x, y, w, h, color): Draw hollow rectangle outline', snippet: 'draw_rect(${1:x}, ${2:y}, ${3:w}, ${4:h}, ${5:color});' },
            { label: 'draw_rect_fill', detail: 'rt.draw_rect_fill(x, y, w, h, color): Draw filled solid rectangle', snippet: 'draw_rect_fill(${1:x}, ${2:y}, ${3:w}, ${4:h}, ${5:color});' },
            { label: 'draw_text', detail: 'rt.draw_text(x, y, text, fg, bg): Render HUD text label', snippet: 'draw_text(${1:x}, ${2:y}, "${3:text}", ${4:15}, ${5:0});' },
            { label: 'open_window', detail: 'rt.open_window(title, scale): Open native graphical window', snippet: 'open_window("${1:Viss Game}", ${2:4});' },
            { label: 'is_window_open', detail: 'rt.is_window_open(): Check if graphical window is running', snippet: 'is_window_open()' },
            { label: 'has_key', detail: 'rt.has_key(): Check if a keypress is pending', snippet: 'has_key()' },
            { label: 'get_key', detail: 'rt.get_key(): Retrieve pressed key ("UP", "DOWN", "a", etc.)', snippet: 'get_key()' },
            { label: 'is_down', detail: 'rt.is_down(key): Test if key is currently held down', snippet: 'is_down("${1:SPACE}")' },
            { label: 'sound_jump', detail: 'rt.sound_jump(): Play retro jump sound effect', snippet: 'sound_jump();' },
            { label: 'sound_coin', detail: 'rt.sound_coin(): Play retro coin collection chime', snippet: 'sound_coin();' },
            { label: 'sound_stomp', detail: 'rt.sound_stomp(): Play enemy stomp sound', snippet: 'sound_stomp();' },
            { label: 'sound_powerup', detail: 'rt.sound_powerup(): Play powerup melody sound', snippet: 'sound_powerup();' },
            { label: 'sound_hurt', detail: 'rt.sound_hurt(): Play damage impact sound', snippet: 'sound_hurt();' },
            { label: 'sound_bump', detail: 'rt.sound_bump(): Play solid wall bump sound', snippet: 'sound_bump();' },
            { label: 'sound_flag', detail: 'rt.sound_flag(): Play level completion fanfare', snippet: 'sound_flag();' },
            { label: 'sound_game_over', detail: 'rt.sound_game_over(): Play game over sound', snippet: 'sound_game_over();' },
            { label: 'Tone', detail: 'rt.Tone(freq, ms, wave, duty, vol): Synthesize 8-bit sound', snippet: 'Tone(${1:440}, ${2:100}, "${3:square}");' },
            { label: 'Sweep', detail: 'rt.Sweep(start_f, end_f, ms, wave): Play frequency sweep', snippet: 'Sweep(${1:160}, ${2:600}, ${3:120}, "square");' },
            { label: 'PlayMusic', detail: 'rt.PlayMusic(melody, bpm): Play background chiptune track', snippet: 'PlayMusic("${1:E5:8 E5:8 . C5:8}", ${2:120});' },
            { label: 'StopMusic', detail: 'rt.StopMusic(): Stop background music', snippet: 'StopMusic();' },
            { label: 'mask', detail: 'rt.mask: Colormask & palette engine namespace', snippet: 'mask.' },
            { label: 'SetColorMask', detail: 'rt.SetColorMask(&mask): Apply RGB colormask palette to screen', snippet: 'SetColorMask(${1:&mask});' },
            { label: 'ColorScreen', detail: 'rt.ColorScreen(&mask): Set screen colormask and redraw', snippet: 'ColorScreen(${1:&mask});' },
            { label: 'sprite', detail: 'rt.sprite: Hardware sprite engine namespace', snippet: 'sprite.' },
            { label: 'screen', detail: 'rt.screen: Hardware screen buffer namespace', snippet: 'screen.' }
        ],
        'retrotech': null, // Aliased to rt
        'io': [
            { label: 'println', detail: 'io.println(...): Print variadic items with newline', snippet: 'println(${1:msg});' },
            { label: 'print', detail: 'io.print(...): Print variadic items without newline', snippet: 'print(${1:msg});' },
            { label: 'readln', detail: 'io.readln(): Read a full line from stdin', snippet: 'readln()' },
            { label: 'read_char', detail: 'io.read_char(): Read single raw keypress instantly', snippet: 'read_char()' },
            { label: 'has_key', detail: 'io.has_key(): Check if a key was pressed', snippet: 'has_key()' },
            { label: 'get_key', detail: 'io.get_key(): Retrieve pressed key string', snippet: 'get_key()' },
            { label: 'is_down', detail: 'io.is_down(key): Test if key is currently pressed', snippet: 'is_down("${1:SPACE}")' },
            { label: 'color', detail: 'io.color(code): Set terminal text color (0-15)', snippet: 'color(${1:14});' },
            { label: 'clear', detail: 'io.clear(): Clear terminal console screen', snippet: 'clear();' },
            { label: 'cursor', detail: 'io.cursor(x, y): Position terminal cursor', snippet: 'cursor(${1:0}, ${2:0});' },
            { label: 'cursor_hide', detail: 'io.cursor_hide(): Hide console cursor', snippet: 'cursor_hide();' },
            { label: 'cursor_show', detail: 'io.cursor_show(): Show console cursor', snippet: 'cursor_show();' },
            { label: 'cursor_visible', detail: 'io.cursor_visible(visible): Set console cursor visibility', snippet: 'cursor_visible(${1:false});' },
            { label: 'title', detail: 'io.title(text): Set console window title', snippet: 'title("${1:title}");' },
            { label: 'term_width', detail: 'io.term_width(): Get console window width', snippet: 'term_width()' },
            { label: 'term_height', detail: 'io.term_height(): Get console window height', snippet: 'term_height()' },
            { label: 'term_size', detail: 'io.term_size(): Get [width, height] of console window', snippet: 'term_size()' },
            { label: 'eprint', detail: 'io.eprint(msg): Print to stderr without newline', snippet: 'eprint(${1:msg});' },
            { label: 'eprintln', detail: 'io.eprintln(msg): Print to stderr with newline', snippet: 'eprintln(${1:msg});' },
            { label: 'read_file', detail: 'io.read_file(path): Read entire text file', snippet: 'read_file("${1:path.txt}")' },
            { label: 'write_file', detail: 'io.write_file(path, text): Write string to text file', snippet: 'write_file("${1:path.txt}", ${2:content});' }
        ],
        'str': [
            { label: 'len', detail: 'str.len(s): Length of string', snippet: 'len(${1:s})' },
            { label: 'size', detail: 'str.size(s): Length of string', snippet: 'size(${1:s})' },
            { label: 'upper', detail: 'str.upper(s): Convert string to uppercase', snippet: 'upper(${1:s})' },
            { label: 'lower', detail: 'str.lower(s): Convert string to lowercase', snippet: 'lower(${1:s})' },
            { label: 'capitalize', detail: 'str.capitalize(s): Capitalize first letter and lowercase rest', snippet: 'capitalize(${1:s})' },
            { label: 'trim', detail: 'str.trim(s): Strip surrounding whitespace', snippet: 'trim(${1:s})' },
            { label: 'split', detail: 'str.split(s, sep): Split string into list<str>', snippet: 'split(${1:s}, "${2:,}")' },
            { label: 'lines', detail: 'str.lines(s): Split string by newlines into list<str>', snippet: 'lines(${1:s})' },
            { label: 'join', detail: 'str.join(list, sep): Join list into string', snippet: 'join(${1:list}, "${2:, }")' },
            { label: 'replace', detail: 'str.replace(s, old, new): Replace occurrences', snippet: 'replace(${1:s}, "${2:from}", "${3:to}")' },
            { label: 'contains', detail: 'str.contains(s, sub): Check substring existence', snippet: 'contains(${1:s}, "${2:sub}")' },
            { label: 'count', detail: 'str.count(s, sub): Count non-overlapping occurrences', snippet: 'count(${1:s}, "${2:sub}")' },
            { label: 'find', detail: 'str.find(s, sub): First index of substring (-1 if not found)', snippet: 'find(${1:s}, "${2:sub}")' },
            { label: 'rfind', detail: 'str.rfind(s, sub): Last index of substring (-1 if not found)', snippet: 'rfind(${1:s}, "${2:sub}")' },
            { label: 'starts_with', detail: 'str.starts_with(s, prefix): Check prefix', snippet: 'starts_with(${1:s}, "${2:prefix}")' },
            { label: 'ends_with', detail: 'str.ends_with(s, suffix): Check suffix', snippet: 'ends_with(${1:s}, "${2:suffix}")' },
            { label: 'is_digit', detail: 'str.is_digit(s): Test if string consists solely of digits', snippet: 'is_digit(${1:s})' },
            { label: 'is_alpha', detail: 'str.is_alpha(s): Test if string consists solely of letters', snippet: 'is_alpha(${1:s})' },
            { label: 'is_alnum', detail: 'str.is_alnum(s): Test if string is alphanumeric', snippet: 'is_alnum(${1:s})' },
            { label: 'is_space', detail: 'str.is_space(s): Test if string is whitespace', snippet: 'is_space(${1:s})' },
            { label: 'sub', detail: 'str.sub(s, start, len): Substring by length', snippet: 'sub(${1:s}, ${2:0}, ${3:5})' },
            { label: 'substr', detail: 'str.substr(s, start, len): Substring by length', snippet: 'substr(${1:s}, ${2:0}, ${3:5})' },
            { label: 'pad_left', detail: 'str.pad_left(s, total_len, ch): Pad string left', snippet: 'pad_left(${1:s}, ${2:10})' },
            { label: 'pad_right', detail: 'str.pad_right(s, total_len, ch): Pad string right', snippet: 'pad_right(${1:s}, ${2:10})' },
            { label: 'repeat', detail: 'str.repeat(s, count): Repeat string n times', snippet: 'repeat(${1:s}, ${2:3})' },
            { label: 'to_int', detail: 'str.to_int(s): Parse string as integer', snippet: 'to_int(${1:s})' },
            { label: 'to_float', detail: 'str.to_float(s): Parse string as float', snippet: 'to_float(${1:s})' },
            { label: 'from_int', detail: 'str.from_int(v): Convert integer to string', snippet: 'from_int(${1:v})' }
        ],
        'sys': [
            { label: 'random', detail: 'sys.random(min, max): Random integer in [min..max)', snippet: 'random(${1:0}, ${2:10})' },
            { label: 'choice', detail: 'sys.choice(collection): Pick a random element from list or str', snippet: 'choice(${1:collection})' },
            { label: 'shuffle', detail: 'sys.shuffle(list): Return a shuffled copy of list', snippet: 'shuffle(${1:list})' },
            { label: 'clock', detail: 'sys.clock(): High-precision CPU clock cycles', snippet: 'clock()' },
            { label: 'ticks', detail: 'sys.ticks(): Epoch timestamp in milliseconds', snippet: 'ticks()' },
            { label: 'exit', detail: 'sys.exit(code): Terminate application', snippet: 'exit(${1:0});' },
            { label: 'time_ns', detail: 'sys.time_ns(): Epoch time in nanoseconds', snippet: 'time_ns()' },
            { label: 'time_ms', detail: 'sys.time_ms(): Epoch time in milliseconds', snippet: 'time_ms()' },
            { label: 'time_us', detail: 'sys.time_us(): Epoch time in microseconds', snippet: 'time_us()' },
            { label: 'date_string', detail: 'sys.date_string(): Formatted ISO datetime string', snippet: 'date_string()' },
            { label: 'cpu_count', detail: 'sys.cpu_count(): Logical CPU cores', snippet: 'cpu_count()' },
            { label: 'os_name', detail: 'sys.os_name(): OS name string ("windows", "linux", "darwin")', snippet: 'os_name()' }
        ],
        'fs': [
            { label: 'read', detail: 'fs.read(path): Read full text file into string', snippet: 'read("${1:path.txt}")' },
            { label: 'read_lines', detail: 'fs.read_lines(path): Read all lines into list<str>', snippet: 'read_lines("${1:path.txt}")' },
            { label: 'lines', detail: 'fs.lines(path): Read all lines into list<str>', snippet: 'lines("${1:path.txt}")' },
            { label: 'write', detail: 'fs.write(path, text): Write text to file', snippet: 'write("${1:path.txt}", ${2:content});' },
            { label: 'write_lines', detail: 'fs.write_lines(path, lines): Write list of strings separated by newlines', snippet: 'write_lines("${1:path.txt}", ${2:lines});' },
            { label: 'read_bytes', detail: 'fs.read_bytes(path): Read binary file into Bytes', snippet: 'read_bytes("${1:path.bin}")' },
            { label: 'write_bytes', detail: 'fs.write_bytes(path, &buf): Write Bytes buffer to file', snippet: 'write_bytes("${1:path.bin}", &${2:buf});' },
            { label: 'exists', detail: 'fs.exists(path): Check if path exists', snippet: 'exists("${1:path}")' },
            { label: 'is_file', detail: 'fs.is_file(path): Check if path is regular file', snippet: 'is_file("${1:path}")' },
            { label: 'is_dir', detail: 'fs.is_dir(path): Check if path is directory', snippet: 'is_dir("${1:path}")' },
            { label: 'mkdir', detail: 'fs.mkdir(path): Create directory', snippet: 'mkdir("${1:dir}");' },
            { label: 'mkdir_p', detail: 'fs.mkdir_p(path): Recursively create parent directories', snippet: 'mkdir_p("${1:path/to/dir}");' },
            { label: 'remove', detail: 'fs.remove(path): Delete file or directory', snippet: 'remove("${1:path}");' },
            { label: 'copy', detail: 'fs.copy(src, dst): Copy file', snippet: 'copy("${1:src}", "${2:dst}");' },
            { label: 'move', detail: 'fs.move(src, dst): Move/rename file', snippet: 'move("${1:src}", "${2:dst}");' },
            { label: 'list', detail: 'fs.list(dir): List filenames in directory', snippet: 'list("${1:dir}")' },
            { label: 'list_recursive', detail: 'fs.list_recursive(dir): Recursively list directory contents', snippet: 'list_recursive("${1:dir}")' },
            { label: 'glob', detail: 'fs.glob(pattern): Wildcard globbing (e.g. "*.viss")', snippet: 'glob("${1:*.viss}")' },
            { label: 'size', detail: 'fs.size(path): File size in bytes', snippet: 'size("${1:path}")' },
            { label: 'extension', detail: 'fs.extension(path): Extract file extension', snippet: 'extension("${1:path}")' },
            { label: 'filename', detail: 'fs.filename(path): Extract filename from path', snippet: 'filename("${1:path}")' }
        ],
        'json': [
            { label: 'loads', detail: 'json.loads(str): Parse JSON string into dynamic Viss structure', snippet: 'loads(${1:json_str})' },
            { label: 'dumps', detail: 'json.dumps(val): Serialize Viss value to JSON string', snippet: 'dumps(${1:val})' },
            { label: 'parse', detail: 'json.parse(str): Parse JSON string into JsonValue object', snippet: 'parse(${1:json_str})' },
            { label: 'stringify', detail: 'json.stringify(val): Compact JSON serialization', snippet: 'stringify(${1:val})' },
            { label: 'pretty', detail: 'json.pretty(val): Pretty-printed indented JSON string', snippet: 'pretty(${1:val})' }
        ],
        'time': [
            { label: 'sleep', detail: 'time.sleep(ms): Sleep for milliseconds', snippet: 'sleep(${1:100});' },
            { label: 'now_ms', detail: 'time.now_ms(): Current epoch timestamp in milliseconds', snippet: 'now_ms()' },
            { label: 'now_us', detail: 'time.now_us(): Current epoch timestamp in microseconds', snippet: 'now_us()' },
            { label: 'format_now', detail: 'time.format_now(fmt): Formatted date-time string', snippet: 'format_now("${1:%Y-%m-%d %H:%M:%S}")' },
            { label: 'Stopwatch', detail: 'time.Stopwatch(autostart): High-precision performance stopwatch', snippet: 'Stopwatch(true)' }
        ],
        'math': [
            { label: 'PI', detail: 'math.PI: 3.141592653589793', snippet: 'PI' },
            { label: 'E', detail: 'math.E: 2.718281828459045', snippet: 'E' },
            { label: 'sin', detail: 'math.sin(rad): Sine function', snippet: 'sin(${1:rad})' },
            { label: 'cos', detail: 'math.cos(rad): Cosine function', snippet: 'cos(${1:rad})' },
            { label: 'tan', detail: 'math.tan(rad): Tangent function', snippet: 'tan(${1:rad})' },
            { label: 'sqrt', detail: 'math.sqrt(x): Square root', snippet: 'sqrt(${1:x})' },
            { label: 'abs', detail: 'math.abs(x): Absolute value', snippet: 'abs(${1:x})' },
            { label: 'pow', detail: 'math.pow(base, exp): Exponentiation', snippet: 'pow(${1:base}, ${2:exp})' },
            { label: 'clamp', detail: 'math.clamp(val, min, max): Bound value between min and max', snippet: 'clamp(${1:val}, ${2:min}, ${3:max})' },
            { label: 'wrap', detail: 'math.wrap(val, min, max): Wrap value in toroidal range [min..max)', snippet: 'wrap(${1:val}, ${2:min}, ${3:max})' },
            { label: 'sign', detail: 'math.sign(val): Sign of number (-1, 0, or 1)', snippet: 'sign(${1:val})' },
            { label: 'hypot', detail: 'math.hypot(x, y): Euclidean hypotenuse sqrt(x*x + y*y)', snippet: 'hypot(${1:x}, ${2:y})' },
            { label: 'lerp', detail: 'math.lerp(a, b, t): Linear interpolation', snippet: 'lerp(${1:a}, ${2:b}, ${3:0.5})' },
            { label: 'distance', detail: 'math.distance(x1, y1, x2, y2): 2D Euclidean distance', snippet: 'distance(${1:x1}, ${2:y1}, ${3:x2}, ${4:y2})' },
            { label: 'noise2d', detail: 'math.noise2d(x, y): 2D Perlin noise value', snippet: 'noise2d(${1:x}, ${2:y})' },
            { label: 'random_int', detail: 'math.random_int(min, max): Random integer', snippet: 'random_int(${1:0}, ${2:100})' },
            { label: 'random_dec', detail: 'math.random_dec(min, max): Random decimal float', snippet: 'random_dec(${1:0.0}, ${2:1.0})' },
            { label: 'deg_to_rad', detail: 'math.deg_to_rad(deg): Degrees to radians', snippet: 'deg_to_rad(${1:deg})' },
            { label: 'rad_to_deg', detail: 'math.rad_to_deg(rad): Radians to degrees', snippet: 'rad_to_deg(${1:rad})' }
        ],
        'async': [
            { label: 'spawn', detail: 'async.spawn(func): Dispatch function to background thread pool', snippet: 'spawn(${1:func_name})' },
            { label: 'await', detail: 'async.await(task): Await Task completion', snippet: 'await(${1:task})' },
            { label: 'sleep', detail: 'async.sleep(ms): Asynchronous non-blocking sleep', snippet: 'sleep(${1:100});' },
            { label: 'delay', detail: 'async.delay(ms, func): Schedule function execution after delay', snippet: 'delay(${1:500}, ${2:func});' },
            { label: 'interval', detail: 'async.interval(ms, func): Recurring interval timer', snippet: 'interval(${1:1000}, ${2:func});' },
            { label: 'parallel_for', detail: 'async.parallel_for(start, end, func): Multi-core parallel for loop', snippet: 'parallel_for(${1:0}, ${2:count}, ${3:func});' }
        ],
        'crypto': [
            { label: 'sha256', detail: 'crypto.sha256(text): Compute SHA-256 hash', snippet: 'sha256(${1:text})' },
            { label: 'md5', detail: 'crypto.md5(text): Compute MD5 hash', snippet: 'md5(${1:text})' },
            { label: 'crc32', detail: 'crypto.crc32(text): Compute CRC-32 checksum', snippet: 'crc32(${1:text})' },
            { label: 'uuid', detail: 'crypto.uuid(): Generate random UUID v4', snippet: 'uuid()' },
            { label: 'base64_encode', detail: 'crypto.base64_encode(text): Base64 encode string', snippet: 'base64_encode(${1:text})' },
            { label: 'base64_decode', detail: 'crypto.base64_decode(text): Base64 decode string', snippet: 'base64_decode(${1:text})' }
        ],
        'net': [
            { label: 'http_get', detail: 'net.http_get(url): Make HTTP/HTTPS GET request', snippet: 'http_get("${1:https://api.github.com}")' },
            { label: 'download_file', detail: 'net.download_file(url, path): Download remote resource to local file', snippet: 'download_file("${1:url}", "${2:dest.bin}");' },
            { label: 'url_encode', detail: 'net.url_encode(text): URL component encoding', snippet: 'url_encode(${1:text})' },
            { label: 'url_decode', detail: 'net.url_decode(text): URL component decoding', snippet: 'url_decode(${1:text})' },
            { label: 'TcpClient', detail: 'net.TcpClient(): TCP socket connection handle', snippet: 'TcpClient()' }
        ],
        'env': [
            { label: 'args', detail: 'env.args(): Command-line arguments list', snippet: 'args()' },
            { label: 'cwd', detail: 'env.cwd(): Current working directory', snippet: 'cwd()' },
            { label: 'set_cwd', detail: 'env.set_cwd(path): Change working directory', snippet: 'set_cwd("${1:path}");' },
            { label: 'os', detail: 'env.os(): OS name ("windows", "linux", "darwin")', snippet: 'os()' },
            { label: 'arch', detail: 'env.arch(): CPU architecture ("x64", "arm64")', snippet: 'arch()' },
            { label: 'cpu_count', detail: 'env.cpu_count(): Logical core count', snippet: 'cpu_count()' },
            { label: 'exec', detail: 'env.exec(cmd): Run shell command and capture stdout', snippet: 'exec("${1:git status}")' },
            { label: 'get', detail: 'env.get(key): Read environment variable', snippet: 'get("${1:PATH}")' },
            { label: 'set', detail: 'env.set(key, val): Write environment variable', snippet: 'set("${1:KEY}", "${2:VAL}");' },
            { label: 'exit', detail: 'env.exit(code): Terminate process', snippet: 'exit(${1:0});' }
        ],
        'mask': [
            { label: 'create', detail: 'mask.create(size, fill): Allocate generic 1D byte-mask buffer', snippet: 'create(${1:48}, ${2:0})' },
            { label: 'create_rgb', detail: 'mask.create_rgb(num_colors): Allocate RGB palette buffer (colors * 3 bytes)', snippet: 'create_rgb(${1:16})' },
            { label: 'create_palette', detail: 'mask.create_palette(num_colors): Allocate RGB palette buffer', snippet: 'create_palette(${1:16})' },
            { label: 'create_2d', detail: 'mask.create_2d(w, h, fill): Allocate 2D spatial byte-mask grid', snippet: 'create_2d(${1:32}, ${2:32}, ${3:0})' },
            { label: 'set_rgb', detail: 'mask.set_rgb(m, id, r, g, b): Set palette RGB color at index', snippet: 'set_rgb(${1:&mask}, ${2:id}, ${3:r}, ${4:g}, ${5:b});' },
            { label: 'set', detail: 'mask.set(m, id, r, g, b): Alias for set_rgb', snippet: 'set(${1:&mask}, ${2:id}, ${3:r}, ${4:g}, ${5:b});' },
            { label: 'set_hsv', detail: 'mask.set_hsv(m, id, h, s, v): Set palette HSV color (h:0-360, s:0-1, v:0-1)', snippet: 'set_hsv(${1:&mask}, ${2:id}, ${3:h}, ${4:s}, ${5:v});' },
            { label: 'get_r', detail: 'mask.get_r(m, id): Red component of palette index', snippet: 'get_r(${1:&mask}, ${2:id})' },
            { label: 'get_g', detail: 'mask.get_g(m, id): Green component of palette index', snippet: 'get_g(${1:&mask}, ${2:id})' },
            { label: 'get_b', detail: 'mask.get_b(m, id): Blue component of palette index', snippet: 'get_b(${1:&mask}, ${2:id})' },
            { label: 'gradient', detail: 'mask.gradient(m, s, e, r1, g1, b1, r2, g2, b2): Linear RGB gradient', snippet: 'gradient(${1:&mask}, ${2:0}, ${3:15}, ${4:r1}, ${5:g1}, ${6:b1}, ${7:r2}, ${8:g2}, ${9:b2});' },
            { label: 'gradient_hsv', detail: 'mask.gradient_hsv(m, s, e, h1, s1, v1, h2, s2, v2): HSV rainbow gradient', snippet: 'gradient_hsv(${1:&mask}, ${2:0}, ${3:15}, ${4:0}, ${5:1.0}, ${6:1.0}, ${7:360}, ${8:1.0}, ${9:1.0});' },
            { label: 'preset', detail: 'mask.preset(m, name): Load built-in palette preset', snippet: 'preset(${1:&mask}, "${2|tetris,gameboy,pico8,nes,fire,cyberpunk,monochrome,neon,c64,matrix,lava,pastel|}");' },
            { label: 'count', detail: 'mask.count(m): Total color count in palette buffer', snippet: 'count(${1:&mask})' },
            { label: 'fade', detail: 'mask.fade(m, factor): Dim or brighten color mask (0.0 - 1.0)', snippet: 'fade(${1:&mask}, ${2:0.5});' },
            { label: 'invert', detail: 'mask.invert(m): Invert all bytes in mask buffer', snippet: 'invert(${1:&mask});' },
            { label: 'shift', detail: 'mask.shift(m, dr, dg, db): Delta shift all RGB channels', snippet: 'shift(${1:&mask}, ${2:dr}, ${3:dg}, ${4:db});' },
            { label: 'brightness', detail: 'mask.brightness(m, delta): Adjust mask brightness (+/- delta)', snippet: 'brightness(${1:&mask}, ${2:delta});' },
            { label: 'contrast', detail: 'mask.contrast(m, factor): Adjust contrast by factor', snippet: 'contrast(${1:&mask}, ${2:1.2});' },
            { label: 'grayscale', detail: 'mask.grayscale(m): Convert mask palette to perceptual grayscale', snippet: 'grayscale(${1:&mask});' },
            { label: 'nearest', detail: 'mask.nearest(m, r, g, b): Find closest matching color index', snippet: 'nearest(${1:&mask}, ${2:r}, ${3:g}, ${4:b})' },
            { label: 'lerp', detail: 'mask.lerp(dest, a, b, t): Interpolate between two color masks', snippet: 'lerp(${1:&dest}, ${2:&a}, ${3:&b}, ${4:0.5});' },
            { label: 'set_2d', detail: 'mask.set_2d(m, w, h, x, y, val): Set 2D grid cell value', snippet: 'set_2d(${1:&mask}, ${2:w}, ${3:h}, ${4:x}, ${5:y}, ${6:val});' },
            { label: 'get_2d', detail: 'mask.get_2d(m, w, h, x, y): Get 2D grid cell value', snippet: 'get_2d(${1:&mask}, ${2:w}, ${3:h}, ${4:x}, ${5:y})' },
            { label: 'rect_2d', detail: 'mask.rect_2d(m, w, h, x, y, rw, rh, val): Draw solid rectangle onto 2D mask', snippet: 'rect_2d(${1:&mask}, ${2:w}, ${3:h}, ${4:x}, ${5:y}, ${6:rw}, ${7:rh}, ${8:val});' },
            { label: 'circle_2d', detail: 'mask.circle_2d(m, w, h, cx, cy, radius, val): Draw solid circle onto 2D mask', snippet: 'circle_2d(${1:&mask}, ${2:w}, ${3:h}, ${4:cx}, ${5:cy}, ${6:radius}, ${7:val});' },
            { label: 'line_2d', detail: 'mask.line_2d(m, w, h, x0, y0, x1, y1, val): Draw line onto 2D mask', snippet: 'line_2d(${1:&mask}, ${2:w}, ${3:h}, ${4:x0}, ${5:y0}, ${6:x1}, ${7:y1}, ${8:val});' },
            { label: 'threshold', detail: 'mask.threshold(src, thresh, high, low): Generate binary mask', snippet: 'threshold(${1:&src}, ${2:128}, ${3:1}, ${4:0})' },
            { label: 'apply_stencil', detail: 'mask.apply_stencil(dest, src, stencil, pass_id): Copy where stencil matches', snippet: 'apply_stencil(${1:&dest}, ${2:&src}, ${3:&stencil}, ${4:1});' },
            { label: 'apply_stencil_inverted', detail: 'mask.apply_stencil_inverted(dest, src, stencil, pass_id): Copy where stencil differs', snippet: 'apply_stencil_inverted(${1:&dest}, ${2:&src}, ${3:&stencil}, ${4:1});' },
            { label: 'blend', detail: 'mask.blend(dest, src_a, src_b, alpha_mask): Alpha blend using 8-bit mask', snippet: 'blend(${1:&dest}, ${2:&src_a}, ${3:&src_b}, ${4:&alpha_mask});' },
            { label: 'collides_2d', detail: 'mask.collides_2d(a, ax, ay, aw, ah, b, bx, by, bw, bh, trans): Pixel-perfect mask collision test', snippet: 'collides_2d(${1:&a}, ${2:ax}, ${3:ay}, ${4:aw}, ${5:ah}, ${6:&b}, ${7:bx}, ${8:by}, ${9:bw}, ${10:bh}, ${11:0})' },
            { label: 'raycast_2d', detail: 'mask.raycast_2d(m, w, h, x0, y0, dx, dy, max_d, solid): Raycast through 2D mask', snippet: 'raycast_2d(${1:&m}, ${2:w}, ${3:h}, ${4:x0}, ${5:y0}, ${6:dir_x}, ${7:dir_y}, ${8:max_dist}, ${9:1})' },
            { label: 'and_op', detail: 'mask.and_op(a, b): Bitwise AND returning new mask', snippet: 'and_op(${1:&a}, ${2:&b})' },
            { label: 'or_op', detail: 'mask.or_op(a, b): Bitwise OR returning new mask', snippet: 'or_op(${1:&a}, ${2:&b})' },
            { label: 'xor_op', detail: 'mask.xor_op(a, b): Bitwise XOR returning new mask', snippet: 'xor_op(${1:&a}, ${2:&b})' },
            { label: 'not_op', detail: 'mask.not_op(a): Bitwise NOT returning new mask', snippet: 'not_op(${1:&a})' },
            { label: 'and_into', detail: 'mask.and_into(dest, mask): In-place bitwise AND', snippet: 'and_into(${1:&dest}, ${2:&mask});' },
            { label: 'or_into', detail: 'mask.or_into(dest, mask): In-place bitwise OR', snippet: 'or_into(${1:&dest}, ${2:&mask});' },
            { label: 'xor_into', detail: 'mask.xor_into(dest, mask): In-place bitwise XOR', snippet: 'xor_into(${1:&dest}, ${2:&mask});' },
            { label: 'not_into', detail: 'mask.not_into(dest): In-place bitwise NOT', snippet: 'not_into(${1:&dest});' },
            { label: 'count_matching', detail: 'mask.count_matching(m, val): Count occurrences of byte value', snippet: 'count_matching(${1:&m}, ${2:val})' },
            { label: 'find_first', detail: 'mask.find_first(m, val): Find index of first matching byte', snippet: 'find_first(${1:&m}, ${2:val})' },
            { label: 'replace', detail: 'mask.replace(m, old_val, new_val): Replace byte values in-place', snippet: 'replace(${1:&m}, ${2:old_val}, ${3:new_val});' },
            { label: 'to_hex', detail: 'mask.to_hex(m): Format mask as spaced hex string', snippet: 'to_hex(${1:&m})' },
            { label: 'from_hex', detail: 'mask.from_hex(hex_str): Parse hex string to Bytes mask', snippet: 'from_hex("${1:FF 00 AA}")' },
            { label: 'save_hex', detail: 'mask.save_hex(path, m): Save mask as hex file', snippet: 'save_hex("${1:mask.hex}", ${2:&m});' },
            { label: 'load_hex', detail: 'mask.load_hex(path): Load mask from hex file', snippet: 'load_hex("${1:mask.hex}")' },
            { label: 'save_bin', detail: 'mask.save_bin(path, m): Save raw binary mask file', snippet: 'save_bin("${1:mask.bin}", ${2:&m});' },
            { label: 'load_bin', detail: 'mask.load_bin(path): Load raw binary mask file', snippet: 'load_bin("${1:mask.bin}")' }
        ],
        'collections': [
            { label: 'zip', detail: 'collections.zip(a, b): Pair elements of two lists into list of 2-item lists', snippet: 'zip(${1:a}, ${2:b})' },
            { label: 'enumerate', detail: 'collections.enumerate(list): Return list of [index, item] pairs', snippet: 'enumerate(${1:list})' },
            { label: 'Stack', detail: 'collections.Stack(): LIFO stack data structure', snippet: 'Stack()' },
            { label: 'Queue', detail: 'collections.Queue(): FIFO queue data structure', snippet: 'Queue()' },
            { label: 'Set', detail: 'collections.Set(): Unique elements hash set', snippet: 'Set()' },
            { label: 'RingBuffer', detail: 'collections.RingBuffer(cap): Fixed-size circular buffer', snippet: 'RingBuffer(${1:16})' }
        ],
        'gui': [
            { label: 'create_window', detail: 'gui.create_window(title, w, h): Spawn Win32 GPU/GDI+ window', snippet: 'create_window("${1:Viss App}", ${2:800}, ${3:600});' },
            { label: 'poll', detail: 'gui.poll(): Non-blocking pump of window events and redrawing', snippet: 'poll()' },
            { label: 'run', detail: 'gui.run(): Blocking modal loop until window closes', snippet: 'run();' },
            { label: 'is_open', detail: 'gui.is_open(): Check if window is still running', snippet: 'is_open()' },
            { label: 'button', detail: 'gui.button(x, y, w, h, label): Clickable button widget (returns true if clicked)', snippet: 'button(${1:x}, ${2:y}, ${3:w}, ${4:h}, "${5:Click Me}")' },
            { label: 'label', detail: 'gui.label(x, y, text, size): Text label widget', snippet: 'label(${1:x}, ${2:y}, "${3:Text}", ${4:14});' },
            { label: 'checkbox', detail: 'gui.checkbox(x, y, text, &checked): Toggle checkbox widget', snippet: 'checkbox(${1:x}, ${2:y}, "${3:Label}", ${4:&checked})' },
            { label: 'slider', detail: 'gui.slider(x, y, w, h, &val, min, max): Interactive slider control', snippet: 'slider(${1:x}, ${2:y}, ${3:w}, ${4:h}, ${5:&val}, ${6:0.0}, ${7:1.0})' },
            { label: 'text_input', detail: 'gui.text_input(x, y, w, h, id, &text, placeholder): Single-line text input', snippet: 'text_input(${1:x}, ${2:y}, ${3:w}, ${4:h}, "${5:id}", ${6:&text}, "${7:placeholder}")' },
            { label: 'textarea', detail: 'gui.textarea(x, y, w, h, id, &text, placeholder): Multi-line text editor', snippet: 'textarea(${1:x}, ${2:y}, ${3:w}, ${4:h}, "${5:id}", ${6:&text})' },
            { label: 'progress_bar', detail: 'gui.progress_bar(x, y, w, h, pct): Progress bar indicator', snippet: 'progress_bar(${1:x}, ${2:y}, ${3:w}, ${4:h}, ${5:0.5});' },
            { label: 'card', detail: 'gui.card(x, y, w, h): Modern container panel card', snippet: 'card(${1:x}, ${2:y}, ${3:w}, ${4:h});' },
            { label: 'card_group', detail: 'gui.card_group(x, y, w, h, title): Container group with header title', snippet: 'card_group(${1:x}, ${2:y}, ${3:w}, ${4:h}, "${5:Title}");' },
            { label: 'open_file', detail: 'gui.open_file(title): Native OS Open File dialog', snippet: 'open_file("${1:Select File}")' },
            { label: 'browse_folder', detail: 'gui.browse_folder(title): Native OS Folder browser dialog', snippet: 'browse_folder("${1:Select Folder}")' },
            { label: 'dropped_file', detail: 'gui.dropped_file(): Get path of file dropped onto window', snippet: 'dropped_file()' },
            { label: 'draw_rect', detail: 'gui.draw_rect(x, y, w, h, color, fill): Draw rectangle', snippet: 'draw_rect(${1:x}, ${2:y}, ${3:w}, ${4:h}, ${5:color});' },
            { label: 'draw_round_rect', detail: 'gui.draw_round_rect(x, y, w, h, r, color): Draw rounded rectangle', snippet: 'draw_round_rect(${1:x}, ${2:y}, ${3:w}, ${4:h}, ${5:6}, ${6:color});' },
            { label: 'draw_circle', detail: 'gui.draw_circle(cx, cy, r, color): Draw circle', snippet: 'draw_circle(${1:cx}, ${2:cy}, ${3:radius}, ${4:color});' },
            { label: 'draw_line', detail: 'gui.draw_line(x1, y1, x2, y2, color, w): Draw line', snippet: 'draw_line(${1:x1}, ${2:y1}, ${3:x2}, ${4:y2}, ${5:color}, ${6:1});' },
            { label: 'draw_text', detail: 'gui.draw_text(x, y, text, color, size): Draw custom font text', snippet: 'draw_text(${1:x}, ${2:y}, "${3:text}", ${4:color}, ${5:14});' },
            { label: 'draw_image', detail: 'gui.draw_image(x, y, w, h, path): Draw image from file', snippet: 'draw_image(${1:x}, ${2:y}, ${3:w}, ${4:h}, "${5:image.png}");' },
            { label: 'get_clipboard', detail: 'gui.get_clipboard(): Get clipboard text', snippet: 'get_clipboard()' },
            { label: 'set_clipboard', detail: 'gui.set_clipboard(text): Set clipboard text', snippet: 'set_clipboard("${1:text}");' },
            { label: 'mouse_x', detail: 'gui.mouse_x(): Cursor X coordinate', snippet: 'mouse_x()' },
            { label: 'mouse_y', detail: 'gui.mouse_y(): Cursor Y coordinate', snippet: 'mouse_y()' },
            { label: 'mouse_down', detail: 'gui.mouse_down(): Check if mouse button is held down', snippet: 'mouse_down()' },
            { label: 'mouse_clicked', detail: 'gui.mouse_clicked(): Check if mouse button was clicked', snippet: 'mouse_clicked()' },
            { label: 'key_down', detail: 'gui.key_down(vk): Check if virtual key is down', snippet: 'key_down(${1:vk})' },
            { label: 'key_pressed', detail: 'gui.key_pressed(vk): Check if virtual key was pressed', snippet: 'key_pressed(${1:vk})' }
        ],
        'audio': [
            { label: 'play_tone', detail: 'audio.play_tone(freq, ms, wave, vol): Play tone sound', snippet: 'play_tone(${1:440.0}, ${2:150}, "${3:sine}", ${4:0.5});' },
            { label: 'play_note', detail: 'audio.play_note(note, ms, wave, vol): Play note ("C4", "A#5")', snippet: 'play_note("${1:C4}", ${2:200}, "${3:sine}", ${4:0.5});' },
            { label: 'synth', detail: 'audio.synth(freq, ms, wave, a, d, s, r, vol): ADSR synthesizer', snippet: 'synth(${1:440.0}, ${2:200}, "${3:saw}", ${4:10.0}, ${5:40.0}, ${6:0.7}, ${7:50.0}, ${8:0.5});' },
            { label: 'sfx', detail: 'audio.sfx(name): Play built-in sound effect', snippet: 'sfx("${1|laser,hit,explosion,powerup,gem,jump,dash|}");' },
            { label: 'laser', detail: 'audio.laser(): Play laser sound effect', snippet: 'laser();' },
            { label: 'hit', detail: 'audio.hit(): Play hit sound effect', snippet: 'hit();' },
            { label: 'explosion', detail: 'audio.explosion(): Play explosion sound effect', snippet: 'explosion();' },
            { label: 'powerup', detail: 'audio.powerup(): Play powerup chime sound effect', snippet: 'powerup();' },
            { label: 'gem', detail: 'audio.gem(): Play gem collection sound effect', snippet: 'gem();' },
            { label: 'jump', detail: 'audio.jump(): Play retro jump sound effect', snippet: 'jump();' },
            { label: 'dash', detail: 'audio.dash(): Play dash sound effect', snippet: 'dash();' },
            { label: 'play_bgm', detail: 'audio.play_bgm(path, vol): Stream background music (MP3/OGG/WAV)', snippet: 'play_bgm("${1:music.mp3}", ${2:0.25});' },
            { label: 'stop_bgm', detail: 'audio.stop_bgm(): Stop background music', snippet: 'stop_bgm();' },
            { label: 'set_bgm_volume', detail: 'audio.set_bgm_volume(vol): Set background music volume (0.0 - 1.0)', snippet: 'set_bgm_volume(${1:0.5});' },
            { label: 'play_file', detail: 'audio.play_file(path): Play audio file sample', snippet: 'play_file("${1:sample.wav}");' },
            { label: 'stop_file', detail: 'audio.stop_file(): Stop file playback', snippet: 'stop_file();' },
            { label: 'is_file_playing', detail: 'audio.is_file_playing(): Check if file is currently playing', snippet: 'is_file_playing()' },
            { label: 'set_volume', detail: 'audio.set_volume(vol): Master volume', snippet: 'set_volume(${1:0.5});' },
            { label: 'get_volume', detail: 'audio.get_volume(): Current master volume', snippet: 'get_volume()' },
            { label: 'stop_all', detail: 'audio.stop_all(): Stop all active audio voices', snippet: 'stop_all();' }
        ],
        'media': [
            { label: 'read', detail: 'media.read(path): Parse metadata tags and embedded cover art', snippet: 'read("${1:audio.ogg}")' },
            { label: 'write_tags', detail: 'media.write_tags(audio, tags): Update metadata tags atomically', snippet: 'write_tags("${1:audio.ogg}", ${2:tags_map});' },
            { label: 'embed_cover', detail: 'media.embed_cover(audio, img, tags, out): Embed album cover image', snippet: 'embed_cover("${1:audio.ogg}", "${2:cover.jpg}", ${3:tags_map}, "${4:out.ogg}");' },
            { label: 'extract_cover', detail: 'media.extract_cover(audio, out_img): Extract embedded cover image', snippet: 'extract_cover("${1:audio.ogg}", "${2:cover.jpg}");' },
            { label: 'extract_cover_bytes', detail: 'media.extract_cover_bytes(audio): Extract raw bytes of embedded cover', snippet: 'extract_cover_bytes("${1:audio.ogg}")' },
            { label: 'remove_cover', detail: 'media.remove_cover(audio, out): Strip cover art from audio file', snippet: 'remove_cover("${1:audio.ogg}", "${2:out.ogg}");' },
            { label: 'render_video', detail: 'media.render_video(audio, cover, out_mp4): Render high-res MP4 with cover art & audio', snippet: 'render_video("${1:audio.ogg}", "${2:cover.jpg}", "${3:video.mp4}");' },
            { label: 'start_render_video', detail: 'media.start_render_video(audio, cover, out_mp4): Asynchronously render MP4', snippet: 'start_render_video("${1:audio.ogg}", "${2:cover.jpg}", "${3:video.mp4}");' },
            { label: 'is_rendering_video', detail: 'media.is_rendering_video(): Check if background video render is running', snippet: 'is_rendering_video()' },
            { label: 'get_render_video_progress', detail: 'media.get_render_video_progress(): Render progress percentage (0 - 100)', snippet: 'get_render_video_progress()' },
            { label: 'get_render_video_status', detail: 'media.get_render_video_status(): Status ("RENDERING", "DONE", "ERROR", "IDLE")', snippet: 'get_render_video_status()' },
            { label: 'get_render_video_info', detail: 'media.get_render_video_info(): Progress string info with speed multiplier', snippet: 'get_render_video_info()' },
            { label: 'cancel_render_video', detail: 'media.cancel_render_video(): Abort active background video render', snippet: 'cancel_render_video();' },
            { label: 'clear_render_video', detail: 'media.clear_render_video(): Reset render state', snippet: 'clear_render_video();' }
        ]
    };
    moduleMethods['retrotech'] = moduleMethods['rt'];
    moduleMethods['bytemask'] = moduleMethods['mask'];
    moduleMethods['colormask'] = moduleMethods['mask'];
    moduleMethods['rt.mask'] = moduleMethods['mask'];
    moduleMethods['retrotech.mask'] = moduleMethods['mask'];

    // Object and Collection Methods
    const objectCollectionMethods = [
        { label: 'size', detail: 'size(): Get number of elements in collection or string', kind: vscode.CompletionItemKind.Method, snippet: 'size()' },
        { label: 'len', detail: 'len: Property length of collection', kind: vscode.CompletionItemKind.Property, snippet: 'len' },
        { label: 'append', detail: 'append(item): Add item to end of list', kind: vscode.CompletionItemKind.Method, snippet: 'append(${1:item});' },
        { label: 'add', detail: 'add(item): Add item to end of list', kind: vscode.CompletionItemKind.Method, snippet: 'add(${1:item});' },
        { label: 'pop', detail: 'pop(): Remove and return last element', kind: vscode.CompletionItemKind.Method, snippet: 'pop()' },
        { label: 'insert', detail: 'insert(idx, item): Insert element at specified index', kind: vscode.CompletionItemKind.Method, snippet: 'insert(${1:0}, ${2:item});' },
        { label: 'remove', detail: 'remove(item): Remove first occurrence of item', kind: vscode.CompletionItemKind.Method, snippet: 'remove(${1:item});' },
        { label: 'removeAt', detail: 'removeAt(idx): Remove element at index', kind: vscode.CompletionItemKind.Method, snippet: 'removeAt(${1:0});' },
        { label: 'removeLast', detail: 'removeLast(): Remove the last element', kind: vscode.CompletionItemKind.Method, snippet: 'removeLast();' },
        { label: 'contains', detail: 'contains(item): Check if item exists in collection', kind: vscode.CompletionItemKind.Method, snippet: 'contains(${1:item})' },
        { label: 'clear', detail: 'clear(): Remove all elements from collection', kind: vscode.CompletionItemKind.Method, snippet: 'clear();' },
        { label: 'slice', detail: 'slice(start, len): Return sub-list slice', kind: vscode.CompletionItemKind.Method, snippet: 'slice(${1:start}, ${2:count})' },
        { label: 'sort', detail: 'sort(): Sort collection in-place', kind: vscode.CompletionItemKind.Method, snippet: 'sort();' },
        { label: 'reverse', detail: 'reverse(): Reverse elements in-place', kind: vscode.CompletionItemKind.Method, snippet: 'reverse();' },
        { label: 'shuffle', detail: 'shuffle(): Shuffle elements in-place randomly', kind: vscode.CompletionItemKind.Method, snippet: 'shuffle();' },
        { label: 'choice', detail: 'choice(): Return random element from collection', kind: vscode.CompletionItemKind.Method, snippet: 'choice()' },
        { label: 'all', detail: 'all(predicate): Returns true if all elements satisfy predicate', kind: vscode.CompletionItemKind.Method, snippet: 'all(!func(@x) { !return ${1:@x > 0}; })' },
        { label: 'any', detail: 'any(predicate): Returns true if any element satisfies predicate', kind: vscode.CompletionItemKind.Method, snippet: 'any(!func(@x) { !return ${1:@x == 0}; })' },
        { label: 'join', detail: 'join(sep): Join elements into string', kind: vscode.CompletionItemKind.Method, snippet: 'join("${1:, }")' },
        { label: 'get', detail: 'get(key): Retrieve item by key or index', kind: vscode.CompletionItemKind.Method, snippet: 'get(${1:key})' },
        { label: 'has', detail: 'has(key): Check if map has key', kind: vscode.CompletionItemKind.Method, snippet: 'has(${1:key})' },
        { label: 'keys', detail: 'keys(): Get list of map keys', kind: vscode.CompletionItemKind.Method, snippet: 'keys()' },
        { label: 'values', detail: 'values(): Get list of map values', kind: vscode.CompletionItemKind.Method, snippet: 'values()' }
    ];

    // Hardware Buffer Methods
    const bufferMethods = [
        { label: 'size', detail: 'size(): Buffer length in bytes', kind: vscode.CompletionItemKind.Method, snippet: 'size()' },
        { label: 'byte_size', detail: 'byte_size(): Physical memory size in bytes', kind: vscode.CompletionItemKind.Method, snippet: 'byte_size()' },
        { label: 'bit_size', detail: 'bit_size(): Logical bit count', kind: vscode.CompletionItemKind.Method, snippet: 'bit_size()' },
        { label: 'fill', detail: 'fill(val): Fill entire buffer with byte value', kind: vscode.CompletionItemKind.Method, snippet: 'fill(${1:0});' },
        { label: 'resize', detail: 'resize(new_size): Reallocate buffer size', kind: vscode.CompletionItemKind.Method, snippet: 'resize(${1:1024});' },
        { label: 'set_bit', detail: 'set_bit(byte_i, bit_i, val): Set single bit', kind: vscode.CompletionItemKind.Method, snippet: 'set_bit(${1:0}, ${2:0}, ${3:true});' },
        { label: 'get_bit', detail: 'get_bit(byte_i, bit_i): Read single bit', kind: vscode.CompletionItemKind.Method, snippet: 'get_bit(${1:0}, ${2:0})' },
        { label: 'count_ones', detail: 'count_ones(): Count number of set 1-bits', kind: vscode.CompletionItemKind.Method, snippet: 'count_ones()' },
        { label: 'count_zeros', detail: 'count_zeros(): Count number of cleared 0-bits', kind: vscode.CompletionItemKind.Method, snippet: 'count_zeros()' },
        { label: 'invert', detail: 'invert(): Bitwise invert all bits in-place', kind: vscode.CompletionItemKind.Method, snippet: 'invert();' },
        { label: 'toggle', detail: 'toggle(bit_idx): Flip bit between 0 and 1', kind: vscode.CompletionItemKind.Method, snippet: 'toggle(${1:0});' },
        { label: 'write_str', detail: 'write_str(text): Write string into stream', kind: vscode.CompletionItemKind.Method, snippet: 'write_str("${1:text}");' },
        { label: 'write_int', detail: 'write_int(val): Write 64-bit int into stream', kind: vscode.CompletionItemKind.Method, snippet: 'write_int(${1:0});' },
        { label: 'write_dec', detail: 'write_dec(val): Write 64-bit float into stream', kind: vscode.CompletionItemKind.Method, snippet: 'write_dec(${1:0.0});' },
        { label: 'write_bool', detail: 'write_bool(val): Write boolean into stream', kind: vscode.CompletionItemKind.Method, snippet: 'write_bool(${1:true});' },
        { label: 'write_u8', detail: 'write_u8(val): Write 8-bit unsigned byte', kind: vscode.CompletionItemKind.Method, snippet: 'write_u8(${1:0});' },
        { label: 'read_str', detail: 'read_str(): Read string from stream', kind: vscode.CompletionItemKind.Method, snippet: 'read_str()' },
        { label: 'read_int', detail: 'read_int(): Read 64-bit integer from stream', kind: vscode.CompletionItemKind.Method, snippet: 'read_int()' },
        { label: 'read_dec', detail: 'read_dec(): Read 64-bit float from stream', kind: vscode.CompletionItemKind.Method, snippet: 'read_dec()' },
        { label: 'read_u8', detail: 'read_u8(): Read 8-bit byte from stream', kind: vscode.CompletionItemKind.Method, snippet: 'read_u8()' },
        { label: 'seek', detail: 'seek(pos): Move binary stream cursor', kind: vscode.CompletionItemKind.Method, snippet: 'seek(${1:0});' },
        { label: 'tell', detail: 'tell(): Current stream cursor offset', kind: vscode.CompletionItemKind.Method, snippet: 'tell()' },
        { label: 'rewind', detail: 'rewind(): Reset stream cursor to 0', kind: vscode.CompletionItemKind.Method, snippet: 'rewind();' },
        { label: 'dump', detail: 'dump(): Print formatted hex inspection to console', kind: vscode.CompletionItemKind.Method, snippet: 'dump();' },
        { label: 'to_hex', detail: 'to_hex(): Format buffer as hex string', kind: vscode.CompletionItemKind.Method, snippet: 'to_hex()' },
        { label: 'crc32', detail: 'crc32(): Hardware CRC-32 checksum', kind: vscode.CompletionItemKind.Method, snippet: 'crc32()' },
        { label: 'grid_clear', detail: 'grid_clear(): Clear all grid layers to 0', kind: vscode.CompletionItemKind.Method, snippet: 'grid_clear();' },
        { label: 'grid_fill', detail: 'grid_fill(val): Fill grid layers with value', kind: vscode.CompletionItemKind.Method, snippet: 'grid_fill(${1:0});' },
        { label: 'grid_invert', detail: 'grid_invert(): Invert all grid layer bits', kind: vscode.CompletionItemKind.Method, snippet: 'grid_invert();' },

        // Byte-Mask & Palette Methods
        { label: 'set_rgb', detail: 'set_rgb(id, r, g, b): Set palette RGB color at index', kind: vscode.CompletionItemKind.Method, snippet: 'set_rgb(${1:0}, ${2:255}, ${3:255}, ${4:255});' },
        { label: 'set_hsv', detail: 'set_hsv(id, h, s, v): Set palette HSV color (h:0-360, s:0-1, v:0-1)', kind: vscode.CompletionItemKind.Method, snippet: 'set_hsv(${1:0}, ${2:180}, ${3:1.0}, ${4:1.0});' },
        { label: 'get_r', detail: 'get_r(id): Red component of palette index', kind: vscode.CompletionItemKind.Method, snippet: 'get_r(${1:0})' },
        { label: 'get_g', detail: 'get_g(id): Green component of palette index', kind: vscode.CompletionItemKind.Method, snippet: 'get_g(${1:0})' },
        { label: 'get_b', detail: 'get_b(id): Blue component of palette index', kind: vscode.CompletionItemKind.Method, snippet: 'get_b(${1:0})' },
        { label: 'gradient', detail: 'gradient(start, end, r1, g1, b1, r2, g2, b2): Linear RGB gradient', kind: vscode.CompletionItemKind.Method, snippet: 'gradient(${1:0}, ${2:15}, ${3:0}, ${4:0}, ${5:0}, ${6:255}, ${7:255}, ${8:255});' },
        { label: 'gradient_hsv', detail: 'gradient_hsv(start, end, h1, s1, v1, h2, s2, v2): HSV rainbow gradient', kind: vscode.CompletionItemKind.Method, snippet: 'gradient_hsv(${1:0}, ${2:15}, ${3:0}, ${4:1.0}, ${5:1.0}, ${6:360}, ${7:1.0}, ${8:1.0});' },
        { label: 'preset', detail: 'preset(name): Load built-in palette preset', kind: vscode.CompletionItemKind.Method, snippet: 'preset("${1|tetris,gameboy,pico8,nes,fire,cyberpunk,monochrome,neon,c64,matrix,lava,pastel|}");' },
        { label: 'fade', detail: 'fade(factor): Dim or brighten color mask (0.0 - 1.0)', kind: vscode.CompletionItemKind.Method, snippet: 'fade(${1:0.5});' },
        { label: 'brightness', detail: 'brightness(delta): Shift mask brightness (+/- delta)', kind: vscode.CompletionItemKind.Method, snippet: 'brightness(${1:10});' },
        { label: 'contrast', detail: 'contrast(factor): Adjust contrast by factor', kind: vscode.CompletionItemKind.Method, snippet: 'contrast(${1:1.2});' },
        { label: 'grayscale', detail: 'grayscale(): Convert mask palette to perceptual grayscale', kind: vscode.CompletionItemKind.Method, snippet: 'grayscale();' },
        { label: 'nearest', detail: 'nearest(r, g, b): Find closest matching color index', kind: vscode.CompletionItemKind.Method, snippet: 'nearest(${1:r}, ${2:g}, ${3:b})' },
        { label: 'shift', detail: 'shift(dr, dg, db): Delta shift all RGB channels', kind: vscode.CompletionItemKind.Method, snippet: 'shift(${1:dr}, ${2:dg}, ${3:db});' },
        { label: 'lerp', detail: 'lerp(target, t): Interpolate between two color masks', kind: vscode.CompletionItemKind.Method, snippet: 'lerp(${1:&target}, ${2:0.5});' },
        { label: 'colors', detail: 'colors(): Number of RGB colors (size / 3)', kind: vscode.CompletionItemKind.Method, snippet: 'colors()' },
        { label: 'color_count', detail: 'color_count(): Number of RGB colors (size / 3)', kind: vscode.CompletionItemKind.Method, snippet: 'color_count()' },
        { label: 'apply', detail: 'apply(): Apply colormask directly to retrotech screen palette', kind: vscode.CompletionItemKind.Method, snippet: 'apply();' },

        // Flag and Pattern Masking Methods
        { label: 'has_flag', detail: 'has_flag(byte_idx, flag_mask): Check if bitflag mask is active', kind: vscode.CompletionItemKind.Method, snippet: 'has_flag(${1:0}, ${2:0x01})' },
        { label: 'set_flag', detail: 'set_flag(byte_idx, flag_mask): Set bitflag mask', kind: vscode.CompletionItemKind.Method, snippet: 'set_flag(${1:0}, ${2:0x01});' },
        { label: 'clear_flag', detail: 'clear_flag(byte_idx, flag_mask): Clear bitflag mask', kind: vscode.CompletionItemKind.Method, snippet: 'clear_flag(${1:0}, ${2:0x01});' },
        { label: 'toggle_flag', detail: 'toggle_flag(byte_idx, flag_mask): Toggle bitflag mask', kind: vscode.CompletionItemKind.Method, snippet: 'toggle_flag(${1:0}, ${2:0x01});' },
        { label: 'find_pattern', detail: 'find_pattern(pattern): Find byte sequence offset', kind: vscode.CompletionItemKind.Method, snippet: 'find_pattern([${1:0xAA, 0xBB}])' },
        { label: 'contains_pattern', detail: 'contains_pattern(pattern): Check if byte sequence exists', kind: vscode.CompletionItemKind.Method, snippet: 'contains_pattern([${1:0xAA, 0xBB}])' },

        // Spatial 2D Mask Operations
        { label: 'get_2d', detail: 'get_2d(x, y, pitch): Get 2D grid mask byte', kind: vscode.CompletionItemKind.Method, snippet: 'get_2d(${1:x}, ${2:y}, ${3:pitch})' },
        { label: 'set_2d', detail: 'set_2d(x, y, pitch, val): Set 2D grid mask byte', kind: vscode.CompletionItemKind.Method, snippet: 'set_2d(${1:x}, ${2:y}, ${3:pitch}, ${4:val});' },
        { label: 'rect_2d', detail: 'rect_2d(w, h, x, y, rw, rh, val): Draw solid rectangle onto 2D mask', kind: vscode.CompletionItemKind.Method, snippet: 'rect_2d(${1:w}, ${2:h}, ${3:x}, ${4:y}, ${5:rw}, ${6:rh}, ${7:val});' },
        { label: 'rect', detail: 'rect(x, y, rw, rh, val, pitch): Draw solid rectangle onto 2D mask', kind: vscode.CompletionItemKind.Method, snippet: 'rect(${1:x}, ${2:y}, ${3:rw}, ${4:rh}, ${5:val}, ${6:pitch});' },
        { label: 'circle_2d', detail: 'circle_2d(w, h, cx, cy, radius, val): Draw solid circle onto 2D mask', kind: vscode.CompletionItemKind.Method, snippet: 'circle_2d(${1:w}, ${2:h}, ${3:cx}, ${4:cy}, ${5:radius}, ${6:val});' },
        { label: 'circle', detail: 'circle(cx, cy, radius, val, pitch): Draw solid circle onto 2D mask', kind: vscode.CompletionItemKind.Method, snippet: 'circle(${1:cx}, ${2:cy}, ${3:radius}, ${4:val}, ${5:pitch});' },
        { label: 'line_2d', detail: 'line_2d(w, h, x0, y0, x1, y1, val): Draw line onto 2D mask', kind: vscode.CompletionItemKind.Method, snippet: 'line_2d(${1:w}, ${2:h}, ${3:x0}, ${4:y0}, ${5:x1}, ${6:y1}, ${7:val});' },
        { label: 'line', detail: 'line(x0, y0, x1, y1, val, pitch): Draw line onto 2D mask', kind: vscode.CompletionItemKind.Method, snippet: 'line(${1:x0}, ${2:y0}, ${3:x1}, ${4:y1}, ${5:val}, ${6:pitch});' },
        { label: 'threshold', detail: 'threshold(thresh, high, low): Convert mask to binary threshold', kind: vscode.CompletionItemKind.Method, snippet: 'threshold(${1:128}, ${2:1}, ${3:0});' },
        { label: 'apply_stencil', detail: 'apply_stencil(stencil, pass_id): Mask buffer using stencil', kind: vscode.CompletionItemKind.Method, snippet: 'apply_stencil(${1:&stencil}, ${2:1});' },
        { label: 'blend', detail: 'blend(other, alpha_mask): Blend two buffers using 8-bit alpha mask', kind: vscode.CompletionItemKind.Method, snippet: 'blend(${1:&other}, ${2:&alpha_mask});' },
        { label: 'scroll_2d', detail: 'scroll_2d(dx, dy, w, h): Scroll/wrap 2D mask grid', kind: vscode.CompletionItemKind.Method, snippet: 'scroll_2d(${1:dx}, ${2:dy}, ${3:w}, ${4:h});' },
        { label: 'flip_h', detail: 'flip_h(w, h): Mirror 2D mask horizontally', kind: vscode.CompletionItemKind.Method, snippet: 'flip_h(${1:w}, ${2:h});' },
        { label: 'flip_v', detail: 'flip_v(w, h): Mirror 2D mask vertically', kind: vscode.CompletionItemKind.Method, snippet: 'flip_v(${1:w}, ${2:h});' },
        { label: 'collides_2d', detail: 'collides_2d(ax, ay, aw, ah, b, bx, by, bw, bh, trans): Pixel-perfect mask collision test', kind: vscode.CompletionItemKind.Method, snippet: 'collides_2d(${1:ax}, ${2:ay}, ${3:aw}, ${4:ah}, ${5:&b}, ${6:bx}, ${7:by}, ${8:bw}, ${9:bh}, ${10:0})' },
        { label: 'raycast_2d', detail: 'raycast_2d(x0, y0, dir_x, dir_y, max_dist, pitch, solid): Raycast through 2D mask', kind: vscode.CompletionItemKind.Method, snippet: 'raycast_2d(${1:x0}, ${2:y0}, ${3:dir_x}, ${4:dir_y}, ${5:max_dist}, ${6:pitch}, ${7:1})' },

        // In-Place Bitwise Masking
        { label: 'and_into', detail: 'and_into(mask): In-place bitwise AND with mask', kind: vscode.CompletionItemKind.Method, snippet: 'and_into(${1:&mask});' },
        { label: 'or_into', detail: 'or_into(mask): In-place bitwise OR with mask', kind: vscode.CompletionItemKind.Method, snippet: 'or_into(${1:&mask});' },
        { label: 'xor_into', detail: 'xor_into(mask): In-place bitwise XOR with mask', kind: vscode.CompletionItemKind.Method, snippet: 'xor_into(${1:&mask});' },
        { label: 'not_into', detail: 'not_into(): In-place bitwise NOT inversion', kind: vscode.CompletionItemKind.Method, snippet: 'not_into();' }
    ];

    const presetCompletions = [
        { label: 'tetris', name: 'tetris', detail: 'Tetris 16-color Guideline palette', doc: '**Tetris Guideline Palette (16 colors)**\n- 0: Board background (Dark slate)\n- 1: I (Cyan)\n- 2: J (Blue)\n- 3: L (Orange)\n- 4: O (Yellow)\n- 5: S (Green)\n- 6: T (Purple)\n- 7: Z (Red)\n- 8: Wall/Border (Steel gray)\n- 9: Ghost shadow\n- 10: White text\n- 11: Gold accent\n- 12: Clear flash\n- 13: UI dark slate\n- 14: Sky cyan\n- 15: Game over red' },
        { label: 'gameboy', name: 'gameboy', detail: 'Game Boy original 4-shade greenish palette', doc: '**Game Boy Classic Palette (4 colors)**\n- 0: Lightest green (#9BBC0F)\n- 1: Light green (#8BAC0F)\n- 2: Dark green (#306230)\n- 3: Darkest green (#0F380F)' },
        { label: 'pico8', name: 'pico8', detail: 'PICO-8 authentic 16-color fantasy console palette', doc: '**PICO-8 Palette (16 colors)**\nStandard fantasy console colors: Black, Dark Blue, Dark Purple, Dark Green, Brown, Dark Gray, Light Gray, White, Red, Orange, Yellow, Green, Blue, Indigo, Pink, Peach.' },
        { label: 'nes', name: 'nes', detail: 'NES authentic 16-color arcade palette', doc: '**NES Classic Arcade Palette (16 colors)**\nAuthentic arcade hardware colors (sky blue, brick red, crimson, cobalt blue, peach skin, emerald, mario gold, white, black, brown, dark green, orange).' },
        { label: 'fire', name: 'fire', detail: '16-color flame heat gradient (black -> red -> orange -> yellow -> white)', doc: '**Fire Heat Gradient (16 colors)**\nContinuous smooth gradient from deep black through dark crimson, bright orange, burning yellow to searing white.' },
        { label: 'cyberpunk', name: 'cyberpunk', detail: 'Neon synthwave gradient (deep violet -> cyan -> pink -> gold)', doc: '**Cyberpunk Synthwave Palette (16 colors)**\nHigh-contrast futuristic gradient: dark midnight violet, electric cyan, hot magenta pink, neon gold, white.' },
        { label: 'monochrome', name: 'monochrome', detail: '2-color 1-bit high contrast palette (black & white)', doc: '**Monochrome Palette (2 colors)**\n- 0: Pure Black (#000000)\n- 1: Pure White (#FFFFFF)' },
        { label: 'neon', name: 'neon', detail: '16-color vibrant neon glow palette', doc: '**Vibrant Neon Palette (16 colors)**\nVivid fluorescent glowing colors: Neon Pink, Neon Cyan, Neon Green, Neon Yellow, Neon Purple, Neon Orange, Deep Sky.' },
        { label: 'c64', name: 'c64', detail: 'Commodore 64 iconic 16-color vintage computer palette', doc: '**Commodore 64 Palette (16 colors)**\nClassic 1982 home computer palette: Black, White, Red, Cyan, Purple, Green, Blue, Yellow, Orange, Brown, Light Red, Dark Gray, Gray, Light Green, Light Blue, Light Gray.' },
        { label: 'matrix', name: 'matrix', detail: 'Matrix digital rain green gradient', doc: '**Matrix Digital Rain Gradient (16 colors)**\nSmooth phosphor terminal gradient from deep void through matrix forest green to glowing emerald and core white-green.' },
        { label: 'lava', name: 'lava', detail: 'Magma molten lava heat gradient', doc: '**Molten Lava Gradient (16 colors)**\nDeep obsidian basalt rock through incandescent magma crimson, blazing orange to incandescent yellow.' },
        { label: 'pastel', name: 'pastel', detail: 'Soft aesthetic 16-color pastel palette', doc: '**Aesthetic Pastel Palette (16 colors)**\nDelicate soft tones: Pastel Pink, Pastel Orange, Pastel Yellow, Pastel Green, Pastel Blue, Pastel Lavender, Pastel Rose.' }
    ];

    const completionProvider = vscode.languages.registerCompletionItemProvider('viss', {
        provideCompletionItems(document, position) {
            const lineText = document.lineAt(position).text;
            const linePrefix = lineText.substring(0, position.character);
            const items = [];

            // -----------------------------------------------------------
            // 0. Preset completions: preset(...) or preset("...")
            // -----------------------------------------------------------
            const presetMatch = linePrefix.match(/(?:preset|\.preset)\s*\(\s*(?:&[a-zA-Z0-9_]+\s*,\s*)?["']?([a-zA-Z0-9_]*)$/);
            if (presetMatch) {
                const quoteOpen = linePrefix.endsWith('"') || linePrefix.endsWith("'") || linePrefix.includes('"') || linePrefix.includes("'");
                presetCompletions.forEach(p => {
                    const item = new vscode.CompletionItem(p.label, vscode.CompletionItemKind.Constant);
                    item.detail = p.detail;
                    item.documentation = new vscode.MarkdownString(p.doc);
                    item.insertText = quoteOpen ? p.name : `"${p.name}"`;
                    items.push(item);
                });
                return items;
            }

            // -----------------------------------------------------------
            // 0.1. Buffer creation: create | 
            // -----------------------------------------------------------
            if (/create\s*\|\s*$/.test(linePrefix)) {
                const bufferKinds = [
                    { label: 'bytemask', detail: 'Byte-mask buffer (default 48 bytes = 16 RGB colors)', snippet: 'bytemask, ${1:16};' },
                    { label: 'colormask', detail: 'RGB palette buffer (default 16 colors = 48 bytes)', snippet: 'colormask, ${1:16};' },
                    { label: 'mask', detail: 'Spatial byte mask buffer (default 1024 bytes)', snippet: 'mask, ${1:1024};' },
                    { label: 'bytes', detail: 'Raw byte buffer (default 1024 bytes)', snippet: 'bytes, ${1:1024};' },
                    { label: 'bits', detail: 'Bitfield array (default 8192 bits)', snippet: 'bits, ${1:8192};' },
                    { label: 'hybrid', detail: 'Hybrid byte/bit buffer', snippet: 'hybrid, bytes, ${1:512}, bits, ${2:1024};' },
                    { label: 'grid', detail: 'Volumetric 2D grid matrix', snippet: 'grid, ${1:32}, ${2:32};' }
                ];
                bufferKinds.forEach(bk => {
                    const item = new vscode.CompletionItem(bk.label, vscode.CompletionItemKind.TypeParameter);
                    item.detail = bk.detail;
                    item.insertText = new vscode.SnippetString(bk.snippet);
                    items.push(item);
                });
                return items;
            }

            // -----------------------------------------------------------
            // 1. Module Member Completions: <module>.
            // -----------------------------------------------------------
            const dotMatch = linePrefix.match(/([@&]?[a-zA-Z0-9_]+(?:\.[a-zA-Z0-9_]+)*)\.$/);
            if (dotMatch) {
                const target = dotMatch[1];

                // Known Standard Module or Submodule
                if (moduleMethods[target]) {
                    moduleMethods[target].forEach(m => {
                        const item = new vscode.CompletionItem(m.label, vscode.CompletionItemKind.Method);
                        item.detail = m.detail;
                        item.insertText = new vscode.SnippetString(m.snippet);
                        items.push(item);
                    });
                    return items;
                }

                // Buffer Object: &buffer.
                if (target.startsWith('&')) {
                    bufferMethods.forEach(m => {
                        const item = new vscode.CompletionItem(m.label, m.kind);
                        item.detail = m.detail;
                        item.insertText = new vscode.SnippetString(m.snippet);
                        items.push(item);
                    });
                    return items;
                }

                // If variable suggests mask / palette / buffer
                const lowerTarget = target.toLowerCase();
                if (lowerTarget.includes('mask') || lowerTarget.includes('pal') || lowerTarget.includes('stencil') || lowerTarget.includes('buf')) {
                    const combined = [...bufferMethods, ...objectCollectionMethods];
                    const seen = new Set();
                    combined.forEach(m => {
                        if (!seen.has(m.label)) {
                            seen.add(m.label);
                            const item = new vscode.CompletionItem(m.label, m.kind);
                            item.detail = m.detail;
                            item.insertText = new vscode.SnippetString(m.snippet);
                            items.push(item);
                        }
                    });
                    return items;
                }

                // Generic Variable / Collection / String: @var. or obj.
                objectCollectionMethods.forEach(m => {
                    const item = new vscode.CompletionItem(m.label, m.kind);
                    item.detail = m.detail;
                    item.insertText = new vscode.SnippetString(m.snippet);
                    items.push(item);
                });
                return items;
            }

            // -----------------------------------------------------------
            // 2. Variable Sigil: @
            // -----------------------------------------------------------
            if (linePrefix.endsWith('@')) {
                // Collect all declared variables from current document
                const docText = document.getText();
                const varMatches = docText.match(/@([a-zA-Z_][a-zA-Z0-9_]*)/g) || [];
                const uniqueVars = new Set(varMatches.map(v => v.substring(1)));
                uniqueVars.add('me');

                uniqueVars.forEach(vname => {
                    const item = new vscode.CompletionItem(vname, vscode.CompletionItemKind.Variable);
                    item.detail = `@${vname} (variable)`;
                    items.push(item);
                });
                return items;
            }

            // -----------------------------------------------------------
            // 3. Buffer Sigil: &
            // -----------------------------------------------------------
            if (linePrefix.endsWith('&')) {
                const docText = document.getText();
                const bufMatches = docText.match(/&([a-zA-Z_][a-zA-Z0-9_]*)/g) || [];
                const uniqueBufs = new Set(bufMatches.map(b => b.substring(1)));

                uniqueBufs.forEach(bname => {
                    const item = new vscode.CompletionItem(bname, vscode.CompletionItemKind.Variable);
                    item.detail = `&${bname} (hardware buffer / mask)`;
                    items.push(item);
                });
                return items;
            }

            // -----------------------------------------------------------
            // 4. Pipe Type Annotation: | 
            // -----------------------------------------------------------
            if (/\|\s*$/.test(linePrefix)) {
                const types = [
                    { label: 'int', detail: '64-bit integer type' },
                    { label: 'str', detail: 'UTF-8 string type' },
                    { label: 'dec', detail: '64-bit float type' },
                    { label: 'bool', detail: 'Boolean flag type' },
                    { label: 'bytemask', detail: 'Byte-mask buffer (default 48 bytes = 16 RGB colors)' },
                    { label: 'colormask', detail: 'RGB palette buffer (default 16 colors = 48 bytes)' },
                    { label: 'mask', detail: 'Spatial or generic byte mask buffer (default 1024 bytes)' },
                    { label: 'bytes', detail: 'Raw byte buffer' },
                    { label: 'bits', detail: 'Bitfield array' },
                    { label: 'list', detail: 'Generic dynamic list' },
                    { label: 'list<int>', detail: 'Typed integer list' },
                    { label: 'list<str>', detail: 'Typed string list' },
                    { label: 'map', detail: 'Key-value map' },
                    { label: 'map<str, var>', detail: 'Typed dictionary' },
                    { label: 'const', detail: 'Immutable constant modifier' },
                    { label: 'hybrid', detail: 'Packed byte/bit buffer' },
                    { label: 'grid', detail: 'Volumetric multi-layer grid' }
                ];
                types.forEach(t => {
                    const item = new vscode.CompletionItem(t.label, vscode.CompletionItemKind.TypeParameter);
                    item.detail = t.detail;
                    items.push(item);
                });
                return items;
            }

            // -----------------------------------------------------------
            // 5. Directives: $
            // -----------------------------------------------------------
            if (linePrefix.endsWith('$')) {
                const directives = [
                    { label: 'import lib', detail: 'Import standard library module', snippet: 'import lib "${1:retrotech}" as ${2:rt};' },
                    { label: 'import', detail: 'Import user file module', snippet: 'import "${1:module.viss}" as ${2:mod};' },
                    { label: 'include', detail: 'Include header file', snippet: 'include "${1:header.viss}";' }
                ];
                directives.forEach(d => {
                    const item = new vscode.CompletionItem(d.label, vscode.CompletionItemKind.Snippet);
                    item.detail = d.detail;
                    item.insertText = new vscode.SnippetString(d.snippet);
                    items.push(item);
                });
                return items;
            }

            // -----------------------------------------------------------
            // 6. Action Directives: !
            // -----------------------------------------------------------
            if (linePrefix.endsWith('!')) {
                const actions = [
                    { label: 'func', detail: 'Define function', snippet: 'func ${1:name}(${2:params}) {\n    $0\n}' },
                    { label: 'async func', detail: 'Define asynchronous worker task', snippet: 'async func ${1:name}(${2:params}) {\n    $0\n}' },
                    { label: 'main', detail: 'Application entry point block', snippet: 'main {\n    $0\n}' },
                    { label: 'class', detail: 'Define class structure', snippet: 'class ${1:ClassName} {\n    $0\n}' },
                    { label: 'struct', detail: 'Define struct schema', snippet: 'struct ${1:StructName} {\n    $0\n}' },
                    { label: 'enum', detail: 'Define enum constants', snippet: 'enum ${1:EnumName} {\n    ${2:Item1},\n    ${3:Item2}\n}' },
                    { label: 'for in range', detail: 'Numeric interval for-loop', snippet: 'for @${1:i} in ${2:0}..${3:10} {\n    $0\n}' },
                    { label: 'for in collection', detail: 'Collection iteration loop', snippet: 'for @${1:item} in @${2:coll} {\n    $0\n}' },
                    { label: 'while', detail: 'While loop block', snippet: 'while ${1:!@game_over} {\n    $0\n}' },
                    { label: 'await', detail: 'Await async task result', snippet: 'await @${1:task};' },
                    { label: 'return', detail: 'Return from function', snippet: 'return ${1:value};' },
                    { label: 'break', detail: 'Break from loop', snippet: 'break;' },
                    { label: 'continue', detail: 'Continue next loop iteration', snippet: 'continue;' },
                    { label: 'defer', detail: 'Defer statement execution to scope exit', snippet: 'defer ${1:cleanup()};' }
                ];
                actions.forEach(a => {
                    const item = new vscode.CompletionItem(a.label, vscode.CompletionItemKind.Keyword);
                    item.detail = a.detail;
                    item.insertText = new vscode.SnippetString(a.snippet);
                    items.push(item);
                });
                return items;
            }

            // -----------------------------------------------------------
            // 7. Logic Directives: ?
            // -----------------------------------------------------------
            if (linePrefix.endsWith('?')) {
                const logics = [
                    { label: 'if', detail: 'If conditional statement', snippet: 'if ${1:condition} {\n    $0\n}' },
                    { label: 'elif', detail: 'Else-if branch condition', snippet: 'elif ${1:condition} {\n    $0\n}' },
                    { label: 'else', detail: 'Else fallback branch', snippet: 'else {\n    $0\n}' },
                    { label: 'match', detail: 'Pattern match switch block', snippet: 'match @${1:value} {\n    case ${2:pattern} {\n        $3\n    }\n    else {\n        $0\n    }\n}' },
                    { label: 'try', detail: 'Exception try-expect block', snippet: 'try {\n    $1\n} ?expect Exception as @e {\n    io.println(i"Error: {@e}");\n}' },
                    { label: 'error', detail: 'Throw runtime exception', snippet: 'error "${1:error message}";' }
                ];
                logics.forEach(l => {
                    const item = new vscode.CompletionItem(l.label, vscode.CompletionItemKind.Keyword);
                    item.detail = l.detail;
                    item.insertText = new vscode.SnippetString(l.snippet);
                    items.push(item);
                });
                return items;
            }

            // -----------------------------------------------------------
            // 8. General Scope Keywords & Standard Modules (Clean, no bloat)
            // -----------------------------------------------------------
            const generalKeywords = [
                { label: '!func', detail: 'Define function', kind: vscode.CompletionItemKind.Snippet, snippet: '!func ${1:name}(${2:params}) {\n    $0\n}' },
                { label: '!main', detail: 'Define main entry point', kind: vscode.CompletionItemKind.Snippet, snippet: '!main {\n    $0\n}' },
                { label: '?if', detail: 'Conditional if block', kind: vscode.CompletionItemKind.Snippet, snippet: '?if ${1:condition} {\n    $0\n}' },
                { label: '?elif', detail: 'Else-if condition block', kind: vscode.CompletionItemKind.Snippet, snippet: '?elif ${1:condition} {\n    $0\n}' },
                { label: '?else', detail: 'Else fallback block', kind: vscode.CompletionItemKind.Snippet, snippet: '?else {\n    $0\n}' },
                { label: '!for', detail: 'Range for-loop', kind: vscode.CompletionItemKind.Snippet, snippet: '!for @${1:i} in ${2:0}..${3:10} {\n    $0\n}' },
                { label: '!while', detail: 'While loop block', kind: vscode.CompletionItemKind.Snippet, snippet: '!while ${1:condition} {\n    $0\n}' },
                { label: 'return', detail: 'Return from function', kind: vscode.CompletionItemKind.Keyword, snippet: 'return ${1:value};' },
                { label: 'in', detail: 'Membership or loop operator', kind: vscode.CompletionItemKind.Keyword },
                { label: '!in', detail: 'Negative membership check', kind: vscode.CompletionItemKind.Keyword },
                { label: 'true', detail: 'Boolean true', kind: vscode.CompletionItemKind.Constant },
                { label: 'false', detail: 'Boolean false', kind: vscode.CompletionItemKind.Constant }
            ];

            generalKeywords.forEach(k => {
                const item = new vscode.CompletionItem(k.label, k.kind);
                item.detail = k.detail;
                if (k.snippet) item.insertText = new vscode.SnippetString(k.snippet);
                items.push(item);
            });

            // Standard Library Modules
            const stdModules = [
                { name: 'rt', desc: 'Retrotech 2D arcade graphics & audio engine' },
                { name: 'io', desc: 'Formatted console and terminal I/O' },
                { name: 'sys', desc: 'System utilities, random generator, and clock' },
                { name: 'str', desc: 'String operations and transformations' },
                { name: 'fs', desc: 'Filesystem manipulation and globbing' },
                { name: 'json', desc: 'JSON parser and serializer' },
                { name: 'time', desc: 'High-precision timers and sleep' },
                { name: 'math', desc: 'Math functions, physics, and noise' },
                { name: 'async', desc: 'Concurrency and multithreading' },
                { name: 'mask', desc: 'Hardware byte-masks, RGB palettes, and spatial stencils' },
                { name: 'bytemask', desc: 'Alias for mask module' },
                { name: 'colormask', desc: 'RGB palette engine' }
            ];

            stdModules.forEach(m => {
                const item = new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Module);
                item.detail = m.desc;
                items.push(item);
            });

            return items;
        }
    }, '.', '@', '&', '!', '?', '|', '$', '(', '"', '\'');

    // ==========================================
    // 6. Inlay Parameter Name Hints
    // ==========================================
    const inlaySignatures = {
        'rt.init_screen': ['w:', 'h:'],
        'rt.clear_screen': ['color:'],
        'rt.draw_pixel': ['x:', 'y:', 'color:'],
        'rt.draw_rect': ['x:', 'y:', 'w:', 'h:', 'color:'],
        'rt.draw_rect_fill': ['x:', 'y:', 'w:', 'h:', 'color:'],
        'rt.draw_text': ['x:', 'y:', 'text:', 'fg:', 'bg:'],
        'rt.open_window': ['title:', 'scale:'],
        'rt.is_down': ['key:'],
        'rt.SetColorMask': ['mask:'],
        'rt.ColorScreen': ['mask:'],
        'mask.set_rgb': ['mask:', 'id:', 'r:', 'g:', 'b:'],
        'mask.set_hsv': ['mask:', 'id:', 'h:', 's:', 'v:'],
        'mask.gradient': ['mask:', 'start:', 'end:', 'r1:', 'g1:', 'b1:', 'r2:', 'g2:', 'b2:'],
        'mask.preset': ['mask:', 'name:'],
        'mask.fade': ['mask:', 'factor:'],
        'mask.blend': ['dest:', 'src_a:', 'src_b:', 'alpha:'],
        'mask.apply_stencil': ['dest:', 'src:', 'stencil:', 'pass_id:'],
        'mask.collides_2d': ['a:', 'ax:', 'ay:', 'aw:', 'ah:', 'b:', 'bx:', 'by:', 'bw:', 'bh:'],
        'sys.random': ['min:', 'max:'],
        'str.split': ['str:', 'delim:'],
        'str.sub': ['str:', 'start:', 'len:'],
        'str.substr': ['str:', 'start:', 'len:'],
        'time.sleep': ['ms:'],
        'math.clamp': ['val:', 'min:', 'max:'],
        'math.lerp': ['a:', 'b:', 't:'],
        'math.distance': ['x1:', 'y1:', 'x2:', 'y2:']
    };

    const inlayHintsProvider = vscode.languages.registerInlayHintsProvider('viss', {
        provideInlayHints(document, range) {
            const hints = [];
            const text = document.getText(range);
            const lines = text.split('\n');

            for (let l = 0; l < lines.length; ++l) {
                const line = lines[l];
                const lineNum = range.start.line + l;

                for (const [fnName, params] of Object.entries(inlaySignatures)) {
                    let idx = 0;
                    while ((idx = line.indexOf(fnName + '(', idx)) !== -1) {
                        const callStart = idx + fnName.length + 1;
                        let depth = 0;
                        let argIndex = 0;
                        let argStart = callStart;

                        for (let c = callStart; c < line.length; ++c) {
                            const char = line[c];
                            if (char === '(' || char === '[' || char === '{') depth++;
                            else if (char === ')' || char === ']' || char === '}') {
                                if (depth === 0) {
                                    if (argIndex < params.length && line.slice(argStart, c).trim().length > 0) {
                                        hints.push(new vscode.InlayHint(
                                            new vscode.Position(lineNum, argStart),
                                            params[argIndex] + ' ',
                                            vscode.InlayHintKind.Parameter
                                        ));
                                    }
                                    break;
                                }
                                depth--;
                            } else if (char === ',' && depth === 0) {
                                if (argIndex < params.length && line.slice(argStart, c).trim().length > 0) {
                                    hints.push(new vscode.InlayHint(
                                        new vscode.Position(lineNum, argStart),
                                        params[argIndex] + ' ',
                                        vscode.InlayHintKind.Parameter
                                    ));
                                }
                                argIndex++;
                                while (c + 1 < line.length && line[c + 1] === ' ') c++;
                                argStart = c + 1;
                            }
                        }
                        idx = callStart;
                    }
                }
            }
            return hints;
        }
    });

    // ==========================================
    // 7. Document Outline & Symbols Provider
    // ==========================================
    const documentSymbolProvider = vscode.languages.registerDocumentSymbolProvider('viss', {
        provideDocumentSymbols(document) {
            const symbols = [];
            const lineCount = document.lineCount;

            for (let i = 0; i < lineCount; ++i) {
                const line = document.lineAt(i);
                const text = line.text;

                // Match Class / Struct / Enum
                const mClass = text.match(/^\s*(!class|class|!struct|struct|!enum|enum)\s+([a-zA-Z0-9_]+)/);
                if (mClass) {
                    const kindStr = mClass[1].replace('!', '');
                    const cname = mClass[2];
                    const sk = kindStr === 'enum' ? vscode.SymbolKind.Enum : (kindStr === 'struct' ? vscode.SymbolKind.Struct : vscode.SymbolKind.Class);
                    symbols.push(new vscode.DocumentSymbol(cname, kindStr, sk, line.range, line.range));
                    continue;
                }

                // Match Functions
                const mFunc = text.match(/^\s*(!func|func|!async\s+func)\s+([a-zA-Z0-9_?!]+)\s*\(([^)]*)\)/);
                if (mFunc) {
                    const fname = mFunc[2];
                    const params = mFunc[3];
                    symbols.push(new vscode.DocumentSymbol(fname, `(${params})`, vscode.SymbolKind.Function, line.range, line.range));
                    continue;
                }

                // Match Main
                if (/^\s*(!main|!func\s+main\b|func\s+main\b)\b/.test(text)) {
                    symbols.push(new vscode.DocumentSymbol('main', 'Entry point', vscode.SymbolKind.Function, line.range, line.range));
                    continue;
                }

                // Match Raw Buffers
                const mBuf = text.match(/^\s*&([a-zA-Z0-9_]+)\s+create\s*\|\s*(bytes|bits|bytemask|colormask|mask)/);
                if (mBuf) {
                    const bname = mBuf[1];
                    const btype = mBuf[2];
                    symbols.push(new vscode.DocumentSymbol('&' + bname, `Hardware ${btype} buffer`, vscode.SymbolKind.Variable, line.range, line.range));
                }
            }
            return symbols;
        }
    });

    context.subscriptions.push(
        hoverProvider,
        inlayHintsProvider,
        documentSymbolProvider,
        completionProvider
    );
}

function deactivate() {}

module.exports = {
    activate,
    deactivate
};
