#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>
#include "../libs/std/json.hpp"

namespace viss {
namespace lsp {

    // Store virtual open documents: URI -> Content
    inline std::map<std::string, std::string>& getDocuments() {
        static std::map<std::string, std::string> docs;
        return docs;
    }

    inline void sendRpcResponse(const std::string& json_payload) {
        std::cout << "Content-Length: " << json_payload.size() << "\r\n\r\n" << json_payload << std::flush;
    }

    inline void sendNotification(const std::string& method, const std::string& params_json) {
        std::string payload = "{\"jsonrpc\":\"2.0\",\"method\":\"" + method + "\",\"params\":" + params_json + "}";
        sendRpcResponse(payload);
    }

    // Standard Library Symbol Database for Autocomplete & Hover
    struct VissSymbolInfo {
        std::string name;
        std::string kind; // Function, Module, Keyword, Type
        std::string signature;
        std::string doc;
    };

    inline const std::vector<VissSymbolInfo>& getStdlibSymbols() {
        static std::vector<VissSymbolInfo> syms = {
            // Modules
            {"io", "Module", "module io", "Console Input/Output and standard streams"},
            {"str", "Module", "module str", "String manipulation, UTF-8, formatting, splitting"},
            {"fs", "Module", "module fs", "File system operations, paths, reading and writing files"},
            {"sys", "Module", "module sys", "System processes, environment variables, exit codes"},
            {"math", "Module", "module math", "Mathematical operations, trigonometry, random numbers"},
            {"time", "Module", "module time", "High-resolution clocks, timestamps, sleep, dates"},
            {"json", "Module", "module json", "High-performance JSON parser, builder, and serializer"},
            {"net", "Module", "module net", "HTTP client/server, TCP sockets, WebSockets"},
            {"db", "Module", "module db", "Embedded relational database with SQLite syntax and persistence"},
            {"chan", "Module", "module chan", "Thread-safe buffered and unbuffered Go-style channels"},
            {"gui", "Module", "module gui", "Native cross-platform 2D graphics and desktop GUI windowing"},
            {"audio", "Module", "module audio", "Audio engine, sound effects, music playback"},
            {"crypto", "Module", "module crypto", "Cryptographic hashes (SHA256, MD5, AES, base64)"},
            {"async", "Module", "module async", "Thread pool, async tasks, spawn and await primitives"},
            {"collections", "Module", "module collections", "Specialized collections: Queue, Stack, Set"},

            // IO methods
            {"io.println", "Function", "io.println(val...)", "Prints values followed by a newline"},
            {"io.print", "Function", "io.print(val...)", "Prints values without newline"},
            {"io.read_line", "Function", "io.read_line() -> str", "Reads a string line from standard input"},
            {"io.read_int", "Function", "io.read_int() -> int", "Reads an integer from standard input"},
            {"io.color", "Function", "io.color(color_name)", "Sets console text color (e.g. 'green', 'cyan')"},

            // STR methods
            {"str.len", "Function", "str.len(s: str) -> int", "Returns character length of string"},
            {"str.lower", "Function", "str.lower(s: str) -> str", "Converts string to lowercase"},
            {"str.upper", "Function", "str.upper(s: str) -> str", "Converts string to uppercase"},
            {"str.split", "Function", "str.split(s: str, delim: str) -> list", "Splits string by delimiter"},
            {"str.join", "Function", "str.join(list, delim: str) -> str", "Joins string list with delimiter"},
            {"str.replace", "Function", "str.replace(s: str, from: str, to: str) -> str", "Replaces occurrences in string"},
            {"str.trim", "Function", "str.trim(s: str) -> str", "Trims leading and trailing whitespace"},
            {"str.contains", "Function", "str.contains(s: str, sub: str) -> bool", "Checks if substring exists"},
            {"str.starts_with", "Function", "str.starts_with(s: str, prefix: str) -> bool", "Tests prefix match"},
            {"str.ends_with", "Function", "str.ends_with(s: str, suffix: str) -> bool", "Tests suffix match"},

            // DB methods
            {"db.open", "Function", "db.open(path: str) -> DbHandle", "Opens or creates a persistent or :memory: database"},
            {"db.exec", "Function", "db.exec(db, sql: str) -> int", "Executes SQL DDL/DML statement (CREATE, INSERT, UPDATE, DELETE)"},
            {"db.query", "Function", "db.query(db, sql: str) -> list", "Executes SQL SELECT and returns list of row maps"},
            {"db.query_json", "Function", "db.query_json(db, sql: str) -> str", "Executes SQL SELECT and returns JSON formatted string"},
            {"db.close", "Function", "db.close(db)", "Closes database and commits changes to disk"},
            {"db.tables", "Function", "db.tables(db) -> list", "Returns list of table names in database"},
            {"db.count", "Function", "db.count(db, table: str) -> int", "Returns row count for table"},
            {"db.begin", "Function", "db.begin(db) -> bool", "Begins a transactional snapshot"},
            {"db.commit", "Function", "db.commit(db) -> bool", "Commits active transaction to disk"},
            {"db.rollback", "Function", "db.rollback(db) -> bool", "Rolls back active transaction"},

            // NET methods
            {"net.create_server", "Function", "net.create_server() -> HttpServer", "Creates a high-performance HTTP server instance"},
            {"net.server_listen", "Function", "net.server_listen(port: int, handler) -> bool", "Starts a one-liner HTTP server on specified port"},
            {"net.http_get", "Function", "net.http_get(url: str) -> str", "Performs HTTP GET request and returns response body"},
            {"net.http_post", "Function", "net.http_post(url: str, body: str) -> str", "Performs HTTP POST request and returns response body"},
            {"net.download_file", "Function", "net.download_file(url: str, dest: str) -> bool", "Downloads file to specified local path"},

            // CHAN methods
            {"chan.create", "Function", "chan.create<T>(cap: int = 0) -> Chan<T>", "Creates buffered (cap > 0) or unbuffered channel"},
            {"chan.send", "Function", "chan.send(ch, val)", "Sends value to channel, blocking if buffer is full"},
            {"chan.recv", "Function", "chan.recv(ch) -> T", "Receives value from channel, blocking until available"},
            {"chan.try_recv", "Function", "chan.try_recv(ch, out_val) -> bool", "Non-blocking receive attempt"},
            {"chan.close", "Function", "chan.close(ch)", "Closes channel for further sends"},
            {"chan.len", "Function", "chan.len(ch) -> int", "Returns current item count buffered in channel"},

            // GUI methods
            {"gui.create", "Function", "gui.create(title: str, w: int, h: int) -> bool", "Creates native application window"},
            {"gui.is_open", "Function", "gui.is_open() -> bool", "Checks if GUI window is currently active"},
            {"gui.poll_events", "Function", "gui.poll_events() -> bool", "Processes window events and message loop"},
            {"gui.clear", "Function", "gui.clear(color)", "Clears background buffer with specified color"},
            {"gui.update", "Function", "gui.update()", "Flips buffer and presents frame to screen"},
            {"gui.draw_rect", "Function", "gui.draw_rect(x, y, w, h, color, fill = true)", "Draws filled or outlined rectangle"},
            {"gui.draw_circle", "Function", "gui.draw_circle(cx, cy, r, color, fill = true)", "Draws circle"},
            {"gui.draw_text", "Function", "gui.draw_text(x, y, text: str, color, size = 14)", "Renders text on screen"},
            {"gui.button", "Function", "gui.button(x, y, w, h, label: str) -> bool", "Renders interactive button; returns true on click"},
            {"gui.label", "Function", "gui.label(x, y, text: str, size = 14)", "Renders UI text label widget"},
            {"gui.open_file", "Function", "gui.open_file(title: str) -> str", "Opens native OS file picker dialog"},

            // Keywords & Sigils
            {"!func", "Keyword", "!func name(params) { ... }", "Defines a Viss function"},
            {"!main", "Keyword", "!main { ... }", "Defines main entrypoint of application"},
            {"!struct", "Keyword", "!struct Name { ... }", "Defines a custom composite data structure"},
            {"!if", "Keyword", "!if (condition) { ... }", "Conditional execution block"},
            {"!elif", "Keyword", "!elif (condition) { ... }", "Else-if conditional execution branch"},
            {"!else", "Keyword", "!else { ... }", "Else fallback execution branch"},
            {"!for", "Keyword", "!for @i in 0..10 { ... }", "Range or collection iteration loop"},
            {"!while", "Keyword", "!while (condition) { ... }", "Loop while condition evaluates to true"},
            {"!return", "Keyword", "!return value;", "Returns value from function"},
            {"!spawn", "Keyword", "!spawn !func() { ... }", "Spawns asynchronous task on worker thread pool"},
            {"!await", "Keyword", "!await task", "Awaits asynchronous task completion"}
        };
        return syms;
    }

    // Simple diagnostics linter for Viss
    inline void lintDocument(const std::string& uri, const std::string& content) {
        std::istringstream stream(content);
        std::string line;
        int line_num = 0;
        int brace_balance = 0;
        std::vector<std::string> errors;

        while (std::getline(stream, line)) {
            for (char c : line) {
                if (c == '{') brace_balance++;
                if (c == '}') brace_balance--;
            }
            line_num++;
        }

        std::ostringstream diag_json;
        diag_json << "{\"uri\":\"" << uri << "\",\"diagnostics\":[";

        if (brace_balance != 0) {
            diag_json << "{"
                      << "\"range\":{\"start\":{\"line\":" << (line_num > 0 ? line_num - 1 : 0) << ",\"character\":0},"
                      << "\"end\":{\"line\":" << (line_num > 0 ? line_num - 1 : 0) << ",\"character\":1}},"
                      << "\"severity\":1,"
                      << "\"source\":\"viss-linter\","
                      << "\"message\":\"Unbalanced braces: " << (brace_balance > 0 ? "Missing closing brace '}'" : "Extra closing brace '}'") << "\""
                      << "}";
        }

        diag_json << "]}";
        sendNotification("textDocument/publishDiagnostics", diag_json.str());
    }

    // Main LSP Event Loop over stdin/stdout
    inline int runServer() {
        // Disable sync with stdio for high-speed binary I/O
        std::ios::sync_with_stdio(false);
        std::cin.tie(nullptr);

        while (true) {
            std::string header_line;
            size_t content_length = 0;

            // Read HTTP-like headers
            while (std::getline(std::cin, header_line)) {
                if (header_line == "\r" || header_line.empty()) {
                    break;
                }
                if (header_line.find("Content-Length:") == 0) {
                    content_length = std::stoull(header_line.substr(15));
                }
            }

            if (content_length == 0) {
                if (std::cin.eof()) break;
                continue;
            }

            // Read message body
            std::vector<char> body(content_length);
            std::cin.read(body.data(), content_length);
            std::string msg(body.data(), content_length);

            try {
                auto rpc = json::parse(msg);
                std::string method = rpc.contains("method") ? rpc["method"].to_str() : "";
                std::string id = rpc.contains("id") ? rpc["id"].to_str() : "";

                // --- 1. INITIALIZE ---
                if (method == "initialize") {
                    std::string res = "{"
                        "\"jsonrpc\":\"2.0\","
                        "\"id\":" + id + ","
                        "\"result\":{"
                            "\"capabilities\":{"
                                "\"textDocumentSync\":1,"
                                "\"completionProvider\":{\"resolveProvider\":false,\"triggerCharacters\":[\".\",\"@\",\"!\",\"?\",\"&\",\"$\"]},"
                                "\"hoverProvider\":true,"
                                "\"definitionProvider\":true,"
                                "\"documentSymbolProvider\":true"
                            "},"
                            "\"serverInfo\":{\"name\":\"viss-lsp\",\"version\":\"0.3.0\"}"
                        "}"
                    "}";
                    sendRpcResponse(res);
                }
                // --- 2. INITIALIZED ---
                else if (method == "initialized") {
                    // Ready
                }
                // --- 3. SHUTDOWN ---
                else if (method == "shutdown") {
                    std::string res = "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":null}";
                    sendRpcResponse(res);
                }
                // --- 4. EXIT ---
                else if (method == "exit") {
                    return 0;
                }
                // --- 5. DID OPEN ---
                else if (method == "textDocument/didOpen") {
                    if (rpc.contains("params") && rpc["params"].contains("textDocument")) {
                        auto td = rpc["params"]["textDocument"];
                        std::string uri = td["uri"].to_str();
                        std::string text = td["text"].to_str();
                        getDocuments()[uri] = text;
                        lintDocument(uri, text);
                    }
                }
                // --- 6. DID CHANGE ---
                else if (method == "textDocument/didChange") {
                    if (rpc.contains("params") && rpc["params"].contains("textDocument")) {
                        std::string uri = rpc["params"]["textDocument"]["uri"].to_str();
                        if (rpc["params"].contains("contentChanges")) {
                            auto changes = rpc["params"]["contentChanges"];
                            if (changes.is_array() && changes.size() > 0) {
                                std::string new_text = changes[changes.size() - 1]["text"].to_str();
                                getDocuments()[uri] = new_text;
                                lintDocument(uri, new_text);
                            }
                        }
                    }
                }
                // --- 7. DID CLOSE ---
                else if (method == "textDocument/didClose") {
                    if (rpc.contains("params") && rpc["params"].contains("textDocument")) {
                        std::string uri = rpc["params"]["textDocument"]["uri"].to_str();
                        getDocuments().erase(uri);
                    }
                }
                // --- 8. COMPLETION ---
                else if (method == "textDocument/completion") {
                    std::ostringstream items;
                    items << "[";
                    const auto& syms = getStdlibSymbols();
                    for (size_t i = 0; i < syms.size(); ++i) {
                        int item_kind = (syms[i].kind == "Function") ? 3 : (syms[i].kind == "Module") ? 9 : 14;
                        items << "{"
                              << "\"label\":\"" << syms[i].name << "\","
                              << "\"kind\":" << item_kind << ","
                              << "\"detail\":\"" << json::escape(syms[i].signature) << "\","
                              << "\"documentation\":{\"kind\":\"markdown\",\"value\":\"" << json::escape(syms[i].doc) << "\"}"
                              << "}";
                        if (i + 1 < syms.size()) items << ",";
                    }
                    items << "]";

                    std::string res = "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":" + items.str() + "}";
                    sendRpcResponse(res);
                }
                // --- 9. HOVER ---
                else if (method == "textDocument/hover") {
                    std::string doc_content = "";
                    if (rpc.contains("params") && rpc["params"].contains("textDocument")) {
                        std::string uri = rpc["params"]["textDocument"]["uri"].to_str();
                        if (getDocuments().count(uri)) doc_content = getDocuments()[uri];
                    }

                    int target_line = 0;
                    int target_char = 0;
                    if (rpc["params"].contains("position")) {
                        target_line = (int)rpc["params"]["position"]["line"].to_int();
                        target_char = (int)rpc["params"]["position"]["character"].to_int();
                    }

                    // Extract hovered token
                    std::string word = "";
                    std::istringstream stream(doc_content);
                    std::string cur_line;
                    int cur_l = 0;
                    while (std::getline(stream, cur_line)) {
                        if (cur_l == target_line) {
                            int start = target_char;
                            int end = target_char;
                            while (start > 0 && (isalnum((unsigned char)cur_line[start - 1]) || cur_line[start - 1] == '_' || cur_line[start - 1] == '.' || cur_line[start - 1] == '!' || cur_line[start - 1] == '@')) start--;
                            while (end < (int)cur_line.size() && (isalnum((unsigned char)cur_line[end]) || cur_line[end] == '_' || cur_line[end] == '.')) end++;
                            if (start < end) word = cur_line.substr(start, end - start);
                            break;
                        }
                        cur_l++;
                    }

                    std::string hover_md = "";
                    const auto& syms = getStdlibSymbols();
                    for (const auto& s : syms) {
                        if (s.name == word) {
                            hover_md = "```viss\n" + s.signature + "\n```\n\n" + s.doc;
                            break;
                        }
                    }

                    if (!hover_md.empty()) {
                        std::string res = "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":{\"contents\":{\"kind\":\"markdown\",\"value\":\"" + json::escape(hover_md) + "\"}}}";
                        sendRpcResponse(res);
                    } else {
                        std::string res = "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":null}";
                        sendRpcResponse(res);
                    }
                }
                // --- 10. DEFINITION ---
                else if (method == "textDocument/definition") {
                    // Definition lookup
                    std::string res = "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":null}";
                    sendRpcResponse(res);
                }
                // --- DEFAULT UNHANDLED ---
                else if (!id.empty()) {
                    std::string res = "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":null}";
                    sendRpcResponse(res);
                }

            } catch (...) {
                // Keep server resilient
            }
        }
        return 0;
    }

} // namespace lsp
} // namespace viss
