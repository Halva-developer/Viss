// =============================================================================
// Viss Autonomous Windows Installer (Setup_Viss.exe)
// Version 0.2.2 "Lemongrab & Lemonhope"
// =============================================================================

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <windows.h>
#include <shlobj.h>

namespace fs = std::filesystem;

void enableAnsi() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
}

std::string getLocalAppData() {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
        return std::string(path);
    }
    const char* env = getenv("LOCALAPPDATA");
    return env ? std::string(env) : "C:\\Users\\Default\\AppData\\Local";
}

bool addPathToUserEnvironment(const std::string& new_dir) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_READ | KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    char cur_path[32768] = {0};
    DWORD buf_size = sizeof(cur_path);
    DWORD type = REG_EXPAND_SZ;

    LONG res = RegQueryValueExA(hKey, "Path", NULL, &type, (LPBYTE)cur_path, &buf_size);
    std::string path_str = (res == ERROR_SUCCESS) ? cur_path : "";

    // Check if directory is already present
    if (path_str.find(new_dir) == std::string::npos) {
        if (!path_str.empty() && path_str.back() != ';') {
            path_str += ";";
        }
        path_str += new_dir;

        RegSetValueExA(hKey, "Path", 0, REG_EXPAND_SZ, (const BYTE*)path_str.c_str(), (DWORD)(path_str.size() + 1));
        std::cout << "\033[32m[+] Added to User PATH environment variable.\033[0m\n";
    } else {
        std::cout << "\033[36m[*] Already present in User PATH.\033[0m\n";
    }

    RegCloseKey(hKey);

    // Broadcast environment change to Explorer and running processes
    DWORD_PTR dwResult;
    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment", SMTO_ABORTIFHUNG, 3000, &dwResult);
    return true;
}

bool registerFileAssociations(const std::string& viss_exe) {
    HKEY hKey;
    // .viss extension
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Classes\\.viss", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        const char* progId = "VissSourceFile";
        RegSetValueExA(hKey, NULL, 0, REG_SZ, (const BYTE*)progId, (DWORD)(strlen(progId) + 1));
        RegCloseKey(hKey);
    }

    // ProgID
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Classes\\VissSourceFile", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        const char* desc = "Viss Source File";
        RegSetValueExA(hKey, NULL, 0, REG_SZ, (const BYTE*)desc, (DWORD)(strlen(desc) + 1));
        RegCloseKey(hKey);
    }

    // Command: open with viss run
    std::string cmd_str = "\"" + viss_exe + "\" run \"%1\"";
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Classes\\VissSourceFile\\shell\\open\\command", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, NULL, 0, REG_SZ, (const BYTE*)cmd_str.c_str(), (DWORD)(cmd_str.size() + 1));
        RegCloseKey(hKey);
    }

    // Context menu: "Run with Viss"
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Classes\\VissSourceFile\\shell\\Run with Viss", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
    }
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Classes\\VissSourceFile\\shell\\Run with Viss\\command", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, NULL, 0, REG_SZ, (const BYTE*)cmd_str.c_str(), (DWORD)(cmd_str.size() + 1));
        RegCloseKey(hKey);
    }

    std::cout << "\033[32m[+] Registered .viss file association & 'Run with Viss' context menu.\033[0m\n";
    return true;
}

int main(int argc, char* argv[]) {
    enableAnsi();

    std::cout << "\033[36m\033[1m";
    std::cout << "  __      ___             \n";
    std::cout << "  \\ \\    / (_)            \n";
    std::cout << "   \\ \\  / / _ ___ ___     \n";
    std::cout << "    \\ \\/ / | / __/ __|    \n";
    std::cout << "     \\  /  | \\__ \\__ \\    \n";
    std::cout << "      \\/   |_|___/___/    \n";
    std::cout << "\033[0m\n";
    std::cout << "\033[33m\033[1mViss Language Toolchain v0.2.2 \"Lemongrab & Lemonhope\"\033[0m\n";
    std::cout << "\033[37mAutonomous Windows Standalone Setup\033[0m\n\n";

    fs::path install_dir = fs::path(getLocalAppData()) / "Programs" / "Viss";

    // Allow custom directory via arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dir" && i + 1 < argc) {
            install_dir = argv[++i];
        }
    }

    std::cout << "[*] Target directory: \033[36m" << install_dir.string() << "\033[0m\n";

    try {
        fs::create_directories(install_dir);
        fs::create_directories(install_dir / "libs");

        // Find source directory
        char exe_path_buf[MAX_PATH];
        GetModuleFileNameA(NULL, exe_path_buf, MAX_PATH);
        fs::path setup_dir = fs::path(exe_path_buf).parent_path();

        // Check potential candidate root folders
        fs::path src_root = setup_dir;
        if (!fs::exists(src_root / "viss.exe")) {
            if (fs::exists(setup_dir.parent_path() / "viss.exe")) {
                src_root = setup_dir.parent_path();
            } else if (fs::exists(setup_dir.parent_path().parent_path() / "viss.exe")) {
                src_root = setup_dir.parent_path().parent_path();
            }
        }

        fs::path src_viss = src_root / "viss.exe";
        fs::path src_libs = src_root / "libs";

        if (!fs::exists(src_viss)) {
            std::cerr << "\033[31m[ERROR] Cannot locate 'viss.exe' in: " << src_root.string() << "\033[0m\n";
            return 1;
        }

        auto robustCopy = [](const fs::path& src, const fs::path& dst) {
            std::error_code ec;
            if (fs::exists(dst)) {
                fs::remove(dst, ec);
            }
            return CopyFileW(src.wstring().c_str(), dst.wstring().c_str(), FALSE) != 0;
        };

        std::cout << "[*] Copying compiler binary...\n";
        robustCopy(src_viss, install_dir / "viss.exe");

        if (fs::exists(src_root / "viss.cmd")) {
            robustCopy(src_root / "viss.cmd", install_dir / "viss.cmd");
        }

        std::cout << "[*] Copying standard libraries...\n";
        if (fs::exists(src_libs)) {
            for (const auto& item : fs::recursive_directory_iterator(src_libs)) {
                fs::path rel = fs::relative(item.path(), src_libs);
                fs::path dest = install_dir / "libs" / rel;
                if (item.is_directory()) {
                    fs::create_directories(dest);
                } else if (item.is_regular_file()) {
                    robustCopy(item.path(), dest);
                }
            }
        }

        // Add to user PATH
        addPathToUserEnvironment(install_dir.string());

        // Register file associations
        registerFileAssociations((install_dir / "viss.exe").string());

        // Check if VS Code CLI is present and install .vsix extension
        fs::path vsix_path = src_root / "extensions" / "viss-vscode" / "viss-vscode-0.3.2.vsix";
        if (fs::exists(vsix_path)) {
            std::cout << "[*] Checking for Visual Studio Code...\n";
            int code_check = std::system("where code >nul 2>&1");
            if (code_check == 0) {
                std::cout << "\033[32m[+] Installing official VS Code extension v0.3.2...\033[0m\n";
                std::string code_cmd = "code --install-extension \"" + vsix_path.string() + "\" --force >nul 2>&1";
                std::system(code_cmd.c_str());
                std::cout << "\033[32m[+] VS Code extension installed cleanly! ^_^\033[0m\n";
            }
        }

        std::cout << "\n\033[32m\033[1m====================================================\033[0m\n";
        std::cout << "\033[32m\033[1m  Viss Language successfully installed! :3           \033[0m\n";
        std::cout << "\033[32m\033[1m====================================================\033[0m\n\n";
        std::cout << "Open any new Terminal or PowerShell window and run:\n";
        std::cout << "  \033[36mviss -v\033[0m\n";
        std::cout << "  \033[36mviss run main.viss\033[0m\n\n";

    } catch (const std::exception& e) {
        std::cerr << "\033[31m[ERROR] Installation failed: " << e.what() << "\033[0m\n";
        return 1;
    }

    return 0;
}
