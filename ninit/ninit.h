#ifndef NINIT_H
#define NINIT_H

#include <sys/types.h>

typedef struct {
    char name[64];
    char description[128];
    char exec[256];
    char restart[32];
    char timeout[32];
} NService;

/**
 * @brief Setting up signal handlers (SIGCHLD, SIGINT, SIGTERM).
 * Blocks external attempts to kill PID 1 and prepares the system for zombie
 * collection.
 */
void setup_signals(void);

/**
 * @brief Asynchronously collects all accumulated zombie processes (orphans).
 * Called within the SIGCHLD handler.
 */
void reap_zombies(void);

/**
 * @brief Spawning the main child process (bootstrap / daemon).
 * @param path Path to the executable file.
 * @return pid_t PID of the spawned process, or -1 on error.
 */
pid_t spawn_child(const char *path);

/**
 * @brief Parses an nsv file and populates the NService structure.
 * @param file_path Path to the nsv file.
 * @return NService The populated NService structure, or an empty structure on error.
 */
NService parse_nsv_file(const char *file_path);


/**
 * @brief Runs a service based on the parsed nsv file.
 * @param srv The NService structure to run.
 */
void run_service(const NService *srv);

#endif /* NINIT_H */
