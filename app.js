const ctx = document.getElementById('sensorChart').getContext('2d');
const maxPuntos = 20;

const chart = new Chart(ctx, {
  type: 'line',
  data: {
    labels: [],
    datasets: [
      {
        label: 'Lectura del Sensor (%)',
        data: [],
        borderColor: '#2196F3',
        backgroundColor: 'rgba(33, 150, 243, 0.1)',
        fill: true,
        tension: 0.3
      },
      {
        label: 'Umbral Límite (%)',
        data: [],
        borderColor: '#FF9800',
        borderDash: [5, 5],
        pointRadius: 0,
        fill: false
      }
    ]
  },
  options: {
    responsive: true,
    scales: { y: { min: 0, max: 100 } }
  }
});

const umbralInput = document.getElementById('umbral');
const umbralValDisplay = document.getElementById('umbral-val');
const sensorValDisplay = document.getElementById('sensor-val');
const statusBanner = document.getElementById('status');

let contadorTiempo = 0;

function sonarAlarma() {
  try {
    const audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    osc.type = 'sawtooth';
    osc.frequency.setValueAtTime(880, audioCtx.currentTime);
    gain.gain.setValueAtTime(0.1, audioCtx.currentTime);
    osc.connect(gain);
    gain.connect(audioCtx.destination);
    osc.start();
    osc.stop(audioCtx.currentTime + 0.5);
  } catch (e) {
    console.log("Audio esperando interacción del usuario.");
  }
}

function leerSensorSimulado() {
  const base = (Date.now() / 100) % 100;
  const ruido = (Math.random() - 0.5) * 10;
  let valor = Math.min(Math.max(base + ruido, 0), 100);
  return Math.round(valor * 10) / 10;
}

umbralInput.addEventListener('input', (e) => {
  umbralValDisplay.textContent = e.target.value;
});

setInterval(() => {
  const valorSensor = leerSensorSimulado();
  const umbralActual = parseFloat(umbralInput.value);

  contadorTiempo++;
  sensorValDisplay.textContent = valorSensor;

  if (chart.data.labels.length >= maxPuntos) {
    chart.data.labels.shift();
    chart.data.datasets[0].data.shift();
    chart.data.datasets[1].data.shift();
  }

  chart.data.labels.push(`${contadorTiempo}s`);
  chart.data.datasets[0].data.push(valorSensor);
  chart.data.datasets[1].data.push(umbralActual);

  if (valorSensor >= umbralActual) {
    statusBanner.className = 'status-banner alarm';
    statusBanner.innerHTML = `🚨 ¡ALARMA DISPARADA! Nivel actual: ${valorSensor}% (Límite: ${umbralActual}%)`;
    chart.data.datasets[0].borderColor = '#f44336';
    sonarAlarma();
  } else {
    statusBanner.className = 'status-banner normal';
    statusBanner.innerHTML = `✅ Estado Normal. Nivel actual: ${valorSensor}%`;
    chart.data.datasets[0].borderColor = '#2196F3';
  }

  chart.update();
}, 1000);
