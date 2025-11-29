#include "server.h"
#include "worker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>

// Обработчик SIGPIPE — игнорировать (чтобы не падать при разрыве соединения)
static void sigpipe_handler(int sig) {
    (void)sig;
}

int server_run(const char *docroot, int port, int worker_count) {
    if (!docroot || port <= 0 || worker_count <= 0) {
        return -1;
    }

    // Игнорируем SIGPIPE — send() будет возвращать -1 вместо срабатывания сигнала
    signal(SIGPIPE, sigpipe_handler);

    // Создаём сокет
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return -1;
    }

    // SO_REUSEADDR — чтобы быстро перезапускать сервер
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(listen_fd);
        return -1;
    }

    // Привязка
    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, 1024) < 0) {
        perror("listen");
        close(listen_fd);
        return -1;
    }

    printf("Server listening on port %d...\n", port);

    // Запускаем worker pool
    if (worker_pool_start(docroot, worker_count) != 0) {
        fprintf(stderr, "Failed to start worker pool\n");
        close(listen_fd);
        return -1;
    }

    // Главный accept-цикл
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (1) {
        int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }

        // Получаем IP и порт клиента
        char client_ip[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);

        // Передаём соединение worker'у (round-robin или очередь — реализуем в worker.c)
        if (worker_assign_connection(client_fd, client_ip, client_port) != 0) {
            // Если не удалось — закрываем
            close(client_fd);
        }
    }

    close(listen_fd);
    worker_pool_stop();
    return 0;
}