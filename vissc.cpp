#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <cstdlib>

namespace fs = std::filesystem;

// Helper to check if string starts with another
bool startsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

// Trim whitespace from string
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Split string by delimiter
std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(trim(token));
    }
    return tokens;
}

// Replace all occurrences of a substring
std::string replaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

// Count occurrences of a character
size_t countChar(const std::string& str, char c) {
    return std::count(str.begin(), str.end(), c);
}

// Validation function for Viss syntax
void validateVissSyntax(const std::string& code, const std::string& filename) {
    // 1. Check unbalanced curly braces
    size_t openCount = countChar(code, '{');
    size_t closeCount = countChar(code, '}');
    if (openCount != closeCount) {
        std::cerr << "Error in " << filename << ": Unbalanced curly braces detected.\n";
        std::cerr << "Details: Found " << openCount << " open braces '{' and " << closeCount << " close braces '}'.\n";
        std::cerr << "Please check your block closures.\n";
        std::exit(1);
    }

    std::istringstream stream(code);
    std::string line;
    size_t lineNum = 0;

    std::regex ifRegex(R"(\bif\s*\()");
    std::regex elseRegex(R"(\belse\b)");
    std::regex funcRegex(R"(\bfunc\s+)");
    std::regex loopRegex(R"(\bloop\s*\()");
    std::regex varDeclRegex(R"(\b(Str|Int|Dec|Bool)\s+([a-zA-Z0-9_]+)\s*=)");

    while (std::getline(stream, line)) {
        lineNum++;
        std::string stripped = trim(line);

        if (stripped.empty() || startsWith(stripped, "//")) {
            continue;
        }

        // Check ? prefix on conditionals
        if (std::regex_search(stripped, ifRegex) && !startsWith(stripped, "?if") && stripped.find("+cpp") == std::string::npos) {
            std::cerr << "Syntax Error in " << filename << ":" << lineNum << ":\n";
            std::cerr << "  " << line << "\n";
            std::cerr << "  " << std::string(line.length(), '^') << "\n";
            std::cerr << "  Detail: Logical conditionals must be prefixed with '?'. Did you mean '?if'?\n";
            std::exit(1);
        }

        if (std::regex_search(stripped, elseRegex) && stripped.find("?else") == std::string::npos && stripped.find("+cpp") == std::string::npos) {
            std::cerr << "Syntax Error in " << filename << ":" << lineNum << ":\n";
            std::cerr << "  " << line << "\n";
            std::cerr << "  " << std::string(line.length(), '^') << "\n";
            std::cerr << "  Detail: Logical else statements must be prefixed with '?'. Did you mean '?else'?\n";
            std::exit(1);
        }

        // Check ! prefix on blocks
        if (std::regex_search(stripped, funcRegex) && !startsWith(stripped, "!func") && stripped.find("+cpp") == std::string::npos && stripped.find("!") == std::string::npos) {
            std::cerr << "Syntax Error in " << filename << ":" << lineNum << ":\n";
            std::cerr << "  " << line << "\n";
            std::cerr << "  " << std::string(line.length(), '^') << "\n";
            std::cerr << "  Detail: Function blocks must be prefixed with '!'. Did you mean '!func'?\n";
            std::exit(1);
        }

        if (std::regex_search(stripped, loopRegex) && !startsWith(stripped, "!loop") && stripped.find("+cpp") == std::string::npos) {
            std::cerr << "Syntax Error in " << filename << ":" << lineNum << ":\n";
            std::cerr << "  " << line << "\n";
            std::cerr << "  " << std::string(line.length(), '^') << "\n";
            std::cerr << "  Detail: Loop blocks must be prefixed with '!'. Did you mean '!loop'?\n";
            std::exit(1);
        }

        // Check @ prefix on variable declarations
        std::smatch varMatch;
        if (std::regex_search(stripped, varMatch, varDeclRegex) && stripped.find("+cpp") == std::string::npos) {
            std::string varType = varMatch[1].str();
            std::string varName = varMatch[2].str();
            std::cerr << "Syntax Error in " << filename << ":" << lineNum << ":\n";
            std::cerr << "  " << line << "\n";
            std::cerr << "  " << std::string(line.length(), '^') << "\n";
            std::cerr << "  Detail: Variables in Viss must start with a '@' prefix. Did you mean '" << varType << " @" << varName << "'?\n";
            std::exit(1);
        }
    }
}

// Transpilation for a single line of Viss code
std::string transpileLine(std::string line, int lineNum, const std::string& filename) {
    std::string stripped = trim(line);
    if (stripped.empty() || startsWith(stripped, "//")) {
        return line;
    }

    // 1. Imports and standard libraries
    line = std::regex_replace(line, std::regex(R"(@use\s+<IOstream>\s+for\s+\*)"), "#include \"vissrt.hpp\"\nusing namespace viss;");
    
    // Custom use lists
    std::smatch useListMatch;
    std::regex useListRegex(R"(@use\s+([a-zA-Z0-9_]+)\s+for\s+([a-zA-Z0-9_,\s]+))");
    if (std::regex_search(line, useListMatch, useListRegex)) {
        std::string lib = useListMatch[1].str();
        std::vector<std::string> funcs = split(useListMatch[2].str(), ',');
        std::string replacement = "";
        for (const auto& f : funcs) {
            replacement += "using viss::" + lib + "::" + f + "; ";
        }
        line = std::regex_replace(line, useListRegex, replacement);
    }
    
    line = std::regex_replace(line, std::regex(R"(@use\s+([a-zA-Z0-9_]+)\s+for\s+\*)"), "using namespace viss::$1;");
    line = std::regex_replace(line, std::regex(R"(@use\s+\+cpp\s+<([^>]+)>)"), "#include <$1>");
    line = std::regex_replace(line, std::regex(R"(@use\s+\+cpp\s+"([^"]+)")"), "#include \"$1\"");

    // 2. Block definitions starting with !
    line = std::regex_replace(line, std::regex(R"(!func\s+main\s*\(\s*\))"), "int main()");
    line = std::regex_replace(line, std::regex(R"(!func\s+([a-zA-Z0-9_]+)\s*\(([^)]*)\))"), "auto $1($2)");
    line = std::regex_replace(line, std::regex(R"(!loop\s*\(([^)]+)\))"), "while ($1)");
    line = std::regex_replace(line, std::regex(R"(!class\s+([a-zA-Z0-9_]+))"), "struct $1");
    line = std::regex_replace(line, std::regex(R"(!space\s+([a-zA-Z0-9_]+))"), "namespace $1");

    // 3. Logical actions starting with ?
    line = std::regex_replace(line, std::regex(R"(\?if\s*\(([^)]+)\))"), "if ($1)");
    line = std::regex_replace(line, std::regex(R"(\?else)"), "else");
    line = std::regex_replace(line, std::regex(R"(\?try)"), "try");
    line = std::regex_replace(line, std::regex(R"(\?catch\s*\(\s*Error\s+@([a-zA-Z0-9_]+)\s*\)\s*\{)"), "catch (const std::exception& _std_err) { viss::Error $1(_std_err.what());");
    line = std::regex_replace(line, std::regex(R"(\?catch\s*\{)"), "catch (...) {");

    // 4. Pure C++ block prefix
    line = std::regex_replace(line, std::regex(R"(\+cpp)"), "");

    // 5.5 Custom function references
    line = std::regex_replace(line, std::regex(R"(\bCustom\s+@([a-zA-Z0-9_]+)\s*=\s*([a-zA-Z0-9_\.\:]+)(?:\(\))?)"), 
        "auto $1 = [](auto&&... args) { return $2(std::forward<decltype(args)>(args)...); }");

    // 5. Type declarations
    line = std::regex_replace(line, std::regex(R"(\b([a-zA-Z0-9_<>\*&]+)\s+@([a-zA-Z0-9_]+))"), "$1 $2");

    // 6. Leftover variable references
    line = std::regex_replace(line, std::regex(R"(@([a-zA-Z0-9_]+))"), "$1");

    // 8. Namespace mappings
    line = std::regex_replace(line, std::regex(R"(\bio\.)"), "viss::io::");
    line = std::regex_replace(line, std::regex(R"(\bfs\.)"), "viss::fs::");
    line = std::regex_replace(line, std::regex(R"(\bgfx\.)"), "viss::gfx::");
    line = std::regex_replace(line, std::regex(R"(\bsys\.)"), "viss::sys::");
    line = std::regex_replace(line, std::regex(R"(\bgl\.)"), "viss::gl::");
    line = std::regex_replace(line, std::regex(R"(\bvk\.)"), "viss::vk::");
    line = std::regex_replace(line, std::regex(R"(\bnet\.)"), "viss::net::");
    line = std::regex_replace(line, std::regex(R"(\bthread\.)"), "viss::thread::");
    line = std::regex_replace(line, std::regex(R"(\bmath\.)"), "viss::math::");
    line = std::regex_replace(line, std::regex(R"(\btime\.)"), "viss::time::");
    line = std::regex_replace(line, std::regex(R"(\bstr\.)"), "viss::str::");
    line = std::regex_replace(line, std::regex(R"(\benv\.)"), "viss::env::");

    // 9. Conversions
    line = std::regex_replace(line, std::regex(R"(\btoInt\b)"), "viss::toInt");
    line = std::regex_replace(line, std::regex(R"(\btoDec\b)"), "viss::toDec");
    line = std::regex_replace(line, std::regex(R"(\btoStr\b)"), "viss::toStr");

    return line;
}

// Transpile full source code string
std::string transpile(const std::string& vissCode, const std::string& filename) {
    // 1. Extract string literals
    std::vector<std::string> stringLiterals;
    std::string processed = vissCode;
    std::regex stringRegex(R"("(?:[^"\\]|\\.)*")");
    
    std::smatch match;
    std::string temp = processed;
    size_t offset = 0;
    while (std::regex_search(temp, match, stringRegex)) {
        stringLiterals.push_back(match.str());
        std::string placeholder = "__VISS_STR_LIT_" + std::to_string(stringLiterals.size() - 1) + "__";
        processed.replace(processed.find(match.str()), match.str().length(), placeholder);
        temp = processed;
    }

    // 2. Replace using blocks
    std::regex usingBlockRegex(R"(using\s+[a-zA-Z0-9_]+\s*\{\s*return\s+([^;]+);\s*\})");
    processed = std::regex_replace(processed, usingBlockRegex, "return $1;");

    // 3. Process line-by-line
    std::istringstream stream(processed);
    std::string line;
    std::vector<std::string> cppLines;
    std::vector<std::string> blockStack;
    int lineIndex = 0;

    std::regex classDefRegex(R"(!class\s+[a-zA-Z0-9_]+)");

    while (std::getline(stream, line)) {
        lineIndex++;
        std::string stripped = trim(line);
        
        bool isClassOpen = std::regex_search(stripped, classDefRegex);
        size_t openBraces = countChar(stripped, '{');
        size_t closeBraces = countChar(stripped, '}');

        for (size_t i = 0; i < openBraces; i++) {
            if (isClassOpen) {
                blockStack.push_back("class");
                isClassOpen = false;
            } else {
                blockStack.push_back("other");
            }
        }

        std::string transpiled = transpileLine(line, lineIndex, filename);

        for (size_t i = 0; i < closeBraces; i++) {
            if (!blockStack.empty()) {
                std::string type = blockStack.back();
                blockStack.pop_back();
                if (type == "class") {
                    size_t pos = transpiled.rfind('}');
                    if (pos != std::string::npos) {
                        transpiled.replace(pos, 1, "};");
                    }
                }
            }
        }

        if (!transpiled.empty() && !startsWith(transpiled, "#include") && transpiled.find("using namespace") == std::string::npos) {
            cppLines.push_back("#line " + std::to_string(lineIndex) + " \"" + filename + "\"");
        }
        cppLines.push_back(transpiled);
    }

    std::string cppCode = "";
    for (const auto& l : cppLines) {
        cppCode += l + "\n";
    }

    // 4. Restore string literals
    for (size_t i = 0; i < stringLiterals.size(); ++i) {
        std::string placeholder = "__VISS_STR_LIT_" + std::to_string(i) + "__";
        cppCode = replaceAll(cppCode, placeholder, stringLiterals[i]);
    }

    return cppCode;
}

// Function to resolve VCM imports recursively
std::string processVcmImports(const std::string& vissCode, const std::string& currentDir, 
                             std::vector<std::string>& includes, std::vector<std::string>& usings) {
    std::regex vcmRegex(R"(^\s*&vcm\s+import\s+([a-zA-Z0-9_\.\-]+)\s+as\s+@([a-zA-Z0-9_]+)\s+for\s+([\*\w,\s]+)\s*$)");
    std::istringstream stream(vissCode);
    std::string line;
    std::string newCode = "";

    while (std::getline(stream, line)) {
        std::smatch match;
        std::string stripped = trim(line);
        if (std::regex_match(stripped, match, vcmRegex)) {
            std::string moduleName = match[1].str();
            std::string alias = match[2].str();
            std::string importsStr = trim(match[3].str());

            // Remove .viss extension if present
            if (moduleName.size() > 5 && moduleName.compare(moduleName.size() - 5, 5, ".viss") == 0) {
                moduleName = moduleName.substr(0, moduleName.size() - 5);
            }

            fs::path vissPath = fs::path(currentDir) / (moduleName + ".viss");
            fs::path hppPath;

            if (!fs::exists(vissPath)) {
                vissPath = fs::path(currentDir) / "viss_modules" / (moduleName + ".viss");
                hppPath = fs::path(currentDir) / "viss_modules" / (moduleName + ".hpp");
            } else {
                hppPath = fs::path(currentDir) / (moduleName + ".hpp");
            }

            if (fs::exists(vissPath)) {
                std::cout << "[Viss Compiler] Processing imported module: " << moduleName << ".viss\n";
                
                std::ifstream modFile(vissPath);
                std::stringstream buffer;
                buffer << modFile.rdbuf();
                std::string modCode = buffer.str();
                modFile.close();

                std::vector<std::string> nestedIncludes, nestedUsings;
                std::string processedModCode = processVcmImports(modCode, currentDir, nestedIncludes, nestedUsings);
                std::string transpiledMod = transpile(processedModCode, moduleName + ".viss");

                std::string nestedHeaderAdditions = "";
                for (const auto& inc : nestedIncludes) nestedHeaderAdditions += inc + "\n";
                for (const auto& usg : nestedUsings) nestedHeaderAdditions += usg + "\n";

                std::ofstream hppFile(hppPath);
                hppFile << "#pragma once\n\n";
                hppFile << nestedHeaderAdditions << "\n";
                hppFile << "namespace " << alias << " {\n";
                hppFile << transpiledMod << "\n";
                hppFile << "}\n";
                hppFile.close();

                includes.push_back("#include \"" + moduleName + ".hpp\"");
                if (importsStr == "*") {
                    usings.push_back("using namespace " + alias + ";");
                } else {
                    std::vector<std::string> funcs = split(importsStr, ',');
                    for (const auto& f : funcs) {
                        usings.push_back("using " + alias + "::" + f + ";");
                    }
                }
            } else {
                std::cerr << "[Viss Compiler] Warning: Custom module file '" << vissPath.string() << "' not found.\n";
            }
            continue;
        }
        newCode += line + "\n";
    }
    return newCode;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Viss Programming Language Native Compiler\n";
        std::cout << "Usage: vissc <file.viss>\n";
        return 1;
    }

    std::string vissFile = argv[1];
    if (!fs::exists(vissFile)) {
        std::cerr << "Error: File '" << vissFile << "' not found.\n";
        return 1;
    }

    fs::path vissPath(vissFile);
    std::string base = vissPath.stem().string();
    std::string currentDir = fs::absolute(vissPath).parent_path().string();
    
    std::string cppFile = (vissPath.parent_path() / (base + ".cpp")).string();
    std::string exeFile;
#ifdef _WIN32
    exeFile = (vissPath.parent_path() / (base + ".exe")).string();
#else
    exeFile = (vissPath.parent_path() / base).string();
#endif

    std::cout << "[Viss Compiler] Transpiling " << vissFile << " to " << cppFile << "...\n";

    std::ifstream file(vissFile);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string vissCode = buffer.str();
    file.close();

    // 1. Run Viss Syntax Validation
    validateVissSyntax(vissCode, vissPath.filename().string());

    // 2. Pre-process custom Viss module imports (VCM)
    std::vector<std::string> vcmIncludes, vcmUsings;
    std::string processedVissCode = processVcmImports(vissCode, currentDir, vcmIncludes, vcmUsings);

    // 3. Transpile Viss to C++
    std::string cppCode = transpile(processedVissCode, vissPath.filename().string());

    // 4. Inject VCM Headers right after standard include namespace block
    if (!vcmIncludes.empty()) {
        std::string additions = "\n";
        for (const auto& inc : vcmIncludes) additions += inc + "\n";
        for (const auto& usg : vcmUsings) additions += usg + "\n";

        size_t pos = cppCode.find("using namespace viss;");
        if (pos != std::string::npos) {
            cppCode.insert(pos + 21, additions);
        } else {
            cppCode = additions + cppCode;
        }
    }

    std::ofstream outFile(cppFile);
    outFile << cppCode;
    outFile.close();

    std::cout << "[Viss Compiler] Transpilation complete.\n";

    // 5. Detect and execute native C++ compiler
    std::string compilerFound = "";
    std::vector<std::string> compilers = {"g++", "clang++"};
#ifdef _WIN32
    compilers.push_back("cl");
#endif

    for (const auto& c : compilers) {
        std::string checkCmd;
#ifdef _WIN32
        checkCmd = "where " + c + " >nul 2>nul";
#else
        checkCmd = "which " + c + " >/dev/null 2>&1";
#endif
        if (std::system(checkCmd.c_str()) == 0) {
            compilerFound = c;
            break;
        }
    }

    if (!compilerFound.empty()) {
        std::cout << "[Viss Compiler] Found compiler: '" << compilerFound << "'. Compiling to executable...\n";
        std::string compileCmd;
        if (compilerFound == "g++" || compilerFound == "clang++") {
            compileCmd = compilerFound + " -std=c++17 -g \"" + cppFile + "\" -o \"" + exeFile + "\"";
        } else if (compilerFound == "cl") {
            compileCmd = "cl /std:c++17 /Zi /EHsc \"" + cppFile + "\" /Fe:\"" + exeFile + "\"";
        }
        
        int result = std::system(compileCmd.c_str());
        if (result == 0) {
            std::cout << "[Viss Compiler] Success! Executable created: " << exeFile << "\n";
        } else {
            std::cerr << "[Viss Compiler] Compilation failed.\n";
        }
    } else {
        std::cout << "[Viss Compiler] Note: No C++ compiler (g++, clang++, cl) was found in PATH.\n";
        std::cout << "[Viss Compiler] You can compile the generated C++ file manually using:\n";
        std::cout << "  g++ -std=c++17 \"" << cppFile << "\" -o \"" << exeFile << "\"\n";
    }

    return 0;
}
