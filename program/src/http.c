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

int http_prepare_response(const char *docroot, struct http_request *req, char *resolved_path, long long *file_size, const char **content_type) {
    if (!docroot || !req || !resolved_path || !file_size || !content_type)
        return -1;

    char user_path[2048];
    char *q = strchr(req->path, '?');
    if (q) *q = '\0';

    if (snprintf(user_path, sizeof(user_path), "%s", req->path) >= (int)sizeof(user_path))
        return 400;

    if (user_path[0] == '\0' || strcmp(user_path, "/") == 0) {
        strcpy(user_path, "/index.html");
    } else if (user_path[strlen(user_path) - 1] == '/') {
        if (strlen(user_path) + 11 >= sizeof(user_path))
            return 400;
        strcat(user_path, "index.html");
    }

    if (!is_path_safe(docroot, user_path, resolved_path))
        return 403;

    long long sz = get_file_size(resolved_path);
    if (sz < 0) {
        if (is_directory(resolved_path))
            return 403;
        else
            return 404;
    }

    if (sz > 128LL * 1024 * 1024)
        return 413;

    *file_size = sz;
    *content_type = get_content_type(resolved_path);
    return 0; // OK
}

// Отправка простого текстового ответа (ошибки)
void send_simple_response(int fd, int status_code, const char *status_text) {
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