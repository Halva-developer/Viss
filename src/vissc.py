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
            if not re.search(r'^\s*&[a-zA-Z0-9_]+\s+create\s*\|\s*(bytes|bits|bites|hybrid|grid|bytemask|colormask|mask)', stripped, re.IGNORECASE):
                diagnostics.append({
                    "line": line_idx + 1,
                    "col": 1,
                    "message": "Malformed raw buffer creation. Expected: &name create | bytes|bits|bytemask|colormask|grid|hybrid ...;",
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

def replace_module_calls(text, imported_aliases):
    for imp in imported_aliases:
        text = re.sub(r'(?<![@&a-zA-Z0-9_])' + imp + r'\.([a-zA-Z0-9_]+)\.([a-zA-Z0-9_]+)\(', imp + r'::\1::\2(', text)
        text = re.sub(r'(?<![@&a-zA-Z0-9_])' + imp + r'\.([a-zA-Z0-9_]+)\(', imp + r'::\1(', text)
    return text

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

    # time.*
    res = re.sub(r'\btime\.([a-zA-Z0-9_]+)\(', r'viss::time::\1(', res)
    res = re.sub(r'\btime\.Stopwatch\b', 'viss::time::Stopwatch', res)
    return res

def transpile(viss_code, filename):
    # Step 1: Pre-process string interpolation
    processed_code = translate_interpolation(viss_code)
    
    # Step 2: Pre-process imports before hiding string literals
    includes_section = [
        '#include "libs/vissrt.hpp"',
        '#include "libs/std/mask.hpp"',
        'using namespace viss;',
        'namespace io = viss::io;',
        'namespace sys = viss::sys;',
        'namespace fs = viss::fs;',
        'namespace math = viss::math;',
        'namespace str = viss::str;',
        'namespace rt = viss::retrotech;',
        'namespace async = viss::async;',
        'namespace json = viss::json;',
        'namespace crypto = viss::crypto;',
        'namespace collections = viss::collections;',
        'namespace env = viss::env;',
        'namespace net = viss::net;',
        'namespace mask = viss::bytemask;',
        'namespace bytemask = viss::bytemask;',
        'namespace colormask = viss::bytemask;'
    ]
    clean_lines = []
    imported_aliases = {"io", "async", "rt", "sys", "fs", "math", "str", "json", "crypto", "collections", "env", "net", "mask", "bytemask", "colormask"}

    
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
                includes_section.append(f'#include "libs/std/retrotech.hpp"\nnamespace {alias} = viss::retrotech;\nnamespace draw = viss::retrotech::draw;\nnamespace screen = viss::retrotech::screen;')
            elif lib in ("mask", "bytemask", "colormask"):
                includes_section.append(f'#include "libs/std/mask.hpp"\nnamespace {alias} = viss::bytemask;')
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
        # Strip single-line comments so they don't comment out collapsed multi-line lists
        lines = []
        for l in text.split('\n'):
            lines.append(re.sub(r'//.*$', '', l))
        text_clean = '\n'.join(lines)

        out = []
        depth = 0
        for ch in text_clean:
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

        # Grid edit block collection
        if block_stack and block_stack[-1][0] == 'grid_edit':
            if stripped == '}' or stripped.startswith('}'):
                binfo = block_stack.pop()
                vname = binfo[1]
                rows = binfo[2]
                pre_brace = stripped[:stripped.index('}')].strip()
                if pre_brace:
                    clean_row = pre_brace.rstrip(':;,').strip()
                    tokens = [t.strip() for t in clean_row.split(',') if t.strip()]
                    if tokens:
                        rows.append(tokens)
                rows_cpp = []
                for r in rows:
                    row_tokens = []
                    for t in r:
                        clean_t = t.replace('@', '')
                        clean_t = re.sub(r'&([a-zA-Z0-9_]+)', r'\1', clean_t)
                        clean_t = apply_primitive_static_transforms(clean_t)
                        row_tokens.append(clean_t)
                    rows_cpp.append("{" + ", ".join(row_tokens) + "}")
                all_rows = ",\n        ".join(rows_cpp)
                current_target.append(f"{vname}.grid_edit({{\n        {all_rows}\n    }});")
                continue
            else:
                clean_row = stripped.rstrip(':;,').strip()
                if clean_row:
                    tokens = [t.strip() for t in clean_row.split(',') if t.strip()]
                    if tokens:
                        block_stack[-1][2].append(tokens)
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

        # Single-line !async.func: !async.func name(params) { body; }
        m_single_async = re.match(r'^\s*!async\.func\s+([a-zA-Z0-9_?!]+)\s*\(([^)]*)\)(?:\s+to\s+([a-zA-Z0-9_<>]+))?\s*\{\s*(.+?)\s*\}\s*;?\s*$', stripped)
        if m_single_async:
            raw_fname = m_single_async.group(1)
            fname = raw_fname.replace('?', '_q').replace('!', '_bang')
            params = transpile_params(m_single_async.group(2))
            body = m_single_async.group(4).strip()
            body = body.replace('@', '')
            body = re.sub(r'&([a-zA-Z0-9_]+)', r'\1', body)
            body = apply_primitive_static_transforms(body)
            body = replace_module_calls(body, imported_aliases)
            if not body.endswith(';'): body += ';'
            functions_section.append(f"inline auto {fname}({params}) {{ return viss::getGlobalThreadPool().enqueue([=]() {{ {body} }}); }}")
            continue

        # Single-line !func: !func name(params) { body; }
        m_single_func = re.match(r'^\s*!func\s+([a-zA-Z0-9_?!]+)\s*\(([^)]*)\)(?:\s+to\s+([a-zA-Z0-9_<>]+))?\s*\{\s*(.+?)\s*\}\s*;?\s*$', stripped)
        if m_single_func:
            raw_fname = m_single_func.group(1)
            fname = raw_fname.replace('?', '_q').replace('!', '_bang')
            params, t_clause = transpile_params_and_template(m_single_func.group(2))
            ret_type = m_single_func.group(3)
            ret_decl = f" -> {ret_type}" if ret_type else ""
            body = m_single_func.group(4).strip()
            body = body.replace('@', '')
            body = re.sub(r'&([a-zA-Z0-9_]+)', r'\1', body)
            body = apply_primitive_static_transforms(body)
            body = replace_module_calls(body, imported_aliases)
            if not body.endswith(';'): body += ';'
            target_sec = classes_section if current_class_name else functions_section
            prefix = "    static " if current_class_name else ""
            target_sec.append(f"{prefix}{t_clause}inline auto {fname}({params}){ret_decl} {{ {body} }}")
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
            cond = replace_module_calls(cond, imported_aliases)
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
            cond = replace_module_calls(cond, imported_aliases)
            if block_stack and block_stack[-1][0] in ('if', 'else_if'):
                block_stack.pop()
            block_stack.append(('else_if', 'else_if'))
            current_target.append(f"}} else if ({cond}) {{")
            continue

        # Single-line if: ?if (cond) { body; }
        m_single_if = re.match(r'^\s*(?:\?if|if)\s*\((.+?)\)\s*\{\s*(.+?)\s*\}\s*;?\s*$', stripped)
        if m_single_if:
            cond = m_single_if.group(1)
            body = m_single_if.group(2)
            cond = cond.replace('@', '')
            cond = re.sub(r'&([a-zA-Z0-9_]+)', r'\1', cond)
            cond = re.sub(r'\bnot\b', '!', cond)
            cond = re.sub(r'\band\b', '&&', cond)
            cond = re.sub(r'\bor\b', '||', cond)
            cond = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', cond)
            cond = apply_primitive_static_transforms(cond)
            cond = replace_module_calls(cond, imported_aliases)
            
            body = body.replace('@', '')
            body = re.sub(r'&([a-zA-Z0-9_]+)', r'\1', body)
            body = replace_module_calls(body, imported_aliases)
            if not body.endswith(';'): body += ';'
            current_target.append(f"if ({cond}) {{ {body} }}")
            continue

        # Single-line else: ?else { body; }
        m_single_else = re.match(r'^\s*(?:\?else|else)\s*\{\s*(.+?)\s*\}\s*;?\s*$', stripped)
        if m_single_else:
            body = m_single_else.group(1)
            body = body.replace('@', '')
            body = re.sub(r'&([a-zA-Z0-9_]+)', r'\1', body)
            body = replace_module_calls(body, imported_aliases)
            if not body.endswith(';'): body += ';'
            current_target.append(f"else {{ {body} }}")
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
            cond = replace_module_calls(cond, imported_aliases)
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

        # 8a. Hybrid buffer creation: &name create | bytes, 3, bits, 4; or &name create | bytes, 3, bites, 4;
        m_hybrid = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+create\s*\|\s*(?:hybrid\s*,\s*)?bytes\s*,\s*(\d+)\s*,\s*bi(?:t|te)s\s*,\s*(\d+)\s*;?\s*$', stripped, re.IGNORECASE)
        if m_hybrid:
            vname = m_hybrid.group(1)
            n_bytes = m_hybrid.group(2)
            n_bits = m_hybrid.group(3)
            current_target.append(f"viss::Hybrid {vname}({n_bytes}, {n_bits});")
            continue

        m_hybrid_rev = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+create\s*\|\s*(?:hybrid\s*,\s*)?bi(?:t|te)s\s*,\s*(\d+)\s*,\s*bytes\s*,\s*(\d+)\s*;?\s*$', stripped, re.IGNORECASE)
        if m_hybrid_rev:
            vname = m_hybrid_rev.group(1)
            n_bits = m_hybrid_rev.group(2)
            n_bytes = m_hybrid_rev.group(3)
            current_target.append(f"viss::Hybrid {vname}({n_bytes}, {n_bits});")
            continue

        # 8b. Multi-layer grid creation: &name create | grid 4,2 3,1; or grid, 4,2, 3,1;
        m_grid = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+create\s*\|\s*grid(?:\s*,\s*|\s+)([^;]+);?\s*$', stripped, re.IGNORECASE)
        if m_grid:
            vname = m_grid.group(1)
            args_str = m_grid.group(2).strip()
            nums = [int(n) for n in re.findall(r'\d+', args_str)]
            pairs = []
            for i in range(0, len(nums) - 1, 2):
                pairs.append(f"{{{nums[i]}, {nums[i+1]}}}")
            if len(nums) % 2 != 0:
                pairs.append(f"{{{nums[-1]}, 1}}")
            pairs_str = ", ".join(pairs)
            current_target.append(f"viss::Grid {vname}({{{pairs_str}}});")
            continue

        # 8c. Standard Raw buffer creation: &name create | bytes, <size>; or &name create | bits, <size>; or &name create | bytemask, <size>;
        m_raw_create = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+create\s*\|\s*(bytes|bits|bites|bytemask|colormask|mask)(?:\s*,\s*([^;]+))?\s*;?\s*$', stripped, re.IGNORECASE)
        if m_raw_create:
            vname = m_raw_create.group(1)
            declared_vars.add(vname)
            btype = m_raw_create.group(2).lower()
            sz = m_raw_create.group(3)
            if btype in ('bytes', 'bytemask', 'mask'):
                if sz:
                    parts = [p.strip() for p in sz.split(',')]
                    if len(parts) == 2 and parts[0].isdigit() and parts[1].isdigit():
                        size_val = str(int(parts[0]) * int(parts[1]))
                    else:
                        size_val = sz.strip()
                else:
                    size_val = "1024" # Default maximum size 1024 bytes (1 KB)
                current_target.append(f"viss::Bytes {vname}({size_val});")
            elif btype == 'colormask':
                if sz:
                    sz_clean = sz.strip()
                    if sz_clean.isdigit():
                        size_val = str(int(sz_clean) * 3)
                    else:
                        size_val = f"({sz_clean}) * 3"
                else:
                    size_val = "48" # Default 16 colors = 48 bytes
                current_target.append(f"viss::Bytes {vname}({size_val});")
            elif btype in ('bits', 'bites'):
                size_val = sz.strip() if sz else "8192" # Default maximum size 8192 bits (1024 bytes)
                current_target.append(f"viss::Bits {vname}({size_val});")
            continue

        # 8d. Grid set: &name grid set 10,1 10,2; or &name grid set 10,1, 10,2;
        m_grid_set = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+grid\s+set(?:\s*,\s*|\s+)([^;]+);?\s*$', stripped, re.IGNORECASE)
        if m_grid_set:
            vname = m_grid_set.group(1)
            args_str = m_grid_set.group(2).strip()
            nums = [int(n) for n in re.findall(r'\d+', args_str)]
            pairs = []
            for i in range(0, len(nums) - 1, 2):
                pairs.append(f"{{{nums[i]}, {nums[i+1]}}}")
            if len(nums) % 2 != 0:
                pairs.append(f"{{{nums[-1]}, 1}}")
            pairs_str = ", ".join(pairs)
            current_target.append(f"{vname}.grid_set({{{pairs_str}}});")
            continue

        # 8e. Single-line grid edit: &name grid edit { 1, 2: 3, 4; }
        m_single_grid_edit = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+grid\s+edit\s*\{\s*(.+?)\s*\}\s*;?\s*$', stripped, re.IGNORECASE)
        if m_single_grid_edit:
            vname = m_single_grid_edit.group(1)
            body = m_single_grid_edit.group(2).strip()
            raw_rows = re.split(r'[:;]\s*', body)
            rows_cpp = []
            for r in raw_rows:
                r_clean = r.strip()
                if not r_clean: continue
                tokens = [t.strip().replace('@', '') for t in r_clean.split(',') if t.strip()]
                clean_tokens = [re.sub(r'&([a-zA-Z0-9_]+)', r'\1', apply_primitive_static_transforms(tok)) for tok in tokens]
                rows_cpp.append("{" + ", ".join(clean_tokens) + "}")
            all_rows = ", ".join(rows_cpp)
            current_target.append(f"{vname}.grid_edit({{{all_rows}}});")
            continue

        # 8f. Multi-line grid edit: &name grid edit {
        m_grid_edit = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+grid\s+edit\s*\{', stripped, re.IGNORECASE)
        if m_grid_edit:
            vname = m_grid_edit.group(1)
            block_stack.append(('grid_edit', vname, []))
            continue

        # 8g. Grid commands: &name grid print/show/dump/clear/invert/flip_h/flip_v
        m_grid_cmd = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+grid\s+(print|show|dump|clear|invert|flip_h|flip_v|flip\s+h|flip\s+v);?\s*$', stripped, re.IGNORECASE)
        if m_grid_cmd:
            vname = m_grid_cmd.group(1)
            cmd = m_grid_cmd.group(2).lower().replace(' ', '_')
            if cmd in ('print', 'show', 'dump'):
                current_target.append(f"{vname}.grid_print();")
            elif cmd == 'clear':
                current_target.append(f"{vname}.grid_clear();")
            elif cmd == 'invert':
                current_target.append(f"{vname}.grid_invert();")
            elif cmd in ('flip_h', 'flip_v'):
                current_target.append(f"{vname}.{cmd}();")
            continue

        # 8h. Grid fill: &name grid fill <val>;
        m_grid_fill = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+grid\s+fill\s+([^;]+);?\s*$', stripped, re.IGNORECASE)
        if m_grid_fill:
            vname = m_grid_fill.group(1)
            val = m_grid_fill.group(2).strip().replace('@', '')
            val = re.sub(r'&([a-zA-Z0-9_]+)', r'\1', val)
            val = apply_primitive_static_transforms(val)
            current_target.append(f"{vname}.grid_fill({val});")
            continue

        # 8i. Grid resize: &name grid resize <w>, <h>;
        m_grid_resize = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+grid\s+resize\s+([^;]+);?\s*$', stripped, re.IGNORECASE)
        if m_grid_resize:
            vname = m_grid_resize.group(1)
            args = m_grid_resize.group(2).strip().replace('@', '')
            args = re.sub(r'&([a-zA-Z0-9_]+)', r'\1', args)
            current_target.append(f"{vname}.grid_resize({args});")
            continue

        # 8j. Stream write: &name write <type> <val> [at <offset>];
        m_write_typed = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+write\s+(str|string|int|dec|double|float|bool|byte|u8|i8|u16|i16|u32|i32|u64|i64|bytes)\s+(.+?)(?:\s+at\s+([^;]+))?;?\s*$', stripped, re.IGNORECASE)
        if m_write_typed:
            vname = m_write_typed.group(1)
            wtype = m_write_typed.group(2).lower()
            val = m_write_typed.group(3).strip()
            val = re.sub(r'[@&]([a-zA-Z0-9_]+)', r'\1', val)
            val = apply_primitive_static_transforms(val)
            val = replace_module_calls(val, imported_aliases)
            offset = m_write_typed.group(4)
            offset_clean = re.sub(r'[@&]([a-zA-Z0-9_]+)', r'\1', offset.strip()) if offset else ""
            offset_arg = f", {offset_clean}" if offset else ""

            if wtype in ('str', 'string'):
                current_target.append(f"{vname}.write_str({val}{offset_arg});")
            elif wtype == 'int':
                current_target.append(f"{vname}.write_int({val}{offset_arg});")
            elif wtype in ('dec', 'double'):
                current_target.append(f"{vname}.write_dec({val}{offset_arg});")
            elif wtype == 'float':
                current_target.append(f"{vname}.write_float({val}{offset_arg});")
            elif wtype == 'bool':
                current_target.append(f"{vname}.write_bool({val}{offset_arg});")
            elif wtype in ('byte', 'u8'):
                current_target.append(f"{vname}.write_u8({val}{offset_arg});")
            elif wtype == 'i8':
                current_target.append(f"{vname}.write_i8({val}{offset_arg});")
            elif wtype == 'u16':
                current_target.append(f"{vname}.write_u16({val}{offset_arg});")
            elif wtype == 'i16':
                current_target.append(f"{vname}.write_i16({val}{offset_arg});")
            elif wtype == 'u32':
                current_target.append(f"{vname}.write_u32({val}{offset_arg});")
            elif wtype == 'i32':
                current_target.append(f"{vname}.write_i32({val}{offset_arg});")
            elif wtype == 'u64':
                current_target.append(f"{vname}.write_u64({val}{offset_arg});")
            elif wtype == 'i64':
                current_target.append(f"{vname}.write_i64({val}{offset_arg});")
            elif wtype == 'bytes':
                current_target.append(f"{vname}.write_bytes({val}{offset_arg});")
            continue

        # 8k. Auto stream write: &name write <val> [at <offset>];
        m_write_auto = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+write\s+(.+?)(?:\s+at\s+([^;]+))?;?\s*$', stripped, re.IGNORECASE)
        if m_write_auto:
            vname = m_write_auto.group(1)
            val = m_write_auto.group(2).strip()
            val_clean = re.sub(r'[@&]([a-zA-Z0-9_]+)', r'\1', val)
            val_clean = apply_primitive_static_transforms(val_clean)
            val_clean = replace_module_calls(val_clean, imported_aliases)
            offset = m_write_auto.group(3)
            offset_clean = re.sub(r'[@&]([a-zA-Z0-9_]+)', r'\1', offset.strip()) if offset else ""
            offset_arg = f", {offset_clean}" if offset else ""
            if val.startswith('"') or val.startswith('i"') or val.startswith('viss::Str('):
                current_target.append(f"{vname}.write_str({val_clean}{offset_arg});")
            elif val.lower() in ('true', 'false'):
                current_target.append(f"{vname}.write_bool({val_clean}{offset_arg});")
            elif re.match(r'^-?\d+\.\d+$', val):
                current_target.append(f"{vname}.write_dec({val_clean}{offset_arg});")
            else:
                current_target.append(f"{vname}.write_int({val_clean}{offset_arg});")
            continue

        # 8l. Stream cursor and buffer commands: &name seek/rewind/dump/clear/invert/fill
        m_stream_seek = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+seek\s+([^;]+);?\s*$', stripped, re.IGNORECASE)
        if m_stream_seek:
            vname = m_stream_seek.group(1)
            pos = m_stream_seek.group(2).strip().replace('@', '').replace('&', '')
            current_target.append(f"{vname}.seek({pos});")
            continue

        m_stream_rewind = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+rewind;?\s*$', stripped, re.IGNORECASE)
        if m_stream_rewind:
            vname = m_stream_rewind.group(1)
            current_target.append(f"{vname}.rewind();")
            continue

        m_stream_dump = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+(?:dump|hexdump);?\s*$', stripped, re.IGNORECASE)
        if m_stream_dump:
            vname = m_stream_dump.group(1)
            current_target.append(f"{vname}.dump();")
            continue

        m_buf_clear = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+clear;?\s*$', stripped, re.IGNORECASE)
        if m_buf_clear:
            vname = m_buf_clear.group(1)
            current_target.append(f"{vname}.clear();")
            continue

        m_buf_invert = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+invert;?\s*$', stripped, re.IGNORECASE)
        if m_buf_invert:
            vname = m_buf_invert.group(1)
            current_target.append(f"{vname}.invert();")
            continue

        m_buf_fill = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+fill\s+([^;]+);?\s*$', stripped, re.IGNORECASE)
        if m_buf_fill:
            vname = m_buf_fill.group(1)
            val = m_buf_fill.group(2).strip().replace('@', '').replace('&', '')
            val = apply_primitive_static_transforms(val)
            current_target.append(f"{vname}.fill({val});")
            continue

        # 8m. Bitfield set: &name bitfield set <start>, <count>, <val>;
        m_bitfield_set = re.match(r'^\s*&([a-zA-Z0-9_]+)\s+bitfield\s+set\s+([^,]+),\s*([^,]+),\s*([^;]+);?\s*$', stripped, re.IGNORECASE)
        if m_bitfield_set:
            vname = m_bitfield_set.group(1)
            s_bit = m_bitfield_set.group(2).strip().replace('@', '').replace('&', '')
            c_bit = m_bitfield_set.group(3).strip().replace('@', '').replace('&', '')
            val = m_bitfield_set.group(4).strip().replace('@', '').replace('&', '')
            val = apply_primitive_static_transforms(val)
            current_target.append(f"{vname}.set_bitfield({s_bit}, {c_bit}, {val});")
            continue

        # 8n. Stream read: @var = &name read <type>[(<len>)] [at <offset>] [| <ptype>];
        m_stream_read = re.match(
            r'^\s*@([a-zA-Z0-9_]+)\s*=\s*&([a-zA-Z0-9_]+)\s+read\s+(str|string|int|dec|double|float|bool|byte|u8|i8|u16|i16|u32|i32|u64|i64|bytes)(?:\s*\(\s*([^)]*)\s*\)|\s+len\s+([a-zA-Z0-9_]+))?(?:\s+at\s+([^;|]+))?(?:\s*\|\s*([a-zA-Z0-9_]+))?;?\s*$',
            stripped, re.IGNORECASE
        )
        if m_stream_read:
            var_name = m_stream_read.group(1)
            buf_name = m_stream_read.group(2)
            rtype = m_stream_read.group(3).lower()
            len_arg = m_stream_read.group(4) or m_stream_read.group(5)
            at_arg = m_stream_read.group(6)
            pipe_type = m_stream_read.group(7)

            offset_clean = at_arg.strip().replace('@', '').replace('&', '') if at_arg else "-1"
            len_clean = len_arg.strip().replace('@', '').replace('&', '') if len_arg else ""

            call_expr = ""
            cpp_type = "auto"

            if rtype in ('str', 'string'):
                l_param = len_clean if len_clean else "-1"
                call_expr = f"{buf_name}.read_str({l_param}, {offset_clean})"
                cpp_type = "viss::Str"
            elif rtype == 'int':
                call_expr = f"{buf_name}.read_int({offset_clean})"
                cpp_type = "viss::Int"
            elif rtype in ('dec', 'double'):
                call_expr = f"{buf_name}.read_dec({offset_clean})"
                cpp_type = "viss::Dec"
            elif rtype == 'float':
                call_expr = f"{buf_name}.read_float({offset_clean})"
                cpp_type = "viss::Dec"
            elif rtype == 'bool':
                call_expr = f"{buf_name}.read_bool({offset_clean})"
                cpp_type = "viss::Bool"
            elif rtype in ('byte', 'u8'):
                call_expr = f"(viss::Int){buf_name}.read_u8({offset_clean})"
                cpp_type = "viss::Int"
            elif rtype == 'i8':
                call_expr = f"(viss::Int){buf_name}.read_i8({offset_clean})"
                cpp_type = "viss::Int"
            elif rtype in ('u16', 'i16', 'u32', 'i32', 'u64', 'i64'):
                call_expr = f"(viss::Int){buf_name}.read_{rtype}({offset_clean})"
                cpp_type = "viss::Int"
            elif rtype == 'bytes':
                l_param = len_clean if len_clean else "0"
                call_expr = f"{buf_name}.read_bytes({l_param}, {offset_clean})"
                cpp_type = "viss::Bytes"

            if pipe_type:
                if pipe_type == 'str': cpp_type = "viss::Str"
                elif pipe_type == 'int': cpp_type = "viss::Int"
                elif pipe_type in ('dec', 'double', 'float'): cpp_type = "viss::Dec"
                elif pipe_type == 'bool': cpp_type = "viss::Bool"
                elif pipe_type == 'bytes': cpp_type = "viss::Bytes"

            if var_name in declared_vars:
                current_target.append(f"{var_name} = {call_expr};")
            else:
                declared_vars.add(var_name)
                current_target.append(f"{cpp_type} {var_name} = {call_expr};")
            continue

        # 8o. Bitfield get: @var = &name bitfield get <start>, <count> [| <ptype>];
        m_bitfield_get = re.match(r'^\s*@([a-zA-Z0-9_]+)\s*=\s*&([a-zA-Z0-9_]+)\s+bitfield\s+get\s+([^,]+),\s*([^;|]+)(?:\s*\|\s*([a-zA-Z0-9_]+))?;?\s*$', stripped, re.IGNORECASE)
        if m_bitfield_get:
            var_name = m_bitfield_get.group(1)
            buf_name = m_bitfield_get.group(2)
            s_bit = m_bitfield_get.group(3).strip().replace('@', '').replace('&', '')
            c_bit = m_bitfield_get.group(4).strip().replace('@', '').replace('&', '')
            call_expr = f"(viss::Int){buf_name}.get_bitfield({s_bit}, {c_bit})"
            if var_name in declared_vars:
                current_target.append(f"{var_name} = {call_expr};")
            else:
                declared_vars.add(var_name)
                current_target.append(f"viss::Int {var_name} = {call_expr};")
            continue

        # 9. Raw buffer index assignment: &name[idx] = val; or &name[0][10] = val;
        m_raw_idx = re.match(r'^\s*&([a-zA-Z0-9_]+)((?:\[[^\]]+\])+)\s*=\s*(.+?)\s*;?\s*$', stripped)
        if m_raw_idx:
            vname = m_raw_idx.group(1)
            idx_expr = m_raw_idx.group(2).replace('@', '').replace('&', '')
            val_expr = m_raw_idx.group(3).strip().replace('@', '').replace('&', '')
            val_expr = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', val_expr)
            val_expr = apply_primitive_static_transforms(val_expr)
            current_target.append(f"{vname}{idx_expr} = {val_expr};")
            continue

        # 10. Raw buffer assignment: &name = val;
        m_raw_assign = re.match(r'^\s*&([a-zA-Z0-9_]+)\s*=\s*(.+?)\s*;?\s*$', stripped)
        if m_raw_assign:
            vname = m_raw_assign.group(1)
            val_expr = m_raw_assign.group(2).strip()
            val_clean = val_expr.replace('@', '').replace('&', '')
            for c in known_classes:
                val_clean = re.sub(r'\b' + c + r'\.([a-zA-Z0-9_!]+)\(', r'_cls_' + c + r'::\1(', val_clean)
            val_clean = replace_module_calls(val_clean, imported_aliases)
            val_clean = re.sub(r'([a-zA-Z0-9_]+)!\(', r'\1_bang(', val_clean)
            val_clean = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', val_clean)
            val_clean = apply_primitive_static_transforms(val_clean)
            if val_clean.startswith('[') and val_clean.endswith(']'):
                val_clean = '{' + val_clean[1:-1] + '}'
            if vname in declared_vars:
                current_target.append(f"{vname} = {val_clean};")
            else:
                declared_vars.add(vname)
                current_target.append(f"auto {vname} = {val_clean};")
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
            val = replace_module_calls(val, imported_aliases)
            val = re.sub(r'[@&]([a-zA-Z0-9_]+)', r'\1', val).replace('await ', '')
            val = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', val)
            declared_vars.add(vname)
            current_target.append(f"const auto {vname} = {val};")
            continue

        # Variable declarations: @var = value | type;
        m_pipe = re.match(r'^\s*@([a-zA-Z0-9_]+)\s*=\s*(.+?)\s*\|\s*([a-zA-Z0-9_]+)\s*;?\s*$', transpiled_line)
        if m_pipe:
            vname, val, ptype = m_pipe.group(1), m_pipe.group(2), m_pipe.group(3)
            val = replace_module_calls(val, imported_aliases)
            val = re.sub(r'[@&]([a-zA-Z0-9_]+)', r'\1', val)
            val = re.sub(r'\bawait\s+([a-zA-Z0-9_.]+)\(([^)]*)\)', r'(\1(\2)).get()', val)
            val = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', val)
            for c in known_classes:
                val = re.sub(r'\b' + c + r'\.([a-zA-Z0-9_!]+)\(', r'_cls_' + c + r'::\1(', val)
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
            elif ptype in ('bytes', 'bytemask', 'mask'):
                current_target.append(f"viss::Bytes {vname} = {val};")
            elif ptype in ('bits', 'bites'):
                current_target.append(f"viss::Bits {vname} = {val};")
            elif ptype in ('hybrid', 'hybrid_t'):
                current_target.append(f"viss::Hybrid {vname} = {val};")
            elif ptype in ('grid', 'grid_t'):
                current_target.append(f"viss::Grid {vname} = {val};")
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
            val = replace_module_calls(val, imported_aliases)
            val = re.sub(r'[@&]([a-zA-Z0-9_]+)', r'\1', val)
            val = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', val)
            val = apply_primitive_static_transforms(val)
            current_target.append(f"{vname} {op} {val};")
            continue

        # Generic assignment: @var = value;
        m_assign = re.match(r'^\s*@([a-zA-Z0-9_]+)\s*=\s*(.+?)\s*;?\s*$', transpiled_line)
        if m_assign:
            vname, val = m_assign.group(1), m_assign.group(2)
            val = replace_module_calls(val, imported_aliases)
            val = re.sub(r'[@&]([a-zA-Z0-9_]+)', r'\1', val)
            val = re.sub(r'\bawait\s+([a-zA-Z0-9_.]+)\(([^)]*)\)', r'(\1(\2)).get()', val)
            val = re.sub(r'([a-zA-Z0-9_]+)\?', r'\1_q', val)
            for c in known_classes:
                val = re.sub(r'\b' + c + r'\.([a-zA-Z0-9_!]+)\(', r'_cls_' + c + r'::\1(', val)
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

        # Imported modules alias.func( -> alias::func( and nested alias.sub.func( -> alias::sub::func(
        transpiled_line = replace_module_calls(transpiled_line, imported_aliases)

        # Standalone .draw.sprite(...) -> rt::draw::sprite(...)
        transpiled_line = re.sub(r'(?:^|[^\w])\.draw\.sprite\s*\(', ' rt::draw::sprite(', transpiled_line)

        # Convert list literals inside function call arguments: ([...]) -> ({...})
        transpiled_line = re.sub(r'\(\s*\[([^\]]+)\]\s*\)', r'({\1})', transpiled_line)

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

        current_target.append(f'#line {line_num} "{filename}"')
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

    if cmd == "transpile":
        if len(sys.argv) < 3:
            print("Usage: viss transpile <input.viss> [output.cpp]")
            sys.exit(1)
        viss_file = sys.argv[2]
        cpp_file = sys.argv[3] if len(sys.argv) > 3 else os.path.splitext(viss_file)[0] + ".cpp"
        with open(viss_file, 'r', encoding='utf-8') as f:
            viss_code = f.read()
        validate_viss_syntax(viss_code, os.path.basename(viss_file))
        cpp_code = transpile(viss_code, os.path.basename(viss_file))
        with open(cpp_file, 'w', encoding='utf-8') as f:
            f.write(cpp_code)
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
    stem = os.path.splitext(os.path.basename(viss_file))[0]
    cache_dir = os.path.join(os.path.dirname(os.path.abspath(viss_file)), ".viss_cache", stem)
    os.makedirs(cache_dir, exist_ok=True)
    cpp_file = os.path.join(cache_dir, stem + ".cpp")
    exe_file = base + ".exe" if os.name == 'nt' else base

    if "-o" in sys.argv:
        o_idx = sys.argv.index("-o")
        if o_idx + 1 < len(sys.argv):
            exe_file = sys.argv[o_idx + 1]

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
        if getattr(sys, 'frozen', False):
            viss_root = os.path.dirname(os.path.abspath(sys.executable))
        else:
            viss_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        file_dir = os.path.dirname(os.path.abspath(viss_file))
        flags = [cxx, "-std=c++17", "-O2", f"-I{viss_root}", f"-I{file_dir}", "-I.", cpp_file]

        if os.name == 'nt':
            if "-mwindows" in sys.argv or "--gui" in sys.argv:
                flags.append("-mwindows")

            # Look for icon
            icon_path = None
            if "--icon" in sys.argv:
                i_idx = sys.argv.index("--icon")
                if i_idx + 1 < len(sys.argv):
                    icon_path = os.path.abspath(sys.argv[i_idx + 1])
            else:
                for cand_icon in [
                    os.path.join(file_dir, "app_icon.ico"),
                    os.path.join(file_dir, "icon.ico"),
                    os.path.join(os.getcwd(), "app_icon.ico"),
                    os.path.join(os.getcwd(), "icon.ico")
                ]:
                    if os.path.exists(cand_icon):
                        icon_path = os.path.abspath(cand_icon)
                        break

            if icon_path and os.path.exists(icon_path):
                windres = shutil.which("windres")
                if not windres:
                    candidate_wr = r"C:\AGY\TOOLS\w64devkit\bin\windres.exe"
                    if os.path.exists(candidate_wr):
                        windres = candidate_wr
                if windres:
                    rc_file = os.path.join(cache_dir, "app_icon.rc")
                    res_obj = os.path.join(cache_dir, "app_icon.res.o")
                    clean_ico = icon_path.replace("\\", "/")
                    with open(rc_file, "w", encoding="utf-8") as rf:
                        rf.write(f'1 ICON "{clean_ico}"\n')
                    wr_res = subprocess.run([windres, "-i", rc_file, "-o", res_obj, "-O", "coff"], env=env)
                    if wr_res.returncode == 0 and os.path.exists(res_obj):
                        flags.append(res_obj)

            flags.extend(["-o", exe_file, "-lwinmm", "-lws2_32", "-lwininet", "-lole32", "-lcomdlg32", "-lshell32", "-lgdiplus"])
        else:
            flags.extend(["-o", exe_file])
        res = subprocess.run(flags, env=env)
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
    print("  viss transpile <in> [out]     Transpile Viss source to C++")
    print("  viss init [project_name]      Create starter project")
    print("  viss version                  Display version info")


if __name__ == "__main__":
    main()
