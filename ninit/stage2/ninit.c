#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ninit.h"

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

    if (pid < 0)
        return -1;

    if (pid == 0) {
        char *args[] = {(char *)path, NULL};
        execv(args[0], args);
        exit(EXIT_FAILURE);
    }
    return pid;
}

void run_services(const char *services_path) {
    pid_t pid = spawn_child("/bin/nparse -f kv -p  /etc/ninit/services/* kv");
    if (pid < 0)
        return;
    
}

int main(void) {
    if (getpid() != 1)
        return EXIT_FAILURE;
    setup_signals();

    spawn_child("/bin/sh"); // TODO: ntty

    while (1) {
        pause();
    }

    return EXIT_SUCCESS;
}
