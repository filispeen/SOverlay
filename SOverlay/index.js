const socket = new WebSocket('ws://127.0.0.1:7853');

const sources = {}

// events
socket.addEventListener('open', () => {
    console.log('Connected to the server');
});

socket.addEventListener('error', (error) => {
    console.error('WebSocket error:', error);
});

socket.addEventListener('message', (event) => {
    const data = JSON.parse(event.data);
    console.log('Received data:', data);
    if (data.type === 'visible_set') {
        renderDebugPanel(data.sources);
    }
});

function renderDebugPanel(sources) {
    const panel = document.getElementById('debug-panel');
    if (!sources || sources.length === 0) {
        panel.textContent = 'No visible sources';
        return;
    }
    panel.textContent = sources
        .map(s => `${s.name}\nx:${s.transform.x.toFixed(3)} y:${s.transform.y.toFixed(3)} w:${s.transform.width.toFixed(3)} h:${s.transform.height.toFixed(3)}`)
        .join('\n\n');
}

socket.addEventListener('close', () => {
    console.log('Disconnected from the server');
    if (sources.length > 0) {
        sources.forEach(source => {
            source.remove();
        });
    }
});