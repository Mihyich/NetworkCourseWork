import pandas as pd
import matplotlib.pyplot as plt

# Загрузка данных
df = pd.read_csv('test1.csv')

# Приведение latency к числу (удаление 'us', 'ms', конвертация)
def parse_latency(lat):
    if 'us' in lat:
        return float(lat.replace('us', '')) / 1000  # в ms
    elif 'ms' in lat:
        return float(lat.replace('ms', ''))
    else:
        return float(lat)

df['latency_ms'] = df['latency, avg'].apply(parse_latency)

plt.rcParams.update({
    'font.size': 14,               # базовый размер шрифта (метки делений, легенда)
    'axes.titlesize': 20,          # заголовок графика
    'axes.labelsize': 20,          # подписи осей X и Y
    'legend.fontsize': 16,         # легенда
    'xtick.labelsize': 16,         # метки на оси X
    'ytick.labelsize': 16,         # метки на оси Y
    'figure.titlesize': 16,        # если используете plt.suptitle()
    'font.family': 'DejaVu Sans'   # или 'Arial', 'Times New Roman' и т.д.
})

# График RPS от connections по threads
plt.figure(figsize=(10, 6))
for thread_count in sorted(df['threads'].unique()):
    subset = df[df['threads'] == thread_count]
    plt.plot(subset['connections'], subset['Requests/sec'], marker='o', label=f'Количество рабочих потоков в пуле: {thread_count}')

plt.xlabel('Количество сетевых соединений')
plt.ylabel('Количество обрабатываемых запросов в секунду')
plt.legend()
plt.grid(True)
plt.xscale('log')  # полезно, так как connections растут экспоненциально
plt.subplots_adjust(left=0.09, bottom=0.09, right=0.95, top=0.95)
plt.show()

# === График 2: Latency vs connections (по threads) ===
plt.figure(figsize=(10, 6))
for thread_count in sorted(df['threads'].unique()):
    subset = df[df['threads'] == thread_count]
    plt.plot(subset['connections'], subset['latency_ms'], marker='o',
             label=f'Потоков: {thread_count}')

plt.xlabel('Количество сетевых соединений')
plt.ylabel('Средняя задержка, мс')
plt.legend()
plt.grid(True)
plt.xscale('log')
# plt.title('Задержка vs Соединения')
plt.subplots_adjust(left=0.09, bottom=0.09, right=0.95, top=0.95)
plt.show()

# === График 2: Latency vs connections (по threads) ===
plt.figure(figsize=(10, 6))
for thread_count in sorted(df['threads'].unique()):
    subset = df[df['threads'] == thread_count]
    plt.plot(subset['connections'], subset['Transfer/sec, MB'], marker='o', label=f'Количество рабочих потоков в пуле: {thread_count}')

plt.xlabel('Количество сетевых соединений')
plt.ylabel('Совокупный объем отдачи, Мб')
plt.legend()
plt.grid(True)
plt.xscale('log')
# plt.title('Задержка vs Соединения')
plt.subplots_adjust(left=0.09, bottom=0.09, right=0.95, top=0.95)
plt.show()