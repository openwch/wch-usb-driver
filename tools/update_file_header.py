#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""批量更新项目源文件头中的 Version、Date 和 Copyright (c) 年份。

扫描范围：
    src/、include/、port/、examples/ 以及 chips/*/board/（递归）
    只处理 .c 和 .h 文件。

排除文件（每个芯片 board 目录下的厂商文件）：
    <chip>_conf.h、<chip>_it.c、<chip>_it.h、
    system_<chip>.c、system_<chip>.h

用法：
    python tools/update_file_header.py -v V1.0.1 -d 2026/10/20
    python tools/update_file_header.py -v V1.0.1 -d 2026-10-20 --dry-run
    python tools/update_file_header.py            # 交互式输入版本和日期

Copyright (c) 的年份取自日期参数，行内其余版权信息保持不变。工具只修改
文件头中的这三行，其余内容（包括换行符、BOM 和非 ASCII 字节）原样保留。
"""

from __future__ import annotations

import argparse
import codecs
import datetime as dt
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
SCOPE_DIRS = ("src", "include", "port", "examples")
CHIP_ROOTS = ("chips", "chip")
SOURCE_SUFFIXES = (".c", ".h")

# 对应 ch32v4x7_conf.h / ch32v4x7_it.c / ch32v4x7_it.h /
# system_ch32v4x7.c / system_ch32v4x7.h 这一组厂商文件（适用于所有芯片）。
VENDOR_BOARD_FILE_RE = re.compile(
    r"(?:^.+_conf\.h$|^.+_it\.(?:c|h)$|^system_.+\.(?:c|h)$)", re.IGNORECASE
)

HEADER_START_RE = re.compile(r"^/\*+\s*\(C\)\s+COPYRIGHT", re.IGNORECASE)
VERSION_LINE_RE = re.compile(
    r"^(?P<prefix>\*\s*Version\s*:\s*)(?P<value>\S*)(?P<rest>[^\r\n]*)(?P<eol>\r?\n?)$",
    re.IGNORECASE,
)
DATE_LINE_RE = re.compile(
    r"^(?P<prefix>\*\s*Date\s*:\s*)(?P<value>\S*)(?P<rest>[^\r\n]*)(?P<eol>\r?\n?)$",
    re.IGNORECASE,
)
COPYRIGHT_LINE_RE = re.compile(
    r"^(?P<prefix>\*\s*Copyright\s*\(c\)\s*)(?P<year>\d{4})(?P<rest>[^\r\n]*)(?P<eol>\r?\n?)$",
    re.IGNORECASE,
)

HEADER_END = "***/"
HEADER_START_SCAN_LINES = 5
MAX_HEADER_SCAN_LINES = 50


def relative_path(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return path.as_posix()


def collect_source_files(root: Path) -> tuple[list[Path], int]:
    """收集待处理文件，返回 (文件列表, 被排除的厂商文件数)。"""
    files: list[Path] = []

    for name in SCOPE_DIRS:
        base = root / name
        if not base.is_dir():
            continue
        files.extend(
            path
            for path in base.rglob("*")
            if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES
        )

    excluded = 0
    for chip_root_name in CHIP_ROOTS:
        chip_root = root / chip_root_name
        if not chip_root.is_dir():
            continue
        for board_dir in sorted(chip_root.glob("*/board")):
            if not board_dir.is_dir():
                continue
            for path in board_dir.rglob("*"):
                if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
                    continue
                if VENDOR_BOARD_FILE_RE.match(path.name):
                    excluded += 1
                    continue
                files.append(path)

    # 去重（chips/ 与 chip/ 同时存在时可能出现同一文件）并稳定排序
    unique = {path.resolve(): path for path in files}
    return sorted(unique.values(), key=lambda path: relative_path(path, root).lower()), excluded


def read_source(path: Path) -> tuple[bytes, str]:
    """按字节读取文件，保留 BOM；正文用 latin-1 做一一映射解码。"""
    data = path.read_bytes()
    bom = codecs.BOM_UTF8 if data.startswith(codecs.BOM_UTF8) else b""
    body = data[len(bom):]
    return bom, body.decode("latin-1")


def write_source(path: Path, bom: bytes, text: str) -> None:
    """原子写入文件，并保留原文件权限。"""
    data = bom + text.encode("latin-1")
    fd, tmp_name = tempfile.mkstemp(
        prefix=f".{path.name}.", suffix=".tmp", dir=str(path.parent)
    )
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
        shutil.copymode(path, tmp_name)
        os.replace(tmp_name, path)
    except BaseException:
        try:
            os.unlink(tmp_name)
        except OSError:
            pass
        raise


def update_header(
    text: str, version: str, date_text: str, year: int
) -> tuple[str | None, list[tuple[str, str, str]] | str]:
    """更新文件头三行。

    返回 (新文本, 修改列表) 或 (None, 跳过原因)。修改列表元素为
    (字段名, 旧行, 新行)，无变化时为空列表。
    """
    lines = text.splitlines(keepends=True)

    start = None
    for index in range(min(HEADER_START_SCAN_LINES, len(lines))):
        if HEADER_START_RE.match(lines[index]):
            start = index
            break
    if start is None:
        return None, "未找到 WCH 文件头"

    end = None
    for index in range(start, min(start + MAX_HEADER_SCAN_LINES, len(lines))):
        if HEADER_END in lines[index]:
            end = index
            break
    if end is None:
        return None, "文件头缺少结束行"

    targets = (
        ("Version", VERSION_LINE_RE, version),
        ("Date", DATE_LINE_RE, date_text),
        ("Copyright", COPYRIGHT_LINE_RE, str(year)),
    )

    indices: dict[str, int] = {}
    for name, pattern, _ in targets:
        for index in range(start, end + 1):
            if pattern.match(lines[index]):
                indices[name] = index
                break

    missing = [name for name, _, _ in targets if name not in indices]
    if missing:
        return None, "文件头缺少字段：" + "、".join(missing)

    changes: list[tuple[str, str, str]] = []
    for name, pattern, value in targets:
        index = indices[name]
        old_line = lines[index]
        match = pattern.match(old_line)
        assert match is not None
        new_line = match.group("prefix") + value + match.group("rest") + match.group("eol")
        if new_line != old_line:
            changes.append((name, old_line.rstrip("\r\n"), new_line.rstrip("\r\n")))
            lines[index] = new_line

    # 安全检查：除目标字段所在行外，任何一行都不允许变化
    original_lines = text.splitlines(keepends=True)
    changed_indices = set(indices.values())
    for index, (old_line, new_line) in enumerate(zip(original_lines, lines)):
        if index not in changed_indices and old_line != new_line:
            return None, "内部错误：文件头以外的内容发生变化"

    return "".join(lines), changes


def normalize_version(raw: str) -> str:
    match = re.fullmatch(r"[vV]?(\d+(?:\.\d+){0,3})", raw.strip())
    if match is None:
        raise ValueError("版本号格式无效，示例：V1.0.0")
    return "V" + match.group(1)


def parse_date(raw: str) -> dt.date:
    value = raw.strip()
    for fmt in ("%Y/%m/%d", "%Y-%m-%d", "%Y.%m.%d", "%Y%m%d"):
        try:
            return dt.datetime.strptime(value, fmt).date()
        except ValueError:
            continue
    raise ValueError("日期格式无效，示例：2026/10/20")


def format_date(value: dt.date) -> str:
    return f"{value.year:04d}/{value.month:02d}/{value.day:02d}"


def prompt_version() -> str:
    while True:
        raw = input("请输入版本号（如 V1.0.0）：")
        try:
            return normalize_version(raw)
        except ValueError as exc:
            print(f"输入无效：{exc}")


def prompt_date() -> dt.date:
    today = dt.date.today()
    while True:
        raw = input(f"请输入日期（YYYY/MM/DD，直接回车使用 {format_date(today)}）：").strip()
        if not raw:
            return today
        try:
            return parse_date(raw)
        except ValueError as exc:
            print(f"输入无效：{exc}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="批量更新项目源文件头中的 Version、Date 和 Copyright (c) 年份。",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "扫描范围：src/ include/ port/ examples/ 以及 chips/*/board/ 下的 .c/.h 文件。\n"
            "排除文件：<chip>_conf.h、<chip>_it.c、<chip>_it.h、"
            "system_<chip>.c、system_<chip>.h。\n\n"
            "示例：\n"
            "  python tools/update_file_header.py -v V1.0.1 -d 2026/10/20\n"
            "  python tools/update_file_header.py -v V1.0.1 -d 2026-10-20 --dry-run\n"
            "  python tools/update_file_header.py\n"
        ),
    )
    parser.add_argument(
        "-v", "--version", dest="version", metavar="VERSION",
        help="版本号，例如 V1.0.1（省略时交互输入）",
    )
    parser.add_argument(
        "-d", "--date", dest="date", metavar="DATE",
        help="日期，例如 2026/10/20 或 2026-10-20（省略时交互输入）",
    )
    parser.add_argument(
        "--root", type=Path, default=ROOT_DIR,
        help=f"项目根目录（默认：{ROOT_DIR}）",
    )
    parser.add_argument(
        "--dry-run", action="store_true",
        help="只显示将修改的内容，不写入文件",
    )
    parser.add_argument(
        "-q", "--quiet", action="store_true",
        help="只输出汇总信息",
    )
    parser.add_argument(
        "--strict", action="store_true",
        help="存在无法处理的文件时返回非零退出码",
    )
    return parser


def configure_output() -> None:
    """统一输出为 UTF-8，避免重定向或管道下中文乱码。"""
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8")
        except (AttributeError, ValueError, OSError):
            pass


def main(argv: list[str] | None = None) -> int:
    configure_output()
    parser = build_parser()
    args = parser.parse_args(argv)

    root = args.root.resolve()
    if not root.is_dir():
        parser.error(f"项目根目录不存在：{root}")

    if args.version is not None:
        try:
            version = normalize_version(args.version)
        except ValueError as exc:
            parser.error(str(exc))
    else:
        if not sys.stdin.isatty():
            parser.error("缺少 --version 参数")
        try:
            version = prompt_version()
        except (EOFError, KeyboardInterrupt):
            print("\n已取消。", file=sys.stderr)
            return 130

    if args.date is not None:
        try:
            date_value = parse_date(args.date)
        except ValueError as exc:
            parser.error(str(exc))
    else:
        if not sys.stdin.isatty():
            parser.error("缺少 --date 参数")
        try:
            date_value = prompt_date()
        except (EOFError, KeyboardInterrupt):
            print("\n已取消。", file=sys.stderr)
            return 130

    date_text = format_date(date_value)
    year = date_value.year

    files, excluded = collect_source_files(root)

    updated = 0
    unchanged = 0
    skipped = 0
    errors = 0

    for path in files:
        display = relative_path(path, root)
        try:
            bom, text = read_source(path)
            new_text, result = update_header(text, version, date_text, year)
        except OSError as exc:
            print(f"[错误] {display}：{exc}", file=sys.stderr)
            errors += 1
            continue

        if new_text is None:
            skipped += 1
            if not args.quiet:
                print(f"[跳过] {display}：{result}")
            continue

        if not result:
            unchanged += 1
            continue

        if args.dry_run:
            updated += 1
            if not args.quiet:
                print(f"[预览] {display}")
                for _, old_line, new_line in result:
                    print(f"    - {old_line}")
                    print(f"    + {new_line}")
            continue

        try:
            write_source(path, bom, new_text)
        except OSError as exc:
            print(f"[错误] {display}：{exc}", file=sys.stderr)
            errors += 1
            continue

        updated += 1
        if not args.quiet:
            print(f"[更新] {display}")

    action = "将更新" if args.dry_run else "更新"
    prefix = "预览" if args.dry_run else "完成"
    print(
        f"{prefix}：扫描 {len(files)} 个文件，{action} {updated} 个，"
        f"未变化 {unchanged} 个，跳过 {skipped} 个"
        f"（另排除厂商文件 {excluded} 个）"
    )
    if errors:
        print(f"错误：{errors} 个文件处理失败", file=sys.stderr)

    if errors or (args.strict and skipped):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
