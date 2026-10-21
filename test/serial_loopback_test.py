#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""串口回环测试工具。

测试步骤：
    1. 按指定大小生成随机数据文件；
    2. 通过串口发送数据，同时接收设备回环的数据并写入接收文件；
    3. 计算收发文件的 SHA256 并比对，输出测试结果。

用法：
    python test/serial_loopback_test.py -p COM3 -b 115200 -s 1
    python test/serial_loopback_test.py -p /dev/ttyACM0 -b 921600 -s 4
    python test/serial_loopback_test.py --list

依赖：
    pip install pyserial
"""

from __future__ import annotations

import argparse
import hashlib
import os
import sys
import threading
import time
from pathlib import Path

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("错误：缺少 pyserial，请先执行 pip install pyserial", file=sys.stderr)
    raise SystemExit(2)

CHUNK_SIZE = 4096
MB = 1024 * 1024
PROGRESS_INTERVAL = 1.0
DEFAULT_BAUD = 115200
DEFAULT_SIZE_MB = 1
DEFAULT_TIMEOUT = 3.0
TEST_DIR = Path(__file__).resolve().parent
DEFAULT_DATA_FILE = TEST_DIR / "test_data.bin"
DEFAULT_RECV_FILE = TEST_DIR / "recv_data.bin"


def generate_random_file(path: Path, size: int) -> None:
    """生成指定字节数的随机数据文件。"""
    path.parent.mkdir(parents=True, exist_ok=True)
    remaining = size
    with open(path, "wb") as fp:
        while remaining:
            chunk_size = min(CHUNK_SIZE, remaining)
            fp.write(os.urandom(chunk_size))
            remaining -= chunk_size


def compare_files(path_a: Path, path_b: Path) -> tuple[bool, str]:
    """比较两个文件的 SHA256，返回 (是否一致, 结果说明)。"""
    hash_a = hashlib.sha256(path_a.read_bytes()).hexdigest()
    hash_b = hashlib.sha256(path_b.read_bytes()).hexdigest()
    if hash_a != hash_b:
        return False, f"SHA256 不一致：发送 {hash_a}，接收 {hash_b}"
    return True, f"SHA256 一致：{hash_a}"


def _progress_loop(
    size: int,
    state: dict,
    stop_event: threading.Event,
    start: float,
) -> None:
    """后台线程：每秒输出一次发送和接收进度。"""
    while not stop_event.wait(PROGRESS_INTERVAL):
        sent = state["sent"]
        received = state["received"]
        elapsed = time.monotonic() - start
        rate = sent / elapsed / 1024 if elapsed > 0 else 0.0
        print(
            f"发送 {sent}/{size} 字节"
            f"（{sent * 100.0 / size:.1f}%），"
            f"接收 {received}/{size} 字节"
            f"（{received * 100.0 / size:.1f}%），"
            f"{rate:.1f} KiB/s",
            flush=True,
        )


def _receive_loop(
    ser: serial.Serial,
    path: Path,
    expected: int,
    writer_done: threading.Event,
    idle_timeout: float,
    deadline: float,
    state: dict,
) -> None:
    """后台线程：读取回环数据写入文件，收满 expected 字节或超时后退出。"""
    last_data = time.monotonic()
    try:
        with open(path, "wb") as fp:
            while state["received"] < expected:
                if time.monotonic() >= deadline:
                    state["error"] = "等待回环数据超时"
                    return
                chunk = ser.read(min(CHUNK_SIZE, expected - state["received"]))
                now = time.monotonic()
                if chunk:
                    fp.write(chunk)
                    state["received"] += len(chunk)
                    last_data = now
                elif writer_done.is_set() and now - last_data >= idle_timeout:
                    state["error"] = (
                        f"接收超时，缺少 {expected - state['received']} 字节"
                    )
                    return
    except (serial.SerialException, OSError) as exc:
        state["error"] = f"接收失败：{exc}"


def run_loopback(
    ser: serial.Serial,
    data_path: Path,
    recv_path: Path,
    baud: int,
    idle_timeout: float,
) -> tuple[int, int, float, str | None]:
    """发送随机数据并接收回环数据，返回 (发送字节数, 接收字节数, 用时, 错误)。"""
    size = data_path.stat().st_size
    # 8N1 每字节 10 bit，按波特率估算线路耗时，再加上空闲超时和余量。
    wire_seconds = size * 10.0 / baud
    deadline = time.monotonic() + wire_seconds + idle_timeout + 10.0

    state = {"sent": 0, "received": 0, "error": None}
    writer_done = threading.Event()
    progress_stop = threading.Event()
    recv_path.parent.mkdir(parents=True, exist_ok=True)
    receiver = threading.Thread(
        target=_receive_loop,
        args=(ser, recv_path, size, writer_done, idle_timeout, deadline, state),
        daemon=True,
    )

    ser.reset_input_buffer()
    start = time.monotonic()
    receiver.start()
    progress = threading.Thread(
        target=_progress_loop,
        args=(size, state, progress_stop, start),
        daemon=True,
    )
    progress.start()
    sent = 0
    error = None
    try:
        with open(data_path, "rb") as fp:
            while True:
                chunk = fp.read(CHUNK_SIZE)
                if not chunk:
                    break
                ser.write(chunk)
                sent += len(chunk)
                state["sent"] = sent
    except serial.SerialTimeoutException:
        error = "发送超时，设备可能未接收数据"
    except (serial.SerialException, OSError) as exc:
        error = f"发送失败：{exc}"
    finally:
        writer_done.set()
        receiver.join(timeout=idle_timeout + 5.0)
        progress_stop.set()
        progress.join(timeout=PROGRESS_INTERVAL + 1.0)

    elapsed = time.monotonic() - start
    return sent, state["received"], elapsed, error or state["error"]


def open_serial(port: str, baud: int, timeout: float) -> serial.Serial:
    """打开串口并等待设备就绪。"""
    # 低波特率下发送一块数据可能超过空闲超时，写超时需按波特率放大。
    write_timeout = max(timeout, CHUNK_SIZE * 10.0 / baud * 2.0)
    ser = serial.serial_for_url(
        port,
        baudrate=baud,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.2,
        write_timeout=write_timeout,
    )
    try:
        ser.dtr = True
    except (serial.SerialException, OSError):
        pass
    time.sleep(0.2)  # 等待 CDC 设备枚举就绪
    return ser


def list_serial_ports() -> int:
    """打印可用串口列表。"""
    ports = sorted(list_ports.comports(), key=lambda p: p.device)
    if not ports:
        print("未发现串口设备。")
        return 1
    print("可用串口：")
    for port in ports:
        print(f"  {port.device:<12} {port.description}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="串口回环测试：生成随机数据，发送后接收回环数据并比对。",
    )
    parser.add_argument("-p", "--port", help="串口名称，如 COM3 或 /dev/ttyACM0")
    parser.add_argument(
        "-b", "--baud", type=int, default=DEFAULT_BAUD,
        help=f"波特率（默认 {DEFAULT_BAUD}）",
    )
    parser.add_argument(
        "-s", "--size", type=float, default=DEFAULT_SIZE_MB,
        help=f"随机数据大小，单位 MB（默认 {DEFAULT_SIZE_MB:g}）",
    )
    parser.add_argument(
        "-t", "--timeout", type=float, default=DEFAULT_TIMEOUT,
        help=f"接收空闲超时，单位秒（默认 {DEFAULT_TIMEOUT:g}）",
    )
    parser.add_argument(
        "--data-file", type=Path, default=DEFAULT_DATA_FILE,
        help=f"发送数据文件（默认 {DEFAULT_DATA_FILE}）",
    )
    parser.add_argument(
        "--recv-file", type=Path, default=DEFAULT_RECV_FILE,
        help=f"接收数据文件（默认 {DEFAULT_RECV_FILE}）",
    )
    parser.add_argument("--list", action="store_true", help="列出可用串口后退出")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.list:
        return list_serial_ports()
    if not args.port:
        parser.error("必须指定串口，使用 -p/--port；可用 --list 查看串口")
    if args.baud <= 0:
        parser.error("--baud 必须大于 0")
    if args.size <= 0:
        parser.error("--size 必须大于 0")
    size_bytes = int(args.size * MB)
    if size_bytes <= 0:
        parser.error("--size 太小，至少为 1 字节")
    if args.timeout <= 0:
        parser.error("--timeout 必须大于 0")

    print(f"生成随机数据 : {args.data_file} ({args.size:g} MB, {size_bytes} 字节)")
    generate_random_file(args.data_file, size_bytes)

    print(f"打开串口     : {args.port} @ {args.baud} 8N1")
    try:
        ser = open_serial(args.port, args.baud, args.timeout)
    except (serial.SerialException, OSError, ValueError) as exc:
        print(f"错误：无法打开串口 {args.port}：{exc}", file=sys.stderr)
        return 2

    try:
        sent, received, elapsed, error = run_loopback(
            ser, args.data_file, args.recv_file, args.baud, args.timeout,
        )
    finally:
        ser.close()

    rate = sent / elapsed / 1024 if elapsed > 0 else 0.0
    print(f"发送 / 接收  : {sent} / {received} 字节")
    print(f"用时         : {elapsed:.2f} s（{rate:.1f} KiB/s）")
    if error:
        print(f"错误         : {error}", file=sys.stderr)

    if not args.recv_file.exists():
        print(f"比对结果     : 未生成接收文件 {args.recv_file}")
        print("测试结果     : 失败")
        return 1

    ok, detail = compare_files(args.data_file, args.recv_file)
    print(f"比对结果     : {detail}")
    passed = ok and error is None
    print(f"测试结果     : {'通过' if passed else '失败'}")
    return 0 if passed else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n已取消。", file=sys.stderr)
        sys.exit(130)
