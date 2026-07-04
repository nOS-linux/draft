#ifndef NINIT_H
#define NINIT_H

#include <sys/types.h>

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

#endif /* NINIT_H */
