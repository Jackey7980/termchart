#!/usr/bin/env python3
"""生成 termchart_capi 的 Python ctypes 绑定（基于 libclang 解析 C 头 AST）。

流程：用 libclang 解析 libs/capi/include/termchart_capi.h 的语法树，把
  * 枚举常量  → Python int
  * 结构体    → ctypes.Structure（POD 字段）
  * 函数声明  → ctypes 原型（argtypes / restype）
映射成纯 Python 的 ctypes 模块（bindings/python/termchart_capi/_capi.py），
运行时由该模块加载编译出的 termchart_capi 共享库。

为什么走 C 头而不是 C++ 头：C 的 AST 小且稳定（无模板/重载/异常），代码生成
可控；这正是先有 C ABI 适配层、再生成绑定的意义（见 docs/design/ffi_plan.md §1）。

用法：
    python tools/gen_python_capi.py [--header H] [--out O] [--include DIR]
生成物是产物，禁止手改；需改时改头 + 重跑本脚本。
"""
from __future__ import annotations

import argparse
import glob
import os
import sys

try:
    from clang import cindex
except ImportError:  # pragma: no cover
    sys.stderr.write("需要 libclang：pip install libclang\n")
    raise


def _configure_libclang() -> None:
    """让 cindex 找到 libclang 原生库（pip 包的 libclang 自带，无需系统 LLVM）。"""
    try:
        cindex.Config.set_compatibility_check(False)
    except Exception:
        pass
    candidates: list[str] = []
    try:
        import clang

        base = os.path.dirname(clang.__file__)
        candidates += [
            os.path.join(base, "libclang.dll"),
            os.path.join(base, "libclang.so"),
            os.path.join(base, "libclang.dylib"),
            os.path.join(base, "native", "libclang.dll"),
        ]
    except Exception:
        pass
    for c in candidates:
        if os.path.exists(c):
            try:
                cindex.Config.set_library_file(c)
                return
            except Exception:
                continue


def _find_stdint_include() -> str | None:
    """定位一个含 stdint.h 的系统 include 目录（libclang 资源头可能不含它）。"""
    roots = [
        r"C:\msys64\ucrt64\include",
        r"C:\msys64\mingw64\include",
        r"C:\Program Files\mingw64\include",
        "/usr/include",
        "/usr/local/include",
    ]
    for r in roots:
        if os.path.exists(os.path.join(r, "stdint.h")):
            return r
    return None


# ── 类型映射：clang TypeKind → ctypes 表达式字符串 ───────────────────────────
_SIZED = {
    "uint8_t": "ctypes.c_uint8",
    "int8_t": "ctypes.c_int8",
    "uint16_t": "ctypes.c_uint16",
    "int16_t": "ctypes.c_int16",
    "uint32_t": "ctypes.c_uint32",
    "int32_t": "ctypes.c_int32",
    "uint64_t": "ctypes.c_uint64",
    "int64_t": "ctypes.c_int64",
    "size_t": "ctypes.c_size_t",
    "intptr_t": "ctypes.c_int64",
    "uintptr_t": "ctypes.c_uint64",
}

_RECORD_KINDS = (cindex.TypeKind.RECORD, cindex.TypeKind.ELABORATED)


def _struct_py_name(cname: str) -> str:
    if cname.startswith("tc_"):
        return "".join(p.capitalize() for p in cname[3:].split("_"))
    return cname


def _map_type(t) -> str:
    # 先用类型自身的拼写查「定宽整数」表（uint32_t 等 typedef 名），避免被
    # get_canonical 消解成 unsigned int 后丢失精确位宽映射。
    sname = (t.spelling or "").replace("const ", "").strip()
    if sname in _SIZED:
        return _SIZED[sname]
    kind = t.kind
    if kind == cindex.TypeKind.ELABORATED:
        # enum tc_x / struct tc_x 的"elaborated"包装，消解到底层类型再映射。
        can = t.get_canonical()
        if can.kind == kind:  # 自卫：未消解则放弃
            return "ctypes.c_void_p"
        return _map_type(can)
    if kind == cindex.TypeKind.TYPEDEF:
        underlying = t.get_canonical()
        uk = underlying.kind
        if uk == cindex.TypeKind.POINTER:
            pointee = underlying.get_pointee()
            if pointee.kind in _RECORD_KINDS:
                return "ctypes.c_void_p"  # 不透明句柄 typedef
        if uk == cindex.TypeKind.ENUM:
            return "ctypes.c_int"
        return _map_type(underlying)
    if kind == cindex.TypeKind.POINTER:
        pointee = t.get_pointee()
        pk = pointee.kind
        if pk == cindex.TypeKind.CHAR_S:
            pcanon = pointee.get_canonical()
            if pcanon.kind == cindex.TypeKind.POINTER:
                return "ctypes.POINTER(ctypes.c_char_p)"
            return "ctypes.c_char_p"
        if pk in _RECORD_KINDS:
            return "ctypes.c_void_p"
        return "ctypes.POINTER(%s)" % _map_type(pointee)
    if kind == cindex.TypeKind.ENUM:
        return "ctypes.c_int"
    if kind == cindex.TypeKind.RECORD:
        return _struct_py_name(t.get_declaration().spelling)
    if kind in (cindex.TypeKind.VOID,):
        return "None"
    sname = (t.spelling or "").replace("const ", "").strip()
    if sname in _SIZED:
        return _SIZED[sname]
    _SCALAR = {
        cindex.TypeKind.BOOL: "ctypes.c_uint8",
        cindex.TypeKind.INT: "ctypes.c_int",
        cindex.TypeKind.UINT: "ctypes.c_uint",
        cindex.TypeKind.SCHAR: "ctypes.c_int8",
        cindex.TypeKind.UCHAR: "ctypes.c_uint8",
        cindex.TypeKind.SHORT: "ctypes.c_int16",
        cindex.TypeKind.USHORT: "ctypes.c_uint16",
        cindex.TypeKind.LONG: "ctypes.c_long",
        cindex.TypeKind.ULONG: "ctypes.c_ulong",
        cindex.TypeKind.LONGLONG: "ctypes.c_longlong",
        cindex.TypeKind.ULONGLONG: "ctypes.c_ulonglong",
        cindex.TypeKind.FLOAT: "ctypes.c_float",
        cindex.TypeKind.DOUBLE: "ctypes.c_double",
        cindex.TypeKind.CHAR_S: "ctypes.c_char",
    }
    if kind in _SCALAR:
        return _SCALAR[kind]
    return "ctypes.c_void_p"  # 兜底


def _collect(header: str):
    _configure_libclang()
    index = cindex.Index.create()
    args = ["-std=c11"]
    inc = _find_stdint_include()
    if inc:
        args.append("-I" + inc)
    tu = index.parse(header, args=args)
    enums: dict[str, list[tuple[str, int]]] = {}
    structs: dict[str, list[tuple[str, str]]] = {}
    funcs: list[tuple[str, str, list[tuple[str, str]]]] = []
    opaque: set[str] = set()  # 不透明句柄指向的结构体名（不生成类）
    norm = os.path.normpath(header)

    def walk(node):
        for c in node.get_children():
            loc = c.location.file
            if loc is None or os.path.normpath(loc.name) != norm:
                continue  # 只收我们头里的声明，跳过 stdint 等
            if c.kind == cindex.CursorKind.ENUM_DECL:
                items = []
                for e in c.get_children():
                    if e.kind == cindex.CursorKind.ENUM_CONSTANT_DECL:
                        items.append((e.spelling, int(e.enum_value)))
                if items:
                    key = c.spelling or ("anon_enum_%d" % len(enums))
                    enums[key] = items
            elif c.kind == cindex.CursorKind.STRUCT_DECL and c.spelling:
                fields = []
                for f in c.get_children():
                    if f.kind == cindex.CursorKind.FIELD_DECL:
                        fields.append((f.spelling, _map_type(f.type)))
                structs[c.spelling] = fields
            elif c.kind == cindex.CursorKind.TYPEDEF_DECL:
                # 不透明句柄：typedef struct X* tc_x_handle; 记 X 为不透明
                ut = c.type.get_canonical()
                if ut.kind == cindex.TypeKind.POINTER:
                    pd = ut.get_pointee().get_declaration()
                    if pd is not None and pd.spelling:
                        opaque.add(pd.spelling)
            elif c.kind == cindex.CursorKind.FUNCTION_DECL and c.spelling.startswith("tc_"):
                params = [
                    (p.spelling or ("arg%d" % i), _map_type(p.type))
                    for i, p in enumerate(c.get_arguments())
                ]
                funcs.append((c.spelling, _map_type(c.result_type), params))

    walk(tu.cursor)

    for d in tu.diagnostics:
        if d.severity >= cindex.Diagnostic.Error:
            sys.stderr.write("clang: %s\n" % d.spelling)
    return enums, structs, funcs, opaque


def _emit(enums, structs, funcs, opaque, out_path: str) -> None:
    lines: list[str] = []
    lines.append('"""Generated by tools/gen_python_capi.py -- do not edit."""')
    lines.append("from __future__ import annotations")
    lines.append("import ctypes")
    lines.append("import glob")
    lines.append("import os")
    lines.append("")
    lines.append("# ── 枚举常量 ───────────────────────────────────────────────")
    for key, items in enums.items():
        for name, val in items:
            lines.append("%s = %d" % (name, val))
    lines.append("")
    lines.append("# ── 结构体（POD）──────────────────────────────────────────")
    for cname, fields in structs.items():
        if cname in opaque or not fields:
            continue  # 不透明句柄结构体 / 无字段前向声明，不生成类
        py = _struct_py_name(cname)
        lines.append("class %s(ctypes.Structure):" % py)
        if fields:
            body = ", ".join('("%s", %s)' % (fn, ft) for fn, ft in fields)
            lines.append("    _fields_ = [%s]" % body)
        else:
            lines.append("    _fields_ = []")
        lines.append("")
    lines.append("# ── 加载共享库 ─────────────────────────────────────────────")
    lines.append("_LIB = None")
    lines.append("")
    lines.append("")
    lines.append("def _load(path=None):")
    lines.append("    global _LIB")
    lines.append("    if _LIB is not None:")
    lines.append("        return _LIB")
    lines.append("    candidates = []")
    lines.append("    if path:")
    lines.append("        candidates.append(path)")
    lines.append("    env = os.environ.get('TERMCHART_CAPI_LIB')")
    lines.append("    if env:")
    lines.append("        candidates.append(env)")
    lines.append("    here = os.path.dirname(os.path.abspath(__file__))")
    lines.append("    repo = os.path.abspath(os.path.join(here, '..', '..', '..'))")
    lines.append("    names = ['termchart_capi.dll', 'libtermchart_capi.dll',")
    lines.append("             'libtermchart_capi.so', 'libtermchart_capi.dylib']")
    lines.append("    for d in [os.path.join(repo, 'build'), here]:")
    lines.append("        for n in names:")
    lines.append("            m = glob.glob(os.path.join(d, '**', n), recursive=True)")
    lines.append("            if m:")
    lines.append("                candidates.append(m[0])")
    lines.append("    if not candidates:")
    lines.append("        candidates.append(names[0])  # 系统 PATH 兜底")
    lines.append("    _LIB = ctypes.CDLL(candidates[0])")
    lines.append("    _bind(_LIB)")
    lines.append("    return _LIB")
    lines.append("")
    lines.append("")
    lines.append("def _bind(lib):")
    lines.append("    global %s" % ", ".join(f[0] for f in funcs))
    for fname, rtype, params in funcs:
        lines.append("    f = lib.%s" % fname)
        lines.append("    f.restype = %s" % rtype)
        if params:
            argtypes = ", ".join(pt for _, pt in params)
            lines.append("    f.argtypes = [%s]" % argtypes)
        else:
            lines.append("    f.argtypes = []")
        lines.append("    %s = f" % fname)
        lines.append("")
    lines.append("# 函数对象占位（import 时由 _load 填充）")
    for fname, _, _ in funcs:
        lines.append("%s = None" % fname)
    lines.append("")
    with open(out_path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    default_header = os.path.join(here, "..", "libs", "capi", "include", "termchart_capi.h")
    default_out = os.path.join(
        here, "..", "bindings", "python", "termchart_capi", "_capi.py"
    )
    ap = argparse.ArgumentParser(description="Generate ctypes binding for termchart_capi.h")
    ap.add_argument("--header", default=default_header)
    ap.add_argument("--out", default=default_out)
    ap.add_argument("--include", help="extra include dir (e.g. MinGW include for stdint.h)")
    args = ap.parse_args()

    enums, structs, funcs, opaque = _collect(args.header)
    if not funcs:
        sys.stderr.write("warning: 未在头中解析到任何 tc_* 函数，请检查头路径与 clang 诊断\n")
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    _emit(enums, structs, funcs, opaque, args.out)
    sys.stderr.write(
        "generated %s: %d enum-group(s), %d struct(s), %d function(s)\n"
        % (args.out, len(enums), len(structs), len(funcs))
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
