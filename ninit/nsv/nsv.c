#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include "../ninit.h"

static int send_line(const char *fmt, ...) {
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    int fd = open(FIFO_PATH, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "nsv: ninit not running (%s)\n", FIFO_PATH);
        return 1;
    }
    write(fd, msg, strlen(msg));
    close(fd);
    return 0;
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <command> [<service>]\n", prog);
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  up <svc>     start service\n");
    fprintf(stderr, "  down <svc>   stop service\n");
    fprintf(stderr, "  restart <svc> restart service\n");
    fprintf(stderr, "  enable <svc>  enable autostart\n");
    fprintf(stderr, "  disable <svc> disable autostart\n");
    fprintf(stderr, "  poweroff      shut down the system\n");
    fprintf(stderr, "  reboot        reboot the system\n");
}

int main(int argc, char *argv[]) {
    const char *prog = argv[0];
    const char *base = strrchr(prog, '/');
    if (base) prog = base + 1;

    /* detect poweroff/reboot from argv[0] */
    if (strcmp(prog, "poweroff") == 0)
        return send_line("poweroff\n");
    if (strcmp(prog, "reboot") == 0)
        return send_line("reboot\n");

    if (argc < 2) {
        usage(prog);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "poweroff") == 0 || strcmp(cmd, "reboot") == 0)
        return send_line("%s\n", cmd);

    if (argc != 3) {
        usage(prog);
        return 1;
    }

    const char *name = argv[2];

    if (strcmp(cmd, "enable") == 0 || strcmp(cmd, "disable") == 0) {
        char path[296];
        snprintf(path, sizeof(path), "/etc/ninit/services/%s.nsv", name);

        FILE *f = fopen(path, "r");
        if (!f) {
            fprintf(stderr, "nsv: %s — no such service\n", name);
            return 1;
        }

        char buf[1280];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        fclose(f);

        const char *newval = (strcmp(cmd, "enable") == 0) ? "true" : "false";
        char replace[96];
        snprintf(replace, sizeof(replace), "autostart = \"%s\"", newval);

        char out[1280];
        out[0] = '\0';
        int replaced = 0;
        char *line = buf;
        char *nl;
        while (line && *line) {
            nl = strchr(line, '\n');
            if (nl) *nl = '\0';

            const char *p = line;
            while (isspace((unsigned char)*p)) p++;
            if (strncmp(p, "autostart", 9) == 0) {
                strcat(out, replace);
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
            strcat(out, replace);
            strcat(out, "\n");
        }

        f = fopen(path, "w");
        if (f) { fputs(out, f); fclose(f); }

        send_line("%s %s\n", cmd, name);
    } else if (strcmp(cmd, "up")      == 0 ||
               strcmp(cmd, "down")    == 0 ||
               strcmp(cmd, "restart") == 0) {
        return send_line("%s %s\n", cmd, name);
    } else {
        fprintf(stderr, "nsv: unknown command '%s'\n", cmd);
        usage(prog);
        return 1;
    }

    return 0;
}
