(function () {
  let audio, playBtn, canvas, ctx, triggerOverlay;
  let audioContext, analyser;
  let isReady = false;

  // Инициализация после загрузки DOM
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

  function init() {
    audio = document.getElementById('music-player');
    playBtn = document.getElementById('play-btn');
    canvas = document.getElementById('visualizer');
    triggerOverlay = document.getElementById('sound-trigger');

    // Если элементов нет — выходим (без ошибок)
    if (!audio || !playBtn || !canvas || !triggerOverlay) return;

    ctx = canvas.getContext('2d');

    // Адаптивность canvas
    const resizeCanvas = () => {
      const container = canvas.parentElement;
      canvas.width = container.clientWidth;
      canvas.height = 200;
    };
    resizeCanvas();
    window.addEventListener('resize', resizeCanvas);

    // Первое взаимодействие → запуск звука
    triggerOverlay.addEventListener('click', startExperience);

    // Управление паузой
    playBtn.addEventListener('click', togglePlayback);

    // Синхронизация с системными кнопками (например, панель macOS)
    audio.addEventListener('play', () => {
      if (isReady) {
        playBtn.textContent = '⏸';
        if (!window.visualizerRunning) draw();
      }
    });

    audio.addEventListener('pause', () => {
      if (isReady) playBtn.textContent = '▶';
    });
  }

  function startExperience() {
    if (isReady) return;

    // Инициализация Web Audio API
    const AudioContext = window.AudioContext || window.webkitAudioContext;
    audioContext = new AudioContext();
    analyser = audioContext.createAnalyser();
    const source = audioContext.createMediaElementSource(audio);
    source.connect(analyser);
    analyser.connect(audioContext.destination);

    analyser.fftSize = 512;
    isReady = true;

    // Запуск аудио
    audio.play()
      .then(() => {
        triggerOverlay.classList.add('hidden');
        playBtn.disabled = false;
        playBtn.textContent = '⏸';
        draw(); // запуск визуализации
      })
      .catch(err => {
        console.error('Не удалось воспроизвести аудио:', err);
        triggerOverlay.innerHTML = '<span>⚠️ Аудио недоступно</span>';
      });
  }

  function togglePlayback() {
    if (!isReady) return;

    if (audio.paused) {
      audio.play().catch(console.warn);
    } else {
      audio.pause();
    }
    // Текст кнопки обновляется через события 'play'/'pause'
  }

  // === ОСНОВНАЯ ФУНКЦИЯ ВИЗУАЛИЗАЦИИ ===
  function draw() {
    window.visualizerRunning = true;
    if (audio.paused || !isReady) {
      window.visualizerRunning = false;
      return;
    }

    requestAnimationFrame(draw);

    const width = canvas.width;
    const height = canvas.height;
    ctx.clearRect(0, 0, width, height);

    // Получаем данные спектра ОДИН раз
    const dataArray = new Uint8Array(analyser.frequencyBinCount);
    analyser.getByteFrequencyData(dataArray);

    const barCount = Math.min(128, dataArray.length);
    const barWidth = width / barCount;

    // Энергия басов (0–10 бинов)
    const bassEnergy = dataArray.slice(0, 10).reduce((a, b) => a + b, 0) / 10;

    for (let i = 0; i < barCount; i++) {
      let value = dataArray[i];
      // Усиление низких частот
      if (i < barCount) {
       const boost = 1 + (bassEnergy / 4096); // чуть сильнее, но узкая зона
       value = Math.min(255, value * boost);
      }

      const barHeight = (value / 255) * height;
      const x = i * barWidth;
      
      // Базовые цвета (как раньше)
      let r = 255;
      let g = Math.max(0, 40 - bassEnergy / 4);
      let b = Math.max(0, 20 - bassEnergy / 6);
      
      if (value > 230) {
        r = 160;
        g = 10;
        b = 210;
      }
      
      // === ВЕРТИКАЛЬНЫЙ ГРАДИЕНТ ДЛЯ СТОЛБЦА ===
      const gradient = ctx.createLinearGradient(x, height, x, height - barHeight);
      // Низ: тёмно-красный / почти чёрный
      gradient.addColorStop(0, `rgb(${Math.max(60, r * 0.3)}, ${g * 0.2}, ${b * 0.2})`);
      // Верх: яркий акцент
      gradient.addColorStop(1, `rgb(${r}, ${Math.min(255, g + 100)}, ${Math.min(255, b + 100)})`);
      
      ctx.fillStyle = gradient;
      ctx.fillRect(x, height - barHeight, barWidth - 1, barHeight);
      
      // Тень (по желанию)
      if (barHeight > 60) {
        ctx.shadowColor = 'rgba(247, 0, 194, 0.8)';
        ctx.shadowBlur = 8;
      } else {
        ctx.shadowBlur = 0;
      }
    }
    ctx.shadowBlur = 0; // сброс тени после цикла
  }
})();