#!/usr/bin/env bash
set -euo pipefail

SHELL_BIN="$(cd "$(dirname "$0")/.." && pwd)/mini-shell"
TMP_DIR="$(mktemp -d "${PWD}/tests/tmp_redir.XXXXXX")"
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

# 1. Output truncation (>), input redirection (<), and append (>>)
printf 'cd %s\necho test > output.txt\ncat < output.txt\necho another >> output.txt\ncat < output.txt\n' "$TMP_DIR" | "$SHELL_BIN" > "$TMP_DIR/stdout1.txt"
expected="$(printf 'test\ntest\nanother')"
actual="$(cat "$TMP_DIR/stdout1.txt")"
assert_eq "$expected" "$actual" "> truncate, < input, >> append"

# 2. Truncation overwrites existing content
printf 'cd %s\necho fresh > output.txt\ncat output.txt\n' "$TMP_DIR" | "$SHELL_BIN" > "$TMP_DIR/stdout2.txt"
assert_eq "fresh" "$(cat "$TMP_DIR/stdout2.txt")" "> truncates existing file"

# 3. Operators without spaces and file-to-file redirection
printf 'cd %s\nprintf "line1\\nline2\\n">in.txt\ncat in.txt>copy.txt\ncat in.txt>>copy.txt\nwc -l < copy.txt\n' "$TMP_DIR" | "$SHELL_BIN" > "$TMP_DIR/stdout3.txt"
assert_eq "4" "$(tr -d ' ' < "$TMP_DIR/stdout3.txt")" "redirection without spaces and >> append"

# 4. Built-in redirection and fd restoration
out="$(printf 'cd %s\npwd > pwd_out.txt\necho restored_stdout\n' "$TMP_DIR" | "$SHELL_BIN")"
assert_eq "restored_stdout" "$out" "stdout restored after built-in redirection"
assert_eq "$TMP_DIR" "$(cat "$TMP_DIR/pwd_out.txt")" "pwd redirected to file"

# 5. Multiple redirections on a single command
printf 'cd %s\necho final > first.txt > second.txt\n' "$TMP_DIR" | "$SHELL_BIN"
assert_eq "" "$(cat "$TMP_DIR/first.txt")" "first redirected file created empty"
assert_eq "final" "$(cat "$TMP_DIR/second.txt")" "last output redirection receives output"

# 6. Error handling for missing input file and invalid output path
set +e
err="$(printf 'cat < %s/does_not_exist.txt\n' "$TMP_DIR" | "$SHELL_BIN" 2>&1)"
rc=$?
set -e
[ "$rc" -ne 0 ] || fail "missing input file must return non-zero"
echo "$err" | grep -q "No such file" || fail "missing input file error message"

set +e
err="$(printf 'echo hi > %s/no_dir/file.txt\n' "$TMP_DIR" | "$SHELL_BIN" 2>&1)"
rc=$?
set -e
[ "$rc" -ne 0 ] || fail "invalid output path must return non-zero"

echo "test_redirection.sh: ALL PASSED"
