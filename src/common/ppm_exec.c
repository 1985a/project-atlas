#include <stdarg.h>
#include <stdio.h>

#include "atlas.h"
#include "ppm_exec.h"

#define PPM_EXEC_MAX_ARGS   16
#define PPM_EXEC_TIMEOUT_MS 10000

int ppm_exec(const char *program, ...)
{
    const char *argv[PPM_EXEC_MAX_ARGS + 1];
    va_list ap;
    int argc = 0, i;

    argv[argc++] = program;
    va_start(ap, program);
    while (argc < PPM_EXEC_MAX_ARGS) {
        const char *arg = va_arg(ap, const char *);
        if (!arg)
            break;
        argv[argc++] = arg;
    }
    va_end(ap);
    argv[argc] = NULL;

    printf("[EXEC]");
    for (i = 0; i < argc; i++)
        printf(" %s", argv[i]);
    putchar('\n');
    fflush(stdout);

    return atlas_run(argv, NULL, PPM_EXEC_TIMEOUT_MS, ATLAS_RUN_SHOW_STDERR);
}
