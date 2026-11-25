#define _POSIX_C_SOURCE 200809L

#include "shell.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void shell_init(ShellState *state) {
    state->last_status = 0;
    state->should_exit = false;
    state->exit_code = 0;
    state->interactive = (isatty(STDIN_FILENO) == 1);

    const char *oldpwd = getenv("OLDPWD");
    state->prev_dir = oldpwd ? strdup(oldpwd) : NULL;
}

void shell_cleanup(ShellState *state) {
    free(state->prev_dir);
    state->prev_dir = NULL;
}

int shell_execute_line(ShellState *state, const char *line) {
    if (!line) {
        return state->last_status;
    }

    bool syntax_error = false;
    Pipeline *pipeline = parse_line(line, &syntax_error);
    if (syntax_error) {
        state->last_status = 2;
        return state->last_status;
    }

    if (!pipeline || pipeline->count == 0) {
        free_pipeline(pipeline);
        return state->last_status;
    }

    state->last_status = execute_pipeline(state, pipeline);
    free_pipeline(pipeline);
    return state->last_status;
}

int shell_run(ShellState *state) {
    if (state->interactive) {
        setup_interactive_signals();
    }

    char *line = NULL;
    size_t cap = 0;

    while (!state->should_exit) {
        if (state->interactive) {
            printf("mini-shell $ ");
            fflush(stdout);
        }

        errno = 0;
        ssize_t nread = getline(&line, &cap, stdin);
        if (nread == -1) {
            if (errno == EINTR) {
                clearerr(stdin);
                if (consume_sigint_flag() && state->interactive) {
                    putchar('\n');
                    fflush(stdout);
                }
                continue;
            }
            if (feof(stdin)) {
                if (state->interactive) {
                    putchar('\n');
                    fflush(stdout);
                }
                break;
            }
            perror("mini-shell: getline");
            state->last_status = 1;
            break;
        }

        if (consume_sigint_flag() && state->interactive) {
            putchar('\n');
        }

        shell_execute_line(state, line);
    }

    free(line);
    return state->should_exit ? state->exit_code : state->last_status;
}
