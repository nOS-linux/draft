#ifndef NINIT_H
#define NINIT_H

#include <sys/types.h>
#include <unistd.h>

#define MAX_SERVICES 64
#define FIFO_PATH "/run/ninit.ctl"

typedef struct {
    char name[64];
    char description[128];
    char exec[256];
    char restart[32];
    char timeout[32];
    char autostart[8];
    char requires[256];
} NService;

typedef struct {
    NService srv;
    char filepath[296];
    pid_t pid;
    int active;
    int autost;
} SvcEntry;

/* service table */
SvcEntry *find_service(const char *name);
int service_count(void);
SvcEntry *get_service(int i);
void scan_services(void);

/* service lifecycle */
void svc_start(const char *name);
void svc_stop(const char *name);
void svc_restart(const char *name);

/* autostart toggle */
void svc_enable(const char *name);
void svc_disable(const char *name);

/* shutdown */
void shutdown_all(void);
void do_poweroff(void);
void do_reboot(void);

/* main loop */
void check_restarts(void);
void process_fifo_buffer(const char *buf, size_t len);

/* nsv parser */
NService parse_nsv_file(const char *file_path);

/* process helpers */
pid_t spawn_child(const char *path);
pid_t spawn_argv(char **argv);

/* signal / getty */
void setup_signals(void);
void reap_zombies(void);
void spawn_getty(const char *tty, const char *shell);

#endif
