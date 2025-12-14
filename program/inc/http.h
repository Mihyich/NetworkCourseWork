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

// Парсинг начальной строки запроса (например, "GET /index.html HTTP/1.1\r\n")
// Возврат 1 при успехе, 0 - ошибка (неподдерживаемый метод, плохой формат)
int http_parse_request_line(const char *line, struct http_request *req);

int http_prepare_response(const char *docroot, struct http_request *req, char *resolved_path, long long *file_size, const char **content_type);

void send_simple_response(int fd, int status_code, const char *status_text);

#endif // HTTP_H