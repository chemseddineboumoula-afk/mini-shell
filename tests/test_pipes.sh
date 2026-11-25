#!/usr/bin/env bash
set -euo pipefail

SHELL_BIN="$(cd "$(dirname "$0")/.." && pwd)/mini-shell"
TMP_DIR="$(mktemp -d "${PWD}/tests/tmp_pipes.XXXXXX")"
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

# 1. Basic two-stage pipe
out="$("$SHELL_BIN" -c 'ls /bin | wc -l')"
[ "$out" -gt 0 ] 2>/dev/null || fail "ls /bin | wc -l returned invalid count: $out"

# 2. Three-stage pipe
out="$("$SHELL_BIN" -c 'echo hello | cat | cat')"
assert_eq "hello" "$out" "3-stage pipe echo hello | cat | cat"

# 3. /etc/passwd grep root
out="$("$SHELL_BIN" -c 'cat /etc/passwd | grep "^root:"')"
echo "$out" | grep -q "^root:" || fail "cat /etc/passwd | grep root failed"

# 4. Four-stage pipeline: cat file.txt | grep hello | sort | uniq
cat > "$TMP_DIR/data.txt" <<'EOF'
hello zebra
world ignore
hello apple
hello zebra
hello banana
EOF
out="$(printf 'cd %s\ncat data.txt | grep hello | sort | uniq\n' "$TMP_DIR" | "$SHELL_BIN")"
expected="$(printf 'hello apple\nhello banana\nhello zebra')"
assert_eq "$expected" "$out" "4-stage pipeline cat | grep | sort | uniq"

# 5. Built-ins inside pipeline
out="$(printf 'cd %s\npwd | cat\n' "$TMP_DIR" | "$SHELL_BIN")"
assert_eq "$TMP_DIR" "$out" "builtin pwd in pipeline"

out="$("$SHELL_BIN" -c 'help | grep -c "cd"')"
[ "$out" -ge 1 ] || fail "builtin help in pipeline"

# 6. Redirection combined with pipeline
printf 'cd %s\ncat < data.txt | grep hello | sort | uniq > out.txt\n' "$TMP_DIR" | "$SHELL_BIN"
assert_eq "$expected" "$(cat "$TMP_DIR/out.txt")" "pipeline with input and output redirection"

# 7. Pipeline exit status equals last command status
set +e
"$SHELL_BIN" -c 'true | false'
rc1=$?
"$SHELL_BIN" -c 'false | true'
rc2=$?
set -e
assert_eq "1" "$rc1" "true | false exit status"
assert_eq "0" "$rc2" "false | true exit status"

# 8. Failed upstream command in pipeline does not deadlock
set +e
out="$("$SHELL_BIN" -c 'nonexistent_cmd_pipe_test | wc -l' 2>"$TMP_DIR/pipe_err.txt")"
rc=$?
set -e
assert_eq "0" "$rc" "nonexistent | wc -l returns wc status 0"
assert_eq "0" "$(echo "$out" | tr -d ' ')" "nonexistent | wc -l counts 0 lines"
grep -q "command not found" "$TMP_DIR/pipe_err.txt" || fail "missing command not found error in pipeline"

# 9. Large data (> 128 KB) through multi-stage pipe to verify no fd/buffer deadlock
python3 -c 'print("line\n" * 30000, end="")' > "$TMP_DIR/big.txt"
out="$(printf 'cd %s\ncat big.txt | cat | wc -l\n' "$TMP_DIR" | "$SHELL_BIN")"
assert_eq "30000" "$(echo "$out" | tr -d ' ')" "large payload through pipeline without deadlock"

echo "test_pipes.sh: ALL PASSED"
