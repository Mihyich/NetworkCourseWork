#ifndef WORKER_H
#define WORKER_H

// Инициализирует пул потоков
int worker_pool_start(const char *docroot, int thread_count);

// Останавливает пул (ждёт завершения всех потоков)
int worker_pool_stop(void);

// Назначает новое соединение одному из worker'ов (round-robin)
int worker_assign_connection(int client_fd, const char *ip, int port);

#endif // WORKER_H