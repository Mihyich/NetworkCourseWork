#include "server.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    const char *docroot = "./htdocs";
    int port = 8080;
    int worker_threads = 8;

    // Простая обработка аргументов (опционально)
    if (argc >= 2) {
        docroot = argv[1];
    }
    if (argc >= 3) {
        port = atoi(argv[2]);
    }
    if (argc >= 4) {
        worker_threads = atoi(argv[3]);
    }

    if (port <= 0 || port > 65535 || worker_threads <= 0) {
        fprintf(stderr, "Usage: %s [docroot] [port] [worker_threads]\n", argv[0]);
        return 1;
    }

    printf("Starting server:\n");
    printf("  Docroot: %s\n", docroot);
    printf("  Port: %d\n", port);
    printf("  Workers: %d\n", worker_threads);

    if (log_init("server.log") != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    int result = server_run(docroot, port, worker_threads);

    log_close();
    return result;
}