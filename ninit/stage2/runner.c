#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <linux/reboot.h>
#include <unistd.h>

#include "../ninit.h"

/* ----------------------------------------------------------------- */
/*  service table                                                     */
/* ----------------------------------------------------------------- */

static SvcEntry svc_table[MAX_SERVICES];
static int svc_count = 0;
static pid_t exited_pids[256];
static int exited_status[256];
static int exited_count = 0;

int service_count(void) {
    return svc_count;
}

SvcEntry *get_service(int i) {
    return (i >= 0 && i < svc_count) ? &svc_table[i] : NULL;
}

SvcEntry *find_service(const char *name) {
    for (int i = 0; i < svc_count; i++) {
        if (strcmp(svc_table[i].srv.name, name) == 0)
            return &svc_table[i];
    }
    return NULL;
}

/* ----------------------------------------------------------------- */
/*  exec parser                                                       */
/* ----------------------------------------------------------------- */

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

pid_t spawn_argv(char **argv) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execv(argv[0], argv);
        _exit(127);
    }
    return pid;
}

pid_t spawn_child(const char *path) {
    char *args[] = {(char *)path, NULL};
    return spawn_argv(args);
}

/* ----------------------------------------------------------------- */
/*  reap zombies — called from SIGCHLD handler                        */
/* ----------------------------------------------------------------- */

void reap_zombies(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (exited_count < 256) {
            exited_pids[exited_count] = pid;
            exited_status[exited_count] = status;
            exited_count++;
        }
    }
}

/* ----------------------------------------------------------------- */
/*  start / stop a single service                                     */
/* ----------------------------------------------------------------- */

static void do_start(SvcEntry *e) {
    if (!e || e->srv.exec[0] == '\0') return;

    /* resolve dependencies: start required services first */
    if (e->srv.requires[0] != '\0') {
        char req_copy[256];
        strncpy(req_copy, e->srv.requires, sizeof(req_copy) - 1);
        req_copy[sizeof(req_copy) - 1] = '\0';

        char *tok = strtok(req_copy, " ");
        while (tok) {
            SvcEntry *dep = find_service(tok);
            if (dep && (!dep->active || dep->pid <= 0)) {
                printf("  %s requires %s — starting\n", e->srv.name, tok);
                do_start(dep);
            }
            tok = strtok(NULL, " ");
        }
    }

    char exec_copy[1024];
    strncpy(exec_copy, e->srv.exec, sizeof(exec_copy) - 1);
    exec_copy[sizeof(exec_copy) - 1] = '\0';

    char *argv[128];
    int argc = parse_exec_to_argv(exec_copy, argv, 128);
    if (argc == 0 || !argv[0]) return;

    pid_t pid = fork();
    if (pid < 0) return;

    if (pid == 0) {
        /* redirect stdout/stderr to log file */
        mkdir("/var/log", 0755);
        char logpath[296];
        snprintf(logpath, sizeof(logpath), "/var/log/%s.log", e->srv.name);
        int fd = open(logpath, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd >= 0) {
            dup2(fd, 1);
            dup2(fd, 2);
            if (fd > 2) close(fd);
        }
        execv(argv[0], argv);
        _exit(127);
    }

    if (pid > 0) {
        e->pid = pid;
        e->active = 1;
        printf("  started %s (pid %d)\n", e->srv.name, pid);
    }
}

static void do_stop(SvcEntry *e) {
    if (!e || e->pid <= 0) return;
    kill(e->pid, SIGTERM);
    printf("  stopped %s (pid %d)\n", e->srv.name, e->pid);
    e->active = 0;
    e->pid = 0;
}

void svc_start(const char *name) {
    SvcEntry *e = find_service(name);
    if (!e) { printf("  service '%s' not found\n", name); return; }
    if (e->active && e->pid > 0) { printf("  %s already running\n", name); return; }
    do_start(e);
}

void svc_stop(const char *name) {
    SvcEntry *e = find_service(name);
    if (!e) { printf("  service '%s' not found\n", name); return; }
    do_stop(e);
}

void svc_restart(const char *name) {
    SvcEntry *e = find_service(name);
    if (!e) { printf("  service '%s' not found\n", name); return; }
    if (e->active && e->pid > 0)
        do_stop(e);
    do_start(e);
}

/* ----------------------------------------------------------------- */
/*  enable / disable (modify .nsv file on disk)                       */
/* ----------------------------------------------------------------- */

static void set_autostart(const char *name, const char *value) {
    char path[296];
    snprintf(path, sizeof(path), "/etc/ninit/services/%s.nsv", name);

    FILE *f = fopen(path, "r");
    if (!f) { printf("  %s: no such service\n", name); return; }

    char tmp[1280];
    size_t n = fread(tmp, 1, sizeof(tmp) - 1, f);
    tmp[n] = '\0';
    fclose(f);

    char newval[256];
    snprintf(newval, sizeof(newval), "autostart = \"%s\"", value);

    char out[1280];
    out[0] = '\0';
    int replaced = 0;

    char *line = tmp;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        const char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (strncmp(p, "autostart", 9) == 0) {
            strcat(out, newval);
            strcat(out, "\n");
            replaced = 1;
        } else {
            strcat(out, line);
            strcat(out, "\n");
        }
        if (nl) line = nl + 1;
        else break;
    }

    if (!replaced) {
        strcat(out, newval);
        strcat(out, "\n");
    }

    f = fopen(path, "w");
    if (f) { fputs(out, f); fclose(f); }

    SvcEntry *e = find_service(name);
    if (e) {
        strncpy(e->srv.autostart, value, sizeof(e->srv.autostart) - 1);
        e->autost = (strcmp(value, "true") == 0);
    }

    printf("  %s autostart set to %s\n", name, value);
}

void svc_enable(const char *name)  { set_autostart(name, "true"); }
void svc_disable(const char *name) { set_autostart(name, "false"); }

/* ----------------------------------------------------------------- */
/*  graceful shutdown                                                 */
/* ----------------------------------------------------------------- */

void shutdown_all(void) {
    printf("stopping services\n");
    for (int i = svc_count - 1; i >= 0; i--) {
        SvcEntry *e = &svc_table[i];
        if (e->active && e->pid > 0) {
            printf("  stopping %s\n", e->srv.name);
            kill(e->pid, SIGTERM);
        }
    }

    /* wait up to 2 s for graceful stop */
    for (int wait = 0; wait < 20; wait++) {
        int any = 0;
        for (int i = 0; i < svc_count; i++) {
            if (svc_table[i].active && svc_table[i].pid > 0) any = 1;
        }
        if (!any) break;
        usleep(100000);
        reap_zombies();
    }

    /* force kill remaining */
    for (int i = 0; i < svc_count; i++) {
        SvcEntry *e = &svc_table[i];
        if (e->pid > 0) {
            printf("  killing %s\n", e->srv.name);
            kill(e->pid, SIGKILL);
            e->active = 0;
            e->pid = 0;
        }
    }
}

void do_poweroff(void) {
    shutdown_all();
    printf("poweroff\n");
    sync();
    reboot(LINUX_REBOOT_CMD_POWER_OFF);
    _exit(0);
}

void do_reboot(void) {
    shutdown_all();
    printf("reboot\n");
    sync();
    reboot(LINUX_REBOOT_CMD_RESTART);
    _exit(0);
}

/* ----------------------------------------------------------------- */
/*  restart logic (called from main loop)                             */
/* ----------------------------------------------------------------- */

void check_restarts(void) {
    for (int i = 0; i < exited_count; i++) {
        pid_t pid = exited_pids[i];
        int status = exited_status[i];

        for (int j = 0; j < svc_count; j++) {
            if (svc_table[j].pid == pid) {
                svc_table[j].pid = 0;

                if (!svc_table[j].active)
                    continue;

                const char *restart = svc_table[j].srv.restart;

                if (restart[0] == '\0' || strcmp(restart, "never") == 0) {
                    svc_table[j].active = 0;
                    printf("  %s exited, restart disabled\n", svc_table[j].srv.name);
                    continue;
                }

                /* timeout before restart */
                const char *timeout_str = svc_table[j].srv.timeout;
                int secs = 0;
                if (timeout_str[0] != '\0')
                    secs = atoi(timeout_str);
                if (secs > 0)
                    sleep(secs);

                if (strcmp(restart, "always") == 0) {
                    printf("  restarting %s\n", svc_table[j].srv.name);
                    do_start(&svc_table[j]);
                } else if (strcmp(restart, "on-failure") == 0) {
                    int failed = 0;
                    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
                        failed = 1;
                    else if (WIFSIGNALED(status))
                        failed = 1;

                    if (failed) {
                        printf("  %s failed, restarting\n", svc_table[j].srv.name);
                        do_start(&svc_table[j]);
                    } else {
                        svc_table[j].active = 0;
                        printf("  %s exited cleanly\n", svc_table[j].srv.name);
                    }
                }
            }
        }
    }
    exited_count = 0;
}

/* ----------------------------------------------------------------- */
/*  FIFO command processing                                           */
/* ----------------------------------------------------------------- */

void process_fifo_buffer(const char *buf, size_t len) {
    char line[512];
    size_t pos = 0;

    while (pos < len) {
        size_t start = pos;
        while (pos < len && buf[pos] != '\n') pos++;
        size_t llen = pos - start;
        if (llen == 0) { pos++; continue; }

        if (llen >= sizeof(line)) llen = sizeof(line) - 1;
        memcpy(line, buf + start, llen);
        line[llen] = '\0';

        char *space = strchr(line, ' ');
        if (!space) {
            /* single-word command: poweroff / reboot */
            if      (strcmp(line, "poweroff") == 0) do_poweroff();
            else if (strcmp(line, "reboot")   == 0) do_reboot();
            pos++;
            continue;
        }

        *space = '\0';
        char *cmd  = line;
        char *name = space + 1;

        if      (strcmp(cmd, "up")      == 0) svc_start(name);
        else if (strcmp(cmd, "down")    == 0) svc_stop(name);
        else if (strcmp(cmd, "restart") == 0) svc_restart(name);
        else if (strcmp(cmd, "enable")  == 0) svc_enable(name);
        else if (strcmp(cmd, "disable") == 0) svc_disable(name);

        if (pos < len && buf[pos] == '\n') pos++;
    }
}

/* ----------------------------------------------------------------- */
/*  populate service table from /etc/ninit/services/*.nsv             */
/* ----------------------------------------------------------------- */

void scan_services(void) {
    DIR *dir = opendir("/etc/ninit/services");
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) && svc_count < MAX_SERVICES) {
        size_t len = strlen(ent->d_name);
        if (len < 5 || strcmp(ent->d_name + len - 4, ".nsv") != 0)
            continue;

        SvcEntry *e = &svc_table[svc_count];
        snprintf(e->filepath, sizeof(e->filepath),
                 "/etc/ninit/services/%s", ent->d_name);

        e->srv = parse_nsv_file(e->filepath);
        if (e->srv.name[0] == '\0')
            continue;

        e->pid = 0;
        e->autost = (strcmp(e->srv.autostart, "false") != 0);
        svc_count++;
    }
    closedir(dir);
}
