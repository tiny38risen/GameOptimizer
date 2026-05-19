from __future__ import annotations

import argparse
import pathlib
import queue
import subprocess
import sys
import threading
import time


def enqueue_output(stream, output_queue: queue.Queue[str]) -> None:
    try:
        for line in iter(stream.readline, ""):
            output_queue.put(line)
    finally:
        stream.close()


def terminate_process(process: subprocess.Popen[str]) -> None:
    if process.poll() is not None:
        return
    process.kill()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        pass


def main() -> int:
    parser = argparse.ArgumentParser(description="Run a soak command with hang detection and log capture.")
    parser.add_argument("--timeout-seconds", required=True, type=int)
    parser.add_argument("--idle-timeout-seconds", required=True, type=int)
    parser.add_argument("--log-file", required=True, type=pathlib.Path)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()

    command = args.command
    if command and command[0] == "--":
        command = command[1:]
    if not command:
        print("[FAIL] hang detection: command is required")
        return 2
    if args.timeout_seconds <= 0 or args.idle_timeout_seconds <= 0:
        print("[FAIL] hang detection: timeout values must be positive")
        return 2

    args.log_file.parent.mkdir(parents=True, exist_ok=True)
    try:
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            encoding="utf-8",
            errors="replace",
        )
    except OSError as exc:
        message = f"[FAIL] hang detection: failed to start command: {exc}\n"
        sys.stdout.write(message)
        args.log_file.write_text(message, encoding="utf-8")
        return 1

    output_queue: queue.Queue[str] = queue.Queue()
    reader = threading.Thread(target=enqueue_output, args=(process.stdout, output_queue), daemon=True)
    reader.start()

    start_time = time.monotonic()
    last_output_time = start_time
    with args.log_file.open("w", encoding="utf-8", newline="") as log_file:
        while True:
            try:
                line = output_queue.get(timeout=0.5)
            except KeyboardInterrupt:
                terminate_process(process)
                message = "[FAIL] hang detection: soak run interrupted by Ctrl+C\n"
                sys.stdout.write(message)
                log_file.write(message)
                return 130
            except queue.Empty:
                line = None

            if line is not None:
                sys.stdout.write(line)
                sys.stdout.flush()
                log_file.write(line)
                log_file.flush()
                last_output_time = time.monotonic()

            now = time.monotonic()
            if process.poll() is not None and output_queue.empty():
                return process.returncode

            if now - start_time > args.timeout_seconds:
                terminate_process(process)
                message = (
                    f"[FAIL] hang detection: command exceeded wall timeout "
                    f"{args.timeout_seconds} seconds\n")
                sys.stdout.write(message)
                log_file.write(message)
                return 1

            if now - last_output_time > args.idle_timeout_seconds:
                terminate_process(process)
                message = (
                    f"[FAIL] hang detection: no process output for "
                    f"{args.idle_timeout_seconds} seconds\n")
                sys.stdout.write(message)
                log_file.write(message)
                return 1


if __name__ == "__main__":
    raise SystemExit(main())
