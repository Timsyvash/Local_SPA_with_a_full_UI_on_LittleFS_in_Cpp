document.addEventListener("DOMContentLoaded", () => {
    const statusEl = document.getElementById("status");
    statusEl.innerText = "Застосунок успішно завантажено з LittleFS!";

    // Опитуємо сервер кожну секунду для отримання відстані та аптайму
    setInterval(fetchServerData, 1000);
});

function fetchServerData() {
    fetch('/api/data')
        .then(response => {
            if (!response.ok) {
                throw new Error('Помилка мережі');
            }
            return response.json();
        })
        .then(data => {
            const statusEl = document.getElementById("status");
            const distText = data.distance >= 0 ? `${data.distance.toFixed(1)} см` : "Поза діапазоном";
            statusEl.innerText = `Аптайм: ${data.uptime} сек | Відстань: ${distText}`;
        })
        .catch(error => {
            console.error('Не вдалося отримати дані:', error);
            const statusEl = document.getElementById("status");
            statusEl.innerText = "Втрачено зв'язок з ESP8266...";
        });
}