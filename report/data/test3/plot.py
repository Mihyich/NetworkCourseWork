import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Загрузка данных
df = pd.DataFrame({
    'name': ['10KB', '100KB', '1MB', '10MB', '100MB'],
    'size_bytes': [10240, 102400, 1048576, 10485760, 104857600],
    'avg_time_ms': [0.511, 0.580, 1.134, 4.572, 27.815],
    'avg_throughput_mbs': [20.767, 184.623, 999.074, 2366.636, 3648.628]
})

# Преобразуем размеры в удобные единицы для подписей
def format_size(size_bytes):
    if size_bytes < 1024:
        return f"{size_bytes} B"
    elif size_bytes < 1024**2:
        return f"{size_bytes // 1024} KB"
    elif size_bytes < 1024**3:
        return f"{size_bytes // (1024**2)} MB"
    else:
        return f"{size_bytes // (1024**3)} GB"

df['size_label'] = df['size_bytes'].apply(format_size)

# Настройки шрифтов
plt.rcParams.update({
    'font.size': 14,
    'axes.titlesize': 20,
    'axes.labelsize': 20,
    'legend.fontsize': 16,
    'xtick.labelsize': 16,
    'ytick.labelsize': 16,
    'font.family': 'DejaVu Sans'
})

# === График 1: Время передачи vs размер файла ===
plt.figure(figsize=(10, 6))
plt.plot(df['size_bytes'], df['avg_time_ms'], marker='o', linewidth=2, markersize=8, color='tab:blue')

plt.xscale('log')
plt.yscale('log')  # время тоже растёт экспоненциально — лог-лог даёт линейность
plt.xlabel('Размер файла')
plt.ylabel('Среднее время передачи, мс')
plt.xticks(df['size_bytes'], df['size_label'])
plt.grid(True, which="both", ls="--", linewidth=0.5)
# plt.title('Время передачи в зависимости от размера файла')
plt.subplots_adjust(left=0.09, bottom=0.09, right=0.95, top=0.95)
plt.savefig('time_vs_size.pdf')  # или .png
plt.show()

# === График 2: Пропускная способность vs размер файла ===
plt.figure(figsize=(10, 6))
plt.plot(df['size_bytes'], df['avg_throughput_mbs'], marker='s', linewidth=2, markersize=8, color='tab:orange')

plt.xscale('log')
plt.xlabel('Размер файла')
plt.ylabel('Средняя пропускная способность, МБ/с')
plt.xticks(df['size_bytes'], df['size_label'])
plt.grid(True, which="both", ls="--", linewidth=0.5)
# plt.title('Пропускная способность в зависимости от размера файла')
plt.subplots_adjust(left=0.09, bottom=0.09, right=0.95, top=0.95)
plt.savefig('throughput_vs_size.pdf')
plt.show()