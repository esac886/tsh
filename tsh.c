#include <bits/posix1_lim.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// TODO make customizations via tshrc. file in $HOME $XDG_CONFIG_HOME
#define DEFAULT_PROMPT "%s@%s # %s >> "
#define MAX_INPUT_CHARS 1024

int parse_tokens(char* buf, char*** tokens);
int execute_cmd(const char* name, char** args);

char* ask_prompt(char* input_buf);
char* gen_prompt(void);

bool is_empty(const char* str);

// TODO move HOME to hashtable
char* home;

int main() {
    char* buf = malloc(sizeof(char) * MAX_INPUT_CHARS);
    if (buf == NULL) {
        fprintf(stderr, "error: could not allocate memory for input buffer: %s\n", strerror(errno));
        return 1;
    }

    char* whoami = getlogin();
    home = malloc((strlen(whoami) + 6) * sizeof(char));
    if (home == NULL) {
        fprintf(stderr, "error: could not allocate memory: %s\n", strerror(errno));
        return 1;
    }
    strcpy(home, "/home/");
    strcat(home, whoami);

    while (ask_prompt(buf) != NULL) {
        // if valid input
        if (!is_empty(buf)) {
            char* buf_copy = strdup(buf);
            if (buf_copy == NULL) {
                fprintf(stderr, "error: could not duplicate buffer string: %s\n", strerror(errno));
                return 1;
            }

            char** tokens = NULL;
            if (parse_tokens(buf_copy, &tokens) == -1) {
                return 1;
            }

            int status;

            // EXIT
            if (strcmp(tokens[0], "exit") == 0) {
                free(tokens);
                free(buf_copy);
                break;
            }

            // CD
            if (strcmp(tokens[0], "cd") == 0) {
                char* path;
                // TODO trim token for ~
                if (tokens[1] == NULL || is_empty(tokens[1]) || strcmp(tokens[1], "~") == 0)
                    path = home;
                else {
                    path = tokens[1];
                }

                if (chdir(path) != 0) {
                    fprintf(stderr, "error: cd: %s\n", strerror(errno));
                }

                free(tokens);
                free(buf_copy);
                continue;
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
    printf("%s", gen_prompt());
    return fgets(input_buf, MAX_INPUT_CHARS, stdin);
}

// TODO
/* marjela@stoneisland # ~/dev/tsh >> cd ../snake-sdl */
/* marjela@stoneisland # ~/dev/snake-sdl >> cd .. */
/* malloc(): invalid next size (unsorted) */
/* zsh: IOT instruction (core dumped)  ./tsh */

char* gen_prompt() {
    char* whoami = getlogin();

    char* hostname = malloc((HOST_NAME_MAX + 1) * sizeof(char));
    gethostname(hostname, HOST_NAME_MAX + 1);

    char* pwd = getcwd(NULL, 0);
    char* clean_pwd;

    // home shortening
    // TODO move home to hashtable
    size_t home_len = 6 + strlen(whoami);
    if (strncmp(home, pwd, home_len) == 0) {
        clean_pwd = malloc((2 + strlen(pwd) - home_len) * sizeof(char));
        strcpy(clean_pwd, "~");
        strcat(clean_pwd, pwd + home_len);
    } else {
        clean_pwd = pwd;
    }

    // TODO
    char* prompt_format = DEFAULT_PROMPT;

    // computing prompt size
    size_t prompt_size = 1;
    for (char* p = prompt_format; *p != '\0'; p++) {
        // skipping format literals
        if (*p == '%') {
            p += 2;
            continue;
        }
        prompt_size++;
    }
    prompt_size += strlen(whoami);
    prompt_size += strlen(hostname);
    prompt_size += strlen(clean_pwd);

    char* prompt = malloc(prompt_size * sizeof(char));
    sprintf(prompt, prompt_format, whoami, hostname, clean_pwd);

    return prompt;
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
