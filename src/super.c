#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include "config.h"
#include "super.h"

void super_fprintf(const char *format __attribute__((unused)), ...)
{
#ifdef SUPER_VERBOSE
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
#endif
}

#ifdef SUPER_UID_CHOICE
int str_to_id(const char *arg, uid_t *res)
{
    char *endptr;
    errno = 0;
    long id_long = strtol(arg, &endptr, 10);
    if (errno || endptr == arg || *endptr != '\0' || id_long < 0 || id_long > UINT_MAX) {
        super_fprintf("Cannot interpret argument as uid_t\n");
        return -1;
    }
    *res = (uid_t)id_long;
    if (*res < SUPER_MIN_UID) {
        super_fprintf("UID or GID must be greater or equal than %d\n", SUPER_MIN_UID);
        return -1;
    }
    return 0;
}
#endif

int early_chroot(int *argc_ptr, char **argv_ptr[])
{
    uid_t current_uid = getuid();
    char *chroot_dir = NULL;
    uid_t chroot_uid = 0;
    gid_t chroot_gid = 0;
    struct stat c_info;
    char *orig_cmd_env, *ptr, *write_ptr;
    static char *orig_cmd;
    static char *new_argv[SUPER_MAX_ARGS];
    int new_argc;

    if (current_uid != SUPER_SSH_UID) {
        super_fprintf("Current UID (%d) is not allowed to run rsync\n", current_uid);
        return SUPER_FAIL;
    }

    if (*argc_ptr <= SUPER_ARGC) {
        super_fprintf("Insufficient argument count\n");
        return SUPER_FAIL;
    }
    if (strcmp((*argv_ptr)[1], "--chroot-dir") != 0 ) {
        super_fprintf("Incorrect sequence of arguments\n");
        return SUPER_FAIL;
    }
#ifdef SUPER_UID_CHOICE
    if (strcmp((*argv_ptr)[3], "--chroot-uid") != 0 ||
            strcmp((*argv_ptr)[5], "--chroot-gid") != 0) {
        super_fprintf("Incorrect sequence of arguments\n");
        return SUPER_FAIL;
    }
#endif

    chroot_dir = strdup((*argv_ptr)[2]);
    if (!(stat(chroot_dir, &c_info) == 0 && (c_info.st_mode & S_IFDIR))) {
        super_fprintf("No such directory\n");
        return SUPER_FAIL;
    }

#ifdef SUPER_UID_CHOICE
    if (str_to_id((*argv_ptr)[4], &chroot_uid) != 0) {
        return SUPER_FAIL;
    }
    if (str_to_id((*argv_ptr)[6], &chroot_gid) != 0) {
        return SUPER_FAIL;
    }
#else
    chroot_uid = current_uid;
    chroot_gid = getgid();
#endif

    if (chdir(chroot_dir) != 0) {
        super_fprintf("chdir: %s\n", strerror(errno));
        return SUPER_FAIL;
    }

    if (chroot(chroot_dir) != 0) {
        super_fprintf("chroot: %s\n", strerror(errno));
        return SUPER_FAIL;
    }

    if (setgid(chroot_gid) != 0) {
        super_fprintf("setgid: %s\n", strerror(errno));
        return SUPER_FAIL;
    }

    if (setuid(chroot_uid) != 0) {
        super_fprintf("setuid: %s\n", strerror(errno));
        return SUPER_FAIL;
    }

    orig_cmd_env = getenv("SSH_ORIGINAL_COMMAND");
    if (orig_cmd_env != NULL) {
        // run by sshd
        orig_cmd = strdup(orig_cmd_env);
        new_argc = 0;
        ptr = orig_cmd_env;
        write_ptr = orig_cmd;

        // parse backslash-escaped command line
        while (*ptr != '\0') {
            while (*ptr == ' ') {
                ptr++;
            }
            if (*ptr == '\0') {
                break;
            }
            if (new_argc < SUPER_MAX_ARGS - 1) {
                new_argv[new_argc++] = write_ptr;
            } else {
                *write_ptr++ = '\0';
                break;
            }
            while (*ptr != ' ' && *ptr != '\0') {
                if (*ptr == '\\' && *(ptr + 1) != '\0') {
                    ptr++;  // skip the backslash
                }
                *write_ptr++ = *ptr++;
            }
            *write_ptr++ = '\0';
        }

        new_argv[new_argc] = NULL;
        *argv_ptr = new_argv;
        *argc_ptr = new_argc;

    } else {
        // not run by sshd
        *argc_ptr -= SUPER_ARGC;
        *argv_ptr = &((*argv_ptr)[SUPER_ARGC]);
        (*argv_ptr)[0] = (*argv_ptr)[-SUPER_ARGC];
    }

    return SUPER_OK;
}
