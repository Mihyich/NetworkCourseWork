import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Загрузка данных
df3 = pd.read_csv('one_conn.csv')  # убедись, что имя файла правильное

# Вычисление среднего
mean_throughput = df3['transfer/Sec'].mean()

plt.rcParams.update({
    'font.size': 14,
    'axes.titlesize': 20,
    'axes.labelsize': 20,
    'legend.fontsize': 16,
    'xtick.labelsize': 16,
    'ytick.labelsize': 16,
    'font.family': 'DejaVu Sans'
})

plt.figure(figsize=(10, 6))

# Точки — измерения
plt.plot(df3['launch'], df3['transfer/Sec'], marker='o', linestyle='-', linewidth=2, markersize=8, label='Пропускная способность')

# Среднее — горизонтальная линия
plt.axhline(y=mean_throughput, color='red', linestyle='--', linewidth=2, label=f'Среднее: {mean_throughput:.2f} МБ/с')

# Настройки
plt.xlabel('Номер запуска')
plt.ylabel('Пропускная способность, МБ/с')
plt.xticks(df3['launch'])  # чтобы были все номера: 1,2,...,7
plt.legend()
plt.grid(True, linestyle=':', alpha=0.7)

# Отступы (без tight_layout!)
plt.subplots_adjust(left=0.09, bottom=0.09, right=0.95, top=0.95)

# plt.title('Пропускная способность при передаче 100 МБ по одному соединению')
plt.show()

plt.figure(figsize=(10, 6))
plt.plot(df3['launch'], df3['time'], marker='o', linestyle='-', linewidth=2, markersize=8, label='Время выполнения')
plt.axhline(df3['time'].mean(), color='green', linestyle='--', label=f'Среднее: {df3["time"].mean():.2f} мс')
plt.xlabel('Номер запуска')
plt.ylabel('Время выполнения, мс')
plt.xticks(df3['launch'])
plt.legend()
plt.grid(True)
plt.subplots_adjust(left=0.09, bottom=0.09, right=0.95, top=0.95)
# plt.title('Время передачи 100 МБ по одному соединению')
plt.show()