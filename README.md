# mini-shell

A modular, POSIX-oriented Unix command-line shell implemented in C11 for Linux systems. Built as a systems programming project demonstrating process creation, file descriptor manipulation, multi-stage pipelines, I/O redirection, quoting-aware lexical analysis, and safe signal handling.

---

## Features

- **Interactive REPL & Script/Command Mode**
  - Custom `mini-shell $ ` prompt when connected to a TTY (`isatty(STDIN_FILENO)`).
  - Dynamic line reading via `getline()` with no fixed input buffer limit.
  - Graceful handling of empty/whitespace lines, `Ctrl+D` (`EOF`), and `Ctrl+C` (`SIGINT`).
  - Optional non-interactive execution via `mini-shell -c "<command>"` or piped `stdin`.
- **External Command Execution**
  - Searches executables via `PATH` using `execvp()`.
  - Forks child processes with `fork()` and reaps foreground children via `waitpid()`.
  - Propagates standard exit codes (`127` for command not found, `126` for permission/exec errors, `128 + signo` for signaled processes).
- **Built-in Commands**
  - `cd [dir]` — changes directory (supports absolute paths, relative paths, `cd` / `cd ~` for `$HOME`, and `cd -` for `$OLDPWD`).
  - `pwd` — prints the current working directory.
  - `exit [code]` — terminates the shell with an optional 8-bit exit status.
  - `help` — displays supported built-ins and syntax.
  - `clear` — clears the terminal screen using ANSI escape codes.
  - Supports I/O redirection on single built-ins (saving/restoring `stdin`/`stdout`) and running built-ins inside pipelines.
- **Quoting-Aware Tokenizer & Parser**
  - Handles single quotes (`'...'`), double quotes (`"..."`), backslash escapes (`\`), empty quoted arguments (`""` / `''`), and adjacent segment concatenation.
  - Recognizes `|`, `<`, `>`, and `>>` with or without surrounding whitespace.
  - Detects and reports unmatched quotes, trailing backslashes, missing redirection targets, and malformed pipelines without crashing or leaking memory.
- **I/O Redirection**
  - `< file` — redirects standard input (`O_RDONLY`).
  - `> file` — redirects and truncates standard output (`O_WRONLY | O_CREAT | O_TRUNC`, mode `0644`).
  - `>> file` — redirects and appends standard output (`O_WRONLY | O_CREAT | O_APPEND`, mode `0644`).
  - Supports multiple redirections per command and per-stage redirection inside pipelines.
- **Multi-Stage Pipelines**
  - Supports arbitrary pipeline depths (`cmd1 | cmd2 | cmd3 | ...`).
  - Connects stages iteratively with `pipe()` and `dup2()`, closing unused descriptors in both parent and child processes to prevent deadlocks and descriptor leaks.
  - Waits for all pipeline children and returns the exit status of the final stage.
- **Signal Handling**
  - Interactive shell catches `SIGINT` (`Ctrl+C`) without `SA_RESTART` so `getline()` cleanly aborts the current line and re-prompts.
  - Interactive shell ignores `SIGQUIT` (`Ctrl+\`) and `SIGTSTP` (`Ctrl+Z`).
  - Child processes restore `SIG_DFL` handlers prior to `execvp()`, allowing foreground commands to be interrupted normally with `Ctrl+C`.

---

## Project Architecture

```text
mini-shell/
├── include/
│   └── shell.h          # Shared structs (Command, Pipeline, ShellState) and module APIs
├── src/
│   ├── main.c           # Entry point, -c flag handling, lifecycle setup/teardown
│   ├── shell.c          # Interactive REPL loop, getline() input, line dispatch
│   ├── parser.c         # State-machine tokenizer, quote/escape handling, Pipeline builder
│   ├── executor.c       # fork/execvp/waitpid orchestration and iterative pipeline wiring
│   ├── builtins.c       # cd, pwd, exit, help, clear implementations
│   ├── redirection.c    # open/dup2/close redirection logic and fd save/restore helpers
│   └── signals.c        # sigaction() setup for parent REPL, waiting state, and children
├── tests/
│   ├── test_basic.sh    # Basic commands, built-ins, exit codes, PTY SIGINT/EOF tests
│   ├── test_parser.sh   # Quotes, escapes, empty args, operator spacing, syntax errors
│   ├── test_redirection.sh # <, >, >>, fd restoration, error handling
│   └── test_pipes.sh    # Multi-stage pipes, built-ins in pipes, large payload deadlock check
├── Makefile
├── README.md
└── .gitignore
```

### Module Responsibilities

1. **Tokenizer & Parser (`parser.c`)**: Scans raw lines character-by-character using explicit quoting states (`unquoted`, `single-quoted`, `double-quoted`) instead of `strtok()`. Produces a `Pipeline` of `Command` structs with `argv` arrays and `Redirection` lists.
2. **Executor (`executor.c`)**: Routes single built-ins directly in the shell process (saving and restoring file descriptors when redirected), single external commands via `fork()` + `execvp()` + `waitpid()`, and multi-stage pipelines by chaining one pipe at a time across stages.
3. **Redirection (`redirection.c`)**: Opens files with POSIX flags/permissions and binds them to `STDIN_FILENO` or `STDOUT_FILENO` via `dup2()`.
4. **Built-ins (`builtins.c`)**: Executes state-modifying commands (`cd`, `exit`) in the parent shell process and supports output/pipeline integration.
5. **Signals (`signals.c`)**: Uses `sigaction()` rather than legacy `signal()` for deterministic, portable POSIX signal behavior.

---

## Technologies

- **Language**: C (`-std=c11`, `_POSIX_C_SOURCE=200809L`)
- **Compiler**: GCC (`-Wall -Wextra -Wpedantic`)
- **Build System**: GNU Make
- **Platform**: Linux (Arch Linux / POSIX compatible)
- **Debugging / Verification**: GDB, AddressSanitizer (`-fsanitize=address,undefined`), Bash/Python PTY integration tests

---

## Compilation & Execution

### Build

```bash
make
```

### Run Interactive Shell

```bash
make run
# or directly:
./mini-shell
```

### Run Single Command

```bash
./mini-shell -c 'ls -la | grep src'
```

### Clean Build Artifacts

```bash
make clean
```

---

## Usage Examples

```text
mini-shell $ pwd
/home/user/Projects/mini-shell
mini-shell $ echo "Hello World"
Hello World
mini-shell $ echo "first line" > notes.txt
mini-shell $ echo "second line" >> notes.txt
mini-shell $ cat < notes.txt
first line
second line
mini-shell $ cat notes.txt | grep line | sort | uniq
first line
second line
mini-shell $ cd /tmp
mini-shell $ pwd
/tmp
mini-shell $ cd -
/home/user/Projects/mini-shell
mini-shell $ invalidcmd
mini-shell: invalidcmd: command not found
mini-shell $ exit 0
```

---

## Supported Syntax

| Feature | Syntax | Notes |
|---|---|---|
| Simple command | `cmd arg1 arg2` | Searched via `PATH` |
| Single quotes | `'literal text'` | Preserves spaces and special characters literally |
| Double quotes | `"text \"quote\""` | Preserves spaces; supports `\"` and `\\` escapes |
| Backslash escape | `arg\ with\ spaces` | Escapes the following character outside quotes |
| Input redirection | `cmd < infile` | Opens `infile` read-only on `stdin` |
| Output truncation | `cmd > outfile` | Creates (`0644`) or truncates `outfile` on `stdout` |
| Output append | `cmd >> outfile` | Creates (`0644`) or appends to `outfile` on `stdout` |
| Pipeline | `cmd1 \| cmd2 \| cmd3` | Connects `stdout` of stage *i* to `stdin` of stage *i+1* |

---

## Important System Calls Used

- **`getline()`**: Dynamically allocates and grows the input line buffer, preventing fixed-length buffer overflows while reading interactive or piped commands.
- **`fork()`**: Clones the calling process to create a child process for external commands and pipeline stages (`0` in child, child PID in parent, `-1` on error).
- **`execvp()`**: Replaces the child's process image with the target executable resolved through `PATH`, passing the parsed `argv` array. Uses `_exit()` on failure to avoid flushing parent `stdio` buffers twice.
- **`waitpid()`**: Suspends the parent until foreground child processes terminate or are signaled, handling `EINTR` safely and decoding status via `WIFEXITED` / `WIFSIGNALED`.
- **`pipe()`**: Allocates a unidirectional kernel pipe (`pipefd[0]` read end, `pipefd[1]` write end) to connect consecutive pipeline commands.
- **`open()` / `close()`**: Opens redirection target files with explicit `O_RDONLY`, `O_WRONLY | O_CREAT | O_TRUNC`, or `O_APPEND` flags and mode `0644`, and closes unused descriptors to prevent resource leaks and pipe EOF hangs.
- **`dup()` / `dup2()`**: Duplicates file descriptors onto `STDIN_FILENO` / `STDOUT_FILENO` for pipes and redirections, and saves/restores standard streams around redirected built-ins.
- **`chdir()` / `getcwd()`**: Implements `cd` and `pwd` directly in the shell process (necessary because directory changes in a child process would not affect the shell).
- **`sigaction()`**: Configures deterministic signal handlers (`SIGINT`, `SIGQUIT`, `SIGTSTP`) in the shell and resets `SIG_DFL` in forked children.

---

## Testing

Run all automated test suites:

```bash
make test
```

The test suites feed commands directly into `./mini-shell` (via `stdin`, `-c`, and a Python `pty` harness for interactive `Ctrl+C` / `Ctrl+D` testing):

- `tests/test_basic.sh`: External commands, `cd` / `cd -` / `pwd` / `help` / `clear` / `exit`, empty input, error status codes, and PTY interactive prompt + `SIGINT` + `EOF`.
- `tests/test_parser.sh`: Single/double quotes, escape sequences, empty quoted arguments (`""`, `''`), adjacent segment merging, and syntax error detection.
- `tests/test_redirection.sh`: `<`, `>`, `>>`, operator spacing, built-in fd restoration, multiple redirections, and missing/invalid file errors.
- `tests/test_pipes.sh`: 2-to-4-stage pipelines, built-ins in pipelines, redirection + pipes, pipeline exit status, failed stages, and >128 KB payload throughput without deadlocks.

To run the suite under AddressSanitizer and UndefinedBehaviorSanitizer:

```bash
make clean
make CFLAGS="-Wall -Wextra -Wpedantic -Werror -std=c11 -Iinclude -g -fsanitize=address,undefined" LDFLAGS="-fsanitize=address,undefined"
make test
```

---

## Known Limitations

- **No Full Job Control**: Background execution (`&`), `fg`, `bg`, `jobs`, and terminal process group management (`tcsetpgrp()`) are not implemented. `SIGTSTP` (`Ctrl+Z`) is ignored by the shell so the shell itself cannot be suspended accidentally.
- **No Environment Variable Expansion or Globbing**: `$VAR`, `$?`, `*`, `?`, and tilde expansion beyond `cd ~` are not expanded by the parser.
- **No Logical Operators or Subshells**: `;`, `&&`, `||`, `( ... )`, and command substitution (`` `...` `` / `$(...)`) are not supported.
- **No Line Editing / History**: Uses standard `getline()` rather than `readline` or raw-mode termios editing (arrow-key history/tab completion).
- **Single-Line Commands Only**: Multi-line quoted strings or line continuations (`\` followed by newline) are treated as syntax errors.

---

## Future Improvements

- Environment variable (`$VAR`, `$?`) and tilde expansion in the parser.
- Background jobs (`&`) and full POSIX job control (`tcsetpgrp`, `jobs`, `fg`, `bg`).
- Logical chaining (`&&`, `||`, `;`) and stderr redirection (`2>`, `2>&1`).
- Interactive line editing and persistent command history (`~/.mini_shell_history`).
- Filename globbing (`*`, `?`) using `glob()`.
