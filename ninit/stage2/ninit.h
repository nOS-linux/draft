#ifndef NINIT_H
#define NINIT_H

#include <sys/types.h>

#define NINIT_VERSION "0.1.0"

#define PATH_PROC   "/proc"
#define PATH_SYS    "/sys"
#define PATH_RUN    "/run"


/* --- System functions --- */

/**
 * @brief Setting up signal handlers (SIGCHLD, SIGINT, SIGTERM).
 * Blocks external attempts to kill PID 1 and prepares the system for zombie collection.
 */
void setup_signals(void);

/**
 * @brief Asynchronously collects all accumulated zombie processes (orphans).
 * Called within the SIGCHLD handler.
 */
void reap_zombies(void);

/**
 * @brief Early mounting of critical file system points (procfs, sysfs, tmpfs).
 * Puts the root file system into Read-Write mode.
 * @return 0 on success, -1 on error.
 */
int mount_early_fs(void);

/**
 * @brief Spawning the main child process (bootstrap / daemon).
 * @param path Path to the executable file.
 * @return pid_t PID of the spawned process, or -1 on error.
 */
pid_t spawn_child(const char *path);

/**
 * @brief Runs all services defined in the specified directory.
 * @param services_path Path to the directory containing service definitions.
 */
void run_services(const char *services_path);

#endif /* NINIT_H */
