#!/bin/bash

set -e

if [ "$#" -lt 4 ]; then
    echo "Использование: $0 <сервер> <htdocs> <порт> <потоки> [повторы]"
    exit 1
fi

SERVER="$1"
HTDOCS="$2"
PORT="$3"
THREADS="$4"
NUM_RUNS="${5:-5}"

RESOURCES_DIR="$HTDOCS/resources"
TEMP_DIR="$RESOURCES_DIR/temp_filesize_test"

OUTPUT_FILE="results.csv"

if [[ ! -f "$OUTPUT_FILE" ]]; then
    echo 'name,size_bytes,run,time_ms,throughput_mbs' > "$OUTPUT_FILE"
fi

cleanup() {
    echo "Выполняется очистка..."

    if [ -n "${SERVER_PID-}" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi

    if [ -d "$TEMP_DIR" ]; then
        rm -rf "$TEMP_DIR"
    fi
    echo "Очистка завершена."
}

trap cleanup EXIT

if command -v lsof >/dev/null 2>&1; then
    if lsof -i :"$PORT" > /dev/null 2>&1; then
        echo "Порт $PORT занят. Завершите другие процессы (например, pkill -f app)."
        exit 1
    fi
else
    echo "Предупреждение: lsof не найден, проверка порта пропущена."
fi

mkdir -p "$TEMP_DIR"

FILES=(
    "10KB:10"
    "100KB:100"
    "1MB:1024"
    "10MB:10240"
    "100MB:102400"
)

wait_for_server() {
    echo "Ожидание запуска сервера на порту $PORT..."
    for i in {1..30}; do
        if curl -s --max-time 1 "http://localhost:$PORT/" >/dev/null; then
            echo "Сервер готов."
            return 0
        fi
        sleep 0.2
    done
    echo "Ошибка: сервер не запустился на порту $PORT"
    exit 1
}

echo "Запуск сервера: $SERVER $HTDOCS $PORT $THREADS"
"$SERVER" "$HTDOCS" "$PORT" "$THREADS" &
SERVER_PID=$!

wait_for_server

echo "filename,size_bytes,run,time_ms,throughput_MBs"

for file in "${FILES[@]}"; do
    name="${file%%:*}"
    size_kb="${file##*:}"
    filepath="$TEMP_DIR/file_$name.bin"
    url="http://localhost:$PORT/\
    resources/temp_filesize_test/\
    file_$name.bin"
    size_bytes=$((size_kb * 1024))

    dd if=/dev/urandom of="$filepath" bs=1024 count="$size_kb" status=none

    for run in $(seq 1 $NUM_RUNS); do
        time_sec=$(curl -o /dev/null -s -w "%{time_total}" "$url")

        time_ms=$(awk "BEGIN {printf \"%.3f\", $time_sec * 1000}")
        if (( $(echo "$time_sec <= 0" | bc -l) )); then
            throughput_mbs="inf"
        else
            throughput_mbs=$(awk "BEGIN {printf \"%.3f\", ($size_bytes / 1024 / 1024) / ($time_sec)}")
        fi
        echo "$name,$size_bytes,$run,$time_ms,$throughput_mbs"
        echo "$name,$size_bytes,$run,$time_ms,$throughput_mbs" >> "$OUTPUT_FILE"
    done
done

AVG_FILE="results_avg.csv"
echo 'name,size_bytes,avg_time_ms,avg_throughput_mbs' > "$AVG_FILE"

awk -F, '
NR > 1 {
    key = $1 "," $2
    sum_time[key] += $4
    sum_thr[key] += $5
    count[key]++
}
END {
    for (k in sum_time) {
        avg_time = sum_time[k] / count[k]
        avg_thr = sum_thr[k] / count[k]
        printf "%s,%.3f,%.3f\n", k, avg_time, avg_thr
    }
}' "$OUTPUT_FILE" >> "$AVG_FILE"