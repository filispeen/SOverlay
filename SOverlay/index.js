const { ipcRenderer } = require('electron');

let socket;
let sources = {};
let reconnecting = false;
const panel = document.getElementById('overlay-div');
const statuspanel = document.getElementById('status-panel');
const RECONNECT_DELAY = 2000;
const DEBUG = true;
const browserTransforms = new Map();

ipcRenderer.on('browser_frame', (event, frame) => {
  let item = panel.querySelector(`[data-uuid="${frame.uuid}"]`);
  let content = null;

  if (!item) {
    item = document.createElement('div');
    item.className = 'preview-item';
    item.dataset.uuid = frame.uuid;

    content = document.createElement('canvas');
    content.className = 'preview-content';
    item.appendChild(content);
    panel.appendChild(item);
  } else {
    content = item.querySelector('.preview-content');
  }

  drawBitmapFrame(content, frame);
  content.style.width = frame.source_width + 'px';
  content.style.height = frame.source_height + 'px';

  applyBrowserTransform(item, content, frame);
});

function drawBitmapFrame(canvas, frame) {
  const w = frame.frame_width;
  const h = frame.frame_height;
  if (!w || !h) return;

  if (canvas.width !== w || canvas.height !== h) {
    canvas.width = w;
    canvas.height = h;
  }

  const ctx = canvas.getContext('2d');
  const imageData = ctx.createImageData(w, h);

  const srcBytes = frame.bitmap;
  const src32 = new Uint32Array(srcBytes.buffer, srcBytes.byteOffset, w * h);
  const dst32 = new Uint32Array(imageData.data.buffer);

  for (let i = 0; i < src32.length; i++) {
    const px = src32[i];
    const b = px & 0xff;
    const g = (px >>> 8) & 0xff;
    const r = (px >>> 16) & 0xff;
    const a = (px >>> 24) & 0xff;
    dst32[i] = (a << 24) | (b << 16) | (g << 8) | r;
  }

  ctx.putImageData(imageData, 0, 0);
}

function applyBrowserTransform(item, content, frame) {
  const t = browserTransforms.get(frame.uuid);
  if (!t) return;

  if (typeof t.z_index === 'number') {
    item.style.zIndex = String(t.z_index);
  }

  const screenWidth = window.innerWidth;
  const screenHeight = window.innerHeight;

  const boxWidth = t.transform.width * screenWidth;
  const boxHeight = t.transform.height * screenHeight;

  item.style.left = (t.transform.x * screenWidth) + 'px';
  item.style.top = (t.transform.y * screenHeight) + 'px';
  item.style.width = boxWidth + 'px';
  item.style.height = boxHeight + 'px';

  const anchorPxX = (t.anchor_x || 0) * screenWidth - t.transform.x * screenWidth;
  const anchorPxY = (t.anchor_y || 0) * screenHeight - t.transform.y * screenHeight;
  item.style.transformOrigin = `${anchorPxX}px ${anchorPxY}px`;
  item.style.transform = `rotate(${t.rotation || 0}deg) scaleX(${t.flip_x ? -1 : 1}) scaleY(${t.flip_y ? -1 : 1})`;

  const scaleX = boxWidth / frame.source_width;
  const scaleY = boxHeight / frame.source_height;

  const BOUNDS_NONE = 0;
  const BOUNDS_STRETCH = 1;
  const BOUNDS_SCALE_OUTER = 3;
  const preserveAspect = t.bounds_type !== BOUNDS_STRETCH && t.bounds_type !== BOUNDS_NONE;

  let finalScaleX = scaleX;
  let finalScaleY = scaleY;
  let offsetX = 0;
  let offsetY = 0;

  if (preserveAspect) {
    const uniformScale = t.bounds_type === BOUNDS_SCALE_OUTER
      ? Math.max(scaleX, scaleY)
      : Math.min(scaleX, scaleY);
    finalScaleX = uniformScale;
    finalScaleY = uniformScale;
    offsetX = (boxWidth - frame.source_width * uniformScale) / 2;
    offsetY = (boxHeight - frame.source_height * uniformScale) / 2;
  }

  content.style.transformOrigin = 'top left';
  content.style.transform = `translate(${offsetX}px, ${offsetY}px) scale(${finalScaleX}, ${finalScaleY})`;
}


if (statuspanel.style.display === 'none' && DEBUG) {
    statuspanel.style.display = 'block';
}

function connect() {
    socket = new WebSocket('ws://127.0.0.1:7853');

    // events
    socket.addEventListener('open', () => {
        console.log('Connected to the server');
        statuspanel.innerHTML = 'Connected to the server <span class="loader success"></span>';
        reconnecting = true;
    });

    socket.addEventListener('error', (error) => {
        console.error('WebSocket error:', error);
    });

    socket.addEventListener('message', (event) => {
        const data = JSON.parse(event.data);
        //console.log('Received data:', data);
        data.sources.forEach(source => {
            console.log(`Source: ${source.name}, Kind: ${source.source_kind}, source_w:${source.source_width} source_h:${source.source_height}, bounds_type:${source.bounds_type}, css_len:${(source.browser_css || '').length}, Transform: x:${source.transform.x.toFixed(3)} y:${source.transform.y.toFixed(3)} w:${source.transform.width.toFixed(3)} h:${source.transform.height.toFixed(3)}`);
        });
        
        if (data.type === 'visible_set') {
            renderstatuspanel(data.sources);
            renderOverlay(data.sources);
            updateBrowserTransforms(data.sources);
            ipcRenderer.send('visible_set', data.sources);
        }
    });

    socket.addEventListener('close', () => {
        statuspanel.style.fontSize = '32px'
        if (!reconnecting) {
            console.log('Server not found. Trying to connect...');
            //statuspanel.innerHTML = 'Waiting until server appears...' + "<span class=\"loader\"></span>";
        } else {
            console.log('Disconnected from the server. Reconnecting...');
            statuspanel.innerHTML = 'Waiting until server appears...' + "<span class=\"loader\"></span>";
        }

        statuspanel.style.display = 'block';
        statuspanel.style.animation = 'fadeIn'; 
        statuspanel.style.animationDuration = '0.5s'; 
        sources = {};
        panel.innerHTML = '';
        browserTransforms.clear();
        ipcRenderer.send('overlay_disconnected');
        setTimeout(connect, RECONNECT_DELAY);
    });
}

function renderstatuspanel(sources) {
    if (!DEBUG) { 
        setTimeout(() => {
            statuspanel.style.animation = 'fadeOut'; 
            statuspanel.style.animationDuration = '0.5s'; 
            setTimeout(() => { statuspanel.style.display = 'none'; }, 500); 
        }, 5000);
        return 
    }
    statuspanel.style.fontSize = '12px';
    if (!sources || sources.length === 0) {
        statuspanel.textContent = 'No visible sources';
        return;
    }
    statuspanel.textContent = sources
        .map(s => {
            const base = `${s.name}\nx:${s.transform.x.toFixed(3)} y:${s.transform.y.toFixed(3)} w:${s.transform.width.toFixed(3)} h:${s.transform.height.toFixed(3)}`;
            if (s.source_kind === 'ffmpeg_source') {
                return `${base}\nmedia:${s.media_state} t:${s.media_time_ms}/${s.media_duration_ms}`;
            }
            return base;
        })
        .join('\n\n');
}

function updateBrowserTransforms(sources) {
    const activeUuids = new Set((sources || []).filter(s => s.source_kind === 'browser_source' || s.source_kind === 'ffmpeg_source').map(s => s.uuid));

    for (const uuid of Array.from(browserTransforms.keys())) {
        if (!activeUuids.has(uuid)) {
            browserTransforms.delete(uuid);
            const item = panel.querySelector(`[data-uuid="${uuid}"]`);
            if (item) panel.removeChild(item);
        }
    }

    (sources || []).forEach(s => {
        if (s.source_kind !== 'browser_source' && s.source_kind !== 'ffmpeg_source') return;
        browserTransforms.set(s.uuid, { transform: s.transform, bounds_type: s.bounds_type, rotation: s.rotation, flip_x: s.flip_x, flip_y: s.flip_y, anchor_x: s.anchor_x, anchor_y: s.anchor_y, z_index: s.z_index });

        const item = panel.querySelector(`[data-uuid="${s.uuid}"]`);
        if (item) {
            const content = item.querySelector('.preview-content');
            if (content) {
                applyBrowserTransform(item, content, { uuid: s.uuid, source_width: s.source_width, source_height: s.source_height });
            }
        }
    });
}

function renderOverlay(sources) {
    const activeUuids = new Set((sources || [])
        .filter(s => s.source_kind === 'image_source' || s.source_kind === 'browser_source' || s.source_kind === 'ffmpeg_source')
        .map(s => s.uuid));

    Array.from(panel.children).forEach(child => {
        if (!activeUuids.has(child.dataset.uuid)) {
            panel.removeChild(child);
        }
    });

    if (!sources) return;

    const screenWidth = window.innerWidth;
    const screenHeight = window.innerHeight;

    sources.forEach(s => {
        if (s.source_kind !== 'image_source' || !s.image_file) return;

        let item = panel.querySelector(`[data-uuid="${s.uuid}"]`);
        let content = null;

        if (!item) {
            item = document.createElement('div');
            item.className = 'preview-item';
            item.dataset.uuid = s.uuid;

            content = document.createElement('img');
            content.src = 'file:///' + s.image_file.replace(/\\/g, '/');
            content.addEventListener('error', () => {
                console.error(`Failed to load image for ${s.name}: ${s.image_file}`);
            });

            content.className = 'preview-content';
            item.appendChild(content);
            panel.appendChild(item);
        } else {
            content = item.querySelector('.preview-content');
        }

        const boxWidth = s.transform.width * screenWidth;
        const boxHeight = s.transform.height * screenHeight;

        item.style.left = (s.transform.x * screenWidth) + 'px';
        item.style.top = (s.transform.y * screenHeight) + 'px';
        item.style.width = boxWidth + 'px';
        item.style.height = boxHeight + 'px';
        if (typeof s.z_index === 'number') {
            item.style.zIndex = String(s.z_index);
        }

        const anchorPxX = (s.anchor_x || 0) * screenWidth - s.transform.x * screenWidth;
        const anchorPxY = (s.anchor_y || 0) * screenHeight - s.transform.y * screenHeight;
        item.style.transformOrigin = `${anchorPxX}px ${anchorPxY}px`;
        item.style.transform = `rotate(${s.rotation || 0}deg) scaleX(${s.flip_x ? -1 : 1}) scaleY(${s.flip_y ? -1 : 1})`;

        if (content && s.source_width > 0 && s.source_height > 0) {
            content.style.width = s.source_width + 'px';
            content.style.height = s.source_height + 'px';

            const scaleX = boxWidth / s.source_width;
            const scaleY = boxHeight / s.source_height;

            const BOUNDS_NONE = 0;
            const BOUNDS_STRETCH = 1;
            const BOUNDS_SCALE_OUTER = 3;
            const preserveAspect = s.bounds_type !== undefined &&
                s.bounds_type !== BOUNDS_STRETCH &&
                s.bounds_type !== BOUNDS_NONE;

            let finalScaleX = scaleX;
            let finalScaleY = scaleY;
            let offsetX = 0;
            let offsetY = 0;

            if (preserveAspect) {
                const uniformScale = s.bounds_type === BOUNDS_SCALE_OUTER
                    ? Math.max(scaleX, scaleY)
                    : Math.min(scaleX, scaleY);
                finalScaleX = uniformScale;
                finalScaleY = uniformScale;
                offsetX = (boxWidth - s.source_width * uniformScale) / 2;
                offsetY = (boxHeight - s.source_height * uniformScale) / 2;
            }

            content.style.transformOrigin = 'top left';
            content.style.transform = `translate(${offsetX}px, ${offsetY}px) scale(${finalScaleX}, ${finalScaleY})`;
        }
    });
}

connect();