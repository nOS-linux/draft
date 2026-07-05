#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../ninit.h"

/* ----------------------------------------------------------------- */
/*  signal handling                                                   */
/* ----------------------------------------------------------------- */

static volatile int sigchld_received = 0;
static volatile int shutdown_requested = 0;
static volatile int saved_sig = 0;

void signal_handler(int sig) {
    switch (sig) {
        case SIGCHLD:
            reap_zombies();
            sigchld_received = 1;
            break;
        case SIGINT:
        case SIGTERM:
            shutdown_requested = 1;
            saved_sig = sig;
            break;
    }
}

void setup_signals(void) {
    struct sigaction sa;
    struct sigaction sa_norestart;
    memset(&sa, 0, sizeof(sa));
    memset(&sa_norestart, 0, sizeof(sa_norestart));

    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    sa_norestart.sa_handler = signal_handler;
    sigemptyset(&sa_norestart.sa_mask);
    sa_norestart.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGINT, &sa_norestart, NULL);
    sigaction(SIGTERM, &sa_norestart, NULL);
}

/* ----------------------------------------------------------------- */
/*  getty spawning                                                    */
/* ----------------------------------------------------------------- */

void spawn_getty(const char *tty, const char *shell) {
    pid_t pid = fork();
    if (pid < 0) return;

    if (pid == 0) {
        setsid();
        int fd = open(tty, O_RDWR);
        if (fd < 0) _exit(1);
        ioctl(fd, TIOCSCTTY, 0);
        dup2(fd, 0); dup2(fd, 1); dup2(fd, 2);
        if (fd > 2) close(fd);
        char *args[] = {(char *)shell, NULL};
        execv(args[0], args);
        _exit(1);
    }
}

/* ----------------------------------------------------------------- */
/*  main                                                              */
/* ----------------------------------------------------------------- */

int main(void) {
    printf("--- ninit stage 2 ---\n");
    if (getpid() != 1) return 1;

    setup_signals();

    printf("scanning services\n");
    scan_services();

    for (int i = 0; i < service_count(); i++) {
        SvcEntry *e = get_service(i);
        if (e->autost) {
            printf("  starting %s\n", e->srv.name);
            svc_start(e->srv.name);
        }
    }

    printf("spawning getty\n");
    spawn_getty("/dev/tty1", "/bin/sh");
    spawn_getty("/dev/ttyS0", "/bin/sh");

    mkfifo(FIFO_PATH, 0666);
    int fifo_fd = open(FIFO_PATH, O_RDWR | O_NONBLOCK);
    if (fifo_fd >= 0)
        fcntl(fifo_fd, F_SETFD, FD_CLOEXEC);

    printf("--- ninit ready ---\n");

    for (;;) {
        if (shutdown_requested) {
            printf("received signal %d\n", saved_sig);
            do_poweroff();
        }

        struct timeval tv = { 0, 500000 };
        fd_set fds;
        FD_ZERO(&fds);
        if (fifo_fd >= 0)
            FD_SET(fifo_fd, &fds);

        int nfds = (fifo_fd >= 0) ? fifo_fd + 1 : 0;
        select(nfds, (fifo_fd >= 0) ? &fds : NULL, NULL, NULL, &tv);

        if (fifo_fd >= 0 && FD_ISSET(fifo_fd, &fds)) {
            char buf[4096];
            ssize_t n = read(fifo_fd, buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
                process_fifo_buffer(buf, (size_t)n);
            }
        }

        if (sigchld_received) {
            check_restarts();
            sigchld_received = 0;
        }
    }

    return 0;
}
