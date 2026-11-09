#ifndef TEST_FIXTURE_H
#define TEST_FIXTURE_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Árbol temporal para simular /proc, /sys y la Hardware DB. */

static char fx_root[256];

static __attribute__((unused)) void fx_mkdirs(const char *rel)
{
    char p[1024], *s;
    snprintf(p, sizeof(p), "%s/%s", fx_root, rel);
    for (s = p + 1; *s; s++)
        if (*s == '/') {
            *s = '\0';
            mkdir(p, 0755);
            *s = '/';
        }
    mkdir(p, 0755);
}

static __attribute__((unused)) void fx_write(const char *rel, const char *text)
{
    char p[1024], dir[1024], *slash;
    FILE *f;
    snprintf(p, sizeof(p), "%s/%s", fx_root, rel);
    snprintf(dir, sizeof(dir), "%s", rel);
    slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        fx_mkdirs(dir);
    }
    f = fopen(p, "w");
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static __attribute__((unused)) void fx_symlink(const char *target, const char *rel)
{
    char p[1024], dir[1024], *slash;
    snprintf(p, sizeof(p), "%s/%s", fx_root, rel);
    snprintf(dir, sizeof(dir), "%s", rel);
    slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        fx_mkdirs(dir);
    }
    if (symlink(target, p) != 0)
        perror("symlink");
}

static __attribute__((unused)) void fx_path(char *out, size_t n, const char *rel)
{
    snprintf(out, n, "%s/%s", fx_root, rel);
}

static __attribute__((unused)) void fx_setup(void)
{
    snprintf(fx_root, sizeof(fx_root), "/tmp/atlas-test-XXXXXX");
    if (!mkdtemp(fx_root)) {
        perror("mkdtemp");
        exit(2);
    }
}

static __attribute__((unused)) void fx_teardown(void)
{
    char cmd[300];
    /* solo borra nuestro árbol temporal */
    if (strncmp(fx_root, "/tmp/atlas-test-", 16) == 0) {
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", fx_root);
        if (system(cmd) != 0)
            fprintf(stderr, "no pude borrar %s\n", fx_root);
    }
}

#endif
