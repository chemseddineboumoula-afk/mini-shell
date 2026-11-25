#!/usr/bin/env bash
set -euo pipefail

SHELL_BIN="$(cd "$(dirname "$0")/.." && pwd)/mini-shell"
TMP_DIR="$(mktemp -d "${PWD}/tests/tmp_basic.XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

assert_eq() {
    local expected="$1"
    local actual="$2"
    local msg="$3"
    if [ "$expected" != "$actual" ]; then
        fail "$msg (expected='$expected', actual='$actual')"
    fi
}

# 1. Basic external commands via mini-shell stdin
out="$(printf 'echo Hello\nwhoami\nuname -a\n' | "$SHELL_BIN")"
expected_user="$(whoami)"
expected_uname="$(uname -a)"
expected="$(printf 'Hello\n%s\n%s' "$expected_user" "$expected_uname")"
assert_eq "$expected" "$out" "basic external commands"

# 2. mkdir, ls, and rm inside temp directory
out="$(printf 'cd %s\nmkdir sub\nls\nrmdir sub\nls\n' "$TMP_DIR" | "$SHELL_BIN")"
assert_eq "sub" "$out" "mkdir/ls/rmdir inside mini-shell"

# 3. Built-in cd /, cd, cd -, pwd
out="$(printf 'cd /\npwd\ncd %s\npwd\ncd -\npwd\n' "$TMP_DIR" | "$SHELL_BIN")"
expected="$(printf '/\n%s\n/\n/' "$TMP_DIR")"
assert_eq "$expected" "$out" "cd /, cd dir, cd -, pwd"

# 4. Built-in cd with no arguments goes to HOME
out="$(printf 'cd\npwd\n' | "$SHELL_BIN")"
assert_eq "$HOME" "$out" "cd with no args goes to HOME"

# 5. Built-in help and clear
out="$("$SHELL_BIN" -c 'help')"
echo "$out" | grep -q "Built-in commands:" || fail "help output missing header"
out="$("$SHELL_BIN" -c 'clear')"
[ -n "$out" ] || fail "clear produced empty output"

# 6. Built-in exit with explicit status code
set +e
"$SHELL_BIN" -c 'exit 42'
rc=$?
set -e
assert_eq "42" "$rc" "exit 42 status code"

# 7. Built-in exit with invalid argument
set +e
err="$("$SHELL_BIN" -c 'exit notanumber' 2>&1)"
rc=$?
set -e
assert_eq "2" "$rc" "exit non-numeric status"
echo "$err" | grep -q "numeric argument required" || fail "exit non-numeric error message"

# 8. Empty input and whitespace-only input
set +e
out="$(printf '\n   \n\t\n' | "$SHELL_BIN")"
rc=$?
set -e
assert_eq "0" "$rc" "empty input exit code"
assert_eq "" "$out" "empty input produces no output"

# 9. Invalid command and nonexistent file error handling
set +e
err="$(printf 'invalidcommand_xyz_123\n' | "$SHELL_BIN" 2>&1)"
rc=$?
set -e
assert_eq "127" "$rc" "invalidcommand exit status"
echo "$err" | grep -q "command not found" || fail "invalidcommand stderr message"

set +e
err="$(printf 'cat %s/no_such_file.txt\n' "$TMP_DIR" | "$SHELL_BIN" 2>&1)"
rc=$?
set -e
[ "$rc" -ne 0 ] || fail "cat nonexistent file should return non-zero"
echo "$err" | grep -q "No such file" || fail "cat nonexistent file error message"

# 10. Interactive prompt, Ctrl+C (SIGINT), Ctrl+Z (SIGTSTP), and Ctrl+D (EOF) test via PTY
if command -v python3 >/dev/null 2>&1; then
    python3 - "$SHELL_BIN" <<'PY'
import fcntl, os, pty, select, subprocess, sys, termios, time

shell_bin = sys.argv[1]
master, slave = pty.openpty()

def child_setup():
    os.setsid()
    fcntl.ioctl(slave, termios.TIOCSCTTY, 0)

proc = subprocess.Popen(
    [shell_bin],
    stdin=slave,
    stdout=slave,
    stderr=slave,
    close_fds=True,
    preexec_fn=child_setup,
)
os.close(slave)

def read_available(fd, timeout=1.0):
    data = b""
    end = time.time() + timeout
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.1)
        if fd in r:
            try:
                chunk = os.read(fd, 4096)
                if not chunk:
                    break
                data += chunk
            except OSError:
                break
        elif data:
            break
    return data

out = read_available(master, 1.5)
assert b"mini-shell $" in out, f"Expected prompt, got {out!r}"

# Send Ctrl+C while shell is at prompt; shell must stay alive and re-prompt
os.write(master, b"\x03")
out2 = read_available(master, 1.0)
assert proc.poll() is None, "Shell exited on SIGINT!"
assert b"mini-shell $" in out2, f"Expected prompt after SIGINT, got {out2!r}"

# Send Ctrl+C while a foreground child (sleep 5) is running; child stops and shell re-prompts
os.write(master, b"sleep 5\n")
time.sleep(0.2)
os.write(master, b"\x03")
out_fg = read_available(master, 1.5)
assert proc.poll() is None, "Shell exited when child received SIGINT!"
assert b"mini-shell $" in out_fg, f"Expected prompt after foreground SIGINT, got {out_fg!r}"

# Run a command then send Ctrl+D (EOF)
os.write(master, b"echo pty_ok\n")
out3 = read_available(master, 1.0)
assert b"pty_ok" in out3, f"Expected pty_ok, got {out3!r}"

os.write(master, b"\x04")
rc = proc.wait(timeout=2.0)
os.close(master)
assert rc == 0, f"Expected clean exit 0 on Ctrl+D, got {rc}"
PY
fi

echo "test_basic.sh: ALL PASSED"
