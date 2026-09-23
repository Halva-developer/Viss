#!/usr/bin/env python3
"""
Viss Compiler (vissc) - Version 0.0.1.2
Full compiler for the Viss 2.0 language specification.
Transforms Viss code into high-performance, native C++17.
Features:
- Full standard library (io, sys, fs, math, time, str, retrotech, async)
- Native primitive namespaces (int.*, double.*, dec.*, str.*, bytes.*, bits.*, list.*)
- Low-level raw memory buffers & bit manipulation (&grid create | bytes/bits)
- Industrial-grade syntax validator & diagnostic checker
- Subcommands: run, compile, check, init, version
"""

import sys
import os
import re
import subprocess
import shutil
import json

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

def check_syntax(viss_code, filename="<file.viss>"):
    """
    Comprehensive linter and syntax diagnostics checker for Viss 2.0.
    Returns a list of diagnostic dictionaries:
    [{ "line": int, "col": int, "message": str, "severity": "error"|"warning" }]
    """
    diagnostics = []
    lines = viss_code.split('\n')

    # 1. Structural balance checks
    stack = []
    for line_idx, line in enumerate(lines):
        in_str = False
        escaped = False
        for col_idx, ch in enumerate(line):
            if in_str:
                if escaped:
                    escaped = False
                elif ch == '\\':
                    escaped = True
                elif ch == '"':
                    in_str = False
                continue
            else:
                if ch == '"':
                    in_str = True
                    continue
                if ch == '#' or (col_idx + 1 < len(line) and line[col_idx:col_idx+2] == '//'):
                    break
                if ch in '{[(':
                    stack.append((ch, line_idx + 1, col_idx + 1))
                elif ch in '}])':
                    if not stack:
                        diagnostics.append({
                            "line": line_idx + 1,
                            "col": col_idx + 1,
                            "message": f"Unmatched closing delimiter '{ch}'",
                            "severity": "error"
                        })
                    else:
                        top, open_line, open_col = stack.pop()
                        matches = { '}': '{', ']': '[', ')': '(' }
                        if matches[ch] != top:
                            diagnostics.append({
                                "line": line_idx + 1,
                                "col": col_idx + 1,
                                "message": f"Mismatched closing delimiter '{ch}', expected match for '{top}' from line {open_line}",
                                "severity": "error"
                            })

    for top, open_line, open_col in stack:
        diagnostics.append({
            "line": open_line,
            "col": open_col,
            "message": f"Unclosed delimiter '{top}'",
            "severity": "error"
        })

    # 2. Syntax pattern checks
    for line_idx, line in enumerate(lines):
        stripped = line.strip()
        # Skip empty lines and comments
        if not stripped or stripped.startswith('#') or stripped.startswith('//') or stripped.startswith('/*'):
            continue

        # Check raw buffer create syntax
        if stripped.startswith('&') and 'create' in stripped:
            if not re.search(r'^\s*&[a-zA-Z0-9_]+\s+create\s*\|\s*(bytes|bits)', stripped):
                diagnostics.append({
                    "line": line_idx + 1,
                    "col": 1,
                    "message": "Malformed raw buffer creation. Expected: &name create | bytes[, size];",
                    "severity": "warning"
                })

        # Check pipe declaration syntax
        if '=' in stripped and '|' in stripped and not stripped.startswith('//'):
            if not re.search(r'(@[a-zA-Z0-9_.]+|this->[a-zA-Z0-9_]+)\s*=\s*.+?\|\s*[a-zA-Z0-9_]+', stripped):
                if not stripped.startswith('&'):
                    diagnostics.append({
                        "line": line_idx + 1,
                        "col": 1,
                        "message": "Malformed pipe declaration. Expected: @var = value | type;",
                        "severity": "warning"
                    })

    return diagnostics

def validate_viss_syntax(viss_code, filename):
    diags = check_syntax(viss_code, filename)
    errors = [d for d in diags if d['severity'] == 'error']
    if errors:
        print(f"Error in {filename}: Syntax check failed with {len(errors)} error(s):")
        for err in errors:
            print(f"  Line {err['line']}, Col {err['col']}: {err['message']}")
        sys.exit(1)

def translate_interpolation(code):
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
            expr_clean = expr.replace('@', '').replace('&', '')
            expr_clean = re.sub(r'\.len\b', '.size()', expr_clean)
            expr_clean = re.sub(r'\.first\b', '.get_first()', expr_clean)
            expr_clean = re.sub(r'\.last\b', '.get_last()', expr_clean)
            parts.append(f'viss::toStr({expr_clean})')
            last_end = end
            
        if last_end < len(content):
            plain_text = content[last_end:]
            parts.append(f'viss::Str("{plain_text}")')
            
        if not parts:
            return 'viss::Str("")'
        return '(' + ' + '.join(parts) + ')'

    return re.sub(pattern, replacer, code)

def transpile_params_and_template(params_str):
    if not params_str.strip():
        return "", ""
    params = params_str.split(',')
    res = []
    template_types = []
    for idx, p in enumerate(params):
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
                'double': 'viss::Dec',
                'float': 'viss::Dec',
                'bool': 'viss::Bool',
                'bytes': 'viss::Bytes',
                'bits': 'viss::Bits',
                'any': 'auto',
                'list': 'viss::List<viss::Str>',
                'map': 'viss::Map<viss::Str, viss::Str>',
                'inf': 'viss::Inf'
            }.get(ptype, ptype)
            
            decl = f"{cpp_type} {pname}"
            if default_val:
                decl += f" = {default_val}"
            res.append(decl)
        else:
            p_clean = p.replace('@', '').replace('&', '')
            tname = f"_T{idx}_{p_clean}"
            template_types.append(f"typename {tname}")
            decl = f"{tname} {p_clean}"
            if default_val:
                decl += f" = {default_val}"
            res.append(decl)
    t_clause = f"template<{', '.join(template_types)}> " if template_types else ""
    return ", ".join(res), t_clause

def transpile_params(params_str):
    params_decl, _ = transpile_params_and_template(params_str)
    return params_decl

def apply_primitive_static_transforms(text):
    """
    Translates high-level static namespace methods:
    int.*, double.*, dec.*, str.*, bytes.*, bits.*
    """
    res = text
    # int.*
    res = re.sub(r'\bint\.random\(', 'viss::math::random_int(', res)
    res = re.sub(r'\bint\.parse\(', 'viss::toInt(', res)
    res = re.sub(r'\bint\.to_hex\(', 'viss::toHex(', res)
    res = re.sub(r'\bint\.to_bin\(', 'viss::toBin(', res)
    res = re.sub(r'\bint\.abs\(', 'std::abs(', res)
    res = re.sub(r'\bint\.min\(', 'std::min(', res)
    res = re.sub(r'\bint\.max\(', 'std::max(', res)
    res = re.sub(r'\bint\.clamp\(', 'viss::math::clamp(', res)

    # double.* / dec.*
    res = re.sub(r'\b(?:double|dec)\.sqrt\(', 'std::sqrt(', res)
    res = re.sub(r'\b(?:double|dec)\.cbrt\(', 'std::cbrt(', res)
    res = re.sub(r'\b(?:double|dec)\.pow\(', 'std::pow(', res)
    res = re.sub(r'\b(?:double|dec)\.sin\(', 'std::sin(', res)
    res = re.sub(r'\b(?:double|dec)\.cos\(', 'std::cos(', res)
    res = re.sub(r'\b(?:double|dec)\.tan\(', 'std::tan(', res)
    res = re.sub(r'\b(?:double|dec)\.asin\(', 'std::asin(', res)
    res = re.sub(r'\b(?:double|dec)\.acos\(', 'std::acos(', res)
    res = re.sub(r'\b(?:double|dec)\.atan\(', 'std::atan(', res)
    res = re.sub(r'\b(?:double|dec)\.atan2\(', 'std::atan2(', res)
    res = re.sub(r'\b(?:double|dec)\.abs\(', 'std::abs(', res)
    res = re.sub(r'\b(?:double|dec)\.round\(', 'std::round(', res)
    res = re.sub(r'\b(?:double|dec)\.floor\(', 'std::floor(', res)
    res = re.sub(r'\b(?:double|dec)\.ceil\(', 'std::ceil(', res)
    res = re.sub(r'\b(?:double|dec)\.min\(', 'std::min(', res)
    res = re.sub(r'\b(?:double|dec)\.max\(', 'std::max(', res)
    res = re.sub(r'\b(?:double|dec)\.clamp\(', 'viss::math::clamp(', res)
    res = re.sub(r'\b(?:double|dec)\.random\(', 'viss::math::random_dec(', res)
    res = re.sub(r'\b(?:double|dec)\.parse\(', 'viss::toDec(', res)

    # str.*
    res = re.sub(r'\bstr\.from\(', 'viss::toStr(', res)
    res = re.sub(r'\bstr\.len\(', 'viss::str::len(', res)
    res = re.sub(r'\bstr\.split\(', 'viss::str::split(', res)
    res = re.sub(r'\bstr\.join\(', 'viss::str::join(', res)
    res = re.sub(r'\bstr\.trim\(', 'viss::str::trim(', res)
    res = re.sub(r'\bstr\.lower\(', 'viss::str::lower(', res)
    res = re.sub(r'\bstr\.upper\(', 'viss::str::upper(', res)
    res = re.sub(r'\bstr\.contains\(', 'viss::str::contains(', res)
    res = re.sub(r'\bstr\.replace\(', 'viss::str::replace(', res)
    res = re.sub(r'\bstr\.starts_with\(', 'viss::str::starts_with(', res)
    res = re.sub(r'\bstr\.ends_with\(', 'viss::str::ends_with(', res)
    res = re.sub(r'\bstr\.sub\(', 'viss::str::sub(', res)
    res = re.sub(r'\bstr\.repeat\(', 'viss::str::repeat(', res)
    res = re.sub(r'\bstr\.pad_left\(', 'viss::str::pad_left(', res)
    res = re.sub(r'\bstr\.pad_right\(', 'viss::str::pad_right(', res)

    # bytes.* and bits.*
    res = re.sub(r'\bbytes\.alloc\(', 'viss::Bytes(', res)
    res = re.sub(r'\bbits\.alloc\(', 'viss::Bits(', res)
    return res

def transpile(viss_code, filename):
    # Step 1: Pre-process string interpolation
    processed_code = translate_interpolation(viss_code)
    
    # Step 2: Pre-process imports before hiding string literals
    includes_section = ['#include "libs/vissrt.hpp"', 'namespace async = viss::async;']
    clean_lines = []
    imported_aliases = {"io", "async", "rt", "sys", "fs", "math", "time", "str"}
    
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
            imported_aliases.add(alias)
            includes_section.append(f'#include "libs/std/sys.hpp"\nnamespace {alias} = viss::sys;')
            continue

        m_import_lib = re.match(r'^\s*\$impo(?:rt|er)\s+lib\s+"([a-zA-Z0-9_]+)"\s+as\s+([a-zA-Z0-9_]+)', stripped)
        if m_import_lib:
            lib = m_import_lib.group(1)
            alias = m_import_lib.group(2)
            imported_aliases.add(alias)
            if lib in ("asyncIO", "async"):
                includes_section.append(f'namespace {alias} = viss::async;')
            elif lib in ("iostream", "io"):
                includes_section.append(f'#include "libs/std/io.hpp"\nnamespace {alias} = viss::io;')
            elif lib in ("retrotech", "rt"):
                includes_section.append(f'#include "libs/std/retrotech.hpp"\nnamespace {alias} = viss::retrotech;')
            else:
                includes_section.append(f'#include "libs/std/{lib}.hpp"\nnamespace {alias} = viss::{lib};')
            continue

        m_import_cpp = re.match(r'^\s*\$import\s+cpp\s+<([^>]+)>\s+as\s+([a-zA-Z0-9_]+)', stripped)
        if m_import_cpp:
            header, alias = m_import_cpp.group(1), m_import_cpp.group(2)
            imported_aliases.add(alias)
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

    def collapse_multiline_brackets(text):
        out = []
        depth = 0
        for ch in text:
            if ch == '[':
                depth += 1
                out.append(ch)
            elif ch == ']':
                if depth > 0: depth -= 1
                out.append(ch)
            elif depth > 0 and ch == '\n':
                out.append(' ')
            else:
                out.append(ch)
        return "".join(out)
    processed_code = collapse_multiline_brackets(processed_code)

    lines = processed_code.split('\n')
    
    # Pre-scan for all declared classes to resolve naming and static calls
    known_classes = set()
    for l in lines:
        m_c = re.match(r'^\s*!class\s+([a-zA-Z0-9_]+)', l.strip())
        if m_c:
            known_classes.add(m_c.group(1))

    # Buckets for C++ order: includes -> classes -> functions -> main
    classes_section = []
    functions_section = []
    main_section = []
    
    current_target = functions_section
    block_stack = []
    current_class_name = None
    class_fields = {}
    declared_vars = set()

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
            current_target.append(f"struct _cls_{cname} : public {pnames} {{")
            continue

        m_class = re.match(r'^\s*!class\s+([a-zA-Z0-9_]+)\s*\{', stripped)
        if m_class:
            cname = m_class.group(1)
            current_class_name = cname
            class_fields[current_class_name] = {}
            block_stack.append(('class', cname))
            current_target = classes_section
            current_target.append(f"struct _cls_{cname} {{")
            continue

        # 3. Functions
        # Inside class: !func main(params) { is constructor
        m_ctor = re.match(r'^\s*!func\s+main\s*\(([^)]*)\)\s*\{', stripped)
        if m_ctor and current_class_name:
            params = transpile_params(m_ctor.group(1))
            block_stack.append(('func', 'ctor'))
            current_target.append(f"    _cls_{current_class_name}({params}) {{")
            continue

        # !async.func main() { or !func main() {
        if re.match(r'^\s*!async\.func\s+main\s*\(\s*\)\s*\{', stripped) or re.match(r'^\s*!func\s+main\s*\(\s*\)\s*\{', stripped) or stripped == "!main {":
            block_stack.append(('func', 'main'))
            current_target = main_section
            current_target.append("int main() {")
            continue

        # !async.func name(params) to return_type {
        m_async_func = re.match(r'^\s*!async\.func\s+([a-zA-Z0-9_?!]+)\s*\(([^)]*)\)(?:\s+to\s+([a-zA-Z0-9_<>]+))?\s*\{', stripped)
        if m_async_func:
            raw_fname = m_async_func.group(1)
            fname = raw_fname.replace('?', '_q').replace('!', '_bang')
            params = transpile_params(m_async_func.group(2))
            block_stack.append(('async_func', fname))
            current_target = functions_section
            current_target.append(f"inline auto {fname}({params}) {{ return viss::getGlobalThreadPool().enqueue([=]() {{")
            continue

        # !func name(params) to return_type {
        m_func = re.match(r'^\s*!func\s+([a-zA-Z0-9_?!]+)\s*\(([^)]*)\)(?:\s+to\s+([a-zA-Z0-9_<>]+))?\s*\{', stripped)
        if m_func:
            raw_fname = m_func.group(1)
            fname = raw_fname.replace('?', '_q').replace('!', '_bang')
            params, t_clause = transpile_params_and_template(m_func.group(2))
            ret_type = m_func.group(3)
            if ret_type:
                cpp_ret = {
                    'str': 'viss::Str',
                    'int': 'viss::Int',
                    'dec': 'viss::Dec',
                    'double': 'viss::Dec',
                    'float': 'viss::Dec',
                    'bool': 'viss::Bool',
                    'bytes': 'viss::Bytes',
                    'bits': 'viss::Bits',
                    'list': 'viss::List<viss::Str>',
                    'map': 'viss::Map<viss::Str, viss::Str>',
                    'inf': 'viss::Inf'
                }.get(ret_type, ret_type)
                ret_decl = f" -> {cpp_ret}"
            else:
                ret_decl = ""
            block_stack.append(('func', fname))
            if not current_class_name:
                current_target = functions_section
                current_target.append(f"{t_clause}inline auto {fname}({params}){ret_decl} {{")
            else:
                uses_me = False
                depth = 1
                for future_line in lines[idx+1:]:
                    if '{' in future_line: depth += future_line.count('{')
                    if '}' in future_line: depth -= future_line.count('}')
                    if '@me' in future_line:
                        uses_me = True
                    if depth <= 0:
                        break
                static_mod = "static " if not uses_me else ""
                current_target.append(f"    {t_clause}{static_mod}inline auto {fname}({params}){ret_decl} {{")
            continue

        # 4. Pattern Matching ?match expr {
        m_match = re.match(r'^\s*\?match\s+([^\{]+)\{', stripped)
        if m_match:
            expr = m_match.group(1).strip().replace('@', '').replace('&', '')
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

        # 5. Loops !for and for
        m_for_range = re.match(r'^\s*!?for\s+@?([a-zA-Z0-9_]+)\s+in\s+([^.]+)\.\.([^\{]+)\s*\{', stripped)
        if m_for_range:
            vname = m_for_range.group(1)
            start = m_for_range.group(2).strip().replace('@', '').replace('&', '')
            end = m_for_range.group(3).strip().replace('@', '').replace('&', '')
            block_stack.append(('for', vname))
            current_target.append(f"for (viss::Int {vname} = ({start}); {vname} < ({end}); ++{vname}) {{")
            continue

        m_for_in = re.match(r'^\s*!?for\s+@?([a-zA-Z0-9_]+)\s+in\s+([^\{]+)\{', stripped)
        if m_for_in:
            vname = m_for_in.group(1)
            coll = m_for_in.group(2).strip().replace('@', '').replace('&', '')
            block_stack.append(('for', vname))
            current_target.append(f"for (auto& {vname} : {coll}) {{")
            continue

        # 5b. While loop: !while (cond) { or while (cond) {
        m_while = re.match(r'^\s*!?while\s*\((.*)\)\s*\{', stripped)
        if m_while:
            cond = m_while.group(1)
            cond = cond.replace('@', '')
            cond = re.sub(r'&([a-zA-Z0-9_]+)', r'\1', cond)
            cond = re.sub(r'\bnot\b', '!', cond)
            cond = re.sub(r'\band\b', '&&', cond)
            cond = re.sub(r'\bor\b', '||', cond)
            cond = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', cond)
            cond = apply_primitive_static_transforms(cond)
            for imp in imported_aliases:
                cond = re.sub(r'\b' + imp + r'\.([a-zA-Z0-9_]+)\(', imp + r'::\1(', cond)
            block_stack.append(('while', 'while'))
            current_target.append(f"while ({cond}) {{")
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

        m_else_if = re.match(r'^\s*(?:\}\s*)?(?:\?else\s+if|else\s+if)\s*\((.*)\)\s*\{', stripped)
        if m_else_if:
            cond = m_else_if.group(1)
            cond = cond.replace('@', '')
            cond = re.sub(r'&([a-zA-Z0-9_]+)', r'\1', cond)
            cond = re.sub(r'\bnot\b', '!', cond)
            cond = re.sub(r'\band\b', '&&', cond)
            cond = re.sub(r'\bor\b', '||', cond)
            cond = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', cond)
            cond = apply_primitive_static_transforms(cond)
            for imp in imported_aliases:
                cond = re.sub(r'\b' + imp + r'\.([a-zA-Z0-9_]+)\(', imp + r'::\1(', cond)
            if block_stack and block_stack[-1][0] in ('if', 'else_if'):
                block_stack.pop()
            block_stack.append(('else_if', 'else_if'))
            current_target.append(f"}} else if ({cond}) {{")
            continue

        m_if = re.match(r'^\s*(?:\?if|if)\s*\((.*)\)\s*\{', stripped)
        if m_if:
            cond = m_if.group(1)
            cond = cond.replace('@', '')
            cond = re.sub(r'&([a-zA-Z0-9_]+)', r'\1', cond)
            cond = re.sub(r'\bnot\b', '!', cond)
            cond = re.sub(r'\band\b', '&&', cond)
            cond = re.sub(r'\bor\b', '||', cond)
            cond = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', cond)
            cond = apply_primitive_static_transforms(cond)
            for imp in imported_aliases:
                cond = re.sub(r'\b' + imp + r'\.([a-zA-Z0-9_]+)\(', imp + r'::\1(', cond)
            block_stack.append(('if', 'if'))
            current_target.append(f"if ({cond}) {{")
            continue

        m_else = re.match(r'^\s*(?:\}\s*)?(?:\?else|else)\s*\{', stripped)
        if m_else and (not block_stack or block_stack[-1][0] != 'match'):
            if block_stack and block_stack[-1][0] in ('if', 'else_if'):
                block_stack.pop()
            block_stack.append(('else', 'else'))
            current_target.append("} else {")
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
                    cname = bdata
                    current_class_name = None
                    current_target.append("};")
                    current_target.append(f"using {cname} = _cls_{cname};")
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

        # 8. Raw buffer creation: &name create | bytes, <size>; or &name create | bytes;
        m_raw_create = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+create\s*\|\s*(bytes|bits)(?:\s*,\s*([^;]+))?\s*;?\s*$', stripped)
        if m_raw_create:
            vname = m_raw_create.group(1)
            btype = m_raw_create.group(2)
            sz = m_raw_create.group(3)
            if btype == 'bytes':
                size_val = sz.strip() if sz else "1024" # Default maximum size 1024 bytes (1 KB)
                current_target.append(f"viss::Bytes {vname}({size_val});")
            elif btype == 'bits':
                size_val = sz.strip() if sz else "8192" # Default maximum size 8192 bits (1024 bytes)
                current_target.append(f"viss::Bits {vname}({size_val});")
            continue

        # 9. Raw buffer index assignment: &name[idx] = val;
        m_raw_idx = re.match(r'^\s*&([a-zA-Z0-9_]+)\[([^\]]+)\]\s*=\s*(.+?)\s*;?\s*$', stripped)
        if m_raw_idx:
            vname = m_raw_idx.group(1)
            idx_expr = m_raw_idx.group(2).strip().replace('@', '').replace('&', '')
            val_expr = m_raw_idx.group(3).strip().replace('@', '').replace('&', '')
            val_expr = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', val_expr)
            val_expr = apply_primitive_static_transforms(val_expr)
            current_target.append(f"{vname}[{idx_expr}] = {val_expr};")
            continue

        # 10. Raw buffer assignment: &name = val;
        m_raw_assign = re.match(r'^\s*&([a-zA-Z0-9_]+)\s*=\s*(.+?)\s*;?\s*$', stripped)
        if m_raw_assign:
            vname = m_raw_assign.group(1)
            val_expr = m_raw_assign.group(2).strip()
            val_clean = val_expr.replace('@', '').replace('&', '')
            for c in known_classes:
                val_clean = re.sub(r'\b' + c + r'\.([a-zA-Z0-9_!]+)\(', r'_cls_' + c + r'::\1(', val_clean)
            for imp in imported_aliases:
                val_clean = re.sub(r'\b' + imp + r'\.([a-zA-Z0-9_]+)\(', imp + r'::\1(', val_clean)
            val_clean = re.sub(r'([a-zA-Z0-9_]+)!\(', r'\1_bang(', val_clean)
            val_clean = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', val_clean)
            val_clean = apply_primitive_static_transforms(val_clean)
            if val_clean.startswith('[') and val_clean.endswith(']'):
                val_clean = '{' + val_clean[1:-1] + '}'
            current_target.append(f"{vname} = {val_clean};")
            continue

        # 11. Variable assignments & declarations with pipe
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
                'double': 'viss::Dec',
                'float': 'viss::Dec',
                'bool': 'viss::Bool',
                'bytes': 'viss::Bytes',
                'bits': 'viss::Bits',
                'list': 'viss::List<viss::Str>',
                'map': 'viss::Map<viss::Str, viss::Str>',
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
            val = val.replace('@', '').replace('&', '').replace('await ', '')
            val = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', val)
            declared_vars.add(vname)
            current_target.append(f"const auto {vname} = {val};")
            continue

        # Variable declarations: @var = value | type;
        m_pipe = re.match(r'^\s*@([a-zA-Z0-9_]+)\s*=\s*(.+?)\s*\|\s*([a-zA-Z0-9_]+)\s*;?\s*$', transpiled_line)
        if m_pipe:
            vname, val, ptype = m_pipe.group(1), m_pipe.group(2), m_pipe.group(3)
            val = val.replace('@', '').replace('&', '')
            val = re.sub(r'\bawait\s+([a-zA-Z0-9_.]+)\(([^)]*)\)', r'(\1(\2)).get()', val)
            val = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', val)
            for c in known_classes:
                val = re.sub(r'\b' + c + r'\.([a-zA-Z0-9_!]+)\(', r'_cls_' + c + r'::\1(', val)
            for imp in imported_aliases:
                val = re.sub(r'\b' + imp + r'\.([a-zA-Z0-9_]+)\(', imp + r'::\1(', val)
            val = re.sub(r'([a-zA-Z0-9_]+)!\(', r'\1_bang(', val)
            val = apply_primitive_static_transforms(val)

            declared_vars.add(vname)
            if ptype == 'list':
                if val.startswith('[') and val.endswith(']'):
                    val = '{' + val[1:-1] + '}'
                current_target.append(f"viss::List {vname} = {val};")
            elif ptype == 'map':
                current_target.append(f"viss::Map {vname} = {val};")
            elif ptype == 'inf':
                current_target.append(f"viss::Inf {vname} = {val};")
            elif ptype == 'bytes':
                current_target.append(f"viss::Bytes {vname} = {val};")
            elif ptype == 'bits':
                current_target.append(f"viss::Bits {vname} = {val};")
            elif ptype == 'str':
                current_target.append(f"viss::Str {vname} = viss::toStr({val});")
            elif ptype == 'int':
                current_target.append(f"viss::Int {vname} = (viss::Int)({val});")
            elif ptype in ('dec', 'double', 'float'):
                current_target.append(f"viss::Dec {vname} = (viss::Dec)({val});")
            elif ptype == 'bool':
                current_target.append(f"viss::Bool {vname} = (viss::Bool)({val});")
            elif ptype == 'class':
                current_target.append(f"auto {vname} = {val};")
            else:
                current_target.append(f"auto {vname} = {val};")
            continue

        # Assignment operators: @var += value; @var -= value; etc.
        m_assign_op = re.match(r'^\s*@([a-zA-Z0-9_]+)\s*(\+=|-=|\*=|/=|%=)\s*(.+?)\s*;?\s*$', transpiled_line)
        if m_assign_op:
            vname, op, val = m_assign_op.group(1), m_assign_op.group(2), m_assign_op.group(3)
            val = val.replace('@', '').replace('&', '')
            val = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', val)
            val = apply_primitive_static_transforms(val)
            current_target.append(f"{vname} {op} {val};")
            continue

        # Generic assignment: @var = value;
        m_assign = re.match(r'^\s*@([a-zA-Z0-9_]+)\s*=\s*(.+?)\s*;?\s*$', transpiled_line)
        if m_assign:
            vname, val = m_assign.group(1), m_assign.group(2)
            val = val.replace('@', '').replace('&', '')
            val = re.sub(r'\bawait\s+([a-zA-Z0-9_.]+)\(([^)]*)\)', r'(\1(\2)).get()', val)
            val = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', val)
            for c in known_classes:
                val = re.sub(r'\b' + c + r'\.([a-zA-Z0-9_!]+)\(', r'_cls_' + c + r'::\1(', val)
            for imp in imported_aliases:
                val = re.sub(r'\b' + imp + r'\.([a-zA-Z0-9_]+)\(', imp + r'::\1(', val)
            val = re.sub(r'([a-zA-Z0-9_]+)!\(', r'\1_bang(', val)
            val = apply_primitive_static_transforms(val)
            if vname in declared_vars:
                current_target.append(f"{vname} = {val};")
            else:
                declared_vars.add(vname)
                current_target.append(f"auto {vname} = {val};")
            continue

        # Await standalone call: await async.sleep(100);
        m_await_call = re.match(r'^\s*await\s+([a-zA-Z0-9_.]+)\(([^)]*)\)\s*;?', transpiled_line)
        if m_await_call:
            fn, args = m_await_call.group(1), m_await_call.group(2).replace('@', '').replace('&', '')
            for imp in imported_aliases:
                fn = re.sub(r'\b' + imp + r'\.', imp + r'::', fn)
            current_target.append(f"{fn}({args});")
            continue

        # Control keywords
        transpiled_line = re.sub(r'!break\s*;?', 'break;', transpiled_line)
        transpiled_line = re.sub(r'!continue\s*;?', 'continue;', transpiled_line)
        transpiled_line = re.sub(r'\bnot\b', '!', transpiled_line)
        transpiled_line = re.sub(r'\band\b', '&&', transpiled_line)
        transpiled_line = re.sub(r'\bor\b', '||', transpiled_line)
        transpiled_line = re.sub(r'\bNull\b', 'nullptr', transpiled_line)

        # In-place clamp: target.clamp(min, max) -> viss::math::clamp_in_place(target, min, max)
        transpiled_line = re.sub(r'([a-zA-Z0-9_@&]+)\.clamp\(([^)]+)\)', r'viss::math::clamp_in_place(\1, \2)', transpiled_line)

        # Property getters (.len, .first, .last)
        transpiled_line = re.sub(r'([a-zA-Z0-9_@&]+)\.len\b', r'\1.size()', transpiled_line)
        transpiled_line = re.sub(r'([a-zA-Z0-9_@&]+)\.first\b', r'\1.get_first()', transpiled_line)
        transpiled_line = re.sub(r'([a-zA-Z0-9_@&]+)\.last\b', r'\1.get_last()', transpiled_line)

        # Apply static primitive helpers
        transpiled_line = apply_primitive_static_transforms(transpiled_line)

        # Class static calls
        for c in known_classes:
            transpiled_line = re.sub(r'\b' + c + r'\.([a-zA-Z0-9_!]+)\(', r'_cls_' + c + r'::\1(', transpiled_line)

        # Imported modules alias.func( -> alias::func(
        for imp in imported_aliases:
            transpiled_line = re.sub(r'\b' + imp + r'\.([a-zA-Z0-9_]+)\(', imp + r'::\1(', transpiled_line)

        # Transform function calls with ! in name: func!(...) -> func_bang(...)
        transpiled_line = re.sub(r'([a-zA-Z0-9_]+)!\(', r'\1_bang(', transpiled_line)

        # Strip leftover & and @
        transpiled_line = re.sub(r'&([a-zA-Z0-9_]+)', r'\1', transpiled_line)
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
            if line.strip() == f"struct _cls_{cname} {{" or line.strip().startswith(f"struct _cls_{cname} :"):
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
        print_help()
        sys.exit(0)

    cmd = sys.argv[1]

    if cmd in ("-v", "--version", "version"):
        print(f"Viss Programming Language Compiler & Toolchain v{VERSION}")
        print("Architecture: Viss 2.0 Native Pipeline -> C++17")
        sys.exit(0)

    if cmd in ("-h", "--help", "help"):
        print_help()
        sys.exit(0)

    if cmd == "check":
        if len(sys.argv) < 3:
            print("Usage: viss check <file.viss> [--json]")
            sys.exit(1)
        viss_file = sys.argv[2]
        as_json = "--json" in sys.argv
        if not os.path.exists(viss_file):
            print(f"Error: File '{viss_file}' not found.")
            sys.exit(1)
        with open(viss_file, 'r', encoding='utf-8') as f:
            code = f.read()
        diags = check_syntax(code, os.path.basename(viss_file))
        if as_json:
            print(json.dumps(diags, indent=2))
        else:
            if not diags:
                print(f"[Viss Checker] No issues found in '{viss_file}'. Everything is clean! ^_^")
            else:
                print(f"[Viss Checker] Found {len(diags)} diagnostic issue(s) in '{viss_file}':")
                for d in diags:
                    sev = d['severity'].upper()
                    print(f"  [{sev}] Line {d['line']}, Col {d['col']}: {d['message']}")
        sys.exit(0)

    if cmd == "init":
        pname = sys.argv[2] if len(sys.argv) > 2 else "my_viss_app"
        os.makedirs(pname, exist_ok=True)
        main_viss = os.path.join(pname, "main.viss")
        with open(main_viss, 'w', encoding='utf-8') as f:
            f.write('$import lib "iostream" as io\n\n!func main() {\n    io.println("Hello, Viss 2.0!");\n}\n')
        print(f"Initialized new Viss project in '{pname}/'.")
        print(f"Run it with: viss run {pname}/main.viss")
        sys.exit(0)

    # If first argument is 'run' or 'compile'
    if cmd == "run":
        viss_file = sys.argv[2] if len(sys.argv) > 2 else ""
        run_after = True
    elif cmd == "compile":
        viss_file = sys.argv[2] if len(sys.argv) > 2 else ""
        run_after = False
    else:
        viss_file = cmd
        run_after = "-r" in sys.argv or "-run" in sys.argv or "run" in sys.argv

    if not viss_file or not os.path.exists(viss_file):
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
    if not cxx:
        candidate = r"C:\AGY\TOOLS\w64devkit\bin\g++.exe"
        if os.path.exists(candidate):
            cxx = candidate
    if cxx:
        cxx_dir = os.path.dirname(os.path.abspath(cxx))
        env = os.environ.copy()
        env["PATH"] = cxx_dir + os.pathsep + env.get("PATH", "")
        print(f"[Viss Compiler v{VERSION}] Compiling binary using {os.path.basename(cxx)}...")
        res = subprocess.run([cxx, "-std=c++17", cpp_file, "-o", exe_file], env=env)
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
        print(f"To compile binary: g++ -std=c++17 {cpp_file} -o {exe_file}")

def print_help():
    print(f"Viss Programming Language CLI (v{VERSION})")
    print("Usage:")
    print("  viss run <file.viss>          Compile and run Viss file")
    print("  viss compile <file.viss>      Transpile and compile to executable")
    print("  viss check <file.viss>        Check syntax and lint diagnostics")
    print("  viss init [project_name]      Create starter project")
    print("  viss version                  Display version info")

if __name__ == "__main__":
    main()
