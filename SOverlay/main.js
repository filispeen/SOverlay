const { app, BrowserWindow, screen, ipcMain } = require('electron');

let overlayWindow;
const browserViews = new Map();

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
      overlayWindow.webContents.send('browser_frame', {
        uuid: source.uuid,
        dataUrl: image.toDataURL(),
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

ipcMain.on('visible_set', (event, sources) => {
  syncBrowserViews(sources);
});

app.whenReady().then(createOverlay);