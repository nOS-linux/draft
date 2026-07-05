#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ninit.h"

const char *valid_keys[] = {
    "name", "description", "exec", "restart", "timeout", "autostart", "requires"
};

#define KEYS_COUNT (sizeof(valid_keys) / sizeof(valid_keys[0]))

static void trim_and_clean(char *dest, const char *src, size_t max_len) {
    while (isspace((unsigned char)*src))
        src++;
    if (*src == '"')
        src++;

    size_t len = strlen(src);
    while (len > 0 && (isspace((unsigned char)src[len - 1]) || src[len - 1] == '"')) {
        len--;
    }

    if (len >= max_len)
        len = max_len - 1;
    strncpy(dest, src, len);
    dest[len] = '\0';
}

NService parse_nsv_file(const char *file_path) {
    NService srv;
    memset(&srv, 0, sizeof(NService));
    strcpy(srv.autostart, "true");

    FILE *file = fopen(file_path, "r");
    if (!file)
        return srv;

    char line[1280];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;

        char *ptr = line;
        while (isspace((unsigned char)*ptr))
            ptr++;
        if (*ptr == '\0' || *ptr == '#')
            continue;

        char *eq = strchr(ptr, '=');
        if (!eq) {
            fclose(file);
            memset(&srv, 0, sizeof(NService));
            return srv;
        }

        *eq = '\0';
        char key[64];
        trim_and_clean(key, ptr, sizeof(key));
        char *raw_val = eq + 1;

        int is_valid_key = 0;
        for (size_t i = 0; i < KEYS_COUNT; i++) {
            if (strcmp(key, valid_keys[i]) == 0) {
                is_valid_key = 1;
                break;
            }
        }
        if (!is_valid_key) {
            fclose(file);
            memset(&srv, 0, sizeof(NService));
            return srv;
        }

        if      (strcmp(key, "name")        == 0) trim_and_clean(srv.name,        raw_val, sizeof(srv.name));
        else if (strcmp(key, "description") == 0) trim_and_clean(srv.description, raw_val, sizeof(srv.description));
        else if (strcmp(key, "exec")        == 0) trim_and_clean(srv.exec,        raw_val, sizeof(srv.exec));
        else if (strcmp(key, "restart")     == 0) trim_and_clean(srv.restart,     raw_val, sizeof(srv.restart));
        else if (strcmp(key, "timeout")     == 0) trim_and_clean(srv.timeout,     raw_val, sizeof(srv.timeout));
        else if (strcmp(key, "autostart")   == 0) trim_and_clean(srv.autostart,   raw_val, sizeof(srv.autostart));
        else if (strcmp(key, "requires")    == 0) trim_and_clean(srv.requires,    raw_val, sizeof(srv.requires));
    }
    fclose(file);

    if (srv.name[0] == '\0' || srv.exec[0] == '\0') {
        memset(&srv, 0, sizeof(NService));
    }

    return srv;
}
