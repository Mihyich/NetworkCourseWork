#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

static FILE *log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int log_inited = 0;

int log_init(const char *filename) {
    if (log_inited) {
        return -1; // уже инициализировано
    }

    if (filename) {
        log_file = fopen(filename, "a");
        if (!log_file) {
            perror("fopen log file");
            return -1;
        }
    } else {
        log_file = stderr;
    }

    log_inited = 1;
    return 0;
}

void log_close(void) {
    if (!log_inited) return;

    pthread_mutex_lock(&log_mutex);
    if (log_file != stderr) {
        fclose(log_file);
        log_file = NULL;
    }
    log_inited = 0;
    pthread_mutex_unlock(&log_mutex);
    // Опционально: pthread_mutex_destroy(&log_mutex);
}

void log_request(
    const char *client_ip,
    int client_port,
    const char *method,
    const char *path,
    int status_code,
    size_t bytes_sent
) {
    if (!log_inited || !log_file) return;

    time_t now = time(NULL);
    struct tm tm_info;
    gmtime_r(&now, &tm_info); // потокобезопасно

    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%SZ", &tm_info);

    pthread_mutex_lock(&log_mutex);
    fprintf(log_file,
            "[%s] [%s:%d] \"%s %s\" %d %zu\n",
            time_buf,
            client_ip,
            client_port,
            method,
            path,
            status_code,
            bytes_sent
    );
    fflush(log_file); // гарантируем запись (важно при краше)
    pthread_mutex_unlock(&log_mutex);
}