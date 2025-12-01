#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

// Проверка, что user_path безопасен относительно root_dir.
// При успехе запись канонического пути в resolved (должен быть >= PATH_MAX).
// Возврат 1 - безопасно, 0 - нет.
int is_path_safe(const char *root_dir, const char *user_path, char *resolved);

// Возврат MIME-типа по расширению (без точки), например "text/html"
const char *get_content_type(const char *path);

// Проверка, является ли путь каталогом
int is_directory(const char *path);

// Возврат размера файла в байтах, или -1 при ошибке
long long get_file_size(const char *path);

#endif // UTIL_H