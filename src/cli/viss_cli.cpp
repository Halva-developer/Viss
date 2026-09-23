#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

const std::string VISS_VERSION = "0.0.2.0";

// Terminal colors
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GRAY    "\033[90m"

std::string get_viss_root() {
    #ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    fs::path exe_path(buffer);
    fs::path dir = exe_path.parent_path();
    // Check if current dir contains src/vissc.py or libs
    if (fs::exists(dir / "src" / "vissc.py")) {
        return dir.string();
    }
    // Hardcoded default fallback
    fs::path default_root = "C:\\Users\\halva\\Desktop\\Viss";
    if (fs::exists(default_root / "src" / "vissc.py")) {
        return default_root.string();
    }
    return dir.string();
    #else
    return "/usr/local/share/viss";
    #endif
}

std::string find_gxx() {
    // 1. Check in PATH
    const char* env_path = std::getenv("PATH");
    // 2. Check bundled w64devkit
    std::string w64 = "C:\\AGY\\TOOLS\\w64devkit\\bin\\g++.exe";
    if (fs::exists(w64)) return w64;

    // Fallback: look for g++ in system
    return "g++";
}

std::string find_python() {
    // Check standard hermes venv or system python
    std::string hermes_py = "C:\\Users\\halva\\AppData\\Local\\hermes\\hermes-agent\\venv\\Scripts\\python.exe";
    if (fs::exists(hermes_py)) return hermes_py;
    return "python";
}

void print_header() {
    std::cout << COLOR_CYAN << "⚡ Viss Language Toolchain " << COLOR_RESET 
              << COLOR_BOLD << "v" << VISS_VERSION << COLOR_RESET 
              << COLOR_GRAY << " (Native High-Performance C++ Pipeline)\n" << COLOR_RESET;
}

void print_help() {
    print_header();
    std::cout << "\n" << COLOR_BOLD << "USAGE:" << COLOR_RESET << "\n";
    std::cout << "  viss <command> [arguments]\n";
    std::cout << "  viss <file.viss> [arguments]\n\n";
    std::cout << COLOR_BOLD << "COMMANDS:" << COLOR_RESET << "\n";
    std::cout << "  " << COLOR_GREEN << "run" << COLOR_RESET << " <file.viss> [args...]      Compile (with smart caching) and run instantly\n";
    std::cout << "  " << COLOR_GREEN << "build" << COLOR_RESET << " <file.viss> [-o out.exe]  Compile to a standalone executable\n";
    std::cout << "  " << COLOR_GREEN << "check" << COLOR_RESET << " <file.viss>               Check syntax and show linter diagnostics\n";
    std::cout << "  " << COLOR_GREEN << "transpile" << COLOR_RESET << " <file.viss> [out.cpp]  Convert Viss source directly to modern C++\n";
    std::cout << "  " << COLOR_GREEN << "new" << COLOR_RESET << " <project_name>             Create a new Viss project\n";
    std::cout << "  " << COLOR_GREEN << "clean" << COLOR_RESET << "                          Remove build cache (.viss_cache)\n";
    std::cout << "  " << COLOR_GREEN << "version" << COLOR_RESET << "                        Show toolchain and compiler information\n";
}

int execute_cmd(const std::string& cmd) {
    #ifdef _WIN32
    std::string wrapped = "\"" + cmd + "\"";
    return std::system(wrapped.c_str());
    #else
    return std::system(cmd.c_str());
    #endif
}

bool file_is_newer(const fs::path& target, const fs::path& source) {
    if (!fs::exists(target) || !fs::exists(source)) return false;
    return fs::last_write_time(target) >= fs::last_write_time(source);
}


int main(int argc, char* argv[]) {
    // Enable ANSI Virtual Terminal Processing on Windows
    #ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    #endif

    if (argc < 2) {
        print_help();
        return 0;
    }

    std::string arg1 = argv[1];

    if (arg1 == "-h" || arg1 == "--help" || arg1 == "help") {
        print_help();
        return 0;
    }

    if (arg1 == "-v" || arg1 == "--version" || arg1 == "version") {
        print_header();
        std::cout << "Viss Root: " << get_viss_root() << "\n";
        std::cout << "C++ Compiler: " << find_gxx() << "\n";
        std::cout << "Python Engine: " << find_python() << "\n";
        return 0;
    }

    if (arg1 == "clean") {
        if (fs::exists(".viss_cache")) {
            fs::remove_all(".viss_cache");
            std::cout << COLOR_GREEN << "✔ Cleaned build cache (.viss_cache)" << COLOR_RESET << "\n";
        } else {
            std::cout << COLOR_GRAY << "Build cache is already clean." << COLOR_RESET << "\n";
        }
        return 0;
    }

    if (arg1 == "new") {
        std::string pname = (argc > 2) ? argv[2] : "viss_app";
        fs::create_directories(pname);
        std::string main_viss = (fs::path(pname) / "main.viss").string();
        std::ofstream out(main_viss);
        out << "// Viss Application Template\n\n";
        out << "!func main() {\n";
        out << "    io.println(\"Hello from Viss 2.0!\");\n";
        out << "    @time_str = time.format_now();\n";
        out << "    io.println(\"Current system time:\", @time_str);\n";
        out << "}\n";
        out.close();
        std::cout << COLOR_GREEN << "✔ Created new Viss project in ./" << pname << "/" << COLOR_RESET << "\n";
        std::cout << "Run it with: " << COLOR_CYAN << "viss run " << pname << "/main.viss" << COLOR_RESET << "\n";
        return 0;
    }

    std::string viss_root = get_viss_root();
    std::string python_bin = find_python();
    std::string gxx_bin = find_gxx();
    std::string vissc_script = (fs::path(viss_root) / "src" / "vissc.py").string();

    if (arg1 == "check") {
        if (argc < 3) {
            std::cerr << COLOR_RED << "Error: Missing filename for check command.\n" << COLOR_RESET;
            std::cerr << "Usage: viss check <file.viss> [--json]\n";
            return 1;
        }
        std::string target_file = argv[2];
        std::string json_flag = (argc > 3 && std::string(argv[3]) == "--json") ? " --json" : "";
        std::string cmd = "\"" + python_bin + "\" \"" + vissc_script + "\" check \"" + target_file + "\"" + json_flag;
        return execute_cmd(cmd);
    }

    if (arg1 == "transpile") {
        if (argc < 3) {
            std::cerr << COLOR_RED << "Error: Missing source file.\n" << COLOR_RESET;
            return 1;
        }
        std::string target_file = argv[2];
        std::string out_cpp = (argc > 3) ? argv[3] : (fs::path(target_file).stem().string() + ".cpp");
        std::string cmd = "\"" + python_bin + "\" \"" + vissc_script + "\" transpile \"" + target_file + "\" \"" + out_cpp + "\"";
        return execute_cmd(cmd);
    }


    // Determine target file and whether to run
    std::string target_file;
    bool should_run = false;
    std::string custom_output = "";
    std::vector<std::string> pass_args;

    if (arg1 == "run") {
        if (argc < 3) {
            std::cerr << COLOR_RED << "Error: Missing filename for run command.\n" << COLOR_RESET;
            return 1;
        }
        target_file = argv[2];
        should_run = true;
        for (int i = 3; i < argc; ++i) pass_args.push_back(argv[i]);
    } else if (arg1 == "build" || arg1 == "compile") {
        if (argc < 3) {
            std::cerr << COLOR_RED << "Error: Missing filename for build command.\n" << COLOR_RESET;
            return 1;
        }
        target_file = argv[2];
        should_run = false;
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "-o" && i + 1 < argc) {
                custom_output = argv[++i];
            }
        }
    } else {
        // First argument is the file itself: e.g. viss main.viss
        target_file = arg1;
        should_run = true;
        for (int i = 2; i < argc; ++i) pass_args.push_back(argv[i]);
    }

    if (!fs::exists(target_file)) {
        std::cerr << COLOR_RED << "Error: File '" << target_file << "' not found.\n" << COLOR_RESET;
        return 1;
    }

    fs::path src_path(target_file);
    fs::path base_path = src_path.parent_path() / src_path.stem();
    fs::path cpp_path = base_path.string() + ".cpp";
    fs::path exe_path = custom_output.empty() ? (base_path.string() + ".exe") : fs::path(custom_output);

    // Smart Cache: If .exe exists and is newer than .viss source, launch instantly!
    bool needs_rebuild = true;
    if (fs::exists(exe_path) && file_is_newer(exe_path, src_path)) {
        needs_rebuild = false;
    }

    if (needs_rebuild) {
        auto t_start = std::chrono::high_resolution_clock::now();
        std::cout << COLOR_CYAN << "⚡ [Viss] Transpiling " << COLOR_RESET << src_path.filename().string() << "...\n";

        // Transpile to C++
        std::string trans_cmd = "\"" + python_bin + "\" \"" + vissc_script + "\" transpile \"" + src_path.string() + "\" \"" + cpp_path.string() + "\"";
        int trans_code = execute_cmd(trans_cmd);
        if (trans_code != 0) {
            std::cerr << COLOR_RED << "✖ Transpilation failed!\n" << COLOR_RESET;
            return trans_code;
        }

        std::cout << COLOR_CYAN << "⚙ [Viss] Compiling native binary (-O2)..." << COLOR_RESET << "\n";

        // Build g++ command with include path to Viss root and necessary libraries
        std::string gxx_dir = fs::path(gxx_bin).parent_path().string();
        #ifdef _WIN32
        std::string full_path = gxx_dir + ";" + (std::getenv("PATH") ? std::getenv("PATH") : "");

        SetEnvironmentVariableA("PATH", full_path.c_str());
        std::string path_env = "PATH=" + full_path;
        _putenv(path_env.c_str());
        #endif


        std::stringstream comp_cmd;
        comp_cmd << "\"" << gxx_bin << "\" -std=c++17 -O2 "
                 << "-I\"" << viss_root << "\" "
                 << "\"" << cpp_path.string() << "\" "
                 << "-o \"" << exe_path.string() << "\" "
                 #ifdef _WIN32
                 << "-lwinmm -lws2_32 -lwininet"
                 #endif
                 ;

        int comp_code = execute_cmd(comp_cmd.str());
        if (comp_code != 0) {
            std::cerr << COLOR_RED << "✖ Native C++ compilation failed!\n" << COLOR_RESET;
            return comp_code;
        }

        auto t_end = std::chrono::high_resolution_clock::now();
        double elapsed_sec = std::chrono::duration<double>(t_end - t_start).count();
        std::cout << COLOR_GREEN << "✔ [Viss] Built " << exe_path.filename().string() 
                  << " in " << std::fixed << std::setprecision(2) << elapsed_sec << "s\n" << COLOR_RESET;
    } else {
        std::cout << COLOR_GRAY << "⚡ [Viss] Using up-to-date binary: " << exe_path.filename().string() << COLOR_RESET << "\n";
    }

    if (should_run) {
        std::string run_cmd = "\"" + exe_path.string() + "\"";
        for (const auto& a : pass_args) {
            run_cmd += " \"" + a + "\"";
        }
        return execute_cmd(run_cmd);
    }


    return 0;
}
