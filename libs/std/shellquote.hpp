#pragma once
// Safe quoting of single command-line arguments for commands the stdlib builds
// (paths, URLs, archive entry names, audio tag values). Self-contained on purpose:
// media.hpp includes it without pulling in the whole runtime.
#include <string>
#include <cstdlib>
#include <filesystem>

namespace viss {
    // Quote ONE argument.
    // POSIX: single quotes, so $(...), `...`, ; and & stay literal inside std::system/popen.
    // Windows: CreateProcess/argv quoting rules (backslashes before quotes are doubled).
    inline std::string quoteArg(const std::string& s) {
#ifdef _WIN32
        std::string r = "\"";
        size_t bs = 0;
        for (char c : s) {
            if (c == '\\') { bs++; continue; }
            if (c == '"') { r.append(bs * 2 + 1, '\\'); r += '"'; }
            else { r.append(bs, '\\'); r += c; }
            bs = 0;
        }
        r.append(bs * 2, '\\');
        r += '"';
        return r;
#else
        std::string r = "'";
        for (char c : s) {
            if (c == '\'') r += "'\\''";
            else r += c;
        }
        r += "'";
        return r;
#endif
    }

    // For commands that go through cmd.exe on Windows (std::system / _popen):
    // cmd.exe has no escape for '"' inside a quoted argument, so such values are refused.
    inline bool shellArgSafe(const std::string& s) {
#ifdef _WIN32
        return s.find_first_of("\"\r\n") == std::string::npos;
#else
        (void)s;
        return true;  // quoteArg() makes any byte sequence literal for /bin/sh
#endif
    }

    inline std::string shellArg(const std::string& s) {
#ifdef _WIN32
        return "\"" + s + "\"";
#else
        return quoteArg(s);
#endif
    }

#ifdef _WIN32
    // The tar that ships with Windows 10+ (bsdtar) reads ZIP files. A bare "tar.exe" may resolve
    // to GNU tar from Git/MSYS first in PATH, which cannot, so zip.read_text failed on such PCs.
    inline std::string systemTar() {
        const char* root = std::getenv("SystemRoot");
        if (root) {
            std::string cand = std::string(root) + "\\System32\\tar.exe";
            // Left unquoted on purpose (cmd.exe quote stripping); only used when it has no spaces.
            if (cand.find(' ') == std::string::npos && std::filesystem::exists(cand)) return cand;
        }
        return "tar.exe";
    }
#endif
}
