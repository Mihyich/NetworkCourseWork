import pandas as pd
import matplotlib.pyplot as plt

# Загрузка данных
df = pd.read_csv('throughput_results.csv')

# Настройка логарифмической шкалы по X
connections = df['connections']
transfer, transfer_mean = df['transfer'], df['transfer'].mean()
aver_transfer, aver_transfer_mean = df['aver transfer'], df['aver transfer'].mean()

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

# График 1: Совокупная пропускная способность
plt.figure(figsize=(10, 6))
plt.plot(connections, transfer, marker='o', linewidth=2, markersize=6, label='Совокупная пропускная способность, (Мб/c)')
plt.axhline(
    y=transfer_mean,
    color='red',
    linestyle='--',
    linewidth=2,
    label=f'Средняя пропускная способность: {transfer_mean:.0f} Мб/с'
)
plt.xscale('log', base=2)
plt.xlabel('Число соединений')
plt.ylabel('Совокупная пропускная способность (Мб/с)')
# plt.title('Совокупная пропускная способность сервера')
plt.grid(True, which="both", ls="--", linewidth=0.5)
plt.xticks(connections)
plt.legend()
plt.subplots_adjust(left=0.09, bottom=0.09, right=0.95, top=0.95)
plt.show()

# График 2: Средняя скорость на соединение
plt.figure(figsize=(10, 6))
plt.plot(connections, aver_transfer, marker='s', color='orange', linewidth=2, markersize=6, label='Средняя скорость на соединение (Мб/с)')
plt.axhline(
    y=aver_transfer_mean,
    color='red',
    linestyle='--',
    linewidth=2,
    label=f'Общая средняя скорость на соединение: {aver_transfer_mean:.0f} Мб/с'
)
plt.xscale('log', base=2)
plt.xlabel('Число соединений')
plt.ylabel('Средняя скорость на соединение (Мб/с)')
# plt.title('Средняя пропускная способность на одно соединение')
plt.grid(True, which="both", ls="--", linewidth=0.5)
plt.xticks(connections)
plt.legend()
plt.subplots_adjust(left=0.09, bottom=0.09, right=0.95, top=0.95)
plt.show()

# График 3: Время передачи
plt.figure(figsize=(10, 6))
plt.plot(connections, df['time'], marker='s', color='green', linewidth=2, markersize=6)
plt.xscale('log', base=2)
plt.xlabel('Число соединений')
plt.ylabel('Время передачи, мс')
plt.grid(True, which="both", ls="--", linewidth=0.5)
plt.xticks(connections)
# plt.title('Время передачи 100 МБ при параллельных соединениях')
plt.subplots_adjust(left=0.09, bottom=0.09, right=0.95, top=0.95)
plt.show()