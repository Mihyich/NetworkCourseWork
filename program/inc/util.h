#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

// Проверяет, что user_path безопасен относительно root_dir.
// При успехе записывает канонический путь в resolved (должен быть >= PATH_MAX).
// Возвращает 1 — безопасно, 0 — нет.
int is_path_safe(const char *root_dir, const char *user_path, char *resolved);

// Возвращает MIME-тип по расширению (без точки), например "text/html"
const char *get_content_type(const char *path);

// Проверяет, является ли путь каталогом
int is_directory(const char *path);

// Возвращает размер файла в байтах, или -1 при ошибке
long long get_file_size(const char *path);

#endif // UTIL_H