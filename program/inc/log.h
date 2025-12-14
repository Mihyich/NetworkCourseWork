#ifndef LOG_H
#define LOG_H

#include <stddef.h>

// Инициализация логгера
// filename == NULL -> лог в stderr
int log_init(const char *filename);

// Завершение работы (закрыть файл, уничтожить мьютекс)
void log_close(void);

// Основная функция логирования
void log_request(
    const char *client_ip,
    int client_port,
    const char *method,
    const char *path,
    int status_code,
    size_t bytes_sent
);

#endif // LOG_H