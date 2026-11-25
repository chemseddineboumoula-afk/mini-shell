#define _POSIX_C_SOURCE 200809L

#include "shell.h"

#include <signal.h>
#include <stdio.h>

static volatile sig_atomic_t g_sigint_received = 0;

static void sigint_handler(int signo) {
    (void)signo;
    g_sigint_received = 1;
}

void setup_interactive_signals(void) {
    struct sigaction sa;

    /* Catch SIGINT without SA_RESTART so getline() returns EINTR cleanly */
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("mini-shell: sigaction SIGINT");
    }

    /* Ignore SIGQUIT and SIGTSTP in the interactive shell process */
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGQUIT, &sa, NULL) == -1) {
        perror("mini-shell: sigaction SIGQUIT");
    }
    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("mini-shell: sigaction SIGTSTP");
    }
}

void set_parent_wait_signals(bool waiting) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = waiting ? SIG_IGN : sigint_handler;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("mini-shell: sigaction SIGINT");
    }
}

void setup_child_signals(void) {
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sa.sa_handler = SIG_IGN;
    sigaction(SIGTSTP, &sa, NULL);
}

bool consume_sigint_flag(void) {
    if (g_sigint_received) {
        g_sigint_received = 0;
        return true;
    }
    return false;
}
