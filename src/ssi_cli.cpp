// =============================================================================
// SSI - Scripting Son of Viss (Standalone CLI Executable)
// Version 0.3.0 "Son of Viss"
// =============================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>

#include "ssi.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static std::string getExecutableDir() {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string p(path);
    size_t pos = p.find_last_of("\\/");
    return (pos != std::string::npos) ? p.substr(0, pos) : ".";
#else
    char path[1024];
    ssize_t count = readlink("/proc/self/exe", path, 1024);
    if (count != -1) {
        std::string p(path, count);
        size_t pos = p.find_last_of("\\/");
        return (pos != std::string::npos) ? p.substr(0, pos) : ".";
    }
    return ".";
#endif
}

void printHelp() {
    std::cout << "SSI - Scripting Son of Viss v" << ssi::SSI_VERSION << " \"" << ssi::SSI_CODENAME << "\"\n\n"
              << "Usage: ssi [options] [script.ssi] [arguments...]\n\n"
              << "Commands & Options:\n"
              << "  <script.ssi> [args...]       Execute SSI script immediately (0ms compile overhead)\n"
              << "  -c \"<code>\" [args...]        Run inline code directly (like python -c or bash -c)\n"
              << "  -e \"<expression>\"            Evaluate expression and print result\n"
              << "  build <script.ssi> [-o bin]  Compile SSI script to standalone native binary\n"
              << "  repl                         Start interactive SSI REPL (or run 'ssi' with no args)\n"
              << "  -v, --version, version       Show version\n"
              << "  -h, --help, help             Show this help message\n\n"
              << "Examples:\n"
              << "  ssi script.ssi\n"
              << "  ssi -c \"println('Hello from SSI!')\"\n"
              << "  ssi -c \"@os = sys.os(); println(@os)\"\n"
              << "  ssi -e \"math.sqrt(144)\"\n"
              << "  ssi build script.ssi -o myapp\n";
}

int buildBinary(const std::string& ssi_file, const std::string& out_name, const std::vector<std::string>& extra_flags) {
    if (!fs::exists(ssi_file)) {
        std::cerr << "[SSI Build Error] File not found: " << ssi_file << "\n";
        return 1;
    }

    std::ifstream in(ssi_file);
    std::stringstream buffer; buffer << in.rdbuf();
    std::string ssi_code = buffer.str();

    std::string viss_code = ssi::transpileSSItoViss(ssi_code);

    fs::path p(ssi_file);
    std::string stem = p.stem().string();
    fs::path temp_viss = fs::current_path() / (stem + ".viss");

    std::ofstream out(temp_viss);
    out << viss_code;
    out.close();

    std::string viss_dir = getExecutableDir();
#ifdef _WIN32
    std::string viss_bin = viss_dir + "\\viss.exe";
    if (!fs::exists(viss_bin)) viss_bin = "viss.exe";
#else
    std::string viss_bin = viss_dir + "/viss";
    if (!fs::exists(viss_bin)) viss_bin = "viss";
#endif

    std::string cmd = "\"" + viss_bin + "\" bundle \"" + temp_viss.string() + "\"";
    if (!out_name.empty()) {
        cmd += " -o \"" + out_name + "\"";
    }
    for (const auto& fl : extra_flags) cmd += " " + fl;

    std::cout << "[SSI Builder] Compiling " << ssi_file << " to native binary via Viss toolchain...\n";
#ifdef _WIN32
    std::string exec_cmd = "cmd.exe /s /c \"" + cmd + "\"";
    int ret = std::system(exec_cmd.c_str());
#else
    int ret = std::system(cmd.c_str());
#endif

    // Clean temp .viss file
    if (fs::exists(temp_viss)) {
        fs::remove(temp_viss);
    }

    return ret;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        ssi::runRepl();
        return 0;
    }

    std::string arg1 = argv[1];

    if (arg1 == "-h" || arg1 == "--help" || arg1 == "help") {
        printHelp();
        return 0;
    }

    if (arg1 == "-v" || arg1 == "--version" || arg1 == "version") {
        std::cout << "SSI v" << ssi::SSI_VERSION << " \"" << ssi::SSI_CODENAME << "\" (Scripting Son of Viss)\n";
        return 0;
    }

    if (arg1 == "repl") {
        ssi::runRepl();
        return 0;
    }

    // Inline code execution: ssi -c "code"
    if (arg1 == "-c") {
        if (argc < 3) {
            std::cerr << "Usage: ssi -c \"<code>\" [arguments...]\n";
            return 1;
        }
        std::string code = argv[2];
        std::vector<std::string> pass_args;
        for (int i = 3; i < argc; ++i) pass_args.push_back(argv[i]);
        return ssi::execute(code, pass_args);
    }

    // Expression evaluation: ssi -e "expr"
    if (arg1 == "-e") {
        if (argc < 3) {
            std::cerr << "Usage: ssi -e \"<expression>\"\n";
            return 1;
        }
        std::string expr = argv[2];
        return ssi::evaluateExpression(expr);
    }

    // Build standalone binary: ssi build file.ssi [-o out]
    if (arg1 == "build") {
        if (argc < 3) {
            std::cerr << "Usage: ssi build <file.ssi> [-o output]\n";
            return 1;
        }
        std::string ssi_file = argv[2];
        std::string out_name = "";
        std::vector<std::string> extra_flags;
        for (int i = 3; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "-o" && i + 1 < argc) {
                out_name = argv[++i];
            } else {
                extra_flags.push_back(a);
            }
        }
        return buildBinary(ssi_file, out_name, extra_flags);
    }

    // Default: execute script file
    std::string ssi_file = arg1;
    std::vector<std::string> pass_args;
    for (int i = 2; i < argc; ++i) pass_args.push_back(argv[i]);
    return ssi::executeFile(ssi_file, pass_args);
}
