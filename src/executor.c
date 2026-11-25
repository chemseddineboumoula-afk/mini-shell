#define _POSIX_C_SOURCE 200809L

#include "shell.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void exec_external_child(const Command *cmd) {
    execvp(cmd->argv[0], cmd->argv);

    int err = errno;
    if (err == ENOENT && strchr(cmd->argv[0], '/') == NULL) {
        fprintf(stderr, "mini-shell: %s: command not found\n", cmd->argv[0]);
        _exit(127);
    }

    fprintf(stderr, "mini-shell: %s: %s\n", cmd->argv[0], strerror(err));
    _exit(err == ENOENT ? 127 : 126);
}

static int status_from_wait(int wait_status, bool interactive) {
    if (WIFEXITED(wait_status)) {
        return WEXITSTATUS(wait_status);
    }
    if (WIFSIGNALED(wait_status)) {
        int sig = WTERMSIG(wait_status);
        if (interactive && sig == SIGINT) {
            putchar('\n');
            fflush(stdout);
        }
        return 128 + sig;
    }
    return 1;
}

static int wait_for_pid(pid_t pid, bool interactive) {
    int status = 0;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            perror("mini-shell: waitpid");
            return 1;
        }
    }
    return status_from_wait(status, interactive);
}

static int execute_single_builtin(ShellState *state, const Command *cmd) {
    if (cmd->redir_count == 0) {
        return execute_builtin(state, cmd);
    }

    int saved_fds[2] = {-1, -1};
    if (save_std_fds(saved_fds) == -1) {
        return 1;
    }

    int status = 1;
    if (apply_redirections(cmd) == 0) {
        status = execute_builtin(state, cmd);
        fflush(stdout);
    }

    if (restore_std_fds(saved_fds) == -1) {
        return 1;
    }
    return status;
}

static int execute_single_external(ShellState *state, const Command *cmd) {
    if (state->interactive) {
        set_parent_wait_signals(true);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("mini-shell: fork");
        if (state->interactive) {
            set_parent_wait_signals(false);
        }
        return 1;
    }

    if (pid == 0) {
        setup_child_signals();
        if (apply_redirections(cmd) == -1) {
            _exit(1);
        }
        exec_external_child(cmd);
    }

    int exit_status = wait_for_pid(pid, state->interactive);
    if (state->interactive) {
        set_parent_wait_signals(false);
    }
    return exit_status;
}

int execute_pipeline(ShellState *state, const Pipeline *pipeline) {
    if (!pipeline || pipeline->count == 0) {
        return state->last_status;
    }

    if (pipeline->count == 1) {
        const Command *cmd = &pipeline->commands[0];
        if (is_builtin(cmd->argv[0])) {
            return execute_single_builtin(state, cmd);
        }
        return execute_single_external(state, cmd);
    }

    size_t n = pipeline->count;
    pid_t *pids = calloc(n, sizeof(pid_t));
    if (!pids) {
        fprintf(stderr, "mini-shell: memory allocation failed\n");
        return 1;
    }

    if (state->interactive) {
        set_parent_wait_signals(true);
    }

    int prev_read_fd = -1;
    size_t spawned = 0;
    bool spawn_failed = false;

    for (size_t i = 0; i < n; i++) {
        int pipefd[2] = {-1, -1};
        if (i < n - 1) {
            if (pipe(pipefd) == -1) {
                perror("mini-shell: pipe");
                spawn_failed = true;
                break;
            }
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("mini-shell: fork");
            if (pipefd[0] != -1) {
                close(pipefd[0]);
            }
            if (pipefd[1] != -1) {
                close(pipefd[1]);
            }
            spawn_failed = true;
            break;
        }

        if (pid == 0) {
            setup_child_signals();

            if (prev_read_fd != -1) {
                if (dup2(prev_read_fd, STDIN_FILENO) == -1) {
                    perror("mini-shell: dup2 pipe stdin");
                    _exit(1);
                }
                close(prev_read_fd);
            }

            if (i < n - 1) {
                close(pipefd[0]);
                if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                    perror("mini-shell: dup2 pipe stdout");
                    _exit(1);
                }
                close(pipefd[1]);
            }

            const Command *cmd = &pipeline->commands[i];
            if (apply_redirections(cmd) == -1) {
                _exit(1);
            }

            if (is_builtin(cmd->argv[0])) {
                int rc = execute_builtin(state, cmd);
                fflush(NULL);
                _exit(rc);
            }

            exec_external_child(cmd);
        }

        /* Parent process */
        pids[spawned++] = pid;

        if (prev_read_fd != -1) {
            close(prev_read_fd);
            prev_read_fd = -1;
        }

        if (i < n - 1) {
            close(pipefd[1]);
            prev_read_fd = pipefd[0];
        }
    }

    if (prev_read_fd != -1) {
        close(prev_read_fd);
    }

    int last_status = spawn_failed ? 1 : 0;
    for (size_t i = 0; i < spawned; i++) {
        int st = wait_for_pid(pids[i], state->interactive && i == spawned - 1);
        if (!spawn_failed && i == spawned - 1) {
            last_status = st;
        }
    }

    if (state->interactive) {
        set_parent_wait_signals(false);
    }

    free(pids);
    return last_status;
}
