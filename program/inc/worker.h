#ifndef WORKER_H
#define WORKER_H

// Инициализация пула потоков
int worker_pool_start(const char *docroot, int thread_count);

// Остановка пула (ожидание завершения всех потоков)
int worker_pool_stop(void);

// Назначить новое соединение одному из worker'ов (round-robin)
int worker_assign_connection(int client_fd, const char *ip, int port);

#endif // WORKER_H