#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

#define HTTP_METHOD_GET  1
#define HTTP_METHOD_HEAD 2

struct http_request {
    int method;              // HTTP_METHOD_GET или HEAD
    char path[2048];         // запрашиваемый путь (без query)
    char client_ip[46];      // IPv6-совместимый (до 39 + \0)
    int client_port;
};

// Парсит начальную строку запроса (например, "GET /index.html HTTP/1.1\r\n")
// Возвращает 1 при успехе, 0 — ошибка (неподдерживаемый метод, плохой формат)
int http_parse_request_line(const char *line, struct http_request *req);

// Отправляет HTTP-ответ по сокету
// is_head: если true — не отправлять тело (только заголовки)
// Возвращает 0 при успехе, -1 — ошибка отправки
int http_send_response(int client_fd, const char *docroot, struct http_request *req, int is_head);

#endif // HTTP_H