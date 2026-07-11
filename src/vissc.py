import sys
import os
import re
import subprocess
import shutil

def transpile_line(line, line_num, filename, string_literals):
    # Skip empty lines or pure comments
    if not line.strip() or line.strip().startswith('//'):
        return line

    # 1. Imports and standard libraries (Moved to end to prevent namespace replacements on header filenames)

    # 2. Handle block definitions (Functions, Loops, Classes, Spaces) starting with !
    # Main function
    line = re.sub(r'!func\s+main\s*\(\s*\)', 'int main()', line)
    # Normal functions (deduced return type via auto)
    line = re.sub(r'!\$func\s+([a-zA-Z0-9_]+)\s*\(([^)]*)\)\s*\{', r'inline auto \1(\2) { return viss::getGlobalThreadPool().enqueue([=]() {', line)
    line = re.sub(r'!func\s+([a-zA-Z0-9_]+)\s*\(([^)]*)\)', r'inline auto \1(\2)', line)
    # Loop block
    line = re.sub(r'!loop\s*\(([^)]+)\)', r'while (\1)', line)
    # Classes
    line = re.sub(r'!class\s+([a-zA-Z0-9_]+)', r'struct \1', line)
    # Spaces (namespaces)
    line = re.sub(r'!space\s+([a-zA-Z0-9_]+)', r'namespace \1', line)

    # 3. Handle logical actions (If/Else, Try/Catch, Match) starting with ?
    line = re.sub(r'\?match\s*\(([^)]+)\)', r'switch (\1)', line)
    line = re.sub(r'\?else\s*=>\s*\{', 'default: {', line)
    line = re.sub(r'([^=]+)\s*=>\s*\{', r'case \1: {', line)
    line = re.sub(r'\?if\s*\(([^)]+)\)', r'if (\1)', line)
    line = re.sub(r'\?else', r'else', line)
    line = re.sub(r'\?try', 'try', line)
    line = re.sub(r'\?catch\s*\(\s*Error\s+@([a-zA-Z0-9_]+)\s*\)\s*\{', r'catch (const std::exception& _std_err) { viss::Error \1(_std_err.what());', line)
    line = re.sub(r'\?catch\s*\{', 'catch (...) {', line)
    line = re.sub(r'\$await\s+([^;]+)', r'(\1).get()', line)

    # 4. Handle pure C++ block prefix +cpp
    line = re.sub(r'\+cpp', '', line)

    # 5.5. Custom function references: Custom @alias = function
    line = re.sub(
        r'\bCustom\s+@([a-zA-Z0-9_]+)\s*=\s*([a-zA-Z0-9_\.\:]+)(?:\(\))?',
        r'auto \1 = [](auto&&... args) { return \2(std::forward<decltype(args)>(args)...); }',
        line
    )

    # 5. Type declarations: Type @name -> Type name (including templates like List<Point>)
    line = re.sub(r'\b([a-zA-Z0-9_<>\*&]+)\s+@([a-zA-Z0-9_]+)', r'\1 \2', line)

    # 6. Leftover variable references: @name -> name
    line = re.sub(r'@([a-zA-Z0-9_]+)', r'\1', line)

    # 8. Namespace mappings (io., fs., gfx., sys., gl., vk., net., thread., math., time., str., env.)
    line = re.sub(r'([^/"<]|^)\bio\.', r'\1viss::io::', line)
    line = re.sub(r'([^/"<]|^)\bfs\.', r'\1viss::fs::', line)
    line = re.sub(r'([^/"<]|^)\bgfx\.', r'\1viss::gfx::', line)
    line = re.sub(r'([^/"<]|^)\bsys\.', r'\1viss::sys::', line)
    line = re.sub(r'([^/"<]|^)\bgl\.', r'\1viss::gl::', line)
    line = re.sub(r'([^/"<]|^)\bvk\.', r'\1viss::vk::', line)
    line = re.sub(r'([^/"<]|^)\bnet\.', r'\1viss::net::', line)
    line = re.sub(r'([^/"<]|^)\bthread\.', r'\1viss::thread::', line)
    line = re.sub(r'([^/"<]|^)\bmath\.', r'\1viss::math::', line)
    line = re.sub(r'([^/"<]|^)\btime\.', r'\1viss::time::', line)
    line = re.sub(r'([^/"<]|^)\bstr\.', r'\1viss::str::', line)
    line = re.sub(r'([^/"<]|^)\benv\.', r'\1viss::env::', line)
    line = re.sub(r'\bwebviss\.', 'viss::webviss::', line)

    # 9. Type conversion helpers
    line = re.sub(r'\btoInt\b', 'viss::toInt', line)
    line = re.sub(r'\btoDec\b', 'viss::toDec', line)
    line = re.sub(r'\btoStr\b', 'viss::toStr', line)

    # 10. Handle C++ imports and library use mappings
    line = re.sub(r'@use\s+<?IOstream>?\s+for\s+\*', '#include "libs/vissrt.hpp"\nusing namespace viss;', line)
    
    def translate_use_list(match):
        lib = match.group(1)
        funcs = match.group(2).split(',')
        return f'#include "libs/std/{lib}.hpp"\n' + ' '.join([f'using viss::{lib}::{f.strip()};' for f in funcs])
        
    line = re.sub(r'@use\s+([a-zA-Z0-9_]+)\s+for\s+([a-zA-Z0-9_,\s]+)', translate_use_list, line)
    line = re.sub(r'@use\s+([a-zA-Z0-9_]+)\s+for\s+\*', r'#include "libs/std/\1.hpp"\nusing namespace viss::\1;', line)
    line = re.sub(r'@use\s+\+cpp\s+<([^>]+)>', r'#include <\1>', line)
    line = re.sub(r'@use\s+\+cpp\s+"([^"]+)"', r'#include "\1"', line)

    return line

def needs_semicolon(line):
    s = line.strip()
    if not s or s.startswith("//"):
        return False
    
    if "__VISS_COMMENT_" in s:
        return False
        
    last_char = s[-1]
    if last_char in ('{', '}', ';', ',', ':'):
        return False
        
    if (s.startswith("@use") or s.startswith("&vcm") or 
        s.startswith("+cpp") or s.startswith("using") or 
        s.startswith("#")):
        return False
        
    if (s.startswith("!func") or s.startswith("!loop") or 
        s.startswith("!class") or s.startswith("!space")):
        return False
        
    if (s.startswith("?if") or s.startswith("?else") or 
        s.startswith("?try") or s.startswith("?catch") or
        s.startswith("?match")):
        return False
        
    return True

def transpile(viss_code, filename):
    # State machine to extract string literals first (to prevent replacing contents of strings)
    string_literals = []
    def replace_str(match):
        string_literals.append(match.group(0))
        return f"__VISS_STR_LIT_{len(string_literals)-1}__"

    # Match double quoted string literals
    processed_code = re.sub(r'"(?:[^"\\]|\\.)*"', replace_str, viss_code)

    # Extract comments to protect them from regex translation
    comments = []
    def replace_comment(match):
        comments.append(match.group(0))
        return f" __VISS_COMMENT_{len(comments)-1}__ "
        
    processed_code = re.sub(r'//.*|/\*[\s\S]*?\*/', replace_comment, processed_code)

    # Replace using blocks (potentially multiline)
    processed_code = re.sub(
        r'using\s+[a-zA-Z0-9_]+\s*\{\s*return\s+([^;]+);\s*\}',
        r'return \1;',
        processed_code,
        flags=re.MULTILINE
    )

    lines = processed_code.split('\n')
    cpp_lines = []
    block_stack = []

    for idx, line in enumerate(lines):
        if needs_semicolon(line):
            line += ";"
        # Check if this line opens a class definition, async function, or match block
        stripped = line.strip()
        is_class_open = re.search(r'!class\s+[a-zA-Z0-9_]+', stripped) is not None
        is_async_func_open = re.search(r'!\$func\s+[a-zA-Z0-9_]+', stripped) is not None
        is_match_open = re.search(r'\?match\s*\(', stripped) is not None

        # Count open and close braces to track block structures
        open_braces = stripped.count('{')
        close_braces = stripped.count('}')

        for _ in range(open_braces):
            if is_class_open:
                block_stack.append('class')
                is_class_open = False
            elif is_async_func_open:
                block_stack.append('async_func')
                is_async_func_open = False
            elif is_match_open:
                block_stack.append('match')
                is_match_open = False
            elif block_stack and block_stack[-1] == 'match':
                block_stack.append('match_case')
            else:
                block_stack.append('other')

        # Transpile this line
        transpiled = transpile_line(line, idx + 1, filename, string_literals)

        # Handle closing braces
        for _ in range(close_braces):
            if block_stack:
                block_type = block_stack.pop()
                if block_type == 'class':
                    if '}' in transpiled:
                        parts = transpiled.rsplit('}', 1)
                        transpiled = '};'.join(parts)
                elif block_type == 'async_func':
                    if '}' in transpiled:
                        parts = transpiled.rsplit('}', 1)
                        transpiled = '}); }'.join(parts)
                elif block_type == 'match_case':
                    if '}' in transpiled:
                        parts = transpiled.rsplit('}', 1)
                        transpiled = 'break; }'.join(parts)
        
        # Add #line directive for debugging (points back to .viss file!)
        if transpiled.strip() and not transpiled.startswith('#include') and not transpiled.startswith('using namespace'):
            cpp_lines.append(f'#line {idx + 1} "{filename}"')
        
        cpp_lines.append(transpiled)

    cpp_code = '\n'.join(cpp_lines)

    # Restore comments
    for i, com in enumerate(comments):
        cpp_code = cpp_code.replace(f" __VISS_COMMENT_{i}__ ", com)

    # Restore string literals
    for i, lit in enumerate(string_literals):
        if '\n' in lit or '\r' in lit:
            if len(lit) >= 2 and lit.startswith('"') and lit.endswith('"'):
                lit = 'R"viss(' + lit[1:-1] + ')viss"'
        cpp_code = cpp_code.replace(f"__VISS_STR_LIT_{i}__", lit)

    return cpp_code

def process_vcm_imports(viss_code, current_dir):
    includes = []
    usings = []
    
    # Pattern: &vcm import mymodule as @alias for * / imports
    pattern = r'^\s*&vcm\s+import\s+([a-zA-Z0-9_\.\-]+)\s+as\s+@([a-zA-Z0-9_]+)\s+for\s+([\*\w,\s]+)\s*$'
    
    lines = viss_code.split('\n')
    new_lines = []
    for line in lines:
        match = re.match(pattern, line.strip())
        if match:
            module_name = match.group(1)
            alias = match.group(2)
            imports_str = match.group(3).strip()
            
            base_module, _ = os.path.splitext(module_name)
            viss_path = os.path.join(current_dir, base_module + ".viss")
            
            # Check viss_modules if not found in current directory
            if not os.path.exists(viss_path):
                viss_path = os.path.join(current_dir, "viss_modules", base_module + ".viss")
                hpp_path = os.path.join(current_dir, "viss_modules", base_module + ".hpp")
            else:
                hpp_path = os.path.join(current_dir, base_module + ".hpp")
            
            if os.path.exists(viss_path):
                print(f"[Viss Compiler] Processing imported module: {base_module}.viss")
                with open(viss_path, 'r', encoding='utf-8') as f:
                    mod_code = f.read()
                
                # Recursively resolve any VCM imports in the dependency
                mod_code_processed, nested_includes, nested_usings = process_vcm_imports(mod_code, current_dir)
                
                transpiled_mod = transpile(mod_code_processed, base_module + ".viss")
                
                if nested_includes:
                    additions = "\n".join(nested_includes) + "\n" + "\n".join(nested_usings) + "\n"
                    transpiled_mod = additions + transpiled_mod
                
                # Wrap the compiled module in the specified alias namespace
                namespaced_code = f"#pragma once\n\nnamespace {alias} {{\n{transpiled_mod}\n}}"
                
                with open(hpp_path, 'w', encoding='utf-8') as f:
                    f.write(namespaced_code)
                
                includes.append(f'#include "{base_module}.hpp"')
                if imports_str == '*':
                    usings.append(f"using namespace {alias};")
                else:
                    funcs = [f.strip() for f in imports_str.split(',')]
                    for f in funcs:
                        usings.append(f"using {alias}::{f};")
            else:
                print(f"[Viss Compiler] Warning: Custom module file '{viss_path}' not found.")
            continue
        new_lines.append(line)
        
    return '\n'.join(new_lines), includes, usings

def sanitize_code(code):
    clean = list(code)
    in_string = False
    in_line_comment = False
    in_block_comment = False
    i = 0
    n = len(clean)
    while i < n:
        if in_block_comment:
            if i + 1 < n and clean[i] == '*' and clean[i+1] == '/':
                clean[i] = ' '
                clean[i+1] = ' '
                in_block_comment = False
                i += 2
                continue
            if clean[i] not in ('\n', '\r'):
                clean[i] = ' '
            i += 1
            continue
            
        if in_line_comment:
            if clean[i] in ('\n', '\r'):
                in_line_comment = False
            else:
                clean[i] = ' '
            i += 1
            continue
            
        if in_string:
            if clean[i] == '\\' and i + 1 < n:
                clean[i] = ' '
                if clean[i+1] not in ('\n', '\r'):
                    clean[i+1] = ' '
                i += 2
                continue
            if clean[i] == '"':
                clean[i] = ' '
                in_string = False
                i += 1
                continue
            if clean[i] not in ('\n', '\r'):
                clean[i] = ' '
            i += 1
            continue
            
        if i + 1 < n and clean[i] == '/' and clean[i+1] == '*':
            clean[i] = ' '
            clean[i+1] = ' '
            in_block_comment = True
            i += 2
            continue
            
        if i + 1 < n and clean[i] == '/' and clean[i+1] == '/':
            clean[i] = ' '
            clean[i+1] = ' '
            in_line_comment = True
            i += 2
            continue
            
        if clean[i] == '"':
            clean[i] = ' '
            in_string = True
            
        i += 1
    return "".join(clean)

def validate_viss_syntax(viss_code, filename):
    # 1. Check unbalanced curly braces
    open_count = viss_code.count('{')
    close_count = viss_code.count('}')
    if open_count != close_count:
        print(f"Error in {filename}: Unbalanced curly braces detected.")
        print(f"Details: Found {open_count} open braces '{{' and {close_count} close braces '}}'.")
        print("Please check your block closures.")
        sys.exit(1)
        
    sanitized = sanitize_code(viss_code)
    original_lines = viss_code.split('\n')
    sanitized_lines = sanitized.split('\n')
    
    # 2. Check prefix violations line by line
    for idx, sanitized_line in enumerate(sanitized_lines):
        stripped = sanitized_line.strip()
        line_num = idx + 1
        original_line = original_lines[idx]
        
        if not stripped:
            continue
            
        # Check: logical conditionals must start with ?
        if re.search(r'\bif\s*\(', stripped) and not stripped.startswith('?if') and not '+cpp' in stripped:
            print(f"Syntax Error in {filename}:{line_num}:")
            print(f"  {original_line}")
            print(f"  {'^' * len(original_line)}")
            print("  Detail: Logical conditionals must be prefixed with '?'. Did you mean '?if'?")
            sys.exit(1)
            
        if re.search(r'\belse\b', stripped) and not re.search(r'\?else\b', stripped) and not '+cpp' in stripped:
            print(f"Syntax Error in {filename}:{line_num}:")
            print(f"  {original_line}")
            print(f"  {'^' * len(original_line)}")
            print("  Detail: Logical else statements must be prefixed with '?'. Did you mean '?else'?")
            sys.exit(1)
            
        # Check: flow blocks must start with !
        if re.search(r'\bfunc\s+', stripped) and not stripped.startswith('!func') and not '+cpp' in stripped and not '!' in stripped:
            print(f"Syntax Error in {filename}:{line_num}:")
            print(f"  {original_line}")
            print(f"  {'^' * len(original_line)}")
            print("  Detail: Function blocks must be prefixed with '!'. Did you mean '!func'?")
            sys.exit(1)
            
        if re.search(r'\bloop\s*\(', stripped) and not stripped.startswith('!loop') and not '+cpp' in stripped:
            print(f"Syntax Error in {filename}:{line_num}:")
            print(f"  {original_line}")
            print(f"  {'^' * len(original_line)}")
            print("  Detail: Loop blocks must be prefixed with '!'. Did you mean '!loop'?")
            sys.exit(1)
            
        # Check: Variable declaration without @ prefix
        m_var = re.search(r'\b(Str|Int|Dec|Bool)\s+([a-zA-Z0-9_]+)\s*=', stripped)
        if m_var and not '+cpp' in stripped:
            var_type = m_var.group(1)
            var_name = m_var.group(2)
            print(f"Syntax Error in {filename}:{line_num}:")
            print(f"  {original_line}")
            print(f"  {'^' * len(original_line)}")
            print(f"  Detail: Variables in Viss must start with a '@' prefix. Did you mean '{var_type} @{var_name}'?")
            sys.exit(1)

def main():
    if len(sys.argv) < 2:
        print("Usage: python vissc.py <file.viss>")
        sys.exit(1)

    viss_file = sys.argv[1]
    if not os.path.exists(viss_file):
        print(f"Error: File '{viss_file}' not found.")
        sys.exit(1)

    base, _ = os.path.splitext(viss_file)
    cpp_file = base + ".cpp"
    exe_file = base + ".exe" if os.name == 'nt' else base

    print(f"[Viss Compiler] Transpiling {viss_file} to {cpp_file}...")

    with open(viss_file, 'r', encoding='utf-8') as f:
        viss_code = f.read()

    # Run Viss Syntax Validation
    validate_viss_syntax(viss_code, os.path.basename(viss_file))

    # Pre-process custom Viss module imports
    viss_dir = os.path.dirname(os.path.abspath(viss_file))
    processed_viss_code, vcm_includes, vcm_usings = process_vcm_imports(viss_code, viss_dir)

    cpp_code = transpile(processed_viss_code, os.path.basename(viss_file))

    # Inject VCM headers
    if vcm_includes:
        additions = "\n" + "\n".join(vcm_includes) + "\n" + "\n".join(vcm_usings) + "\n"
        if 'using namespace viss;' in cpp_code:
            parts = cpp_code.split('using namespace viss;', 1)
            cpp_code = parts[0] + 'using namespace viss;\n' + additions + parts[1]
        else:
            cpp_code = additions + cpp_code

    with open(cpp_file, 'w', encoding='utf-8') as f:
        f.write(cpp_code)

    print("[Viss Compiler] Transpilation complete.")

    # Try to compile using g++ or clang++ or cl
    compilers = ['g++', 'clang++', 'cl']
    compiler_found = None
    for c in compilers:
        if shutil.which(c):
            compiler_found = c
            break

    if compiler_found:
        print(f"[Viss Compiler] Found compiler: '{compiler_found}'. Compiling to executable...")
        try:
            if compiler_found in ['g++', 'clang++']:
                cmd = [compiler_found, "-std=c++17", "-g", cpp_file, "-o", exe_file]
            else: # cl (MSVC)
                cmd = ["cl", "/std:c++17", "/Zi", "/EHsc", cpp_file]

            result = subprocess.run(cmd, capture_output=True, text=True)
            if result.returncode == 0:
                print(f"[Viss Compiler] Success! Executable created: {exe_file}")
            else:
                print("[Viss Compiler] Compilation failed.")
                print(result.stderr)
        except Exception as e:
            print(f"[Viss Compiler] Error during compilation: {e}")
    else:
        print("[Viss Compiler] Note: No C++ compiler (g++, clang++, cl) was found in PATH.")
        print(f"[Viss Compiler] You can compile the generated C++ file manually using:")
        print(f"  g++ -std=c++17 {cpp_file} -o {exe_file}")

if __name__ == "__main__":
    main()
