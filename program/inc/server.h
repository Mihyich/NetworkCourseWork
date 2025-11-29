#ifndef SERVER_H
#define SERVER_H

// Запускает сервер на указанном порту
// docroot — корневая директория для файлов
// worker_count — количество потоков в пуле
// Возвращает 0 при успехе, -1 — ошибка
int server_run(const char *docroot, int port, int worker_count);

#endif // SERVER_H