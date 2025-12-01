#include "http.h"
#include "util.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>

// Отправка простого текстового ответа (ошибки)
static void send_simple_response(int fd, int status_code, const char *status_text) {
    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "<html><head><title>%d %s</title></head><body><h1>%d %s</h1></body></html>",
        status_code, status_text,
        (size_t)(45 + 2 * (strlen(status_text) + 8)), // приблизительно
        status_code, status_text,
        status_code, status_text
    );
    send(fd, buf, len, MSG_NOSIGNAL);
}

// Основная функция отправки ответа
int http_send_response(int client_fd, const char *docroot, struct http_request *req, int is_head) {
    if (!docroot || !req) return -1;

    // 1. Нормализация пути
    char user_path[2048];
    // Убирать query-строку
    char *q = strchr(req->path, '?');
    if (q) *q = '\0';

    // Копирование пути для безопасности
    if (snprintf(user_path, sizeof(user_path), "%s", req->path) >= (int)sizeof(user_path)) {
        send_simple_response(client_fd, 400, "Bad Request");
        log_request(req->client_ip, req->client_port, 
            req->method == HTTP_METHOD_GET ? "GET" : "HEAD",
            req->path, 400, 0);
        return -1;
    }

    // Обработка корня
    if (user_path[0] == '\0' || strcmp(user_path, "/") == 0) {
        strcpy(user_path, "/index_1.html");
    } else if (user_path[strlen(user_path)-1] == '/') {
        // Добавляеся index_1.html
        if (strlen(user_path) + 11 < sizeof(user_path)) {
            strcat(user_path, "index_1.html");
        } else {
            send_simple_response(client_fd, 400, "Bad Request");
            return -1;
        }
    }

    // 2. Проверка безопасности
    char resolved_path[4096];
    if (!is_path_safe(docroot, user_path, resolved_path)) {
        send_simple_response(client_fd, 403, "Forbidden");
        log_request(req->client_ip, req->client_port,
            req->method == HTTP_METHOD_GET ? "GET" : "HEAD",
            req->path, 403, 0);
        return -1;
    }

    // 3. Проверка существования и размера
    long long file_size = get_file_size(resolved_path);
    if (file_size < 0) {
        if (is_directory(resolved_path)) {
            send_simple_response(client_fd, 403, "Forbidden");
            log_request(req->client_ip, req->client_port,
                req->method == HTTP_METHOD_GET ? "GET" : "HEAD",
                req->path, 403, 0);
        } else {
            send_simple_response(client_fd, 404, "Not Found");
            log_request(req->client_ip, req->client_port,
                req->method == HTTP_METHOD_GET ? "GET" : "HEAD",
                req->path, 404, 0);
        }
        return -1;
    }

    if (file_size > 128LL * 1024 * 1024) {
        send_simple_response(client_fd, 413, "Payload Too Large");
        log_request(req->client_ip, req->client_port,
            req->method == HTTP_METHOD_GET ? "GET" : "HEAD",
            req->path, 413, 0);
        return -1;
    }

    // 4. Открыть файл
    int file_fd = open(resolved_path, O_RDONLY);
    if (file_fd < 0) {
        send_simple_response(client_fd, 404, "Not Found");
        log_request(req->client_ip, req->client_port,
            req->method == HTTP_METHOD_GET ? "GET" : "HEAD",
            req->path, 404, 0);
        return -1;
    }

    // 5. Формирование заголовков
    const char *content_type = get_content_type(resolved_path);
    char header[1024];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        content_type, file_size
    );

    // 6. Отправка заголовока
    if (send(client_fd, header, header_len, MSG_NOSIGNAL) < 0) {
        close(file_fd);
        return -1;
    }

    size_t total_sent = 0;
    if (!is_head) {
        // 7. Отправка тела через sendfile (zero-copy)
        while ((long long)total_sent < file_size) {
            ssize_t sent = sendfile(client_fd, file_fd, NULL, 
                (size_t)(file_size - total_sent));
            if (sent <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    continue;
                }
                break;
            }
            total_sent += (size_t)sent;
        }
    }
    close(file_fd);

    // 8. Лог
    log_request(req->client_ip, req->client_port,
        req->method == HTTP_METHOD_GET ? "GET" : "HEAD",
        req->path, 200, is_head ? 0 : total_sent);

    return 0;
}

// Парсинг строки запроса
int http_parse_request_line(const char *line, struct http_request *req) {
    if (!line || !req) return 0;

    char method[16], path[2048], protocol[16];
    int n = sscanf(line, "%15s %2047s %15s", method, path, protocol);
    if (n != 3) return 0;

    // Поддерживается только HTTP/1.0 и HTTP/1.1
    if (strcmp(protocol, "HTTP/1.0") != 0 && strcmp(protocol, "HTTP/1.1") != 0) {
        return 0;
    }

    if (strcmp(method, "GET") == 0) {
        req->method = HTTP_METHOD_GET;
    } else if (strcmp(method, "HEAD") == 0) {
        req->method = HTTP_METHOD_HEAD;
    } else {
        return 0; // неподдерживаемый метод
    }

    // Ограничить путь
    if (strlen(path) >= sizeof(req->path)) return 0;
    strcpy(req->path, path);

    return 1;
}