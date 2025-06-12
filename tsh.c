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

#define MAX_INPUT_CHARS 1024
#define ENV_ALLOWED_CHARSET "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"

// TODO make customizations via tshrc. file in $HOME $XDG_CONFIG_HOME

// TODO make u h d instead of s s s
#define DEF_PROMPT "%s@%s # %s >> "

char** parse_input(char* input);
int preprocess_input(char* buf, char** dst);

int parse_tokens(char* buf, char*** tokens);
int execute_cmd(const char* name, char** args);

char* ask_prompt(char* input_buf);
char* gen_prompt(void);

bool is_empty(const char* str);

int main() {
    char* input_buf = malloc(sizeof(char) * MAX_INPUT_CHARS);
    if (input_buf == NULL) {
        fprintf(stderr, "error: could not allocate memory for input buffer: %s\n", strerror(errno));
        return 1;
    }

    while (ask_prompt(input_buf) != NULL) {
        char* input_copy = strdup(input_buf);
        if (input_copy == NULL) {
            fprintf(stderr, "error: could not duplicate buffer string: %s\n", strerror(errno));
            return 1;
        }

        char** args = parse_input(input_copy);
        if (args == NULL) {
            free(input_copy);
            continue;
        }

        int status;

        if (strcmp(args[0], "exit") == 0) {
            free(args);
            free(input_copy);
            break;
        } else if (strcmp(args[0], "cd") == 0) {
            char* path;
            // TODO trim token for ~
            if (args[1] == NULL || is_empty(args[1]) || strcmp(args[1], "~") == 0)
                path = getenv("HOME");
            else {
                path = args[1];
            }

            if (chdir(path) != 0) {
                fprintf(stderr, "error: cd: %s\n", strerror(errno));
            }
        } else {
            status = execute_cmd(args[0], args);
        }

        free(input_copy);
        free(args);
    }

    free(input_buf);
    return 0;
}

// TODO maybe use custom errno or smth
char** parse_input(char* input) {
    char** tokens = NULL;

    if (is_empty(input)) {
        return NULL;
    }

    char* prepr_str = NULL;
    preprocess_input(input, &prepr_str);

    if (parse_tokens(prepr_str, &tokens) == -1) {
        free(input);
        return NULL;
    }

    return tokens;
}

typedef struct {
    size_t start;
    size_t end;
} var_loc;

int preprocess_input(char* buf, char** dst) {
    // cut off newline
    // buf[strlen(buf) - 1] = '\0';

    // TODO
    // also $VAR and ${VAR}123 parsing
    var_loc** locations = NULL;
    size_t loc_count = 0;
    char *var_ptr, *tmp_buf = buf;
    while ((var_ptr = strchr(tmp_buf, '$')) != NULL) {
        loc_count++;

        intptr_t var_start = var_ptr - buf;
        intptr_t var_end = var_start;

        for (char* ptr = var_ptr + 1; strchr(ENV_ALLOWED_CHARSET, *ptr) != NULL && *ptr != '\0'; ptr++) {
            var_end++;
        }

        var_loc* location = malloc(sizeof(var_loc));
        location->start = var_start;
        location->end = var_end;

        locations = reallocarray(locations, loc_count, sizeof(var_loc*));
        locations[loc_count - 1] = location;

        tmp_buf = var_ptr + 1;
    }

    printf("\n\nlocations: ");
    for (size_t i = 0; i < loc_count; i++) {
        var_loc loc = *(locations[i]);
        printf("[%zu, %zu], ", loc.start, loc.end);
    }
    printf("\n\n");

    *dst = strdup(buf);
    if (*dst == NULL) {
        fprintf(stderr, "error: could not duplicate buffer string: %s\n", strerror(errno));
        return 1;
    }

    // cut off newline
    (*dst)[strlen(*dst) - 1] = '\0';

    // TODO seg fault
    // substitute env vars on locations
    for (size_t i = loc_count - 1; i >= 0; i--) {
        var_loc loc = *(locations[i]);

        // get env var
        char* env_name = malloc((loc.end - loc.start + 1) * sizeof(char));
        if (env_name == NULL) {
            fprintf(stderr,
                    "error: could not allocate memory when preprocessing input for environment variable name: %s\n",
                    strerror(errno));
            return 1;
        }
        // strncpy(env_name, buf + loc.start + 1, loc.end - loc.start);
        // char* env_var = getenv(env_name);

        // if (env_var == NULL) {
        //     continue;
        // }

        // char* tmp_str = malloc((loc.start + strlen(env_var) + strlen(buf + loc.end + 2)) * sizeof(char));
        // if (tmp_str == NULL) {
        //     fprintf(stderr, "error: could not allocate memory when preprocessing input for tmp buffer: %s\n",
        //             strerror(errno));
        //     return 1;
        // }

        // // copy first part of string before $
        // for (size_t i = 0; i < loc.start; i++) {
        //     *tmp_str = buf[i];
        // }

        // // append env var to str
        // strcat(tmp_str, env_var);
        // // append the rest
        // strcat(tmp_str, buf + loc.end + 1);
    }

    return 0;
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

    // null terminate tokens array for execvp
    (*tokens) = reallocarray((*tokens), count + 1, sizeof(char*));
    if ((*tokens) == NULL) {
        fprintf(stderr, "error: could not reallocate memory for tokens array\n");
        return -1;
    }
    (*tokens)[count] = NULL;

    return 0;
}

char* ask_prompt(char* input_buf) {
    char* prompt = gen_prompt();
    printf("%s", prompt);
    free(prompt);
    return fgets(input_buf, MAX_INPUT_CHARS, stdin);
}

// TODO
/* user@host # ~/dev/tsh >> cd ../snake-sdl */
/* user@host # ~/dev/snake-sdl >> cd .. */
/* malloc(): invalid next size (unsorted) */
/* zsh: IOT instruction (core dumped)  ./tsh */

char* gen_prompt() {
    char* whoami = getenv("USER");
    char* hostname = getenv("HOSTNAME");

    char* pwd = getcwd(NULL, 0);
    char* clean_pwd;

    // home shortening
    char* home = getenv("HOME");
    size_t home_len = strlen(home);
    if (strncmp(home, pwd, home_len) == 0) {
        clean_pwd = malloc((2 + strlen(pwd) - home_len) * sizeof(char));
        strcpy(clean_pwd, "~");
        strcat(clean_pwd, pwd + home_len);

        free(pwd);
    } else {
        clean_pwd = pwd;
    }

    // TODO
    char* prompt_format = DEF_PROMPT;

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

    free(clean_pwd);

    return prompt;
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
