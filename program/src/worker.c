#include "worker.h"
#include "http.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>

#define MAX_CONNECTIONS_PER_WORKER 1024
#define READ_BUF_SIZE 4096

// Состояние одного соединения
struct connection {
    int fd;
    char ip[46];
    int port;
    char request_buf[READ_BUF_SIZE];
    size_t request_len;
    int headers_parsed;       // 0 — не распарсены, 1 — готово к отправке
    struct http_request req;
    int response_sent;        // 1 — ответ полностью отправлен
};

// Данные одного worker-потока
struct worker {
    pthread_t thread;
    struct connection conns[MAX_CONNECTIONS_PER_WORKER];
    int conn_count;
    int new_conn_fd;          // для передачи нового fd (упрощённо)
    volatile int new_conn_ready;
    char new_ip[46];
    int new_port;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    const char *docroot;
    volatile int shutdown;
};

static struct worker *workers = NULL;
static int worker_count = 0;
static volatile int workers_shutdown = 0;
static int next_worker = 0; // для round-robin

// Устанавливает сокет в неблокирующий режим
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Удаляет соединение из worker'а
static void remove_connection(struct worker *w, int idx) {
    close(w->conns[idx].fd);
    if (idx != w->conn_count - 1) {
        w->conns[idx] = w->conns[w->conn_count - 1];
    }
    w->conn_count--;
}

// Обработка одного соединения после получения запроса
static void handle_request(struct worker *w, int idx) {
    struct connection *conn = &w->conns[idx];
    if (http_send_response(conn->fd, w->docroot, &conn->req, 
                          conn->req.method == HTTP_METHOD_HEAD) < 0) {
        // Ошибка отправки — закрываем
        remove_connection(w, idx);
        return;
    }
    conn->response_sent = 1;
    // Так как Connection: close, закрываем после отправки
    remove_connection(w, idx);
}

// Основной цикл worker-потока
static void* worker_thread(void *arg) {
    struct worker *w = (struct worker*)arg;
    struct pollfd pfds[MAX_CONNECTIONS_PER_WORKER + 1];
    int timeout_ms = 200; // таймаут для poll()

    while (!w->shutdown) {
        // 1. Проверяем, есть ли новые соединения (в критической секции)
        pthread_mutex_lock(&w->mutex);
        if (w->new_conn_ready) {
            if (w->conn_count < MAX_CONNECTIONS_PER_WORKER) {
                int idx = w->conn_count++;
                w->conns[idx].fd = w->new_conn_fd;
                // strncpy(w->conns[idx].ip, w->new_ip, sizeof(w->conns[idx].ip) - 1);
                snprintf(w->conns[idx].ip, sizeof(w->conns[idx].ip), "%s", w->new_ip);
                w->conns[idx].port = w->new_port;
                w->conns[idx].request_len = 0;
                w->conns[idx].headers_parsed = 0;
                w->conns[idx].response_sent = 0;
                set_nonblocking(w->new_conn_fd);
            } else {
                close(w->new_conn_fd); // отклоняем — перегрузка
            }
            w->new_conn_ready = 0;
            pthread_cond_signal(&w->cond);
        }
        pthread_mutex_unlock(&w->mutex);

        // 2. Заполняем pfds
        int nfds = 0;
        for (int i = 0; i < w->conn_count; i++) {
            pfds[nfds].fd = w->conns[i].fd;
            pfds[nfds].events = POLLIN;
            if (!w->conns[i].headers_parsed || !w->conns[i].response_sent) {
                // Пока не отправили ответ — ждём записи (на случай sendfile задержки)
                pfds[nfds].events |= POLLOUT;
            }
            nfds++;
        }

        // 3. Выполняем poll
        int ready = poll(pfds, nfds, timeout_ms);
        if (ready <= 0) continue;

        // 4. Обрабатываем события
        for (int i = 0; i < nfds; i++) {
            int idx = i;
            if (pfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                remove_connection(w, idx);
                continue;
            }

            // Чтение запроса
            if (pfds[i].revents & POLLIN) {
                struct connection *conn = &w->conns[idx];
                if (!conn->headers_parsed) {
                    ssize_t n = recv(conn->fd, 
                        conn->request_buf + conn->request_len,
                        sizeof(conn->request_buf) - conn->request_len - 1, 
                        MSG_NOSIGNAL);
                    if (n > 0) {
                        conn->request_len += n;
                        conn->request_buf[conn->request_len] = '\0';

                        // Ищем конец заголовка (\r\n\r\n)
                        char *end = strstr(conn->request_buf, "\r\n\r\n");
                        if (end) {
                            *end = '\0'; // обрезаем
                            if (http_parse_request_line(conn->request_buf, &conn->req)) {
                                strcpy(conn->req.client_ip, conn->ip);
                                conn->req.client_port = conn->port;
                                conn->headers_parsed = 1;
                                // Отправим ответ в следующем POLLOUT
                            } else {
                                // Неподдерживаемый метод или плохой запрос
                                send(conn->fd, "HTTP/1.1 405 Method Not Allowed\r\nConnection: close\r\n\r\n", 63, MSG_NOSIGNAL);
                                log_request(conn->ip, conn->port, "UNKNOWN", "/", 405, 0);
                                remove_connection(w, idx);
                            }
                        } else if (conn->request_len >= sizeof(conn->request_buf) - 1) {
                            send(conn->fd, "HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\n\r\n", 60, MSG_NOSIGNAL);
                            log_request(conn->ip, conn->port, "UNKNOWN", "/", 413, 0);
                            remove_connection(w, idx);
                        }
                    } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                        remove_connection(w, idx);
                    }
                }
            }

            // Отправка ответа
            if (pfds[i].revents & POLLOUT) {
                if (w->conns[idx].headers_parsed && !w->conns[idx].response_sent) {
                    handle_request(w, idx);
                }
            }
        }
    }

    // Закрываем все соединения при завершении
    for (int i = 0; i < w->conn_count; i++) {
        close(w->conns[i].fd);
    }
    return NULL;
}

// === Публичные функции ===

int worker_pool_start(const char *docroot, int thread_count) {
    if (thread_count <= 0 || !docroot) return -1;

    workers = calloc(thread_count, sizeof(struct worker));
    if (!workers) return -1;

    worker_count = thread_count;
    workers_shutdown = 0;
    next_worker = 0;

    for (int i = 0; i < thread_count; i++) {
        struct worker *w = &workers[i];
        w->docroot = docroot;
        w->conn_count = 0;
        w->new_conn_ready = 0;
        w->shutdown = 0;
        pthread_mutex_init(&w->mutex, NULL);
        pthread_cond_init(&w->cond, NULL);
        if (pthread_create(&w->thread, NULL, worker_thread, w) != 0) {
            // Ошибка создания — очистка
            worker_pool_stop();
            return -1;
        }
    }
    return 0;
}

int worker_pool_stop(void) {
    if (!workers) return -1;

    workers_shutdown = 1;
    for (int i = 0; i < worker_count; i++) {
        workers[i].shutdown = 1;
        pthread_join(workers[i].thread, NULL);
        pthread_mutex_destroy(&workers[i].mutex);
        pthread_cond_destroy(&workers[i].cond);
    }

    free(workers);
    workers = NULL;
    worker_count = 0;
    return 0;
}

int worker_assign_connection(int client_fd, const char *ip, int port) {
    if (!workers || workers_shutdown) {
        close(client_fd);
        return -1;
    }

    int target = __sync_fetch_and_add(&next_worker, 1) % worker_count;
    struct worker *w = &workers[target];

    pthread_mutex_lock(&w->mutex);
    while (w->new_conn_ready) {
        // Ждём, если предыдущее соединение ещё не забрали
        pthread_cond_wait(&w->cond, &w->mutex);
    }

    w->new_conn_fd = client_fd;
    // strncpy(w->new_ip, ip, sizeof(w->new_ip) - 1);
    snprintf(w->new_ip, sizeof(w->new_ip), "%s", ip);
    w->new_port = port;
    w->new_conn_ready = 1;
    pthread_mutex_unlock(&w->mutex);

    return 0;
}