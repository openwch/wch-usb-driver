#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""根据 examples 生成 MounRiver Studio（MRS）工程。

用法：
    python tools/gen_mrs_project.py ch32v30x USB/New
    python tools/gen_mrs_project.py ch32v30x USB/New --dry-run
    python tools/gen_mrs_project.py                # 交互式输入

生成的工程结构（以 device/hid_km 为例）：
    <输出路径>/device/hid_km/
    ├── .cproject / .project / .template / <示例名>.wvproj
    ├── board/                      # chips/<系列>/board
    ├── src/                        # examples/<分组>/<示例>/src
    └── wch-usb-driver/
        ├── include/
        ├── port/
        └── src/

复制 MRS 模板时，会把 mrs_project.wvproj 重命名为 <示例名>.wvproj，
并把 .cproject、.project、.template、.wvproj 四个文件中的 mrs_project
全部替换为目标示例程序名称（可用 --name-format 覆盖，默认即示例目录名）。
模板通过 PARENT-4-PROJECT_LOC/SRC 链接芯片 SDK，因此输出路径应位于
MRS 工作区的对应层级，例如 <工作区>/USB/New。
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
EXAMPLE_ROOT = "examples"
CHIP_ROOT = "chips"
DRIVER_DIRS = ("include", "port", "src")
MRS_TEMPLATE_NAME = "mrs"
MRS_REQUIRED_FILES = (".cproject", ".project")
MRS_TEXT_TEMPLATE_FILES = (".cproject", ".project", ".template")
MRS_WVPROJ_SUFFIX = ".wvproj"
MRS_PROJECT_NAME = "mrs_project"
PROJECT_NAME_RE = re.compile(r"^[A-Za-z0-9_.-]+$")


def configure_output() -> None:
    """统一输出为 UTF-8，避免重定向或管道下中文乱码。"""
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8")
        except (AttributeError, ValueError, OSError):
            pass


def display_path(path: Path, base: Path) -> str:
    try:
        return path.relative_to(base).as_posix()
    except ValueError:
        return path.as_posix()


def discover_series(root: Path) -> list[str]:
    """返回 chips/ 下带 board/ 目录的芯片系列，按名称排序。"""
    chip_root = root / CHIP_ROOT
    if not chip_root.is_dir():
        return []
    return sorted(
        path.name
        for path in chip_root.iterdir()
        if path.is_dir() and (path / "board").is_dir()
    )


def validate_series(root: Path, series: str) -> tuple[Path, Path]:
    """校验芯片系列及其 MRS 模板，返回 (系列目录, 模板目录)。"""
    series_dir = root / CHIP_ROOT / series
    board_dir = series_dir / "board"
    mrs_dir = series_dir / MRS_TEMPLATE_NAME

    if not board_dir.is_dir():
        available = "、".join(discover_series(root)) or "无"
        raise ValueError(f"芯片系列 '{series}' 不存在（可用系列：{available}）")

    if not mrs_dir.is_dir():
        raise ValueError(
            f"芯片系列 '{series}' 缺少 MRS 工程模板目录：{mrs_dir}"
        )

    missing = [name for name in MRS_REQUIRED_FILES if not (mrs_dir / name).is_file()]
    if missing:
        raise ValueError(
            f"芯片系列 '{series}' 的 MRS 模板缺少文件：{'、'.join(missing)}"
        )

    return series_dir, mrs_dir


def discover_examples(root: Path) -> list[tuple[str, str, Path]]:
    """返回 [(分组, 示例名, 示例目录)]，只包含带 src/ 的示例。"""
    example_root = root / EXAMPLE_ROOT
    if not example_root.is_dir():
        return []

    examples: list[tuple[str, str, Path]] = []
    for group_dir in sorted(example_root.iterdir()):
        if not group_dir.is_dir():
            continue
        for example_dir in sorted(group_dir.iterdir()):
            if example_dir.is_dir() and (example_dir / "src").is_dir():
                examples.append((group_dir.name, example_dir.name, example_dir))
    return examples


def series_chips(series_dir: Path) -> set[str]:
    """返回芯片系列目录下所有芯片型号（由 <chip>.mk 文件名得到）。"""
    return {
        path.stem.lower()
        for path in series_dir.glob("*.mk")
        if path.name != "family.mk"
    }


def parse_supported_chips(makefile: Path) -> set[str] | None:
    """解析 makefile 中的 SUPPORTED_CHIPS 列表。"""
    try:
        text = makefile.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None

    chips: set[str] = set()
    in_list = False
    for raw_line in text.splitlines():
        line = raw_line.split("#", 1)[0].rstrip()
        if not in_list:
            match = re.match(r"\s*SUPPORTED_CHIPS\s*:?=(.*)$", line)
            if match is None:
                continue
            in_list = True
            line = match.group(1)

        continued = line.endswith("\\")
        content = line[:-1] if continued else line
        chips.update(token.lower() for token in content.split())
        if not continued:
            break

    return chips if chips else None


def example_supports_series(
    example_dir: Path, chips: set[str]
) -> bool | None:
    """判断示例是否支持该系列；无法判断时返回 None。"""
    makefile = example_dir / "makefile"
    if not makefile.is_file():
        makefile = example_dir / "Makefile"
    if not makefile.is_file():
        return None

    supported = parse_supported_chips(makefile)
    if supported is None:
        return None
    return bool(supported & chips)


def format_project_name(name_format: str, group: str, name: str, series: str) -> str:
    try:
        project_name = name_format.format(group=group, name=name, series=series)
    except KeyError as exc:
        raise ValueError(f"--name-format 中存在未知占位符：{exc}") from exc
    except (AttributeError, IndexError, ValueError) as exc:
        raise ValueError(f"--name-format 格式无效：{exc}") from exc

    if not PROJECT_NAME_RE.fullmatch(project_name):
        raise ValueError(
            "工程名只能包含字母、数字、下划线、点和连字符："
            f"{project_name!r}（来自 --name-format {name_format!r}）"
        )
    return project_name


def copy_tree(source: Path, destination: Path) -> None:
    shutil.copytree(
        source, destination, copy_function=shutil.copy2, dirs_exist_ok=True
    )


def mrs_template_destination_name(filename: str, project_name: str) -> str:
    """返回模板文件复制到工程后的名称。"""
    if MRS_PROJECT_NAME in filename:
        return filename.replace(MRS_PROJECT_NAME, project_name)
    if filename == f"project{MRS_WVPROJ_SUFFIX}":
        return f"{project_name}{MRS_WVPROJ_SUFFIX}"
    return filename


def copy_mrs_template(
    source: Path, destination: Path, project_name: str
) -> tuple[list[str], list[str]]:
    """复制 MRS 模板并把 mrs_project 替换为实际工程名。

    - 文件名中的 mrs_project 替换为工程名，如 mrs_project.wvproj ->
      <工程名>.wvproj；旧的 project.wvproj 也按同样规则重命名。
    - .cproject、.project、.template 及 .wvproj 中的 mrs_project 字符串
      全部替换为工程名。

    返回 (内容发生替换的文件名列表, 重命名列表)。重命名列表的元素形如
    "mrs_project.wvproj -> hid_km.wvproj"。
    """
    renamed: list[str] = []
    for entry in source.iterdir():
        dest_name = mrs_template_destination_name(entry.name, project_name)
        target = destination / dest_name
        if target.exists():
            raise OSError(
                f"MRS 模板文件重名：{entry.name} -> {dest_name}"
            )

        if entry.is_dir():
            copy_tree(entry, target)
        else:
            shutil.copy2(entry, target)

        if dest_name != entry.name:
            renamed.append(f"{entry.name} -> {dest_name}")

    template_files = [
        destination / name for name in MRS_TEXT_TEMPLATE_FILES
    ]
    template_files.extend(sorted(destination.rglob(f"*{MRS_WVPROJ_SUFFIX}")))

    old_name = MRS_PROJECT_NAME.encode("utf-8")
    new_name = project_name.encode("utf-8")
    replaced: list[str] = []

    for path in template_files:
        if not path.is_file():
            continue

        data = path.read_bytes()
        if old_name not in data:
            continue

        path.write_bytes(data.replace(old_name, new_name))
        replaced.append(path.name)

    remaining_files = [
        path.name
        for path in template_files
        if path.is_file() and old_name in path.read_bytes()
    ]
    remaining_names = [
        path.name
        for path in destination.rglob("*")
        if MRS_PROJECT_NAME in path.name
    ]
    if remaining_files or remaining_names:
        remaining = remaining_files + remaining_names
        raise OSError(
            f"MRS 模板中仍存在 {MRS_PROJECT_NAME}：{'、'.join(remaining)}"
        )

    return replaced, renamed


def remove_path(path: Path) -> None:
    if path.is_symlink() or path.is_file():
        path.unlink()
    elif path.is_dir():
        shutil.rmtree(path)


def generate_project(
    root: Path,
    example_dir: Path,
    target: Path,
    series_dir: Path,
    mrs_dir: Path,
    project_name: str,
) -> tuple[list[str], list[str]]:
    """在临时目录中组装工程，完成后原子替换到目标路径。

    返回 (模板内容替换的文件名列表, 模板文件重命名列表)。
    """
    target.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=f".{target.name}.", dir=target.parent))

    try:
        copy_tree(example_dir / "src", staging / "src")
        copy_tree(series_dir / "board", staging / "board")

        driver_dir = staging / "wch-usb-driver"
        driver_dir.mkdir()
        for name in DRIVER_DIRS:
            copy_tree(root / name, driver_dir / name)

        replaced, renamed = copy_mrs_template(mrs_dir, staging, project_name)

        if target.exists() or target.is_symlink():
            remove_path(target)
        os.replace(staging, target)
    except BaseException:
        shutil.rmtree(staging, ignore_errors=True)
        raise

    return replaced, renamed


def prompt_series(root: Path) -> str:
    series_list = discover_series(root)
    if not series_list:
        raise ValueError(f"未找到任何芯片系列目录：{root / CHIP_ROOT}")

    print("可用芯片系列：")
    for index, series in enumerate(series_list, 1):
        mrs_dir = root / CHIP_ROOT / series / MRS_TEMPLATE_NAME
        suffix = "" if (mrs_dir / ".cproject").is_file() else "（无 MRS 模板）"
        print(f"  {index}. {series}{suffix}")

    while True:
        raw = input("请输入芯片系列：").strip().lower()
        if raw in series_list:
            return raw
        print(f"输入无效：{raw!r}，请输入列表中的系列名称。")


def prompt_output() -> Path:
    while True:
        raw = input("请输入工程输出路径（例如 USB/New）：").strip().strip('"')
        if raw:
            return Path(raw)
        print("输出路径不能为空。")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="根据 examples 生成指定芯片系列的 MRS 工程。",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "生成结构（以 device/hid_km 为例）：\n"
            "  <输出路径>/device/hid_km/\n"
            "  ├── .cproject / .project / .template / <示例名>.wvproj\n"
            "  ├── board/                      # chips/<系列>/board\n"
            "  ├── src/                        # examples/<分组>/<示例>/src\n"
            "  └── wch-usb-driver/\n"
            "      ├── include/\n"
            "      ├── port/\n"
            "      └── src/\n\n"
            "示例：\n"
            "  python tools/gen_mrs_project.py ch32v30x USB/New\n"
            "  python tools/gen_mrs_project.py ch32v30x USB/New --dry-run\n"
            "  python tools/gen_mrs_project.py ch32v30x USB/New --force\n"
            "  python tools/gen_mrs_project.py\n\n"
            "makefile 的 SUPPORTED_CHIPS 不包含该系列任何芯片的示例会被跳过。\n"
        ),
    )
    parser.add_argument(
        "series", nargs="?", metavar="SERIES",
        help="芯片系列，例如 ch32v30x（省略时交互输入）",
    )
    parser.add_argument(
        "output", nargs="?", type=Path, metavar="OUTPUT",
        help="工程输出路径，例如 USB/New（省略时交互输入）",
    )
    parser.add_argument(
        "--root", type=Path, default=ROOT_DIR,
        help=f"项目根目录（默认：{ROOT_DIR}）",
    )
    parser.add_argument(
        "-f", "--force", action="store_true",
        help="目标工程已存在时删除后重新生成",
    )
    parser.add_argument(
        "--dry-run", action="store_true",
        help="只显示将生成的工程，不写入文件",
    )
    parser.add_argument(
        "--name-format", default="{name}", metavar="FORMAT",
        help="MRS 工程名格式，可用 {group}、{name}、{series}（默认：{name}）",
    )
    parser.add_argument(
        "-q", "--quiet", action="store_true",
        help="只输出汇总信息和警告",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    configure_output()
    parser = build_parser()
    args = parser.parse_args(argv)

    root = args.root.resolve()
    if not root.is_dir():
        parser.error(f"项目根目录不存在：{root}")

    for name in DRIVER_DIRS:
        if not (root / name).is_dir():
            parser.error(f"项目缺少目录：{root / name}")

    interactive = args.series is None or args.output is None
    if interactive and not sys.stdin.isatty():
        parser.error("缺少 SERIES 或 OUTPUT 参数")

    try:
        if args.series is None:
            series = prompt_series(root)
        else:
            series = args.series.strip().lower()

        if args.output is None:
            output = prompt_output()
        else:
            output = args.output
    except (EOFError, KeyboardInterrupt):
        print("\n已取消。", file=sys.stderr)
        return 130
    except ValueError as exc:
        parser.error(str(exc))

    try:
        series_dir, mrs_dir = validate_series(root, series)
        examples = discover_examples(root)
    except ValueError as exc:
        parser.error(str(exc))

    if not examples:
        print(f"错误：未找到任何示例工程：{root / EXAMPLE_ROOT}", file=sys.stderr)
        return 1

    output = output.expanduser().resolve()
    if output.exists() and not output.is_dir():
        print(f"错误：输出路径不是目录：{output}", file=sys.stderr)
        return 1

    chips = series_chips(series_dir)

    # --- 组装生成计划 ---
    planned: list[tuple[str, str, Path, str]] = []
    unsupported: list[tuple[str, str, Path]] = []
    unknown_support: list[tuple[str, str, Path]] = []

    for group, name, example_dir in examples:
        try:
            project_name = format_project_name(args.name_format, group, name, series)
        except ValueError as exc:
            parser.error(str(exc))

        target = output / group / name
        support = example_supports_series(example_dir, chips)
        if support is False:
            unsupported.append((group, name, target))
            continue
        if support is None:
            unknown_support.append((group, name, target))

        planned.append((group, name, target, project_name))

    # --- 系列支持提示 ---
    for group, name, _ in unsupported:
        print(
            f"[跳过] {group}/{name}：makefile 的 SUPPORTED_CHIPS 不包含 "
            f"{series} 系列的任何芯片",
            file=sys.stderr,
        )
    for group, name, _ in unknown_support:
        print(
            f"[提示] {group}/{name}：无法从 makefile 判断是否支持 {series}，"
            "仍将生成",
            file=sys.stderr,
        )

    if not planned:
        print(
            f"错误：没有支持 {series} 系列的示例工程，未生成任何工程",
            file=sys.stderr,
        )
        return 1

    # --- 工程名重复提示 ---
    names: dict[str, list[str]] = {}
    for group, name, _, project_name in planned:
        names.setdefault(project_name, []).append(f"{group}/{name}")
    for project_name, owners in sorted(names.items()):
        if len(owners) > 1:
            print(
                f"[警告] 工程名 '{project_name}' 重复：{'、'.join(owners)}；"
                "同一 MRS 工作区不能同时导入同名工程，"
                "可使用 --name-format \"{group}_{name}\"",
                file=sys.stderr,
            )

    # --- 目标冲突检查 ---
    conflicts = [
        target
        for _, _, target, _ in planned
        if target.exists() and (not target.is_dir() or any(target.iterdir()))
    ]
    if conflicts and not args.force:
        print("错误：以下工程目录已存在且非空（使用 --force 覆盖）：",
              file=sys.stderr)
        for target in conflicts:
            print(f"  {display_path(target, Path.cwd().resolve())}", file=sys.stderr)
        return 1

    # --- MRS 模板 SDK 链接提示 ---
    sdk_root = output.parent.parent / "SRC"
    if not args.quiet and not sdk_root.is_dir():
        print(
            f"[提示] 未找到 MRS 模板链接的 SDK 目录：{sdk_root}\n"
            "       .project 会把 Core/Debug/Ld/Peripheral/Startup 链接到 "
            "PARENT-4-PROJECT_LOC/SRC，工程可能无法直接编译",
            file=sys.stderr,
        )

    # --- 生成 ---
    generated = 0
    errors = 0
    base = Path.cwd().resolve()

    for group, name, target, project_name in planned:
        relative = display_path(target, base)
        if args.dry_run:
            generated += 1
            if not args.quiet:
                print(f"[预览] {relative}（工程名：{project_name}）")
            continue

        try:
            replaced, renamed = generate_project(
                root, root / EXAMPLE_ROOT / group / name,
                target, series_dir, mrs_dir, project_name
            )
        except OSError as exc:
            print(f"[错误] {relative}：{exc}", file=sys.stderr)
            errors += 1
            continue

        generated += 1
        if not args.quiet:
            detail = f"工程名：{project_name}"
            if replaced:
                detail += f"，模板替换：{'、'.join(replaced)}"
            if renamed:
                detail += f"，模板重命名：{'、'.join(renamed)}"
            print(f"[生成] {relative}（{detail}）")

    prefix = "预览" if args.dry_run else "完成"
    action = "将生成" if args.dry_run else "生成"
    print(
        f"{prefix}：{action} {generated} 个工程"
        f"（输出：{display_path(output, base)}，芯片系列：{series}）"
    )
    if unsupported:
        print(f"跳过不支持该系列的示例 {len(unsupported)} 个")
    if errors:
        print(f"错误：{errors} 个工程生成失败", file=sys.stderr)

    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
