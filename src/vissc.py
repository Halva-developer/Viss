#!/usr/bin/env python3
"""
Viss Compiler (vissc) - Version 0.0.1.2
Full compiler for the Viss 2.0 language specification.
Transforms Viss code into high-performance, native C++17.
"""

import sys
import os
import re
import subprocess
import shutil

VERSION = "0.0.1.2"

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
    open_count = viss_code.count('{')
    close_count = viss_code.count('}')
    if open_count != close_count:
        print(f"Error in {filename}: Unbalanced curly braces detected.")
        print(f"Details: Found {open_count} open braces '{{' and {close_count} close braces '}}'.")
        sys.exit(1)

def translate_interpolation(code):
    """
    Translates i"Hello @player.name, Rank: @rank, count: {@items.len}"
    into C++ string concatenation.
    """
    pattern = r'i"((?:[^"\\]|\\.)*)"'
    
    def replacer(match):
        content = match.group(1)
        parts = []
        last_end = 0
        
        var_pattern = r'\{([^}]+)\}|@([a-zA-Z0-9_]+(?:\.[a-zA-Z0-9_]+)*)'
        for m in re.finditer(var_pattern, content):
            start, end = m.span()
            if start > last_end:
                plain_text = content[last_end:start]
                parts.append(f'viss::Str("{plain_text}")')
            
            expr = m.group(1) if m.group(1) is not None else m.group(2)
            expr_clean = expr.replace('@', '')
            parts.append(f'viss::toStr({expr_clean})')
            last_end = end
            
        if last_end < len(content):
            plain_text = content[last_end:]
            parts.append(f'viss::Str("{plain_text}")')
            
        if not parts:
            return 'viss::Str("")'
        return '(' + ' + '.join(parts) + ')'

    return re.sub(pattern, replacer, code)

def transpile_params(params_str):
    if not params_str.strip():
        return ""
    params = params_str.split(',')
    res = []
    for p in params:
        p = p.strip()
        if not p:
            continue
        default_val = None
        if '=' in p:
            p, default_val = p.split('=', 1)
            p = p.strip()
            default_val = default_val.strip()
            
        m = re.match(r'([a-zA-Z0-9_]+)\s+as\s+([a-zA-Z0-9_<>]+)', p)
        if m:
            pname = m.group(1)
            ptype = m.group(2)
            cpp_type = {
                'str': 'viss::Str',
                'int': 'viss::Int',
                'dec': 'viss::Dec',
                'bool': 'viss::Bool',
                'any': 'auto',
                'list': 'viss::List',
                'map': 'viss::Map',
                'inf': 'viss::Inf'
            }.get(ptype, ptype)
            
            decl = f"{cpp_type} {pname}"
            if default_val:
                decl += f" = {default_val}"
            res.append(decl)
        else:
            p_clean = p.replace('@', '')
            decl = f"auto {p_clean}"
            if default_val:
                decl += f" = {default_val}"
            res.append(decl)
    return ", ".join(res)

def transpile(viss_code, filename):
    # Step 1: Pre-process string interpolation
    processed_code = translate_interpolation(viss_code)
    
    # Step 2: Pre-process imports before hiding string literals
    includes_section = ['#include "libs/vissrt.hpp"']
    clean_lines = []
    
    for raw_line in processed_code.split('\n'):
        line = raw_line
        comment_part = ""
        m_comm = re.search(r'(//.*|/\*[\s\S]*?\*/)', line)
        if m_comm:
            comment_part = " " + m_comm.group(0)
            line = line[:m_comm.start()]
            
        stripped = line.strip()
        
        m_import_sys = re.match(r'^\s*\$import\s+lib\s+from\s+"system"(?:\s*\[[^\]]*\])?\s+as\s+([a-zA-Z0-9_]+)', stripped)
        if m_import_sys:
            alias = m_import_sys.group(1)
            includes_section.append(f'#include "libs/std/sys.hpp"\nnamespace {alias} = viss::sys;')
            continue

        m_import_lib = re.match(r'^\s*\$import\s+lib\s+"([a-zA-Z0-9_]+)"\s+as\s+([a-zA-Z0-9_]+)', stripped)
        if m_import_lib:
            lib = m_import_lib.group(1)
            alias = m_import_lib.group(2)
            if lib in ("asyncIO", "async"):
                includes_section.append(f'namespace {alias} = viss::async;')
            elif lib in ("iostream", "io"):
                includes_section.append(f'#include "libs/std/io.hpp"\nnamespace {alias} = viss::io;')
            else:
                includes_section.append(f'#include "libs/std/{lib}.hpp"\nnamespace {alias} = viss::{lib};')
            continue

        m_import_cpp = re.match(r'^\s*\$import\s+cpp\s+<([^>]+)>\s+as\s+([a-zA-Z0-9_]+)', stripped)
        if m_import_cpp:
            header, alias = m_import_cpp.group(1), m_import_cpp.group(2)
            includes_section.append(f'#include <{header}>')
            continue

        m_import_old = re.match(r'^\s*@use\s+<?IOstream>?\s+for\s+\*', stripped)
        if m_import_old:
            includes_section.append('#include "libs/vissrt.hpp"\nusing namespace viss;')
            continue
            
        clean_lines.append(raw_line)
        
    code_without_imports = '\n'.join(clean_lines)

    # Step 3: Extract string literals to protect them from regex
    string_literals = []
    def save_string(match):
        string_literals.append(match.group(0))
        return f"__VISS_STR_LIT_{len(string_literals)-1}__"
    processed_code = re.sub(r'"""[\s\S]*?"""', save_string, code_without_imports)
    processed_code = re.sub(r'"(?:[^"\\]|\\.)*"', save_string, processed_code)

    lines = processed_code.split('\n')
    
    # Buckets for C++ order: includes -> classes -> functions -> main
    classes_section = []
    functions_section = []
    main_section = []
    
    current_target = functions_section
    block_stack = []
    current_class_name = None
    class_fields = {}
    for idx, line in enumerate(lines):
        orig_line = line
        
        # Strip comments for code matching, preserve comment at line end
        comment_part = ""
        m_comm = re.search(r'(//.*|/\*[\s\S]*?\*/)', line)
        if m_comm:
            comment_part = " " + m_comm.group(0)
            line = line[:m_comm.start()]

        stripped = line.strip()
        line_num = idx + 1
        
        if not stripped:
            continue

        # 2. Classes
        m_class_inherits = re.match(r'^\s*!class\s+([a-zA-Z0-9_]+)\s*::\s*([a-zA-Z0-9_]+)\s*\{', stripped)
        if m_class_inherits:
            cname, pnames = m_class_inherits.group(1), m_class_inherits.group(2)
            current_class_name = cname
            class_fields[current_class_name] = {}
            block_stack.append(('class', cname))
            current_target = classes_section
            current_target.append(f"struct {cname} : public {pnames} {{")
            continue

        m_class = re.match(r'^\s*!class\s+([a-zA-Z0-9_]+)\s*\{', stripped)
        if m_class:
            cname = m_class.group(1)
            current_class_name = cname
            class_fields[current_class_name] = {}
            block_stack.append(('class', cname))
            current_target = classes_section
            current_target.append(f"struct {cname} {{")
            continue

        # 3. Functions
        # Inside class: !func main(params) { is constructor
        m_ctor = re.match(r'^\s*!func\s+main\s*\(([^)]*)\)\s*\{', stripped)
        if m_ctor and current_class_name:
            params = transpile_params(m_ctor.group(1))
            block_stack.append(('func', 'ctor'))
            current_target.append(f"    {current_class_name}({params}) {{")
            continue

        # !async.func main() { or !func main() {
        if re.match(r'^\s*!async\.func\s+main\s*\(\s*\)\s*\{', stripped) or re.match(r'^\s*!func\s+main\s*\(\s*\)\s*\{', stripped):
            block_stack.append(('func', 'main'))
            current_target = main_section
            current_target.append("int main() {")
            continue

        # !async.func name(params) to return_type {
        m_async_func = re.match(r'^\s*!async\.func\s+([a-zA-Z0-9_?]+)\s*\(([^)]*)\)(?:\s+to\s+([a-zA-Z0-9_<>]+))?\s*\{', stripped)
        if m_async_func:
            fname = m_async_func.group(1).replace('?', '_q')
            params = transpile_params(m_async_func.group(2))
            block_stack.append(('async_func', fname))
            current_target = functions_section
            current_target.append(f"inline auto {fname}({params}) {{ return viss::getGlobalThreadPool().enqueue([=]() {{")
            continue

        # !func name(params) to return_type {
        m_func = re.match(r'^\s*!func\s+([a-zA-Z0-9_?]+)\s*\(([^)]*)\)(?:\s+to\s+([a-zA-Z0-9_<>]+))?\s*\{', stripped)
        if m_func:
            fname = m_func.group(1).replace('?', '_q')
            params = transpile_params(m_func.group(2))
            ret_type = m_func.group(3)
            if ret_type:
                cpp_ret = {
                    'str': 'viss::Str',
                    'int': 'viss::Int',
                    'dec': 'viss::Dec',
                    'bool': 'viss::Bool',
                    'list': 'viss::List',
                    'map': 'viss::Map',
                    'inf': 'viss::Inf'
                }.get(ret_type, ret_type)
                ret_decl = f" -> {cpp_ret}"
            else:
                ret_decl = ""
            block_stack.append(('func', fname))
            if not current_class_name:
                current_target = functions_section
            current_target.append(f"inline auto {fname}({params}){ret_decl} {{")
            continue

        # 4. Pattern Matching ?match expr {
        m_match = re.match(r'^\s*\?match\s+([^\{]+)\{', stripped)
        if m_match:
            expr = m_match.group(1).strip().replace('@', '')
            block_stack.append(('match', expr))
            current_target.append(f"switch ({expr}) {{")
            continue

        m_case = re.match(r'^\s*case\s+([^\{]+)\{', stripped)
        if m_case:
            val = m_case.group(1).strip()
            block_stack.append(('match_case', val))
            current_target.append(f"case {val}: {{")
            continue

        m_else_match = re.match(r'^\s*else\s*\{', stripped)
        if m_else_match and block_stack and (block_stack[-1][0] == 'match' or (len(block_stack) >= 2 and block_stack[-2][0] == 'match')):
            block_stack.append(('match_case', 'default'))
            current_target.append("default: {")
            continue

        # 5. Loops !for
        m_for_range = re.match(r'^\s*!for\s+@?([a-zA-Z0-9_]+)\s+in\s+([0-9]+)\.\.([0-9]+)\s*\{', stripped)
        if m_for_range:
            vname, start, end = m_for_range.group(1), m_for_range.group(2), m_for_range.group(3)
            block_stack.append(('for', vname))
            current_target.append(f"for (viss::Int {vname} = {start}; {vname} < {end}; ++{vname}) {{")
            continue

        m_for_in = re.match(r'^\s*!for\s+@?([a-zA-Z0-9_]+)\s+in\s+([^\{]+)\{', stripped)
        if m_for_in:
            vname, coll = m_for_in.group(1), m_for_in.group(2).strip().replace('@', '')
            block_stack.append(('for', vname))
            current_target.append(f"for (auto& {vname} : {coll}) {{")
            continue

        # 6. Logic: ?if, ?else, ?try, ?expect, ?error
        if stripped.startswith('?try') and '{' in stripped:
            block_stack.append(('try', 'try'))
            current_target.append("try {")
            continue

        m_expect = re.match(r'^\s*\?expect\s+([a-zA-Z0-9_]+)\s+as\s+@?([a-zA-Z0-9_]+)\s*\{', stripped)
        if m_expect:
            ename = m_expect.group(2)
            block_stack.append(('expect', ename))
            current_target.append(f"catch (const std::exception& _err) {{ viss::Exception {ename}(_err.what());")
            continue

        m_if = re.match(r'^\s*\?if\s*\(([^)]+)\)\s*\{', stripped)
        if m_if:
            cond = m_if.group(1)
            cond = re.sub(r'\bnot\b', '!', cond)
            cond = re.sub(r'\band\b', '&&', cond)
            cond = re.sub(r'\bor\b', '||', cond)
            cond = cond.replace('@', '')
            cond = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', cond)
            block_stack.append(('if', 'if'))
            current_target.append(f"if ({cond}) {{")
            continue

        if stripped.startswith('?else') and '{' in stripped:
            block_stack.append(('else', 'else'))
            current_target.append("else {")
            continue

        m_error = re.match(r'^\s*\?error\s+([^;]+);?', stripped)
        if m_error:
            msg = m_error.group(1).strip()
            current_target.append(f"throw viss::Exception({msg});")
            continue

        # 7. Block closures '}'
        if stripped == '}':
            if block_stack:
                btype, bdata = block_stack.pop()
                if btype == 'class':
                    current_class_name = None
                    current_target.append("};")
                    current_target = functions_section
                    continue
                elif btype == 'async_func':
                    current_target.append("}); }")
                    continue
                elif btype == 'match_case':
                    current_target.append("break; }")
                    continue
            current_target.append("}")
            continue

        # 8. Variable assignments & declarations with pipe
        transpiled_line = line

        # Replace @me.field
        transpiled_line = re.sub(r'@me\.([a-zA-Z0-9_]+)', r'this->\1', transpiled_line)

        # Catch class field definitions: this->name = ... | type;
        m_this_field = re.match(r'^\s*this->([a-zA-Z0-9_]+)\s*=\s*(.+?)(?:\s*\|\s*([a-zA-Z0-9_]+))?\s*;?\s*$', transpiled_line)
        if m_this_field and current_class_name:
            field_name = m_this_field.group(1)
            val = m_this_field.group(2).strip()
            ptype = m_this_field.group(3)
            
            cpp_type = {
                'str': 'viss::Str',
                'int': 'viss::Int',
                'dec': 'viss::Dec',
                'bool': 'viss::Bool',
                'list': 'viss::List',
                'map': 'viss::Map',
                'inf': 'viss::Inf'
            }.get(ptype, 'viss::any_t')
            class_fields[current_class_name][field_name] = cpp_type
            
            if ptype == 'list' and (val == '[]' or not val):
                val = '{}'
            elif ptype == 'map' and (val == '{}' or not val):
                val = '{}'
            elif ptype == 'inf' and (val == '{}' or not val):
                val = '{}'
            current_target.append(f"        this->{field_name} = {val};")
            continue

        # Variable declarations: @var = value | type, const;
        m_pipe_const = re.match(r'^\s*@([a-zA-Z0-9_]+)\s*=\s*(.+?)\s*\|\s*([a-zA-Z0-9_]+)\s*,\s*const\s*;?\s*$', transpiled_line)
        if m_pipe_const:
            vname, val, ptype = m_pipe_const.group(1), m_pipe_const.group(2), m_pipe_const.group(3)
            val = val.replace('@', '').replace('await ', '')
            val = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', val)
            current_target.append(f"const auto {vname} = {val};")
            continue

        # Variable declarations: @var = value | type;
        m_pipe = re.match(r'^\s*@([a-zA-Z0-9_]+)\s*=\s*(.+?)\s*\|\s*([a-zA-Z0-9_]+)\s*;?\s*$', transpiled_line)
        if m_pipe:
            vname, val, ptype = m_pipe.group(1), m_pipe.group(2), m_pipe.group(3)
            val = val.replace('@', '')
            val = re.sub(r'\bawait\s+([a-zA-Z0-9_.]+)\(([^)]*)\)', r'(\1(\2)).get()', val)
            val = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', val)
            if ptype == 'list':
                if val.startswith('[') and val.endswith(']'):
                    val = '{' + val[1:-1] + '}'
                current_target.append(f"viss::List {vname} = {val};")
            elif ptype == 'map':
                current_target.append(f"viss::Map {vname} = {val};")
            elif ptype == 'inf':
                current_target.append(f"viss::Inf {vname} = {val};")
            elif ptype == 'str':
                current_target.append(f"viss::Str {vname} = viss::toStr({val});")
            elif ptype == 'int':
                current_target.append(f"viss::Int {vname} = (viss::Int)({val});")
            elif ptype == 'class':
                current_target.append(f"auto {vname} = {val};")
            else:
                current_target.append(f"auto {vname} = {val};")
            continue

        # Generic assignment: @var = value;
        m_assign = re.match(r'^\s*@([a-zA-Z0-9_]+)\s*=\s*(.+?)\s*;?\s*$', transpiled_line)
        if m_assign:
            vname, val = m_assign.group(1), m_assign.group(2)
            val = val.replace('@', '')
            val = re.sub(r'\bawait\s+([a-zA-Z0-9_.]+)\(([^)]*)\)', r'(\1(\2)).get()', val)
            val = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', val)
            current_target.append(f"auto {vname} = {val};")
            continue

        # Await standalone call: await async.sleep(100);
        m_await_call = re.match(r'^\s*await\s+([a-zA-Z0-9_.]+)\(([^)]*)\)\s*;?', transpiled_line)
        if m_await_call:
            fn, args = m_await_call.group(1), m_await_call.group(2).replace('@', '')
            current_target.append(f"{fn}({args});")
            continue

        # Control keywords
        transpiled_line = re.sub(r'!break\s*;?', 'break;', transpiled_line)
        transpiled_line = re.sub(r'!continue\s*;?', 'continue;', transpiled_line)
        transpiled_line = re.sub(r'\bnot\b', '!', transpiled_line)
        transpiled_line = re.sub(r'\band\b', '&&', transpiled_line)
        transpiled_line = re.sub(r'\bor\b', '||', transpiled_line)
        transpiled_line = re.sub(r'\bNull\b', 'nullptr', transpiled_line)

        # Strip leftover @
        transpiled_line = re.sub(r'@([a-zA-Z0-9_]+)', r'\1', transpiled_line)
        # Function question marks
        transpiled_line = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', transpiled_line)

        # Semicolons
        t_strip = transpiled_line.strip()
        if t_strip and not t_strip.endswith(('{', '}', ';', ':', ',')):
            transpiled_line += ";"

        current_target.append(transpiled_line)

    # Insert auto field declarations into struct definitions
    final_classes_lines = []
    for line in classes_section:
        final_classes_lines.append(line)
        for cname, fields in class_fields.items():
            if line.strip() == f"struct {cname} {{" or line.strip().startswith(f"struct {cname} :"):
                for fname, ftype in sorted(fields.items()):
                    final_classes_lines.append(f"    {ftype} {fname};")

    # Assemble complete C++ source file in optimal order
    all_cpp_lines = []
    all_cpp_lines.extend(includes_section)
    all_cpp_lines.append("")
    all_cpp_lines.extend(final_classes_lines)
    all_cpp_lines.append("")
    all_cpp_lines.extend(functions_section)
    all_cpp_lines.append("")
    all_cpp_lines.extend(main_section)

    cpp_code = '\n'.join(all_cpp_lines)

    # Restore string literals
    for i, lit in enumerate(string_literals):
        cpp_code = cpp_code.replace(f"__VISS_STR_LIT_{i}__", lit)

    return cpp_code

def main():
    if len(sys.argv) < 2:
        print(f"Viss Compiler (vissc) v{VERSION}")
        print("Usage: python src/vissc.py <file.viss> [-r / -run]")
        sys.exit(1)

    viss_file = sys.argv[1]
    run_after = "-r" in sys.argv or "-run" in sys.argv

    if not os.path.exists(viss_file):
        print(f"Error: File '{viss_file}' not found.")
        sys.exit(1)

    base, _ = os.path.splitext(viss_file)
    cpp_file = base + ".cpp"
    exe_file = base + ".exe" if os.name == 'nt' else base

    print(f"[Viss Compiler v{VERSION}] Transpiling {viss_file} to {cpp_file}...")

    with open(viss_file, 'r', encoding='utf-8') as f:
        viss_code = f.read()

    validate_viss_syntax(viss_code, os.path.basename(viss_file))
    cpp_code = transpile(viss_code, os.path.basename(viss_file))

    with open(cpp_file, 'w', encoding='utf-8') as f:
        f.write(cpp_code)

    print(f"[Viss Compiler v{VERSION}] Transpilation complete: {cpp_file}")

    # Check for C++ compiler
    cxx = shutil.which("g++") or shutil.which("clang++") or shutil.which("cl")
    if cxx:
        print(f"[Viss Compiler v{VERSION}] Compiling binary using {os.path.basename(cxx)}...")
        res = subprocess.run([cxx, "-std=c++17", cpp_file, "-o", exe_file])
        if res.returncode == 0:
            print(f"[Viss Compiler v{VERSION}] Build successful: {exe_file}")
            if run_after:
                print(f"[Viss Compiler v{VERSION}] Running {exe_file}...\n")
                subprocess.run([os.path.abspath(exe_file)])
        else:
            print(f"[Viss Compiler v{VERSION}] Compilation failed with exit code {res.returncode}.")
            sys.exit(res.returncode)
    else:
        print(f"[Viss Compiler v{VERSION}] Note: No C++ compiler found in PATH.")
        print(f"You can compile manually: g++ -std=c++17 {cpp_file} -o {exe_file}")

if __name__ == "__main__":
    main()
