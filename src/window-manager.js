const { BrowserWindow, screen } = require('electron');

class WindowManager {
  constructor() {
    this.windows = new Map();
    this.states = new Map();
  }

  _getScale(baseCx, baseCy) {
    const { width, height } = screen.getPrimaryDisplay().workAreaSize;
    if (!baseCx || !baseCy) return { scaleX: 1, scaleY: 1 };
    return { scaleX: width / baseCx, scaleY: height / baseCy };
  }

  _ensureWindow(sourceName) {
    let win = this.windows.get(sourceName);
    if (win && !win.isDestroyed()) return win;

    win = new BrowserWindow({
      width: 320,
      height: 240,
      frame: false,
      autoHideMenuBar: true,
      transparent: true,
      alwaysOnTop: true,
      skipTaskbar: true,
      show: false,
      webPreferences: {
        contextIsolation: false,
        nodeIntegration: true,
      },
    });

    win.setIgnoreMouseEvents(true, { forward: true });
    win.setOpacity(0);
    this.windows.set(sourceName, win);

    win.on('closed', () => {
      this.windows.delete(sourceName);
      this.states.delete(sourceName);
    });

    return win;
  }

  _getState(sourceName) {
    let state = this.states.get(sourceName);
    if (!state) {
      state = { url: null, enabled: true, showOnscreen: false, loaded: false };
      this.states.set(sourceName, state);
    }
    return state;
  }

  _applyVisibility(sourceName) {
    const win = this.windows.get(sourceName);
    const state = this.states.get(sourceName);
    if (!win || win.isDestroyed() || !state) return;

    const shouldBeVisible = state.enabled && state.showOnscreen && state.loaded;
    win.setOpacity(shouldBeVisible ? 1 : 0);
  }

  handleTransform(sourceName, payload) {
    const win = this._ensureWindow(sourceName);
    const state = this._getState(sourceName);

    if (payload.url && payload.url !== state.url) {
      state.url = payload.url;
      state.loaded = false;
      win.loadURL(payload.url).then(() => {
        state.loaded = true;
        this._applyVisibility(sourceName);
      }).catch((err) => {
        console.error(`[window-manager] failed to load URL for "${sourceName}":`, err.message);
      });
    } else if (!state.url && state.loaded) {
      state.loaded = true;
    }

    const { scaleX, scaleY } = this._getScale(payload.base_cx, payload.base_cy);
    const x = Math.round(payload.x * scaleX);
    const y = Math.round(payload.y * scaleY);
    const width = Math.max(1, Math.round(payload.cx * scaleX));
    const height = Math.max(1, Math.round(payload.cy * scaleY));

    win.setBounds({ x, y, width, height });
    this._applyVisibility(sourceName);
  }

  handleShowOnscreen(sourceName, value) {
    const win = this._ensureWindow(sourceName);
    const state = this._getState(sourceName);
    state.showOnscreen = value;
    this._applyVisibility(sourceName);
  }

  handleEnabled(sourceName, value) {
    const win = this._ensureWindow(sourceName);
    const state = this._getState(sourceName);
    state.enabled = value;
    this._applyVisibility(sourceName);
  }

  handleRemoved(sourceName) {
    const win = this.windows.get(sourceName);
    if (win && !win.isDestroyed()) {
      win.close();
    }
    this.windows.delete(sourceName);
    this.states.delete(sourceName);
  }

  closeAll() {
    for (const win of this.windows.values()) {
      if (!win.isDestroyed()) win.close();
    }
    this.windows.clear();
    this.states.clear();
  }

  getWindow(sourceName) {
    return this.windows.get(sourceName);
  }

  getAllNames() {
    return Array.from(this.windows.keys());
  }
}

module.exports = { WindowManager };
