#ifndef SUPER_INC
#define SUPER_INC

#ifdef  SUPER_UID_CHOICE
#define SUPER_ARGC         6
#define SUPER_MIN_UID      1000
#else
#define SUPER_ARGC         2
#endif
#define SUPER_MAX_ARGS     1000
#define SUPER_OK           0
#define SUPER_FAIL         60

int early_chroot(int *argc, char **argv[]);

#endif
