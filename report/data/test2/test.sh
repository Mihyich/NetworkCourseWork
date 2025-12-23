#!/bin/bash
export LC_NUMERIC=C

URL="http://localhost:8080/resources/test/dataset.bin"
SIZES=(1 2 4 8 16 32 64 128 256 512 1024)
OUTPUT_FILE="throughput_results.csv"

if [[ ! -f "$OUTPUT_FILE" ]]; then
    echo 'connections,"aver transfer","transfer",time' > "$OUTPUT_FILE"
fi

echo "Тестирование совокупной пропускной способности"
echo "Файл: 100 МБ, Сервер: 8 потоков"
echo

for n in "${SIZES[@]}"; do
    echo ">>> Запуск с $n соединениями..."
    
    start_time=$(date +%s.%N)
    for ((i=0; i<n; i++)); do
        curl -o /dev/null -s "$URL" &
    done
    wait
    end_time=$(date +%s.%N)

    total_time=$(echo "$end_time - $start_time" | bc -l)
    time_rounded=$(printf "%.0f" "$total_time")
    total_mb=$((n * 100))
    throughput=$(echo "scale=2; $total_mb / $total_time" | bc)
    avg_per_conn=$(echo "scale=2; $throughput / $n" | bc)

    echo "  Время: $(printf "%.3f" $total_time) с"
    echo "  Совокупная пропускная способность: $throughput МБ/с"
    echo "  На соединение (среднее): $avg_per_conn МБ/с"
done