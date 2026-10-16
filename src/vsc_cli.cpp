// =============================================================================
// VSC - Viss Script Core (Standalone CLI Executable)
// Version 0.3.3 "Flame Princess"
// =============================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <chrono>

#include "vsc.hpp"

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
    std::cout << "VSC - Viss Script Core v" << vsc::VSC_VERSION << " \"" << vsc::VSC_CODENAME << "\"\n\n"
              << "Usage: vsc [options] [script.vsc] [arguments...]\n\n"
              << "Commands & Options:\n"
              << "  <script.vsc|script.ssi> [args...] Execute script immediately (0ms compile overhead)\n"
              << "  -c \"<code>\" [args...]             Run inline code directly (like python -c or bash -c)\n"
              << "  -e \"<expression>\"                 Evaluate expression and print result\n"
              << "  build <script.vsc> [-o bin]       Compile script to standalone native binary\n"
              << "  repl                              Start interactive VSC REPL (or run 'vsc' with no args)\n"
              << "  -v, --version, version            Show version\n"
              << "  -h, --help, help                  Show this help message\n\n"
              << "Examples:\n"
              << "  vsc script.vsc\n"
              << "  vsc -c \"println('Hello from VSC!')\"\n"
              << "  vsc -c \"@os = sys.os(); println(@os)\"\n"
              << "  vsc -e \"math.sqrt(144)\"\n"
              << "  vsc build script.vsc -o myapp\n";
}

int buildBinary(const std::string& script_file, const std::string& out_name, const std::vector<std::string>& extra_flags) {
    if (!fs::exists(script_file)) {
        std::cerr << "[VSC Build Error] File not found: " << script_file << "\n";
        return 1;
    }

    std::ifstream in(script_file);
    std::stringstream buffer; buffer << in.rdbuf();
    std::string script_code = buffer.str();

    std::string viss_code = vsc::transpileVSCtoViss(script_code);

    fs::path p(script_file);
    std::string stem = p.stem().string();
    std::error_code ec;
    fs::path temp_dir = fs::temp_directory_path(ec);
    if (ec) temp_dir = fs::current_path();
    fs::path temp_viss = temp_dir / ("vsc_build_tmp_" + stem + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".viss");

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

    std::cout << "[VSC Builder] Compiling " << script_file << " to native binary via Viss toolchain...\n";
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
        vsc::runRepl();
        return 0;
    }

    std::string arg1 = argv[1];

    if (arg1 == "-h" || arg1 == "--help" || arg1 == "help") {
        printHelp();
        return 0;
    }

    if (arg1 == "-v" || arg1 == "--version" || arg1 == "version") {
        std::cout << "VSC v" << vsc::VSC_VERSION << " \"" << vsc::VSC_CODENAME << "\"\n";
        return 0;
    }

    if (arg1 == "repl") {
        vsc::runRepl();
        return 0;
    }

    // Inline code execution: vsc -c "code"
    if (arg1 == "-c") {
        if (argc < 3) {
            std::cerr << "Usage: vsc -c \"<code>\" [arguments...]\n";
            return 1;
        }
        std::string code = argv[2];
        std::vector<std::string> pass_args;
        for (int i = 3; i < argc; ++i) pass_args.push_back(argv[i]);
        return vsc::execute(code, pass_args);
    }

    // Expression evaluation: vsc -e "expr"
    if (arg1 == "-e") {
        if (argc < 3) {
            std::cerr << "Usage: vsc -e \"<expression>\"\n";
            return 1;
        }
        std::string expr = argv[2];
        return vsc::evaluateExpression(expr);
    }

    // Build standalone binary: vsc build file.vsc [-o out]
    if (arg1 == "build") {
        if (argc < 3) {
            std::cerr << "Usage: vsc build <file.vsc> [-o output]\n";
            return 1;
        }
        std::string script_file = argv[2];
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
        return buildBinary(script_file, out_name, extra_flags);
    }

    // Default: execute script file
    std::string script_file = arg1;
    std::vector<std::string> pass_args;
    for (int i = 2; i < argc; ++i) pass_args.push_back(argv[i]);
    return vsc::executeFile(script_file, pass_args);
}
