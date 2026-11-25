#define _POSIX_C_SOURCE 200809L

#include "shell.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int apply_redirections(const Command *cmd) {
    for (size_t i = 0; i < cmd->redir_count; i++) {
        const Redirection *r = &cmd->redirs[i];
        int fd = -1;
        int target_fd = STDIN_FILENO;

        if (r->type == REDIR_INPUT) {
            fd = open(r->filename, O_RDONLY);
            target_fd = STDIN_FILENO;
        } else if (r->type == REDIR_OUTPUT) {
            fd = open(r->filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            target_fd = STDOUT_FILENO;
        } else if (r->type == REDIR_APPEND) {
            fd = open(r->filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
            target_fd = STDOUT_FILENO;
        }

        if (fd == -1) {
            fprintf(stderr, "mini-shell: %s: %s\n", r->filename, strerror(errno));
            return -1;
        }

        if (dup2(fd, target_fd) == -1) {
            fprintf(stderr, "mini-shell: dup2 failed for %s: %s\n",
                    r->filename, strerror(errno));
            close(fd);
            return -1;
        }

        if (fd != target_fd) {
            close(fd);
        }
    }

    return 0;
}

int save_std_fds(int saved_fds[2]) {
    saved_fds[0] = dup(STDIN_FILENO);
    if (saved_fds[0] == -1) {
        perror("mini-shell: dup stdin");
        return -1;
    }

    saved_fds[1] = dup(STDOUT_FILENO);
    if (saved_fds[1] == -1) {
        perror("mini-shell: dup stdout");
        close(saved_fds[0]);
        saved_fds[0] = -1;
        return -1;
    }

    return 0;
}

int restore_std_fds(int saved_fds[2]) {
    int status = 0;

    if (saved_fds[0] != -1) {
        if (dup2(saved_fds[0], STDIN_FILENO) == -1) {
            perror("mini-shell: restore stdin");
            status = -1;
        }
        close(saved_fds[0]);
        saved_fds[0] = -1;
    }

    if (saved_fds[1] != -1) {
        if (dup2(saved_fds[1], STDOUT_FILENO) == -1) {
            perror("mini-shell: restore stdout");
            status = -1;
        }
        close(saved_fds[1]);
        saved_fds[1] = -1;
    }

    return status;
}
