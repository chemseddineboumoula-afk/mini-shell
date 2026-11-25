#define _POSIX_C_SOURCE 200809L

#include "shell.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool is_builtin(const char *cmd) {
    if (!cmd) {
        return false;
    }
    return strcmp(cmd, "cd") == 0 ||
           strcmp(cmd, "exit") == 0 ||
           strcmp(cmd, "pwd") == 0 ||
           strcmp(cmd, "help") == 0 ||
           strcmp(cmd, "clear") == 0;
}

static char *get_current_dir_alloc(void) {
    size_t size = 1024;
    for (;;) {
        char *buf = malloc(size);
        if (!buf) {
            return NULL;
        }
        if (getcwd(buf, size) != NULL) {
            return buf;
        }
        free(buf);
        if (errno != ERANGE || size > 65536) {
            return NULL;
        }
        size *= 2;
    }
}

static int builtin_cd(ShellState *state, const Command *cmd) {
    if (cmd->argc > 2) {
        fprintf(stderr, "mini-shell: cd: too many arguments\n");
        return 1;
    }

    const char *target = NULL;
    bool print_dir = false;

    if (cmd->argc == 1 || strcmp(cmd->argv[1], "~") == 0) {
        target = getenv("HOME");
        if (!target || target[0] == '\0') {
            fprintf(stderr, "mini-shell: cd: HOME not set\n");
            return 1;
        }
    } else if (strcmp(cmd->argv[1], "-") == 0) {
        target = state->prev_dir ? state->prev_dir : getenv("OLDPWD");
        if (!target || target[0] == '\0') {
            fprintf(stderr, "mini-shell: cd: OLDPWD not set\n");
            return 1;
        }
        print_dir = true;
    } else {
        target = cmd->argv[1];
    }

    char *old_cwd = get_current_dir_alloc();

    if (chdir(target) != 0) {
        fprintf(stderr, "mini-shell: cd: %s: %s\n", target, strerror(errno));
        free(old_cwd);
        return 1;
    }

    char *new_cwd = get_current_dir_alloc();
    if (print_dir) {
        if (new_cwd) {
            printf("%s\n", new_cwd);
        } else {
            printf("%s\n", target);
        }
        fflush(stdout);
    }

    if (old_cwd) {
        free(state->prev_dir);
        state->prev_dir = old_cwd;
        setenv("OLDPWD", old_cwd, 1);
    }
    if (new_cwd) {
        setenv("PWD", new_cwd, 1);
        free(new_cwd);
    }

    return 0;
}

static int builtin_pwd(const Command *cmd) {
    if (cmd->argc > 1) {
        fprintf(stderr, "mini-shell: pwd: too many arguments\n");
        return 1;
    }

    char *cwd = get_current_dir_alloc();
    if (!cwd) {
        fprintf(stderr, "mini-shell: pwd: %s\n", strerror(errno));
        return 1;
    }

    printf("%s\n", cwd);
    fflush(stdout);
    free(cwd);
    return 0;
}

static int builtin_exit(ShellState *state, const Command *cmd) {
    if (cmd->argc > 2) {
        fprintf(stderr, "mini-shell: exit: too many arguments\n");
        return 1;
    }

    if (cmd->argc == 1) {
        state->should_exit = true;
        state->exit_code = state->last_status;
        return state->exit_code;
    }

    const char *arg = cmd->argv[1];
    if (arg[0] == '\0') {
        fprintf(stderr, "mini-shell: exit: %s: numeric argument required\n", arg);
        state->should_exit = true;
        state->exit_code = 2;
        return 2;
    }

    char *endptr = NULL;
    errno = 0;
    long val = strtol(arg, &endptr, 10);
    if (errno == ERANGE || endptr == arg || *endptr != '\0') {
        fprintf(stderr, "mini-shell: exit: %s: numeric argument required\n", arg);
        state->should_exit = true;
        state->exit_code = 2;
        return 2;
    }

    state->should_exit = true;
    state->exit_code = (int)((unsigned long)val & 0xFFUL);
    return state->exit_code;
}

static int builtin_help(void) {
    printf("mini-shell - A POSIX-oriented Unix command-line shell in C\n\n");
    printf("Built-in commands:\n");
    printf("  cd [dir]     Change directory (supports '', '-', '~', relative & absolute paths)\n");
    printf("  pwd          Print current working directory\n");
    printf("  exit [code]  Exit the shell with optional status code\n");
    printf("  clear        Clear the terminal screen\n");
    printf("  help         Display this help message\n\n");
    printf("Supported features:\n");
    printf("  - External commands searched via PATH\n");
    printf("  - Single ('...') and double (\"...\") quotes, backslash escapes\n");
    printf("  - I/O redirection: < input, > truncate, >> append\n");
    printf("  - Multi-stage pipelines: cmd1 | cmd2 | cmd3\n");
    fflush(stdout);
    return 0;
}

static int builtin_clear(const Command *cmd) {
    if (cmd->argc > 1) {
        fprintf(stderr, "mini-shell: clear: too many arguments\n");
        return 1;
    }
    printf("\033[H\033[2J");
    fflush(stdout);
    return 0;
}

int execute_builtin(ShellState *state, const Command *cmd) {
    if (!cmd || cmd->argc == 0) {
        return 1;
    }

    if (strcmp(cmd->argv[0], "cd") == 0) {
        return builtin_cd(state, cmd);
    }
    if (strcmp(cmd->argv[0], "pwd") == 0) {
        return builtin_pwd(cmd);
    }
    if (strcmp(cmd->argv[0], "exit") == 0) {
        return builtin_exit(state, cmd);
    }
    if (strcmp(cmd->argv[0], "help") == 0) {
        return builtin_help();
    }
    if (strcmp(cmd->argv[0], "clear") == 0) {
        return builtin_clear(cmd);
    }

    return 1;
}
