#ifndef SHELL_H
#define SHELL_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    REDIR_INPUT,
    REDIR_OUTPUT,
    REDIR_APPEND
} RedirType;

typedef struct {
    RedirType type;
    char *filename;
} Redirection;

typedef struct {
    char **argv;
    size_t argc;
    Redirection *redirs;
    size_t redir_count;
} Command;

typedef struct {
    Command *commands;
    size_t count;
} Pipeline;

typedef struct {
    int last_status;
    bool should_exit;
    int exit_code;
    bool interactive;
    char *prev_dir;
} ShellState;

/* shell.c */
void shell_init(ShellState *state);
void shell_cleanup(ShellState *state);
int shell_run(ShellState *state);
int shell_execute_line(ShellState *state, const char *line);

/* parser.c */
Pipeline *parse_line(const char *line, bool *syntax_error);
void free_pipeline(Pipeline *pipeline);

/* executor.c */
int execute_pipeline(ShellState *state, const Pipeline *pipeline);

/* builtins.c */
bool is_builtin(const char *cmd);
int execute_builtin(ShellState *state, const Command *cmd);

/* redirection.c */
int apply_redirections(const Command *cmd);
int save_std_fds(int saved_fds[2]);
int restore_std_fds(int saved_fds[2]);

/* signals.c */
void setup_interactive_signals(void);
void setup_child_signals(void);
void set_parent_wait_signals(bool waiting);
bool consume_sigint_flag(void);

#endif /* SHELL_H */
