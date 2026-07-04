#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../ninit.h"

void reap_zombies(void) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    }
}

void signal_handler(int sig) {
    switch (sig) {
        case SIGCHLD:
            reap_zombies();
            break;
        case SIGINT:
        case SIGTERM:
            break;
    }
}

void setup_signals(void) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);

    // SA_RESTART automatically restarts system calls interrupted by a signal
    // SA_NOCLDSTOP prevents SIGCHLD from being called when the child is simply
    // stopped (e.g., by Ctrl+Z)
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    sigaction(SIGCHLD, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

pid_t spawn_child(const char *path) {
    pid_t pid = fork();

    if (pid < 0) return -1;

    if (pid == 0) {
        char *args[] = {(char *)path, NULL};
        execv(args[0], args);
        exit(EXIT_FAILURE);
    }
    return pid;
}

int main(void) {
    printf("--- ninit stage 2 ---\n");
    if (getpid() != 1) return EXIT_FAILURE;
    setup_signals();

    spawn_child("/bin/sh"); // TODO: ntty

    DIR *dir = opendir("/etc/ninit/services");
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir))) {
            size_t len = strlen(ent->d_name);
            if (len < 5 || strcmp(ent->d_name + len - 4, ".nsv") != 0)
                continue;

            char path[296];
            snprintf(path, sizeof(path), "/etc/ninit/services/%s", ent->d_name);

            NService srv = parse_nsv_file(path);
            if (srv.name[0] == '\0')
                continue;

            if (strcmp(srv.autostart, "false") == 0) {
                printf("skipped %s\n", srv.name);
                continue;
            }

            printf("started %s\n", srv.name);
            run_service(&srv);
        }
        closedir(dir);
    }

    while (1) {
        pause();
    }

    return EXIT_SUCCESS;
}
