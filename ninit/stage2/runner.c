#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "../ninit.h"

static int parse_exec_to_argv(char *exec_str, char **argv, int max_args) {
    int argc = 0;
    char *ptr = exec_str;
    char quote = 0;
    char *arg_start = NULL;

    while (*ptr && argc < max_args - 1) {
        if (quote) {
            if (*ptr == quote) {
                *ptr = '\0';
                argv[argc++] = arg_start;
                arg_start = NULL;
                quote = 0;
            }
            ptr++;
            continue;
        }

        if (*ptr == '"' || *ptr == '\'') {
            quote = *ptr;
            if (!arg_start)
                arg_start = ptr + 1;
            ptr++;
            continue;
        }

        if (*ptr == ' ' || *ptr == '\t') {
            if (arg_start) {
                *ptr = '\0';
                argv[argc++] = arg_start;
                arg_start = NULL;
            }
        } else {
            if (!arg_start)
                arg_start = ptr;
        }
        ptr++;
    }

    if (arg_start && argc < max_args - 1)
        argv[argc++] = arg_start;

    argv[argc] = NULL;
    return argc;
}

void run_service(const NService *srv) {
    if (srv->name[0] == '\0') return;

    char exec_copy[1024];
    strncpy(exec_copy, srv->exec, sizeof(exec_copy) - 1);
    exec_copy[sizeof(exec_copy) - 1] = '\0';

    char *argv[128];
    int argc = parse_exec_to_argv(exec_copy, argv, 128);

    if (argc == 0 || argv[0] == NULL) return;

    pid_t pid = fork();
    if (pid < 0) return;

    if (pid == 0) {
        execv(argv[0], argv);
        exit(1);
    }
    // TODO: service control
}
