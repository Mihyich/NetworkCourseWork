#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// Проверка безопасности пути
int is_path_safe(const char *root_dir, const char *user_path, char *resolved) {
    if (!root_dir || !user_path || !resolved) return 0;

    // Разрешить только относительные пути без ".." в начале
    if (user_path[0] == '/') {
        // Убирать начальный слэш для относительного разрешения
        user_path++;
    }

    // Сборка полного пути: root_dir + "/" + user_path
    char full_path[PATH_MAX];
    if (snprintf(full_path, sizeof(full_path), "%s/%s", root_dir, user_path) >= (int)sizeof(full_path)) {
        return 0; // путь слишком длинный
    }

    // Нормализация пути через realpath (разрешает symlinks, убирает .. и .)
    if (realpath(full_path, resolved) == NULL) {
        return 0; // путь не существует или недоступен
    }

    // Получить канонический путь к root_dir
    char root_real[PATH_MAX];
    if (realpath(root_dir, root_real) == NULL) {
        return 0;
    }

    // Проверка: resolved начинается с root_real + '/'
    size_t root_len = strlen(root_real);
    if (strncmp(resolved, root_real, root_len) != 0) {
        return 0;
    }

    // Допустить совпадение (доступ к самой директории)
    if (resolved[root_len] == '\0') {
        return 1;
    }

    // Или подкаталог
    if (resolved[root_len] == '/') {
        return 1;
    }

    return 0;
}

// MIME-типы (упрощённо)
const char *get_content_type(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";

    const char *ext = dot + 1;

    // HTML / текст
    if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0)  return "text/html";
    if (strcasecmp(ext, "css")  == 0) return "text/css";
    if (strcasecmp(ext, "js")   == 0) return "application/javascript";
    if (strcasecmp(ext, "json") == 0) return "application/json";
    if (strcasecmp(ext, "txt")  == 0) return "text/plain";

    // Изображения
    if (strcasecmp(ext, "png")  == 0) return "image/png";
    if (strcasecmp(ext, "jpg")  == 0 || strcasecmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcasecmp(ext, "gif")  == 0) return "image/gif";
    if (strcasecmp(ext, "ico")  == 0) return "image/x-icon";
    if (strcasecmp(ext, "webp") == 0) return "image/webp";  // ← WebP

    // Документы
    if (strcasecmp(ext, "pdf")  == 0) return "application/pdf";

    // Видео
    if (strcasecmp(ext, "mp4")  == 0) return "video/mp4";
    if (strcasecmp(ext, "webm") == 0) return "video/webm";
    if (strcasecmp(ext, "ogg")  == 0 || strcasecmp(ext, "ogv") == 0) return "video/ogg";
    if (strcasecmp(ext, "avi")  == 0) return "video/x-msvideo";
    if (strcasecmp(ext, "mov")  == 0) return "video/quicktime";
    if (strcasecmp(ext, "mkv")  == 0) return "video/x-matroska";

    return "application/octet-stream";
}

// Проверка, является ли путь каталогом
int is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

// Размер файла
long long get_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    if (S_ISDIR(st.st_mode)) return -1; // каталоги не отдаются
    return (long long)st.st_size;
}