const { app, BrowserWindow, screen, ipcMain } = require('electron');
const WebSocket = require('ws');
require('v8-compile-cache');

app.disableHardwareAcceleration();

let overlayWindow;
const browserViews = new Map();
const mediaViews = new Map();
let commandSocket = null;

function connectCommandSocket() {
  commandSocket = new WebSocket('ws://127.0.0.1:7853');
  commandSocket.on('close', () => {
    setTimeout(connectCommandSocket, 2000);
  });
  commandSocket.on('error', () => {});
}

function sendMediaCommand(uuid, action, extra) {
  if (!commandSocket || commandSocket.readyState !== WebSocket.OPEN) return;
  const payload = Object.assign({ type: 'media_command', uuid, action }, extra || {});
  commandSocket.send(JSON.stringify(payload));
}

function createOverlay() {
  const primaryDisplay = screen.getPrimaryDisplay();
  const { width, height } = primaryDisplay.size;
  const { x, y } = primaryDisplay.bounds;

  overlayWindow = new BrowserWindow({
    x: x,
    y: y,
    width: width,
    height: height,
    transparent: true,
    frame: false,
    alwaysOnTop: true,
    skipTaskbar: true,
    hasShadow: false,
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false,
      webviewTag: true
    }
  });

  overlayWindow.setIgnoreMouseEvents(true);
  overlayWindow.loadFile('SOverlay/index.html');
  overlayWindow.webContents.openDevTools({ mode: 'detach' });
  console.log('Overlay window created and loaded.');
}

function applyCssDirect(win, uuid, css) {
  const escaped = css.replace(/\\/g, '\\\\').replace(/`/g, '\\`').replace(/\$/g, '\\$');
  win.webContents.executeJavaScript(`
    (() => {
      let tag = document.getElementById('__soverlay_css__');
      if (!tag) {
        tag = document.createElement('style');
        tag.id = '__soverlay_css__';
        document.head.appendChild(tag);
      }
      tag.textContent = \`${escaped}\`;
      return { applied: true, bodyBg: getComputedStyle(document.body).backgroundColor };
    })()
  `).then(result => {
    console.log('[soverlay] applyCssDirect for', uuid, JSON.stringify(result));
    win.webContents.invalidate();
  }).catch(err => {
    console.log('[soverlay] applyCssDirect FAILED for', uuid, err);
  });
}

function updateBrowserView(source) {
  let entry = browserViews.get(source.uuid);

  if (!entry) {
    const win = new BrowserWindow({
      show: false,
      width: source.source_width,
      height: source.source_height,
      transparent: true,
      frame: false,
      webPreferences: {
        offscreen: true,
        backgroundThrottling: false
      }
    });

    entry = {
      win,
      url: source.browser_url,
      css: '',
      cssKey: null,
      source_width: source.source_width,
      source_height: source.source_height
    };
    browserViews.set(source.uuid, entry);

    win.webContents.setFrameRate(30);

    win.webContents.on('dom-ready', () => {
      console.log('[soverlay] dom-ready for', source.uuid, 'css_len:', entry.css.length);
      if (entry.css) {
        applyCssDirect(win, source.uuid, entry.css);
      }
    });

    win.loadURL(source.browser_url);

    win.webContents.on('paint', (event, dirty, image) => {
      if (!overlayWindow || overlayWindow.isDestroyed()) return;
      console.log('[soverlay] paint for', source.uuid, 'size:', image.getSize());
      const size = image.getSize();
      overlayWindow.webContents.send('browser_frame', {
        uuid: source.uuid,
        bitmap: image.getBitmap(),
        frame_width: size.width,
        frame_height: size.height,
        source_width: entry.source_width,
        source_height: entry.source_height
      });
    });
  } else if (entry.url !== source.browser_url) {
    entry.url = source.browser_url;
    entry.cssKey = null;
    entry.win.loadURL(source.browser_url);
  }

  entry.source_width = source.source_width;
  entry.source_height = source.source_height;

  const newCss = source.browser_css || '';
  if (entry.css !== newCss) {
    console.log('[soverlay] css changed for', source.uuid, 'new_len:', newCss.length, 'loading:', entry.win.webContents.isLoadingMainFrame());
    entry.css = newCss;
    if (!entry.win.webContents.isLoadingMainFrame()) {
      applyCssDirect(entry.win, source.uuid, entry.css);
    }
  }

  if (entry.win.getContentSize()[0] !== source.source_width ||
      entry.win.getContentSize()[1] !== source.source_height) {
    entry.win.setContentSize(source.source_width, source.source_height);
  }
}

function removeBrowserView(uuid) {
  const entry = browserViews.get(uuid);
  if (entry) {
    if (!entry.win.isDestroyed()) entry.win.destroy();
    browserViews.delete(uuid);
  }
}

function syncBrowserViews(sources) {
  const activeUuids = new Set(sources.filter(s => s.source_kind === 'browser_source').map(s => s.uuid));

  for (const uuid of Array.from(browserViews.keys())) {
    if (!activeUuids.has(uuid)) {
      removeBrowserView(uuid);
    }
  }

  sources.forEach(s => {
    if (s.source_kind === 'browser_source' && s.browser_url) {
      updateBrowserView(s);
    }
  });
}

function updateMediaView(source) {
  let entry = mediaViews.get(source.uuid);

  if (!entry) {
    const win = new BrowserWindow({
      show: false,
      width: source.source_width || 1280,
      height: source.source_height || 720,
      transparent: true,
      frame: false,
      webPreferences: {
        offscreen: true,
        backgroundThrottling: false,
        nodeIntegration: true,
        contextIsolation: false
      }
    });

    entry = {
      win,
      file: source.media_file,
      source_width: source.source_width,
      source_height: source.source_height,
      lastPaintAt: 0
    };
    mediaViews.set(source.uuid, entry);

    win.webContents.setFrameRate(24);
    win.loadFile('SOverlay/media-player.html');

    win.webContents.on('did-finish-load', () => {
      win.webContents.send('load_media', {
        file: entry.file,
        loop: entry.loop,
        currentTimeMs: typeof source.media_time_ms === 'number' ? source.media_time_ms : 0
      });
    });

    win.webContents.on('paint', (event, dirty, image) => {
      if (!overlayWindow || overlayWindow.isDestroyed()) return;
      const now = Date.now();
      if (now - entry.lastPaintAt < 33) return;
      entry.lastPaintAt = now;
      const size = image.getSize();
      overlayWindow.webContents.send('browser_frame', {
        uuid: source.uuid,
        bitmap: image.toBitmap(),
        frame_width: size.width,
        frame_height: size.height,
        source_width: entry.source_width,
        source_height: entry.source_height
      });
    });
  } else if (entry.file !== source.media_file) {
    entry.file = source.media_file;
    entry.win.webContents.send('load_media', {
      file: entry.file,
      loop: entry.loop,
      currentTimeMs: typeof source.media_time_ms === 'number' ? source.media_time_ms : 0
    });
  }

  entry.source_width = source.source_width;
  entry.source_height = source.source_height;

  if (entry.loop !== source.media_loop) {
    entry.loop = source.media_loop;
    entry.win.webContents.send('media_command', { action: 'set_loop', value: entry.loop });
  }

  if (entry.win.getContentSize()[0] !== source.source_width ||
      entry.win.getContentSize()[1] !== source.source_height) {
    if (source.source_width > 0 && source.source_height > 0) {
      entry.win.setContentSize(source.source_width, source.source_height);
    }
  }

  if (entry.lastState !== source.media_state) {
    entry.lastState = source.media_state;
    const action = source.media_state === 'playing' ? 'play'
      : source.media_state === 'paused' ? 'pause'
      : source.media_state === 'stopped' ? 'stop'
      : null;
    if (action) {
      entry.win.webContents.send('media_command', { action });
    }
  }

  if (typeof source.media_time_ms === 'number' && source.media_seek === true) {
    entry.win.webContents.send('media_command', { action: 'seek_ms', value: source.media_time_ms });
  }
  if (typeof source.media_time_ms === 'number') {
    entry.lastKnownTimeMs = source.media_time_ms;
  }
}

function removeMediaView(uuid) {
  const entry = mediaViews.get(uuid);
  if (entry) {
    if (!entry.win.isDestroyed()) entry.win.destroy();
    mediaViews.delete(uuid);
  }
}

function syncMediaViews(sources) {
  const activeUuids = new Set(sources.filter(s => s.source_kind === 'ffmpeg_source').map(s => s.uuid));

  for (const uuid of Array.from(mediaViews.keys())) {
    if (!activeUuids.has(uuid)) {
      removeMediaView(uuid);
    }
  }

  sources.forEach(s => {
    if (s.source_kind === 'ffmpeg_source' && s.media_file) {
      updateMediaView(s);
    }
  });
}

function removeAllBrowserViews() {
  for (const uuid of Array.from(browserViews.keys())) {
    removeBrowserView(uuid);
  }
}

function removeAllMediaViews() {
  for (const uuid of Array.from(mediaViews.keys())) {
    removeMediaView(uuid);
  }
}

ipcMain.on('visible_set', (event, sources) => {
  syncBrowserViews(sources);
  syncMediaViews(sources);
});

ipcMain.on('overlay_disconnected', () => {
  removeAllBrowserViews();
  removeAllMediaViews();
});

ipcMain.on('media_control', (event, { uuid, action, seek_ms, volume }) => {
  sendMediaCommand(uuid, action, { seek_ms, volume });
});

app.whenReady().then(() => {
  createOverlay();
  connectCommandSocket();
});