#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define PROMPT "govno007$ "
#define MAX_INPUT_CHARS 1024

int parse_tokens(char* buf, char*** tokens);
int execute_cmd(const char* name, char** args);
void free_tokens(char** tokens, const size_t count);
bool is_empty(const char* str);
char* ask_prompt(char* input_buf);

int main() {
    // TODO errno
    char* buf = malloc(sizeof(char) * MAX_INPUT_CHARS);
    if (buf == NULL) {
        fprintf(stderr, "error: could not allocate memory for input buffer\n");
        return 1;
    }

    while (ask_prompt(buf) != NULL) {
        // if valid input
        if (!is_empty(buf)) {
            char* buf_copy = strdup(buf);
            if (buf_copy == NULL) {
                fprintf(stderr, "error: could not duplicate buffer string\n");
                return 1;
            }

            char** tokens = NULL;
            if (parse_tokens(buf_copy, &tokens) == -1) {
                return 1;
            }

            int status;

            if (strcmp(tokens[0], "exit") == 0) {
                free(tokens);
                free(buf_copy);
                break;
            }

            status = execute_cmd(tokens[0], tokens);

            free(tokens);
            free(buf_copy);
        }
    }

    free(buf);
    return 0;
}

char* ask_prompt(char* input_buf) {
    printf(PROMPT);
    return fgets(input_buf, MAX_INPUT_CHARS, stdin);
}

int parse_tokens(char* buf, char*** tokens) {
    char* token = strtok(buf, " ");

    size_t count = 0;
    while (token != NULL) {
        (*tokens) = reallocarray((*tokens), ++count, sizeof(char*));
        if ((*tokens) == NULL) {
            fprintf(stderr, "error: could not reallocate memory for tokens array\n");
            return -1;
        }

        (*tokens)[count - 1] = token;
        token = strtok(NULL, " ");
    }

    // cut off newline from last token
    char* last_token = (*tokens)[count - 1];
    last_token[strlen(last_token) - 1] = '\0';

    // null terminate tokens array for execvp
    (*tokens) = reallocarray((*tokens), count, sizeof(char*));
    (*tokens)[count] = NULL;

    return 0;
}

int execute_cmd(const char* name, char** args) {
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "error: could not create child process\n");
        return 1;
    } else if (pid == 0) {
        // child process
        execvp(name, args);

        switch (errno) {
            // TODO maybe other errors
            case ENOENT:
                fprintf(stderr, "error: command not found: %s\n", name);
                break;
            case EPERM:
            case EACCES:
                fprintf(stderr, "error: permission denied: %s\n", name);
                break;
            default:
                fprintf(stderr, "error: could not execute: %s\n", name);
        }
        exit(errno);
    } else {
        // parent process
        int status;
        waitpid(pid, &status, 0);
        return status;
    }
}

bool is_empty(const char* s) {
    while (*s != '\0') {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}
