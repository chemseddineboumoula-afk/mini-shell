#!/usr/bin/env bash
set -euo pipefail

SHELL_BIN="$(cd "$(dirname "$0")/.." && pwd)/mini-shell"

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

# 1. Double quotes and single quotes preserving spaces
out="$("$SHELL_BIN" -c 'echo "Hello World"')"
assert_eq "Hello World" "$out" "double quotes"

out="$("$SHELL_BIN" -c "echo 'Hello World'")"
assert_eq "Hello World" "$out" "single quotes"

out="$("$SHELL_BIN" -c 'echo "My name is Shell"')"
assert_eq "My name is Shell" "$out" "multi-word double quotes"

out="$("$SHELL_BIN" -c 'echo "A B C"')"
assert_eq "A B C" "$out" "echo A B C"

# 2. Empty arguments inside quotes
out="$(printf 'python3 -c "import sys; print(len(sys.argv)-1, repr(sys.argv[1]))" ""\n' | "$SHELL_BIN")"
assert_eq "1 ''" "$out" "empty double-quoted argument passed as empty string"

out="$(printf "python3 -c 'import sys; print(len(sys.argv)-1, repr(sys.argv[1]))' ''\n" | "$SHELL_BIN")"
assert_eq "1 ''" "$out" "empty single-quoted argument passed as empty string"

# 3. Escaped characters
out="$("$SHELL_BIN" -c 'echo hello\ world')"
assert_eq "hello world" "$out" "escaped space"

out="$("$SHELL_BIN" -c 'echo "hello \"quoted\""')"
assert_eq 'hello "quoted"' "$out" "escaped double quote inside double quotes"

out="$("$SHELL_BIN" -c 'echo "back\\slash"')"
assert_eq 'back\slash' "$out" "escaped backslash inside double quotes"

# 4. Adjacent quoted/unquoted concatenation
out="$("$SHELL_BIN" -c 'echo pre"mid space"'\''post'\''')"
assert_eq "premid spacepost" "$out" "adjacent quoted segment merging"

# 5. Multiple spaces and tabs
out="$("$SHELL_BIN" -c 'echo   one    two		three')"
assert_eq "one two three" "$out" "multiple spaces and tabs"

# 6. Syntax error cases: unclosed quotes, trailing backslash, invalid pipe/redir
for bad_cmd in \
    'echo "unclosed quote' \
    "echo 'unclosed single" \
    'echo trailing\' \
    '| ls' \
    'ls |' \
    'ls | | wc' \
    'echo hi >' \
    'cat < | wc'
do
    set +e
    err="$("$SHELL_BIN" -c "$bad_cmd" 2>&1)"
    rc=$?
    set -e
    [ "$rc" -eq 2 ] || fail "expected syntax error exit 2 for [$bad_cmd], got $rc (err=$err)"
    echo "$err" | grep -q "syntax error" || fail "expected syntax error message for [$bad_cmd], got [$err]"
done

echo "test_parser.sh: ALL PASSED"
