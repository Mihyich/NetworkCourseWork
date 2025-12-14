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
#include <sys/sendfile.h>

#define MAX_CONNECTIONS_PER_WORKER 1024
#define READ_BUF_SIZE 4096

// Сообщение для передачи нового соединения
struct conn_msg {
    int fd;
    char ip[46];
    int port;
};

enum conn_state {
    CONN_READING,
    CONN_SENDING_HEADER,
    CONN_SENDING_BODY,
    CONN_DONE
};

// Состояние одного соединения
struct connection {
    int fd;
    char ip[46];
    int port;
    char request_buf[READ_BUF_SIZE];
    size_t request_len;
    struct http_request req;

    enum conn_state state;
    char header_buf[1024];
    size_t header_len;
    long long file_size;      // размер файла
    int file_fd;              // открытый файловый дескриптор
    size_t header_bytes_sent; // сколько байт заголовка уже отправлено
    size_t body_bytes_sent;   // сколько байт тела отправлено
};

// Данные одного worker-потока
struct worker {
    pthread_t thread;
    struct connection conns[MAX_CONNECTIONS_PER_WORKER];
    int conn_count;
    int notify_pipe[2];  // [0] - чтение, [1] - запись
    const char *docroot;
    volatile int shutdown;
};

static struct worker *workers = NULL;
static int worker_count = 0;
static volatile int workers_shutdown = 0;
static int next_worker = 0; // для round-robin

// Устанавливить сокет в неблокирующий режим
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Основной цикл worker-потока
static void* worker_thread(void *arg) {
    struct worker *w = (struct worker*)arg;
    struct pollfd pfds[MAX_CONNECTIONS_PER_WORKER + 1]; // +1 для notify_pipe

    while (!w->shutdown) {
        // 1. Подготовка pollfd: notify_pipe
        pfds[0].fd = w->notify_pipe[0];
        pfds[0].events = POLLIN;
        int nfds = 1;

        // 2. Подготовка pollfd: клиентские сокеты
        for (int i = 0; i < w->conn_count; i++) {
            pfds[nfds].fd = w->conns[i].fd;
            pfds[nfds].events = POLLIN;

            // Добавляем POLLOUT, если соединение в состоянии отправки
            if (w->conns[i].state == CONN_SENDING_HEADER ||
                w->conns[i].state == CONN_SENDING_BODY) {
                pfds[nfds].events |= POLLOUT;
            }
            nfds++;
        }

        // 3. Ожидание событий
        int effective_timeout = (w->conn_count == 0) ? 500 : -1;
        int ready = poll(pfds, nfds, effective_timeout);
        if (ready <= 0) continue;

        // 4. Обработка уведомлений (новые соединения)
        if (pfds[0].revents & (POLLIN | POLLERR | POLLHUP)) {
            struct conn_msg msg;
            ssize_t n;
            while ((n = read(w->notify_pipe[0], &msg, sizeof(msg))) == sizeof(msg)) {
                if (w->conn_count < MAX_CONNECTIONS_PER_WORKER) {
                    int idx = w->conn_count++;
                    struct connection *conn = &w->conns[idx];
                    conn->fd = msg.fd;
                    snprintf(conn->ip, sizeof(conn->ip), "%s", msg.ip);
                    conn->port = msg.port;
                    conn->request_len = 0;
                    conn->state = CONN_READING;
                    conn->file_fd = -1;
                    set_nonblocking(msg.fd);
                } else {
                    close(msg.fd); // перегрузка
                }
            }
        }

        // 5. Обработка клиентских сокетов
        for (int i = 1; i < nfds; i++) {
            int idx = i - 1;
            if (idx >= w->conn_count) continue; // защита от изменения во время цикла

            struct connection *conn = &w->conns[idx];

            // Обработка ошибок
            if (pfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                conn->state = CONN_DONE;
                continue;
            }

            // Чтение запроса (если ещё не прочитан)
            if ((pfds[i].revents & POLLIN) && conn->state == CONN_READING) {
                ssize_t n = recv(conn->fd,
                    conn->request_buf + conn->request_len,
                    sizeof(conn->request_buf) - conn->request_len - 1,
                    MSG_NOSIGNAL);
                if (n > 0) {
                    conn->request_len += n;
                    conn->request_buf[conn->request_len] = '\0';

                    char *end = strstr(conn->request_buf, "\r\n\r\n");
                    if (end) {
                        *end = '\0';
                        if (http_parse_request_line(conn->request_buf, &conn->req)) {
                            snprintf(conn->req.client_ip, sizeof(conn->req.client_ip), "%s", conn->ip);
                            conn->req.client_port = conn->port;

                            char resolved_path[4096];
                            long long file_size;
                            const char *content_type;
                            int err = http_prepare_response(w->docroot, &conn->req, resolved_path, &file_size, &content_type);

                            if (err != 0) {
                                send_simple_response(conn->fd, err,
                                    err == 400 ? "Bad Request" :
                                    err == 403 ? "Forbidden" :
                                    err == 404 ? "Not Found" : "Payload Too Large");
                                log_request(conn->ip, conn->port,
                                    conn->req.method == HTTP_METHOD_GET ? "GET" : "HEAD",
                                    conn->req.path, err, 0);
                                conn->state = CONN_DONE;
                            } else {
                                conn->file_fd = open(resolved_path, O_RDONLY);
                                if (conn->file_fd < 0) {
                                    send_simple_response(conn->fd, 404, "Not Found");
                                    log_request(conn->ip, conn->port,
                                        conn->req.method == HTTP_METHOD_GET ? "GET" : "HEAD",
                                        conn->req.path, 404, 0);
                                    conn->state = CONN_DONE;
                                } else {
                                    conn->file_size = file_size;
                                    int len = snprintf(conn->header_buf, sizeof(conn->header_buf),
                                        "HTTP/1.1 200 OK\r\n"
                                        "Content-Type: %s\r\n"
                                        "Content-Length: %lld\r\n"
                                        "Connection: close\r\n"
                                        "\r\n",
                                        content_type, file_size);
                                    if (len <= 0 || len >= (int)sizeof(conn->header_buf)) {
                                        close(conn->file_fd);
                                        send_simple_response(conn->fd, 500, "Internal Server Error");
                                        log_request(conn->ip, conn->port, "GET", "/", 500, 0);
                                        conn->state = CONN_DONE;
                                        conn->file_fd = -1;
                                    } else {
                                        conn->header_len = len;
                                        conn->header_bytes_sent = 0;
                                        conn->body_bytes_sent = 0;
                                        conn->state = CONN_SENDING_HEADER;
                                    }
                                }
                            }
                        } else {
                            send(conn->fd, "HTTP/1.1 405 Method Not Allowed\r\nConnection: close\r\n\r\n", 63, MSG_NOSIGNAL);
                            log_request(conn->ip, conn->port, "UNKNOWN", "/", 405, 0);
                            conn->state = CONN_DONE;
                        }
                    } else if (conn->request_len >= sizeof(conn->request_buf) - 1) {
                        send(conn->fd, "HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\n\r\n", 60, MSG_NOSIGNAL);
                        log_request(conn->ip, conn->port, "UNKNOWN", "/", 413, 0);
                        conn->state = CONN_DONE;
                    }
                } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                    conn->state = CONN_DONE;
                }
            }

            // Отправка заголовка
            if ((pfds[i].revents & POLLOUT) && conn->state == CONN_SENDING_HEADER) {
                ssize_t sent = send(conn->fd,
                    conn->header_buf + conn->header_bytes_sent,
                    conn->header_len - conn->header_bytes_sent,
                    MSG_NOSIGNAL);
                if (sent > 0) {
                    conn->header_bytes_sent += sent;
                    if (conn->header_bytes_sent == conn->header_len) {
                        if (conn->req.method == HTTP_METHOD_HEAD || conn->file_size == 0) {
                            conn->state = CONN_DONE;
                        } else {
                            conn->state = CONN_SENDING_BODY;
                        }
                    }
                } else if (sent < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
                    conn->state = CONN_DONE;
                }
            }

            // Отправка тела
            if ((pfds[i].revents & POLLOUT) && conn->state == CONN_SENDING_BODY) {
                ssize_t sent = sendfile(conn->fd, conn->file_fd, NULL,
                                        conn->file_size - conn->body_bytes_sent);
                if (sent > 0) {
                    conn->body_bytes_sent += sent;
                    if (conn->body_bytes_sent >= (size_t)conn->file_size) {
                        conn->state = CONN_DONE;
                    }
                } else if (sent == 0 || (sent < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))) {
                    conn->state = CONN_DONE;
                }
            }
        }

        // 6. Удаление завершённых соединений
        for (int i = 0; i < w->conn_count; ) {
            if (w->conns[i].state == CONN_DONE) {
                if (w->conns[i].file_fd >= 0) {
                    close(w->conns[i].file_fd);
                }
                close(w->conns[i].fd);
                // Переместить последний элемент на место удаляемого
                if (i != w->conn_count - 1) {
                    w->conns[i] = w->conns[w->conn_count - 1];
                }
                w->conn_count--;
                // НЕ инкрементируем i — проверим новый элемент на этой позиции
            } else {
                i++;
            }
        }
    }

    // Финальная очистка
    for (int i = 0; i < w->conn_count; i++) {
        if (w->conns[i].file_fd >= 0) close(w->conns[i].file_fd);
        close(w->conns[i].fd);
    }
    close(w->notify_pipe[0]);
    close(w->notify_pipe[1]);
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
        w->shutdown = 0;

        if (pipe(w->notify_pipe) != 0) {
            perror("pipe");
            worker_pool_stop();
            return -1;
        }

        // Сделать pipe неблокирующим
        int flags = fcntl(w->notify_pipe[0], F_GETFL, 0);
        fcntl(w->notify_pipe[0], F_SETFL, flags | O_NONBLOCK);

        if (pthread_create(&w->thread, NULL, worker_thread, w) != 0) {
            close(w->notify_pipe[0]);
            close(w->notify_pipe[1]);
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
        // Пробудить worker, чтобы он вышел из poll()
        struct conn_msg dummy = { -1, "", 0 };
        ssize_t written = write(workers[i].notify_pipe[1], &dummy, sizeof(dummy));
        if (written != sizeof(dummy)) {
            perror("Warning: failed to notify worker during shutdown");
        }
        pthread_join(workers[i].thread, NULL);
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

    struct conn_msg msg = {0};
    msg.fd = client_fd;
    snprintf(msg.ip, sizeof(msg.ip), "%s", ip);
    msg.port = port;

    // Запись в pipe не блокируя главный поток
    if (write(w->notify_pipe[1], &msg, sizeof(msg)) != sizeof(msg)) {
        close(client_fd);
        return -1;
    }

    return 0;
}