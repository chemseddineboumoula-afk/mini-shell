#define _POSIX_C_SOURCE 200809L

#include "shell.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    ShellState state;
    shell_init(&state);

    if (argc == 3 && strcmp(argv[1], "-c") == 0) {
        state.interactive = false;
        int status = shell_execute_line(&state, argv[2]);
        int exit_code = state.should_exit ? state.exit_code : status;
        shell_cleanup(&state);
        return exit_code;
    }

    if (argc > 1) {
        fprintf(stderr, "Usage: %s [-c command]\n", argv[0]);
        shell_cleanup(&state);
        return 2;
    }

    int exit_code = shell_run(&state);
    shell_cleanup(&state);
    return exit_code;
}
