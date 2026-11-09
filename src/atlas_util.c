#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "atlas.h"

int atlas_json_mode = 0;
int atlas_quiet = 0;

/* ================================================================ JSON */

void atlas_json_print(JsonBuilder *b)
{
    JsonGenerator *g = json_generator_new();
    JsonNode *root = json_builder_get_root(b);
    gchar *s;

    json_generator_set_pretty(g, TRUE);
    json_generator_set_root(g, root);
    s = json_generator_to_data(g, NULL);
    puts(s);
    fflush(stdout);
    g_free(s);
    json_node_unref(root);
    g_object_unref(g);
    g_object_unref(b);
}

void atlas_json_kv_str(JsonBuilder *b, const char *k, const char *v)
{
    json_builder_set_member_name(b, k);
    if (v)
        json_builder_add_string_value(b, v);
    else
        json_builder_add_null_value(b);
}

void atlas_json_kv_int(JsonBuilder *b, const char *k, long long v)
{
    json_builder_set_member_name(b, k);
    json_builder_add_int_value(b, v);
}

void atlas_json_kv_bool(JsonBuilder *b, const char *k, int v)
{
    json_builder_set_member_name(b, k);
    json_builder_add_boolean_value(b, v ? TRUE : FALSE);
}

void atlas_json_kv_double(JsonBuilder *b, const char *k, double v)
{
    json_builder_set_member_name(b, k);
    json_builder_add_double_value(b, v);
}

/* ============================================================ procesos */

long long atlas_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void atlas_msleep(int ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
        ;
}

void atlas_timestamp(char *buf, size_t n, int compact)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(buf, n, compact ? "%Y%m%d-%H%M%S" : "%Y-%m-%dT%H:%M:%S%z", &tm);
}

/*
 * En el proceso hijo, antes de exec: entrada y salidas estándar.
 * Lo que no se captura va a /dev/null; si ni eso se puede abrir, se deja
 * el descriptor heredado (mejor que dup2 de un -1).
 */
static void child_setup_stdio(int in_fd, int out_fd, int flags)
{
    int devnull = open("/dev/null", O_RDWR);
    int quiet = devnull >= 0 ? devnull : -1;
    int err_fd = -1;

    if (in_fd >= 0)
        dup2(in_fd, STDIN_FILENO);
    else if (quiet >= 0)
        dup2(quiet, STDIN_FILENO);

    if (out_fd >= 0)
        dup2(out_fd, STDOUT_FILENO);
    else if (quiet >= 0)
        dup2(quiet, STDOUT_FILENO);

    if (flags & ATLAS_RUN_MERGE_STDERR)
        err_fd = out_fd >= 0 ? out_fd : quiet;
    else if (!(flags & ATLAS_RUN_SHOW_STDERR))
        err_fd = quiet;
    if (err_fd >= 0)
        dup2(err_fd, STDERR_FILENO);

    if (devnull > STDERR_FILENO)
        close(devnull);
}

int atlas_run(const char *const argv[], char **out, int timeout_ms, int flags)
{
    int pipefd[2] = { -1, -1 };
    pid_t pid;
    char *buf = NULL;
    size_t len = 0, cap = 0;
    long long deadline;
    int status = 0, timed_out = 0, out_of_memory = 0;

    if (!argv || !argv[0])
        return -1;

    if (out)
        *out = NULL;

    if (pipe2(pipefd, O_CLOEXEC) != 0)
        return -1;

    pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        child_setup_stdio(-1, pipefd[1], flags);
        setpgid(0, 0);
        /* execvp no modifica argv; su prototipo es anterior a const */
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    /* también desde el padre: así kill(-pid) funciona aunque el timeout
     * llegue antes de que el hijo haya ejecutado su setpgid */
    setpgid(pid, pid);

    close(pipefd[1]);
    deadline = timeout_ms > 0 ? atlas_now_ms() + timeout_ms : 0;

    for (;;) {
        struct pollfd pfd = { pipefd[0], POLLIN, 0 };
        int wait_ms = -1, r;
        char chunk[4096];
        ssize_t got;

        if (deadline) {
            long long left = deadline - atlas_now_ms();
            if (left <= 0) {
                timed_out = 1;
                break;
            }
            wait_ms = (int)left;
        }

        r = poll(&pfd, 1, wait_ms);
        if (r < 0 && errno == EINTR)
            continue;
        if (r == 0) {
            timed_out = 1;
            break;
        }

        got = read(pipefd[0], chunk, sizeof(chunk));
        if (got < 0 && errno == EINTR)
            continue;
        if (got <= 0)
            break;

        if (out) {
            if (len + (size_t)got + 1 > cap) {
                size_t ncap = cap ? cap * 2 : 8192;
                char *nb;
                while (ncap < len + (size_t)got + 1)
                    ncap *= 2;
                nb = realloc(buf, ncap);
                if (!nb) {
                    out_of_memory = 1;   /* no devolver una salida cortada como buena */
                    break;
                }
                buf = nb;
                cap = ncap;
            }
            memcpy(buf + len, chunk, (size_t)got);
            len += (size_t)got;
        }
    }

    close(pipefd[0]);

    if (timed_out || out_of_memory) {
        kill(-pid, SIGTERM);
        atlas_msleep(100);
        kill(-pid, SIGKILL);
    }

    while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
        ;

    if (out) {
        if (!buf)
            buf = calloc(1, 1);
        else
            buf[len] = '\0';
        *out = buf;
    } else {
        free(buf);
    }

    if (timed_out)
        return 124;
    if (out_of_memory)
        return -1;
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}

int atlas_runl(char **out, int timeout_ms, const char *arg0, ...)
{
    const char *argv[64];
    va_list ap;
    int n = 0;

    argv[n++] = arg0;
    va_start(ap, arg0);
    while (n < 63) {
        const char *a = va_arg(ap, const char *);
        if (!a)
            break;
        argv[n++] = a;
    }
    va_end(ap);
    argv[n] = NULL;

    return atlas_run(argv, out, timeout_ms, 0);
}

int atlas_spawn(const char *const argv[], int fd_in, int *pid_out)
{
    pid_t pid = fork();

    if (pid < 0)
        return -1;

    if (pid == 0) {
        child_setup_stdio(fd_in, -1, 0);
        setpgid(0, 0);
        /* execvp no modifica argv; su prototipo es anterior a const */
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    setpgid(pid, pid);   /* ver atlas_run */

    *pid_out = pid;
    return 0;
}

int atlas_spawn_wait(int pid, int timeout_ms)
{
    long long deadline = atlas_now_ms() + timeout_ms;
    int status;

    for (;;) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid)
            return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        if (r < 0)
            return -1;
        if (atlas_now_ms() >= deadline)
            return 124;
        atlas_msleep(20);
    }
}

void atlas_spawn_kill(int pid)
{
    int status;

    if (pid <= 0)
        return;
    kill(-pid, SIGTERM);
    kill(pid, SIGTERM);
    if (atlas_spawn_wait(pid, 1000) == 124) {
        kill(-pid, SIGKILL);
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
}

int atlas_have_cmd(const char *cmd)
{
    const char *path = getenv("PATH");
    char buf[PATH_MAX];
    const char *p;

    if (!cmd || !*cmd)
        return 0;
    if (strchr(cmd, '/'))
        return access(cmd, X_OK) == 0;
    if (!path)
        path = "/usr/bin:/bin";

    for (p = path; *p;) {
        const char *e = strchr(p, ':');
        size_t l = e ? (size_t)(e - p) : strlen(p);
        if (l > 0 && l + strlen(cmd) + 2 < sizeof(buf)) {
            memcpy(buf, p, l);
            buf[l] = '/';
            strcpy(buf + l + 1, cmd);
            if (access(buf, X_OK) == 0)
                return 1;
        }
        if (!e)
            break;
        p = e + 1;
    }
    return 0;
}

/* =============================================================== rutas */

int atlas_path_join(char *dst, size_t n, const char *a, const char *b)
{
    int r = snprintf(dst, n, "%s/%s", a, b);
    return (r < 0 || (size_t)r >= n) ? -1 : 0;
}

int atlas_file_exists(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int atlas_dir_exists(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int atlas_mkdirs(const char *path)
{
    char tmp[PATH_MAX];
    size_t l;
    char *p;

    if (!path || (l = strlen(path)) == 0 || l >= sizeof(tmp))
        return -1;
    memcpy(tmp, path, l + 1);

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

/*
 * Lee un archivo entero (como texto terminado en '\0'), como mucho `max`
 * bytes. Reserva solo lo que ocupa: los archivos de /proc y /sys dicen
 * tamaño 0, así que se lee por bloques hasta el final. Si el archivo no
 * cabe se avisa por stderr, porque un JSON cortado parecería corrupto.
 */
char *atlas_read_file(const char *path, size_t max)
{
    FILE *f = fopen(path, "rb");
    struct stat st;
    size_t len = 0, cap;
    char *buf;

    if (!f)
        return NULL;
    cap = (fstat(fileno(f), &st) == 0 && st.st_size > 0) ? (size_t)st.st_size : 4096;
    if (cap > max)
        cap = max;
    buf = malloc(cap + 1);
    while (buf) {
        size_t got = fread(buf + len, 1, cap - len, f);
        len += got;
        if (got == 0 || len == max)
            break;
        if (len == cap) {
            size_t ncap = cap * 2 > max ? max : cap * 2;
            char *nb = realloc(buf, ncap + 1);
            if (!nb) {
                free(buf);
                buf = NULL;
                break;
            }
            buf = nb;
            cap = ncap;
        }
    }
    if (buf) {
        buf[len] = '\0';
        if (len == max && fgetc(f) != EOF)
            fprintf(stderr, "Aviso: %s supera %zu bytes; solo se lee el principio.\n", path, max);
    }
    fclose(f);
    return buf;
}

int atlas_write_file_atomic(const char *path, const char *data)
{
    char tmp[PATH_MAX];
    FILE *f;

    if (snprintf(tmp, sizeof(tmp), "%s.tmp.%d", path, (int)getpid()) >= (int)sizeof(tmp))
        return -1;
    f = fopen(tmp, "w");
    if (!f)
        return -1;
    if (fputs(data, f) < 0) {
        fclose(f);
        unlink(tmp);
        return -1;
    }
    if (fclose(f) != 0) {
        unlink(tmp);
        return -1;
    }
    return rename(tmp, path);
}

int atlas_append_line(const char *path, const char *line)
{
    FILE *f = fopen(path, "a");
    if (!f)
        return -1;
    fprintf(f, "%s\n", line);
    return fclose(f);
}

int atlas_safe_name(const char *s)
{
    size_t n = 0;

    if (!s || !*s || !strcmp(s, ".") || !strcmp(s, ".."))
        return 0;
    for (; s[n]; n++) {
        char c = s[n];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
            return 0;
    }
    return n < 64;
}

static const char *home_dir(void)
{
    const char *h = getenv("HOME");
    return (h && *h) ? h : "/tmp";
}

static int data_dir_ok(const char *dir)
{
    char p[PATH_MAX];
    if (atlas_path_join(p, sizeof(p), dir, "profiles") != 0 || !atlas_dir_exists(p))
        return 0;
    if (atlas_path_join(p, sizeof(p), dir, "hardware") != 0 || !atlas_dir_exists(p))
        return 0;
    return 1;
}

const char *atlas_data_dir(void)
{
    static char dir[PATH_MAX];
    char exe[PATH_MAX];
    const char *env;
    ssize_t l;

    if (dir[0])
        return dir;

    env = getenv("ATLAS_DATA_DIR");
    if (env && data_dir_ok(env)) {
        snprintf(dir, sizeof(dir), "%s", env);
        return dir;
    }

    l = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (l > 0) {
        char *slash;
        exe[l] = '\0';
        slash = strrchr(exe, '/');
        if (slash) {
            char cand[PATH_MAX];
            *slash = '\0';
            /* árbol de desarrollo: el binario está en la raíz del repo */
            if (data_dir_ok(exe)) {
                snprintf(dir, sizeof(dir), "%s", exe);
                return dir;
            }
            /* instalado: <prefix>/bin/audio-profile -> <prefix>/share/audio-profile */
            if (snprintf(cand, sizeof(cand), "%s/../share/audio-profile", exe) < (int)sizeof(cand) &&
                data_dir_ok(cand)) {
                if (!realpath(cand, dir))
                    snprintf(dir, sizeof(dir), "%s", cand);
                return dir;
            }
        }
    }

    if (data_dir_ok("/usr/local/share/audio-profile"))
        snprintf(dir, sizeof(dir), "/usr/local/share/audio-profile");
    else if (data_dir_ok("/usr/share/audio-profile"))
        snprintf(dir, sizeof(dir), "/usr/share/audio-profile");
    else if (data_dir_ok("."))
        snprintf(dir, sizeof(dir), ".");
    else
        snprintf(dir, sizeof(dir), "/usr/share/audio-profile");
    return dir;
}

const char *atlas_config_dir(void)
{
    static char dir[PATH_MAX];
    const char *x;

    if (dir[0])
        return dir;
    if ((x = getenv("ATLAS_CONFIG_DIR")) && *x)
        snprintf(dir, sizeof(dir), "%s", x);
    else if ((x = getenv("XDG_CONFIG_HOME")) && *x)
        snprintf(dir, sizeof(dir), "%s/audio-profile", x);
    else
        snprintf(dir, sizeof(dir), "%s/.config/audio-profile", home_dir());
    return dir;
}

const char *atlas_state_dir(void)
{
    static char dir[PATH_MAX];
    const char *x;

    if (dir[0])
        return dir;
    if ((x = getenv("ATLAS_STATE_DIR")) && *x)
        snprintf(dir, sizeof(dir), "%s", x);
    else if ((x = getenv("XDG_STATE_HOME")) && *x)
        snprintf(dir, sizeof(dir), "%s/audio-profile", x);
    else
        snprintf(dir, sizeof(dir), "%s/.local/state/audio-profile", home_dir());
    return dir;
}

/* ============================================================== config */

static const char *conf_defaults[][2] = {
    /* daemon de suspensión */
    { "watch.quiesce_before_sleep", "yes" },   /* cerrar PCM antes de S3 */
    { "watch.quiesce_cards", "all" },          /* all | db | lista separada por comas */
    { "watch.auto_recover", "yes" },
    { "watch.settle_ms", "2500" },
    { "watch.notify", "yes" },
    { "watch.reapply_profile", "yes" },        /* rate/quantum/DSP tras reanudar */
    { "watch.apply_on_start", "yes" },         /* base de volumen + perfil al iniciar sesión */
    { "base.include_master", "yes" },          /* Master forma parte de tu base (asound.state) */
    { "base.enforce", "yes" },                 /* el daemon corrige desvíos de tu base */
    { "base.enforce_seconds", "10" },
    { "profiles.effects", "personal" },        /* personal = tus efectos siempre; profile = los del perfil */
    { "auto.steam_lookup", "yes" },            /* género de juegos vía SteamSpy (solo el appid) */
    /* salida */
    { "output.follow_jack", "yes" },           /* cambiar al enchufar/desenchufar auriculares */
    /* recuperación */
    { "recover.max_level", "3" },
    { "recover.portal_units", "xdg-desktop-portal-wlr.service" },
    { "recover.restore_volumes", "yes" },
    /* sonda funcional */
    { "probe.active", "yes" },                 /* silencio si no hay reproducción */
    { "probe.window_ms", "400" },
    { NULL, NULL }
};

#define CONF_MAX 64
static char conf_keys[CONF_MAX][64];
static char conf_vals[CONF_MAX][256];
static int conf_n = -1;
/* Firma del archivo cargado: el daemon `watch` vive horas y debe ver los
 * cambios de la GUI, la CLI o una edición a mano sin reiniciarse. */
static struct stat conf_st;
static int conf_present;

static int conf_changed(const char *path)
{
    struct stat st;
    int present = stat(path, &st) == 0;

    if (conf_n < 0 || present != conf_present)
        return 1;
    if (!present)
        return 0;
    return st.st_dev != conf_st.st_dev || st.st_ino != conf_st.st_ino ||
           st.st_size != conf_st.st_size ||
           st.st_mtim.tv_sec != conf_st.st_mtim.tv_sec ||
           st.st_mtim.tv_nsec != conf_st.st_mtim.tv_nsec;
}

static void conf_load(void)
{
    char path[PATH_MAX];
    FILE *f;
    char line[512];

    if (atlas_path_join(path, sizeof(path), atlas_config_dir(), "atlas.conf") != 0) {
        conf_n = 0;
        return;
    }
    if (!conf_changed(path))
        return;
    conf_n = 0;
    conf_present = stat(path, &conf_st) == 0;
    f = fopen(path, "r");
    if (!f)
        return;

    while (fgets(line, sizeof(line), f) && conf_n < CONF_MAX) {
        char *eq, *k = line, *v, *e;
        line[strcspn(line, "\r\n")] = '\0';
        while (*k == ' ' || *k == '\t')
            k++;
        if (!*k || *k == '#')
            continue;
        eq = strchr(k, '=');
        if (!eq)
            continue;
        *eq = '\0';
        v = eq + 1;
        for (e = eq - 1; e >= k && (*e == ' ' || *e == '\t'); e--)
            *e = '\0';
        while (*v == ' ' || *v == '\t')
            v++;
        snprintf(conf_keys[conf_n], sizeof(conf_keys[0]), "%s", k);
        snprintf(conf_vals[conf_n], sizeof(conf_vals[0]), "%s", v);
        conf_n++;
    }
    fclose(f);
}

const char *atlas_conf_get(const char *key)
{
    int i;

    conf_load();   /* solo relee si atlas.conf cambió */
    for (i = conf_n - 1; i >= 0; i--)
        if (!strcmp(conf_keys[i], key))
            return conf_vals[i];
    for (i = 0; conf_defaults[i][0]; i++)
        if (!strcmp(conf_defaults[i][0], key))
            return conf_defaults[i][1];
    return "";
}

int atlas_conf_int(const char *key)
{
    return atoi(atlas_conf_get(key));
}

int atlas_conf_bool(const char *key)
{
    const char *v = atlas_conf_get(key);
    return !strcmp(v, "yes") || !strcmp(v, "true") || !strcmp(v, "1") || !strcmp(v, "on");
}
