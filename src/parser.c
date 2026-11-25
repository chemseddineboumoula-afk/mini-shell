#define _POSIX_C_SOURCE 200809L

#include "shell.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TOK_WORD,
    TOK_PIPE,
    TOK_REDIR_IN,
    TOK_REDIR_OUT,
    TOK_REDIR_APPEND
} TokenType;

typedef struct {
    TokenType type;
    char *value;
} Token;

typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} TokenList;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

static void strbuf_free(StrBuf *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

static bool strbuf_ensure(StrBuf *buf, size_t extra) {
    if (buf->len + extra + 1 <= buf->cap) {
        return true;
    }
    size_t new_cap = buf->cap == 0 ? 32 : buf->cap;
    while (new_cap < buf->len + extra + 1) {
        if (new_cap > (size_t)-1 / 2) {
            return false;
        }
        new_cap *= 2;
    }
    char *new_data = realloc(buf->data, new_cap);
    if (!new_data) {
        return false;
    }
    buf->data = new_data;
    buf->cap = new_cap;
    return true;
}

static bool strbuf_append_char(StrBuf *buf, char c) {
    if (!strbuf_ensure(buf, 1)) {
        return false;
    }
    buf->data[buf->len++] = c;
    buf->data[buf->len] = '\0';
    return true;
}

static void token_list_free(TokenList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].value);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static bool token_list_push(TokenList *list, TokenType type, const char *value) {
    if (list->count == list->capacity) {
        size_t new_cap = list->capacity == 0 ? 16 : list->capacity * 2;
        Token *new_items = realloc(list->items, new_cap * sizeof(Token));
        if (!new_items) {
            return false;
        }
        list->items = new_items;
        list->capacity = new_cap;
    }

    char *copy = NULL;
    if (value) {
        copy = strdup(value);
        if (!copy) {
            return false;
        }
    }

    list->items[list->count].type = type;
    list->items[list->count].value = copy;
    list->count++;
    return true;
}

static bool flush_word(TokenList *tokens, StrBuf *buf, bool *has_word) {
    if (!*has_word) {
        return true;
    }
    const char *str = buf->data ? buf->data : "";
    bool ok = token_list_push(tokens, TOK_WORD, str);
    buf->len = 0;
    if (buf->data) {
        buf->data[0] = '\0';
    }
    *has_word = false;
    return ok;
}

static bool tokenize(const char *line, TokenList *tokens, bool *syntax_error) {
    StrBuf buf = {0};
    bool has_word = false;
    bool in_single = false;
    bool in_double = false;

    for (size_t i = 0; line[i] != '\0'; i++) {
        char c = line[i];

        if (in_single) {
            if (c == '\'') {
                in_single = false;
            } else if (!strbuf_append_char(&buf, c)) {
                goto oom;
            }
            continue;
        }

        if (in_double) {
            if (c == '"') {
                in_double = false;
            } else if (c == '\\') {
                char next = line[i + 1];
                if (next == '\0') {
                    fprintf(stderr, "mini-shell: syntax error: unmatched '\"'\n");
                    *syntax_error = true;
                    goto fail;
                }
                if (next == '"' || next == '\\') {
                    if (!strbuf_append_char(&buf, next)) {
                        goto oom;
                    }
                    i++;
                } else {
                    if (!strbuf_append_char(&buf, c)) {
                        goto oom;
                    }
                }
            } else if (!strbuf_append_char(&buf, c)) {
                goto oom;
            }
            continue;
        }

        /* Unquoted context */
        if (isspace((unsigned char)c)) {
            if (!flush_word(tokens, &buf, &has_word)) {
                goto oom;
            }
            continue;
        }

        if (c == '\'' || c == '"') {
            has_word = true;
            if (!strbuf_ensure(&buf, 0)) {
                goto oom;
            }
            if (buf.len == 0) {
                buf.data[0] = '\0';
            }
            if (c == '\'') {
                in_single = true;
            } else {
                in_double = true;
            }
            continue;
        }

        if (c == '\\') {
            char next = line[i + 1];
            if (next == '\0') {
                fprintf(stderr, "mini-shell: syntax error: trailing backslash\n");
                *syntax_error = true;
                goto fail;
            }
            has_word = true;
            if (!strbuf_append_char(&buf, next)) {
                goto oom;
            }
            i++;
            continue;
        }

        if (c == '|' || c == '<' || c == '>') {
            if (!flush_word(tokens, &buf, &has_word)) {
                goto oom;
            }
            if (c == '|') {
                if (!token_list_push(tokens, TOK_PIPE, NULL)) {
                    goto oom;
                }
            } else if (c == '<') {
                if (!token_list_push(tokens, TOK_REDIR_IN, NULL)) {
                    goto oom;
                }
            } else if (line[i + 1] == '>') {
                if (!token_list_push(tokens, TOK_REDIR_APPEND, NULL)) {
                    goto oom;
                }
                i++;
            } else {
                if (!token_list_push(tokens, TOK_REDIR_OUT, NULL)) {
                    goto oom;
                }
            }
            continue;
        }

        has_word = true;
        if (!strbuf_append_char(&buf, c)) {
            goto oom;
        }
    }

    if (in_single || in_double) {
        fprintf(stderr, "mini-shell: syntax error: unmatched quote (%c)\n",
                in_single ? '\'' : '"');
        *syntax_error = true;
        goto fail;
    }

    if (!flush_word(tokens, &buf, &has_word)) {
        goto oom;
    }

    strbuf_free(&buf);
    return true;

oom:
    fprintf(stderr, "mini-shell: memory allocation failed\n");
    *syntax_error = true;
fail:
    strbuf_free(&buf);
    token_list_free(tokens);
    return false;
}

static bool command_add_arg(Command *cmd, const char *arg) {
    char **new_argv = realloc(cmd->argv, (cmd->argc + 2) * sizeof(char *));
    if (!new_argv) {
        return false;
    }
    cmd->argv = new_argv;

    char *copy = strdup(arg);
    if (!copy) {
        return false;
    }
    cmd->argv[cmd->argc++] = copy;
    cmd->argv[cmd->argc] = NULL;
    return true;
}

static bool command_add_redir(Command *cmd, RedirType type, const char *filename) {
    Redirection *new_redirs =
        realloc(cmd->redirs, (cmd->redir_count + 1) * sizeof(Redirection));
    if (!new_redirs) {
        return false;
    }
    cmd->redirs = new_redirs;

    char *copy = strdup(filename);
    if (!copy) {
        return false;
    }
    cmd->redirs[cmd->redir_count].type = type;
    cmd->redirs[cmd->redir_count].filename = copy;
    cmd->redir_count++;
    return true;
}

static const char *token_symbol(TokenType type) {
    switch (type) {
    case TOK_PIPE:
        return "|";
    case TOK_REDIR_IN:
        return "<";
    case TOK_REDIR_OUT:
        return ">";
    case TOK_REDIR_APPEND:
        return ">>";
    default:
        return "word";
    }
}

Pipeline *parse_line(const char *line, bool *syntax_error) {
    *syntax_error = false;

    TokenList tokens = {0};
    if (!tokenize(line, &tokens, syntax_error)) {
        return NULL;
    }

    Pipeline *pipeline = calloc(1, sizeof(Pipeline));
    if (!pipeline) {
        fprintf(stderr, "mini-shell: memory allocation failed\n");
        *syntax_error = true;
        token_list_free(&tokens);
        return NULL;
    }

    if (tokens.count == 0) {
        token_list_free(&tokens);
        return pipeline;
    }

    if (tokens.items[0].type == TOK_PIPE) {
        fprintf(stderr, "mini-shell: syntax error near unexpected token '|'\n");
        *syntax_error = true;
        goto fail;
    }

    size_t stage_count = 1;
    for (size_t i = 0; i < tokens.count; i++) {
        if (tokens.items[i].type == TOK_PIPE) {
            if (i + 1 >= tokens.count || tokens.items[i + 1].type == TOK_PIPE) {
                fprintf(stderr, "mini-shell: syntax error near unexpected token '|'\n");
                *syntax_error = true;
                goto fail;
            }
            stage_count++;
        }
    }

    pipeline->commands = calloc(stage_count, sizeof(Command));
    if (!pipeline->commands) {
        fprintf(stderr, "mini-shell: memory allocation failed\n");
        *syntax_error = true;
        goto fail;
    }
    pipeline->count = stage_count;

    size_t cmd_idx = 0;
    for (size_t i = 0; i < tokens.count; i++) {
        Token *tok = &tokens.items[i];
        Command *cmd = &pipeline->commands[cmd_idx];

        if (tok->type == TOK_PIPE) {
            if (cmd->argc == 0) {
                fprintf(stderr, "mini-shell: syntax error: missing command before '|'\n");
                *syntax_error = true;
                goto fail;
            }
            cmd_idx++;
            continue;
        }

        if (tok->type == TOK_WORD) {
            if (!command_add_arg(cmd, tok->value)) {
                fprintf(stderr, "mini-shell: memory allocation failed\n");
                *syntax_error = true;
                goto fail;
            }
            continue;
        }

        /* Redirection operator */
        if (i + 1 >= tokens.count) {
            fprintf(stderr,
                    "mini-shell: syntax error: missing filename after '%s'\n",
                    token_symbol(tok->type));
            *syntax_error = true;
            goto fail;
        }

        Token *next = &tokens.items[++i];
        if (next->type != TOK_WORD) {
            fprintf(stderr,
                    "mini-shell: syntax error near unexpected token '%s'\n",
                    token_symbol(next->type));
            *syntax_error = true;
            goto fail;
        }

        RedirType rtype = REDIR_INPUT;
        if (tok->type == TOK_REDIR_OUT) {
            rtype = REDIR_OUTPUT;
        } else if (tok->type == TOK_REDIR_APPEND) {
            rtype = REDIR_APPEND;
        }

        if (!command_add_redir(cmd, rtype, next->value)) {
            fprintf(stderr, "mini-shell: memory allocation failed\n");
            *syntax_error = true;
            goto fail;
        }
    }

    for (size_t i = 0; i < pipeline->count; i++) {
        if (pipeline->commands[i].argc == 0) {
            fprintf(stderr, "mini-shell: syntax error: missing command\n");
            *syntax_error = true;
            goto fail;
        }
    }

    token_list_free(&tokens);
    return pipeline;

fail:
    token_list_free(&tokens);
    free_pipeline(pipeline);
    return NULL;
}

void free_pipeline(Pipeline *pipeline) {
    if (!pipeline) {
        return;
    }

    for (size_t i = 0; i < pipeline->count; i++) {
        Command *cmd = &pipeline->commands[i];
        for (size_t j = 0; j < cmd->argc; j++) {
            free(cmd->argv[j]);
        }
        free(cmd->argv);

        for (size_t j = 0; j < cmd->redir_count; j++) {
            free(cmd->redirs[j].filename);
        }
        free(cmd->redirs);
    }

    free(pipeline->commands);
    free(pipeline);
}
